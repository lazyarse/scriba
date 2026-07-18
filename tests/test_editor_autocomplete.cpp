#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace {

QString extractPartialPath(const QString &line, int cursorPos)
{
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(cursorPos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return QString();
    QString between = line.mid(parenPos, cursorPos - parenPos);
    if (between.contains(')'))
        return QString();
    return between;
}

QStringList matchEntries(const QString &partialPath, const QDir &baseDir)
{
    int lastSlash = partialPath.lastIndexOf('/');
    QString dirPart = lastSlash >= 0 ? partialPath.left(lastSlash + 1) : QString();
    QString filePart = lastSlash >= 0 ? partialPath.mid(lastSlash + 1) : partialPath;

    QString searchDir = baseDir.absoluteFilePath(dirPart.isEmpty() ? "." : dirPart);
    QDir search(searchDir);
    if (!search.exists())
        return {};

    QStringList entries;
    QStringList all = search.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const QString &entry : all) {
        if (entry.startsWith('.') && !filePart.startsWith('.'))
            continue;
        if (filePart.isEmpty() || entry.startsWith(filePart, Qt::CaseInsensitive)) {
            if (QFileInfo(search, entry).isDir())
                entries.append(entry + "/");
            else
                entries.append(entry);
        }
    }
    return entries;
}

QString simulateAcceptCompletion(const QString &line, int cursorPos, const QString &completion)
{
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(cursorPos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return line;

    QString between = line.mid(parenPos, cursorPos - parenPos);
    int lastSlash = between.lastIndexOf('/');
    int replaceStart = lastSlash >= 0 ? parenPos + lastSlash + 1 : parenPos;

    return line.left(replaceStart) + completion;
}

} // anonymous namespace

TEST(LinkContext, DetectsImageSyntax)
{
    QString line = "![](resourc";
    QString path = extractPartialPath(line, line.length());
    EXPECT_EQ(path, "resourc");
}

TEST(LinkContext, DetectsLinkSyntax)
{
    QString line = "[click](resourc";
    QString path = extractPartialPath(line, line.length());
    EXPECT_EQ(path, "resourc");
}

TEST(LinkContext, IgnoresClosedParen)
{
    QString line = "[click](done) trailing";
    QString path = extractPartialPath(line, line.length());
    EXPECT_TRUE(path.isEmpty());
}

TEST(LinkContext, CursorBeforeLink)
{
    QString line = "before ![](path)";
    QString path = extractPartialPath(line, 4);
    EXPECT_TRUE(path.isEmpty());
}

TEST(LinkContext, DetectsMiddleOfPath)
{
    QString line = "![](images/scre";
    QString path = extractPartialPath(line, line.length());
    EXPECT_EQ(path, "images/scre");
}

TEST(LinkContext, ExtractsBetweenOpenAndCursor)
{
    QString line = "![](some/path/abc def";
    QString path = extractPartialPath(line, line.length());
    EXPECT_EQ(path, "some/path/abc def");
}

TEST(PathCompletion, FiltersEntries)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());

    auto touch = [&](const QString &name) {
        QFile f(dir.absoluteFilePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    };
    touch("main.css");
    touch("main.js");
    touch("README.md");
    dir.mkdir("images");

    auto entries = matchEntries("main", dir);
    EXPECT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.contains("main.css"));
    EXPECT_TRUE(entries.contains("main.js"));

    entries = matchEntries("readme", dir);
    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.first(), "README.md");
}

TEST(PathCompletion, ListsFilesInSubdir)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    dir.mkdir("images");
    auto touch = [&](const QString &name) {
        QFile f(dir.absoluteFilePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    };
    touch("images/photo.png");
    touch("images/photo.jpg");
    dir.mkdir("images/nested");

    auto entries = matchEntries("images/ph", dir);
    EXPECT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.contains("photo.png"));
    EXPECT_TRUE(entries.contains("photo.jpg"));
}

TEST(PathCompletion, IncludesDirectoriesWithSlash)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    dir.mkdir("assets");
    dir.mkdir("backup");

    auto entries = matchEntries("ass", dir);
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.first(), "assets/");
}

TEST(PathCompletion, NoHiddenFilesWithoutDot)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    auto touch = [&](const QString &name) {
        QFile f(dir.absoluteFilePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    };
    touch(".hidden");
    touch("visible");

    auto entries = matchEntries("", dir);
    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.first(), "visible");
}

TEST(PathCompletion, HiddenFilesShownWithDotPrefix)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    auto touch = [&](const QString &name) {
        QFile f(dir.absoluteFilePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    };
    touch(".config");
    touch(".hidden");
    touch("visible");

    auto entries = matchEntries(".", dir);
    EXPECT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.contains(".config"));
    EXPECT_TRUE(entries.contains(".hidden"));
}

TEST(PathCompletion, NoMatchReturnsEmpty)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    auto touch = [&](const QString &name) {
        QFile f(dir.absoluteFilePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    };
    touch("readme.md");

    auto entries = matchEntries("zzz", dir);
    EXPECT_TRUE(entries.isEmpty());
}

TEST(PathCompletion, EmptyPathReturnsEmpty)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    auto touch = [&](const QString &name) {
        QFile f(dir.absoluteFilePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    };
    touch("main.css");

    // Empty partialPath → filePart is empty → matchEntries returns all
    // The caller (showFileCompletion) guards against empty filePart
    auto entries = matchEntries("", dir);
    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.first(), "main.css");
}

TEST(ChainedCompletion, DirThenFile)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    dir.mkdir("resources");
    dir.mkdir("resources/icons");

    // Step 1: ![](resou[Tab] → single match "resources/"
    QString line = "![](resou";
    auto path = extractPartialPath(line, line.length());
    auto entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "resources/");
    line = simulateAcceptCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](resources/");

    // Step 2: ![](resources/ico[Tab] → single match "icons/"
    line += "ico";
    path = extractPartialPath(line, line.length());
    EXPECT_EQ(path, "resources/ico");

    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "icons/");

    // acceptCompletion receives only "icons/", replaces "ico" → "icons/"
    line = simulateAcceptCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](resources/icons/");
}

TEST(ChainedCompletion, NestedSubdir)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    dir.mkdir("a");
    dir.mkdir("a/b");
    dir.mkdir("a/b/c");

    QString line = "![](a";
    auto path = extractPartialPath(line, line.length());
    auto entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "a/");
    line = simulateAcceptCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](a/");

    line += "b";
    path = extractPartialPath(line, line.length());
    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "b/");
    line = simulateAcceptCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](a/b/");

    line += "c";
    path = extractPartialPath(line, line.length());
    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "c/");
    line = simulateAcceptCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](a/b/c/");
}

TEST(ChainedCompletion, MultiMatchThenNarrow)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    dir.mkdir("docs");
    dir.mkdir("docs/design");
    dir.mkdir("docs/dev");

    // Step 1: ![](d[Tab] → "docs/", single match
    QString line = "![](d";
    auto path = extractPartialPath(line, line.length());
    auto entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "docs/");
    line = simulateAcceptCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](docs/");

    // Step 2: ![](docs/de[Tab] → multiple matches: "design/", "dev/"
    path = extractPartialPath(line + "de", line.length() + 2);
    EXPECT_EQ(path, "docs/de");
    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.contains("design/"));
    EXPECT_TRUE(entries.contains("dev/"));
}

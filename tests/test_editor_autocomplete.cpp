#include <gtest/gtest.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "StaticHelpers.h"

namespace {

QString extractPartialPath(const QString &line, int cursorPos)
{
    QString path;
    extractLinkPath(line, cursorPos, path);
    return path;
}

QStringList matchEntries(const QString &partialPath, const QDir &baseDir)
{
    return matchFileEntries(partialPath, baseDir).entries;
}

QString acceptLinkCompletion(const QString &line, int cursorPos, const QString &completion)
{
    int replaceStart = linkPathReplaceStart(line, cursorPos);
    if (replaceStart < 0)
        return line;
    return line.left(replaceStart) + completion;
}

QString acceptHtmlCompletion(const QString &line, int cursorPos, const QString &completion)
{
    int replaceStart = htmlPathReplaceStart(line, cursorPos);
    if (replaceStart < 0)
        return line;
    return line.left(replaceStart) + completion;
}

void touch(QDir &dir, const QString &name)
{
    QFile f(dir.absoluteFilePath(name));
    (void)f.open(QIODevice::WriteOnly);
    f.close();
}

} // anonymous namespace

TEST(LinkContext, DetectsImageSyntax)
{
    QString line = "![](resourc";
    EXPECT_EQ(extractPartialPath(line, line.length()), "resourc");
}

TEST(LinkContext, DetectsLinkSyntax)
{
    QString line = "[click](resourc";
    EXPECT_EQ(extractPartialPath(line, line.length()), "resourc");
}

TEST(LinkContext, IgnoresClosedParen)
{
    QString line = "[click](done) trailing";
    EXPECT_TRUE(extractPartialPath(line, line.length()).isEmpty());
}

TEST(LinkContext, CursorBeforeLink)
{
    QString line = "before ![](path)";
    EXPECT_TRUE(extractPartialPath(line, 4).isEmpty());
}

TEST(LinkContext, DetectsMiddleOfPath)
{
    QString line = "![](images/scre";
    EXPECT_EQ(extractPartialPath(line, line.length()), "images/scre");
}

TEST(LinkContext, ExtractsBetweenOpenAndCursor)
{
    QString line = "![](some/path/abc def";
    EXPECT_EQ(extractPartialPath(line, line.length()), "some/path/abc def");
}

TEST(LinkReplaceStart, ReplacesFileNamePart)
{
    QString line = "![](images/scre";
    EXPECT_EQ(linkPathReplaceStart(line, line.length()), 11);
}

TEST(LinkReplaceStart, ReplacesWholePathWithoutSlash)
{
    QString line = "![](scre";
    EXPECT_EQ(linkPathReplaceStart(line, line.length()), 4);
}

TEST(LinkReplaceStart, NotInLinkReturnsMinusOne)
{
    EXPECT_EQ(linkPathReplaceStart("plain text", 5), -1);
}

TEST(PathCompletion, FiltersEntries)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());

    touch(dir, "main.css");
    touch(dir, "main.js");
    touch(dir, "README.md");
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
    touch(dir, "images/photo.png");
    touch(dir, "images/photo.jpg");
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
    touch(dir, ".hidden");
    touch(dir, "visible");

    auto entries = matchEntries("", dir);
    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.first(), "visible");
}

TEST(PathCompletion, HiddenFilesShownWithDotPrefix)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    touch(dir, ".config");
    touch(dir, ".hidden");
    touch(dir, "visible");

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
    touch(dir, "readme.md");

    auto entries = matchEntries("zzz", dir);
    EXPECT_TRUE(entries.isEmpty());
}

TEST(PathCompletion, EmptyPathListsAllNonHiddenEntries)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    touch(dir, "main.css");

    // Empty partialPath → filePart is empty → every non-hidden entry matches.
    // The callers (Editor) guard against an empty partial path.
    auto entries = matchEntries("", dir);
    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries.first(), "main.css");
}

TEST(PathCompletion, SortsPrefixMatchesFirst)
{
    QTemporaryDir tmpDir;
    ASSERT_TRUE(tmpDir.isValid());
    QDir dir(tmpDir.path());
    touch(dir, "xmain.css");
    touch(dir, "main.js");
    touch(dir, "main.css");

    auto entries = matchEntries("main", dir);
    ASSERT_EQ(entries.size(), 3);
    EXPECT_EQ(entries.first(), "main.css") << "prefix matches must sort before substring matches";
    EXPECT_EQ(entries.at(1), "main.js");
    EXPECT_EQ(entries.at(2), "xmain.css");
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
    line = acceptLinkCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](resources/");

    // Step 2: ![](resources/ico[Tab] → single match "icons/"
    line += "ico";
    path = extractPartialPath(line, line.length());
    EXPECT_EQ(path, "resources/ico");

    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "icons/");

    // acceptCompletion receives only "icons/", replaces "ico" → "icons/"
    line = acceptLinkCompletion(line, line.length(), entries.first());
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
    line = acceptLinkCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](a/");

    line += "b";
    path = extractPartialPath(line, line.length());
    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "b/");
    line = acceptLinkCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](a/b/");

    line += "c";
    path = extractPartialPath(line, line.length());
    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries.first(), "c/");
    line = acceptLinkCompletion(line, line.length(), entries.first());
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
    line = acceptLinkCompletion(line, line.length(), entries.first());
    EXPECT_EQ(line, "![](docs/");

    // Step 2: ![](docs/de[Tab] → multiple matches: "design/", "dev/"
    path = extractPartialPath(line + "de", line.length() + 2);
    EXPECT_EQ(path, "docs/de");
    entries = matchEntries(path, dir);
    ASSERT_EQ(entries.size(), 2);
    EXPECT_TRUE(entries.contains("design/"));
    EXPECT_TRUE(entries.contains("dev/"));
}

TEST(HtmlContext, DetectsSrcAttribute)
{
    QString line = "src=\"resourc";
    QString value;
    ASSERT_TRUE(extractHtmlPath(line, line.length(), value));
    EXPECT_EQ(value, "resourc");
}

TEST(HtmlContext, DetectsHrefAttribute)
{
    QString line = "href='imgs/scre";
    QString value;
    ASSERT_TRUE(extractHtmlPath(line, line.length(), value));
    EXPECT_EQ(value, "imgs/scre");
}

TEST(HtmlContext, IgnoresClosedAttribute)
{
    QString line = "src=\"done\">";
    QString value;
    EXPECT_FALSE(extractHtmlPath(line, line.length(), value));
}

TEST(HtmlContext, LastAttributeWins)
{
    QString line = "<img src=\"a\" href=\"b";
    QString value;
    ASSERT_TRUE(extractHtmlPath(line, line.length(), value));
    EXPECT_EQ(value, "b");
}

TEST(HtmlContext, EmptyValueIsContext)
{
    QString line = "src=\"";
    QString value;
    ASSERT_TRUE(extractHtmlPath(line, line.length(), value));
    EXPECT_TRUE(value.isEmpty());
}

TEST(HtmlReplaceStart, ReplacesFileNamePart)
{
    QString line = "src=\"images/scre";
    EXPECT_EQ(htmlPathReplaceStart(line, line.length()), 12);
}

TEST(HtmlReplaceStart, ReplacesWholeValueWithoutSlash)
{
    QString line = "src=\"scre";
    EXPECT_EQ(htmlPathReplaceStart(line, line.length()), 5);
}

TEST(HtmlReplaceStart, NotInContextReturnsMinusOne)
{
    QString line = "src=\"done\">";
    EXPECT_EQ(htmlPathReplaceStart(line, line.length()), -1);
}

TEST(HtmlReplaceStart, AcceptSimulation)
{
    QString line = acceptHtmlCompletion("src=\"images/", 12, "screenshot.png");
    EXPECT_EQ(line, "src=\"images/screenshot.png");
}

TEST(EmojiContext, DetectsShortcode)
{
    QString code;
    ASSERT_TRUE(extractEmojiCode(":smi", 4, code));
    EXPECT_EQ(code, "smi");
}

TEST(EmojiContext, RejectsInsideLinkPath)
{
    QString code;
    EXPECT_FALSE(extractEmojiCode("![](x:smi", 9, code));
}

TEST(EmojiContext, AllowsAfterClosedLink)
{
    QString code;
    ASSERT_TRUE(extractEmojiCode("[x](y) :sm", 10, code));
    EXPECT_EQ(code, "sm");
}

TEST(EmojiContext, DetectsMidWordColon)
{
    QString code;
    ASSERT_TRUE(extractEmojiCode("foo:smi", 7, code));
    EXPECT_EQ(code, "smi");
}

TEST(EmojiContext, DetectsPlusShortcode)
{
    QString code;
    ASSERT_TRUE(extractEmojiCode(":+1", 3, code));
    EXPECT_EQ(code, "+1");
}

TEST(EmojiContext, DetectsHyphenShortcode)
{
    QString code;
    ASSERT_TRUE(extractEmojiCode(":a-b", 4, code));
    EXPECT_EQ(code, "a-b");
}

TEST(EmojiContext, RejectsUnsupportedChars)
{
    QString code;
    EXPECT_FALSE(extractEmojiCode(":smi!", 5, code));
}

TEST(EmojiContext, ClosedShortcodeNotContext)
{
    QString code;
    EXPECT_FALSE(extractEmojiCode(":sm:", 4, code));
}

TEST(EmojiContext, TrailingTextNotContext)
{
    QString code;
    EXPECT_FALSE(extractEmojiCode("foo :smi bar", 11, code));
}

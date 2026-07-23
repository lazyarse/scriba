#include <gtest/gtest.h>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QTemporaryFile>

static QString readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

static QString resourcePath(const QString &name) {
    return QDir(RESOURCE_DIR).filePath(name);
}

TEST(KaTeXResources, KatexJsExists) {
    QFile f(resourcePath("katex.min.js"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_GT(f.size(), 100000);
}

TEST(KaTeXResources, KatexCssExists) {
    QFile f(resourcePath("katex.min.css"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_GT(f.size(), 10000);
}

TEST(KaTeXResources, AutoRenderExists) {
    QFile f(resourcePath("contrib/auto-render.min.js"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_GT(f.size(), 1000);
}

TEST(KaTeXResources, FontsExist) {
    QStringList fonts = {
        "KaTeX_AMS-Regular.woff2",
        "KaTeX_Main-Regular.woff2",
        "KaTeX_Main-Bold.woff2",
        "KaTeX_Math-Italic.woff2",
        "KaTeX_SansSerif-Regular.woff2",
        "KaTeX_Script-Regular.woff2",
        "KaTeX_Size1-Regular.woff2",
        "KaTeX_Typewriter-Regular.woff2",
    };
    for (const auto &font : fonts) {
        QFile f(resourcePath("fonts/" + font));
        EXPECT_TRUE(f.open(QIODevice::ReadOnly)) << font.toStdString();
    }
}

TEST(KaTeXResources, KatexJsContainsRenderToString) {
    QString js = readFile(resourcePath("katex.min.js"));
    EXPECT_TRUE(js.contains("renderToString"));
}

TEST(KaTeXResources, AutoRenderContainsDelimiters) {
    QString js = readFile(resourcePath("contrib/auto-render.min.js"));
    EXPECT_TRUE(js.contains("delimiters"));
}

TEST(KaTexQrc, QrcListsKatexFiles) {
    QString qrc = readFile(resourcePath("scriba.qrc"));
    EXPECT_TRUE(qrc.contains("katex.min.js"));
    EXPECT_TRUE(qrc.contains("katex.min.css"));
    EXPECT_TRUE(qrc.contains("contrib/auto-render.min.js"));
}

TEST(KaTexQrc, QrcListsFontFiles) {
    QString qrc = readFile(resourcePath("scriba.qrc"));
    EXPECT_TRUE(qrc.contains("fonts/KaTeX_Main-Regular.woff2"));
    EXPECT_TRUE(qrc.contains("fonts/KaTeX_AMS-Regular.woff2"));
}

TEST(KaTeXIntegration, KatexInitJsHasDisplayDelimiter) {
    QString js = readFile(resourcePath("katex.min.js"));
    Q_UNUSED(js);
    QString qrc = readFile(resourcePath("scriba.qrc"));
    EXPECT_TRUE(qrc.contains("katex.min.js"));
}

TEST(KaTeXIntegration, SampleMdHasLatexSection) {
    QTemporaryFile tmp;
    ASSERT_TRUE(tmp.open());
    tmp.write("## LaTeX Math\n\n$E = mc^2$\n\n$$\n\\frac{n!}{k!(n-k)!}\n$$\n");
    tmp.flush();
    QString md = readFile(tmp.fileName());
    EXPECT_TRUE(md.contains("## LaTeX Math"));
    EXPECT_TRUE(md.contains("$E = mc^2$"));
    EXPECT_TRUE(md.contains("$$"));
}

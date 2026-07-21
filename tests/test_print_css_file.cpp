#include <gtest/gtest.h>
#include <QFile>
#include <QString>

static QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

TEST(PrintCssFileTest, ContainsPageRule)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("@page {"));
    EXPECT_TRUE(css.contains("margin: 15mm;"));
}

TEST(PrintCssFileTest, BodyHasPrintDefaults)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("background: white !important"));
    EXPECT_TRUE(css.contains("color: #000 !important"));
    EXPECT_TRUE(css.contains("font-family: Georgia"));
    EXPECT_TRUE(css.contains("font-size: 12pt"));
    EXPECT_TRUE(css.contains("max-width: none !important"));
}

TEST(PrintCssFileTest, HeadingsAvoidPageBreak)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("page-break-after: avoid"));
}

TEST(PrintCssFileTest, PreAndTableAvoidPageBreak)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("page-break-inside: avoid"));
}

TEST(PrintCssFileTest, MermaidCenteredWithSvgLimits)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains(".mermaid {"));
    EXPECT_TRUE(css.contains("page-break-inside: avoid"));
    EXPECT_TRUE(css.contains("text-align: center"));
    EXPECT_TRUE(css.contains("max-height: 160mm"));
}

TEST(PrintCssFileTest, KatexCentered)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains(".katex-display > .katex"));
    EXPECT_TRUE(css.contains("text-align: center"));
}

TEST(PrintCssFileTest, AdmonitionsExist)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains(".admonition {"));
    EXPECT_TRUE(css.contains("page-break-inside: avoid"));
    EXPECT_TRUE(css.contains(".admonition.note"));
}

TEST(PrintCssFileTest, HighlightJsStylesAreGrayscale)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains(".hljs-comment"));
    EXPECT_TRUE(css.contains("color: #808080"));
    EXPECT_TRUE(css.contains(".hljs-keyword"));
    EXPECT_TRUE(css.contains("color: #333"));
}

TEST(PrintCssFileTest, TableStyles)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("border-collapse: collapse"));
    EXPECT_TRUE(css.contains("page-break-inside: avoid"));
}

TEST(PrintCssFileTest, TaskListCheckboxSized)
{
    QString css = readFile(SOURCE_DIR "/resources/print-base.css");
    ASSERT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains(".task-list-item-checkbox"));
    EXPECT_TRUE(css.contains("width: 12pt"));
    EXPECT_TRUE(css.contains("height: 12pt"));
}

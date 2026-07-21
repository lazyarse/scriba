#include <gtest/gtest.h>
#include <QApplication>
#include <QTest>
#include <QTemporaryFile>
#include <QFile>
#include <QRadioButton>
#include <QSettings>
#include <QMarginsF>

#include "ExportPdfDialog.h"
#include "CssLoader.h"
#include "CssConfig.h"
#include "Preferences.h"

class PrintExportAccess
{
public:
    static QString buildFullHtml(ExportPdfDialog *d, const QString &c) { return d->buildFullHtml(c); }
    static const QByteArray &pdfData(const ExportPdfDialog *d) { return d->m_pdfData; }
    static int generationId(const ExportPdfDialog *d) { return d->m_generationId; }
    static void clearPdfData(ExportPdfDialog *d) { d->m_pdfData.clear(); }
    static void triggerCssChange(ExportPdfDialog *d) { d->onCssModeChanged(); }
    static bool hasTempFile(const ExportPdfDialog *d) { return !d->m_tempFile.isNull(); }
    static bool tempFileExists(const ExportPdfDialog *d) { return d->m_tempFile && d->m_tempFile->exists(); }
    static void setCustomCssPath(ExportPdfDialog *d, const QString &p) { d->m_customCssPath = p; }
    static void setCustomRadio(ExportPdfDialog *d, bool checked) { d->m_customRadio->setChecked(checked); }
    static QMarginsF parsePageMargins(ExportPdfDialog *, const QString &css) { return ExportPdfDialog::parsePageMargins(css); }
};

class PrintExportTest : public testing::Test
{
protected:
    void SetUp() override
    {
        QSettings s;
        s.setValue(Preferences::TableStriping, true);
        s.setValue(Preferences::EmojiMode, "bw");

        config = new CssConfig();
        loader = new CssLoader(config);
        dlg = new ExportPdfDialog("<p>hello world</p>", "/tmp/test.md", loader);
        dlg->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete dlg;
        delete loader;
        delete config;
    }

    CssConfig *config = nullptr;
    CssLoader *loader = nullptr;
    ExportPdfDialog *dlg = nullptr;
};

// ---------- buildFullHtml tests ----------

TEST_F(PrintExportTest, BuildHtmlContainsPageRule)
{
    QString css = "@page { margin: 15mm; } body { color: black; }";
    QString html = PrintExportAccess::buildFullHtml(dlg, css);

    EXPECT_TRUE(html.contains("@page { margin: 15mm; }"));
    EXPECT_TRUE(html.contains("<body id=\"preview\">"));
    EXPECT_TRUE(html.contains("<p>hello world</p>"));
    EXPECT_TRUE(html.contains("</body></html>"));
}

TEST_F(PrintExportTest, BuildHtmlIncludesScripts)
{
    QString html = PrintExportAccess::buildFullHtml(dlg, "/* simple */");

    EXPECT_TRUE(html.contains("qrc:///highlight.min.js"));
    EXPECT_TRUE(html.contains("qrc:///mermaid.min.js"));
    EXPECT_TRUE(html.contains("qrc:///katex.min.css"));
    EXPECT_TRUE(html.contains("qrc:///katex.min.js"));
    EXPECT_TRUE(html.contains("qrc:///vega-lite.min.js"));
    EXPECT_TRUE(html.contains("initMermaid()"));
    EXPECT_TRUE(html.contains("hljs.highlightAll()"));
    EXPECT_TRUE(html.contains("initKaTeX()"));
    EXPECT_TRUE(html.contains("initVegaLite()"));
    EXPECT_TRUE(html.contains("replaceEmoji(document.body)"));
}

TEST_F(PrintExportTest, BuildHtmlTableStripingDisabledInjectsOverride)
{
    QSettings s;
    s.setValue(Preferences::TableStriping, false);

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* stripped */");

    EXPECT_TRUE(html.contains("tr:nth-child(even)"));
}

TEST_F(PrintExportTest, BuildHtmlTableStripingEnabledOmitsOverride)
{
    QSettings s;
    s.setValue(Preferences::TableStriping, true);

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* stripped */");

    EXPECT_FALSE(html.contains("tr:nth-child(even)"));
}

TEST_F(PrintExportTest, BuildHtmlEmojiModeColor)
{
    QSettings s;
    s.setValue(Preferences::EmojiMode, "color");

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* emoji */");

    EXPECT_TRUE(html.contains("twemojiParse('color')"));
}

TEST_F(PrintExportTest, BuildHtmlEmojiModeBw)
{
    QSettings s;
    s.setValue(Preferences::EmojiMode, "bw");

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* emoji */");

    EXPECT_TRUE(html.contains("twemojiParse('bw')"));
}

// ---------- parsePageMargins tests ----------

TEST_F(PrintExportTest, ParsePageMargins15mm)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg, "@page { margin: 15mm; }");
    EXPECT_NEAR(m.left(), 42.52, 0.01);
    EXPECT_NEAR(m.top(), 42.52, 0.01);
    EXPECT_NEAR(m.right(), 42.52, 0.01);
    EXPECT_NEAR(m.bottom(), 42.52, 0.01);
}

TEST_F(PrintExportTest, ParsePageMarginsZero)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg, "@page { margin: 0; }");
    EXPECT_DOUBLE_EQ(m.left(), 0);
    EXPECT_DOUBLE_EQ(m.top(), 0);
    EXPECT_DOUBLE_EQ(m.right(), 0);
    EXPECT_DOUBLE_EQ(m.bottom(), 0);
}

TEST_F(PrintExportTest, ParsePageMarginsMulti)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg,
        "@page { margin: 10mm 15mm 20mm 25mm; }");
    EXPECT_NEAR(m.left(), 25.0 * 72.0 / 25.4, 0.01);
    EXPECT_NEAR(m.top(), 10.0 * 72.0 / 25.4, 0.01);
    EXPECT_NEAR(m.right(), 15.0 * 72.0 / 25.4, 0.01);
    EXPECT_NEAR(m.bottom(), 20.0 * 72.0 / 25.4, 0.01);
}

TEST_F(PrintExportTest, ParsePageMarginsTwoValue)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg,
        "@page { margin: 1in 0.5in; }");
    EXPECT_NEAR(m.left(), 36, 0.01);
    EXPECT_NEAR(m.top(), 72, 0.01);
    EXPECT_NEAR(m.right(), 36, 0.01);
    EXPECT_NEAR(m.bottom(), 72, 0.01);
}

TEST_F(PrintExportTest, ParsePageMarginsInch)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg, "@page { margin: 1in; }");
    EXPECT_NEAR(m.left(), 72, 0.01);
}

TEST_F(PrintExportTest, ParsePageMarginsCm)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg, "@page { margin: 2cm; }");
    EXPECT_NEAR(m.left(), 2.0 * 72.0 / 2.54, 0.01);
}

TEST_F(PrintExportTest, ParsePageMarginsNoAtPage)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg, "body { margin: 15mm; }");
    EXPECT_DOUBLE_EQ(m.left(), 0);
    EXPECT_DOUBLE_EQ(m.top(), 0);
}

TEST_F(PrintExportTest, ParsePageMarginsNoMargin)
{
    QMarginsF m = PrintExportAccess::parsePageMargins(dlg, "@page { size: A4; }");
    EXPECT_DOUBLE_EQ(m.left(), 0);
    EXPECT_DOUBLE_EQ(m.top(), 0);
}

// ---------- PDF generation tests ----------

static bool waitForPdf(const ExportPdfDialog *dlg, int maxWaitMs = 5000)
{
    QTest::qWait(300);
    for (int elapsed = 300; elapsed < maxWaitMs; elapsed += 100) {
        if (!PrintExportAccess::pdfData(dlg).isEmpty())
            return true;
        QTest::qWait(100);
    }
    return !PrintExportAccess::pdfData(dlg).isEmpty();
}

TEST_F(PrintExportTest, GeneratesValidPdf)
{
    ASSERT_TRUE(waitForPdf(dlg));
    EXPECT_TRUE(PrintExportAccess::pdfData(dlg).startsWith("%PDF-"));
}

static QByteArray buildPdfWithPageMargin(ExportPdfDialog *dlg, const QString &marginValue)
{
    // Build a full CSS matching print-base.css but with the given @page margin.
    // This isolates @page margin as the only variable between two PDFs.
    QString css = QStringLiteral(
        "@page { margin: %1; }\n"
        "body {\n"
        "    background: white !important;\n"
        "    color: #000 !important;\n"
        "    font-family: Georgia, 'Times New Roman', serif !important;\n"
        "    font-size: 12pt !important;\n"
        "    line-height: 1.5 !important;\n"
        "    max-width: none !important;\n"
        "    margin: 0 !important;\n"
        "    padding: 0 !important;\n"
        "}\n"
    ).arg(marginValue);

    QTemporaryFile cssFile;
    EXPECT_TRUE(cssFile.open());
    cssFile.write(css.toUtf8());
    cssFile.close();

    PrintExportAccess::setCustomCssPath(dlg, cssFile.fileName());
    PrintExportAccess::setCustomRadio(dlg, true);
    PrintExportAccess::triggerCssChange(dlg);

    QTest::qWait(500);
    for (int i = 0; i < 50 && PrintExportAccess::pdfData(dlg).isEmpty(); ++i)
        QTest::qWait(100);
    return PrintExportAccess::pdfData(dlg);
}

TEST_F(PrintExportTest, PdfHasMargins)
{
    // First PDF: 50mm margins
    QByteArray pdfLargeMargin = buildPdfWithPageMargin(dlg, "50mm");
    ASSERT_FALSE(pdfLargeMargin.isEmpty());

    // Second PDF: 5mm margins
    QByteArray pdfSmallMargin = buildPdfWithPageMargin(dlg, "5mm");
    ASSERT_FALSE(pdfSmallMargin.isEmpty());

    // PDFs must differ: @page margin changes content area, affecting layout
    EXPECT_NE(pdfLargeMargin, pdfSmallMargin);
}

TEST_F(PrintExportTest, CssSwitchWithDifferentCssRegenerates)
{
    ASSERT_TRUE(waitForPdf(dlg));
    ASSERT_FALSE(PrintExportAccess::pdfData(dlg).isEmpty());

    QByteArray firstPdf = PrintExportAccess::pdfData(dlg);
    int oldGen = PrintExportAccess::generationId(dlg);

    QTemporaryFile cssFile;
    ASSERT_TRUE(cssFile.open());
    cssFile.write("@page { margin: 5mm; } body { font-size: 8pt !important; }");
    cssFile.close();

    PrintExportAccess::setCustomCssPath(dlg, cssFile.fileName());
    PrintExportAccess::setCustomRadio(dlg, true);

    QTest::qWait(500);
    for (int i = 0; i < 50 && PrintExportAccess::pdfData(dlg).isEmpty(); ++i)
        QTest::qWait(100);

    EXPECT_GT(PrintExportAccess::generationId(dlg), oldGen);
    EXPECT_FALSE(PrintExportAccess::pdfData(dlg).isEmpty());
    QByteArray secondPdf = PrintExportAccess::pdfData(dlg);
    EXPECT_NE(secondPdf, firstPdf);
}

TEST_F(PrintExportTest, PdfHasSize)
{
    ASSERT_TRUE(waitForPdf(dlg));
    QByteArray pdfA4 = PrintExportAccess::pdfData(dlg);
    ASSERT_FALSE(pdfA4.isEmpty());

    QTemporaryFile cssFile;
    ASSERT_TRUE(cssFile.open());
    cssFile.write("@page { size: A5 landscape; margin: 0; }");
    cssFile.close();

    PrintExportAccess::setCustomCssPath(dlg, cssFile.fileName());
    PrintExportAccess::setCustomRadio(dlg, true);

    QTest::qWait(500);
    ASSERT_TRUE(waitForPdf(dlg));
    QByteArray pdfA5 = PrintExportAccess::pdfData(dlg);
    ASSERT_FALSE(pdfA5.isEmpty());

    EXPECT_NE(pdfA4, pdfA5);
}

TEST_F(PrintExportTest, TempFileCreated)
{
    ASSERT_TRUE(waitForPdf(dlg));
    EXPECT_TRUE(PrintExportAccess::hasTempFile(dlg));
    EXPECT_TRUE(PrintExportAccess::tempFileExists(dlg));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("ScribaTest");
    app.setApplicationName("ScribaTest");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTest>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QRadioButton>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QMarginsF>

#include "ExportPdfDialog.h"
#include "CssLoader.h"
#include "CssConfig.h"
#include "Preferences.h"
#include "TestConfig.h"

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
    static void setHeaderText(ExportPdfDialog *d, const QString &t) { d->m_headerCenter->setPlainText(t); }
    static void setFooterText(ExportPdfDialog *d, const QString &t) { d->m_footerCenter->setPlainText(t); }
    static void setHeaderLeft(ExportPdfDialog *d, const QString &t) { d->m_headerLeft->setPlainText(t); }
    static void setHeaderCenter(ExportPdfDialog *d, const QString &t) { d->m_headerCenter->setPlainText(t); }
    static void setHeaderRight(ExportPdfDialog *d, const QString &t) { d->m_headerRight->setPlainText(t); }
    static void setFooterLeft(ExportPdfDialog *d, const QString &t) { d->m_footerLeft->setPlainText(t); }
    static void setFooterCenter(ExportPdfDialog *d, const QString &t) { d->m_footerCenter->setPlainText(t); }
    static void setFooterRight(ExportPdfDialog *d, const QString &t) { d->m_footerRight->setPlainText(t); }
    static void setShowHeader(ExportPdfDialog *d, bool on) { d->m_showHeader->setChecked(on); }
    static QString buildHeaderFooterCss(ExportPdfDialog *d) { return d->buildHeaderFooterCss(); }
    static const QString &currentPrintCss(const ExportPdfDialog *d) { return d->m_currentPrintCss; }
    static QMarginsF parsePageMargins(ExportPdfDialog *, const QString &css) { return ExportPdfDialog::parsePageMargins(css); }
    static QSizeF parsePageSize(ExportPdfDialog *, const QString &css) { return ExportPdfDialog::parsePageSize(css); }
};

class PrintExportTest : public testing::Test
{
protected:
    void SetUp() override
    {
        QSettings s;
        s.setValue(Preferences::TableStriping, true);
        s.setValue(Preferences::ShowCodeLangExport, true);
        s.setValue(Preferences::EmojiMode, Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw));

        config = new CssConfig();
        loader = new CssLoader(config);
        dlg = new ExportPdfDialog("<p>hello world</p>", QDir::tempPath() + "/test.md", loader);
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

TEST_F(PrintExportTest, BuildHtmlCodeLangExportDisabledInjectsOverride)
{
    QSettings s;
    s.setValue(Preferences::ShowCodeLangExport, false);

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* code lang */");

    EXPECT_TRUE(html.contains("pre[data-lang]::before{content:none}"));
}

TEST_F(PrintExportTest, BuildHtmlCodeLangExportEnabledOmitsOverride)
{
    QSettings s;
    s.setValue(Preferences::ShowCodeLangExport, true);

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* code lang */");

    EXPECT_FALSE(html.contains("pre[data-lang]::before{content:none}"));
}

TEST_F(PrintExportTest, BuildHtmlEmojiModeColor)
{
    QSettings s;
    s.setValue(Preferences::EmojiMode, Preferences::emojiRenderingToString(Preferences::EmojiRendering::Color));

    QString html = PrintExportAccess::buildFullHtml(dlg, "/* emoji */");

    EXPECT_TRUE(html.contains("twemojiParse('color')"));
}

TEST_F(PrintExportTest, BuildHtmlEmojiModeBw)
{
    QSettings s;
    s.setValue(Preferences::EmojiMode, Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw));

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

// ---------- parsePageSize tests ----------

TEST_F(PrintExportTest, ParsePageSizeDefault)
{
    QSizeF s = PrintExportAccess::parsePageSize(dlg, "body { color: black; }");
    EXPECT_NEAR(s.width(), 595.0, 0.1);
    EXPECT_NEAR(s.height(), 842.0, 0.1);
}

TEST_F(PrintExportTest, ParsePageSizeDefaultA4)
{
    QSizeF s = PrintExportAccess::parsePageSize(dlg, "@page { margin: 15mm; }");
    EXPECT_NEAR(s.width(), 595.0, 0.1);
    EXPECT_NEAR(s.height(), 842.0, 0.1);
}

TEST_F(PrintExportTest, ParsePageSizeA4Landscape)
{
    QSizeF s = PrintExportAccess::parsePageSize(dlg,
        "@page { size: A4 landscape; margin: 10mm; }");
    EXPECT_NEAR(s.width(), 842.0, 0.1);
    EXPECT_NEAR(s.height(), 595.0, 0.1);
}

TEST_F(PrintExportTest, ParsePageSizeLetter)
{
    QSizeF s = PrintExportAccess::parsePageSize(dlg,
        "@page { size: letter; margin: 0; }");
    EXPECT_NEAR(s.width(), 612.0, 0.1);
    EXPECT_NEAR(s.height(), 792.0, 0.1);
}

TEST_F(PrintExportTest, ParsePageSizeExplicit)
{
    QSizeF s = PrintExportAccess::parsePageSize(dlg,
        "@page { size: 210mm 297mm; }");
    EXPECT_NEAR(s.width(), 595.28, 0.1);
    EXPECT_NEAR(s.height(), 841.89, 0.1);
}

// ---------- buildHeaderFooterCss tests ----------

TEST_F(PrintExportTest, BuildHeaderFooterCssEmptyWhenBothEmpty)
{
    PrintExportAccess::setHeaderText(dlg, QString());
    PrintExportAccess::setFooterText(dlg, QString());
    EXPECT_TRUE(PrintExportAccess::buildHeaderFooterCss(dlg).isEmpty());
}

TEST_F(PrintExportTest, BuildHeaderFooterCssHeaderOnly)
{
    PrintExportAccess::setHeaderText(dlg, "{title}");
    PrintExportAccess::setFooterText(dlg, QString());
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    EXPECT_TRUE(css.contains("@top-center"));
    EXPECT_FALSE(css.contains("@bottom-center"));
}

TEST_F(PrintExportTest, BuildHeaderFooterCssFooterOnly)
{
    PrintExportAccess::setHeaderText(dlg, QString());
    PrintExportAccess::setFooterText(dlg, "Page {page} of {pages}");
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    EXPECT_FALSE(css.contains("@top-center"));
    EXPECT_TRUE(css.contains("@bottom-center"));
}

TEST_F(PrintExportTest, BuildHeaderFooterCssBoth)
{
    PrintExportAccess::setHeaderText(dlg, "{title}");
    PrintExportAccess::setFooterText(dlg, "{page}");
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    EXPECT_TRUE(css.contains("@top-center"));
    EXPECT_TRUE(css.contains("@bottom-center"));
}

TEST_F(PrintExportTest, BuildHeaderFooterCounterOutsideQuotes)
{
    PrintExportAccess::setHeaderText(dlg, "Page {page} of {pages}");
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    // counter() must NOT be inside CSS string quotes
    EXPECT_TRUE(css.contains(QStringLiteral("counter(page)")));
    EXPECT_TRUE(css.contains(QStringLiteral("counter(pages)")));
    // counter() should appear as a bare function, not inside a quoted string
    EXPECT_FALSE(css.contains(QStringLiteral("\"counter(")));
}

TEST_F(PrintExportTest, BuildHeaderFooterLeftAndRight)
{
    PrintExportAccess::setHeaderLeft(dlg, "{date}");
    PrintExportAccess::setHeaderRight(dlg, "Page {page}");
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    EXPECT_TRUE(css.contains("@top-left"));
    EXPECT_FALSE(css.contains("@top-center"));
    EXPECT_TRUE(css.contains("@top-right"));
}

TEST_F(PrintExportTest, BuildHeaderFooterAllPositions)
{
    PrintExportAccess::setHeaderLeft(dlg, "{date}");
    PrintExportAccess::setHeaderCenter(dlg, "{title}");
    PrintExportAccess::setHeaderRight(dlg, "{page}");
    PrintExportAccess::setFooterLeft(dlg, "{date}");
    PrintExportAccess::setFooterCenter(dlg, "{title}");
    PrintExportAccess::setFooterRight(dlg, "{page}");
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    EXPECT_TRUE(css.contains("@top-left"));
    EXPECT_TRUE(css.contains("@top-center"));
    EXPECT_TRUE(css.contains("@top-right"));
    EXPECT_TRUE(css.contains("@bottom-left"));
    EXPECT_TRUE(css.contains("@bottom-center"));
    EXPECT_TRUE(css.contains("@bottom-right"));
}

TEST_F(PrintExportTest, BuildHeaderFooterCenterOnly)
{
    PrintExportAccess::setHeaderCenter(dlg, "{title}");
    QString css = PrintExportAccess::buildHeaderFooterCss(dlg);
    EXPECT_FALSE(css.contains("@top-left"));
    EXPECT_TRUE(css.contains("@top-center"));
    EXPECT_FALSE(css.contains("@top-right"));
}

TEST_F(PrintExportTest, HeaderFooterCssExcludedWhenCheckboxOff)
{
    PrintExportAccess::setHeaderText(dlg, "{title}");
    PrintExportAccess::setFooterText(dlg, "{page}");
    PrintExportAccess::setShowHeader(dlg, false);
    QApplication::processEvents();
    QString css = PrintExportAccess::currentPrintCss(dlg);
    EXPECT_FALSE(css.contains("@top-center"));
    EXPECT_FALSE(css.contains("@bottom-center"));
}

TEST_F(PrintExportTest, HeaderFooterCssIncludedWhenCheckboxOn)
{
    PrintExportAccess::setHeaderText(dlg, "{title}");
    PrintExportAccess::setFooterText(dlg, "{page}");
    PrintExportAccess::setShowHeader(dlg, true);
    QApplication::processEvents();
    QString css = PrintExportAccess::currentPrintCss(dlg);
    EXPECT_TRUE(css.contains("@top-center"));
    EXPECT_TRUE(css.contains("@bottom-center"));
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

TEST_F(PrintExportTest, NoDefaultHeadersInPdf)
{
    ASSERT_TRUE(waitForPdf(dlg));
    QByteArray pdf = PrintExportAccess::pdfData(dlg);
    ASSERT_FALSE(pdf.isEmpty());
    EXPECT_FALSE(pdf.contains("1/1")) << "page number should not appear in PDF";
    EXPECT_FALSE(pdf.contains("file://")) << "file URL should not appear in PDF";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

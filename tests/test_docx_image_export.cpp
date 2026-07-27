#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QRegularExpression>
#include "HtmlToOoxml.h"
#include "JsRenderEngine.h"

class DocxImageExportTest : public testing::Test
{
protected:
    // Markdown parser output for a page with inline, display, and mhchem math
    QString m_htmlBody;
    QString m_css;

    void SetUp() override
    {
        m_htmlBody = QStringLiteral(
            "<h1>Math Test</h1>"
            "<p>Inline: <span class=\"math inline\">$x^2$</span></p>"
            "<p>Display:</p>"
            "<p><span class=\"math display\">$$"
            "\\\\frac{n!}{k!(n-k)!} = \\\\binom{n}{k}"
            "$$</span></p>"
            "<p>Chemistry: <span class=\"math inline\">$\\\\ce{H2O}$</span></p>"
        );
        m_css = QStringLiteral(
            "body { font-size: 16px; line-height: 1.5; }"
        );
    }
};

TEST_F(DocxImageExportTest, KaTeXImageConversionProducesImgTags)
{
    QUrl baseUrl = QUrl::fromLocalFile(QDir::currentPath() + "/");

    QString fullHtml = JsRenderEngine::buildFullHtmlForDocx(
        m_htmlBody, m_css,
        QStringLiteral("bw"),
        QStringLiteral("default"));

    EXPECT_FALSE(fullHtml.isEmpty());
    EXPECT_TRUE(fullHtml.contains("convertKatexToImages"))
        << "Full HTML must include the convertKatexToImages function";
    EXPECT_TRUE(fullHtml.contains("katex.min.css"))
        << "Full HTML must include KaTeX CSS";
    EXPECT_TRUE(fullHtml.contains("katex.min.js"))
        << "Full HTML must include KaTeX JS";

    // Render in WebEngine
    QString renderedHtml = JsRenderEngine::renderSync(fullHtml, baseUrl.toString());
    EXPECT_FALSE(renderedHtml.isEmpty())
        << "Rendered HTML must not be empty";

    qDebug().noquote() << "Rendered body HTML (first 2000 chars):";
    qDebug().noquote() << renderedHtml.left(2000);

    // Check for <img> tags — evidence that convertKatexToImages ran
    bool hasImgTags = renderedHtml.contains("<img");
    EXPECT_TRUE(hasImgTags)
        << "After image conversion, KaTeX spans must be replaced with <img> tags. "
        << "Got HTML:\n" << renderedHtml.left(2000).toStdString();

    // If no <img> tags found, check if there are still .katex spans (conversion failed)
    if (!hasImgTags) {
        bool hasKatexSpans = renderedHtml.contains("class=\"katex\"");
        if (hasKatexSpans) {
            ADD_FAILURE() << "KaTeX spans remain unconverted "
                          << "- convertKatexToImages likely failed silently";
        }
        if (renderedHtml.contains("data-tex")) {
            ADD_FAILURE() << "data-tex attributes present "
                          << "- the page used OMML mode instead of image mode";
        }
    }

    // Verify the rendered HTML passes through HtmlToOoxml without errors
    OoxmlResult ooxmlResult = HtmlToOoxml::convert(renderedHtml);
    EXPECT_FALSE(ooxmlResult.bodyXml.isEmpty())
        << "OOXML body must not be empty";

    // In image mode, math should be embedded as drawings, not OMML
    bool hasDrawings = ooxmlResult.bodyXml.contains("w:drawing");
    EXPECT_TRUE(hasDrawings)
        << "Image-mode export must produce w:drawing elements for math";

    // Verify unique docPr ids (previously ALL used id=1, causing invisible images)
    {
        QSet<int> docPrIds;
        QRegularExpression docPrRe(QStringLiteral("docPr\\s+id=\"(\\d+)\""));
        auto it = docPrRe.globalMatch(ooxmlResult.bodyXml);
        while (it.hasNext()) {
            docPrIds.insert(it.next().captured(1).toInt());
        }
        int totalDrawings = ooxmlResult.bodyXml.count("<w:drawing>");
        EXPECT_EQ(docPrIds.size(), totalDrawings)
            << "Each drawing must have a unique docPr id, got "
            << docPrIds.size() << " unique ids for " << totalDrawings << " drawings";
        EXPECT_GT(docPrIds.size(), 0)
            << "Must have at least one unique docPr id";
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("scribaTest");
    app.setApplicationName("scribaTest");

    // Hide QWebEngine warnings about GPU/sandbox
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

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
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QRegularExpression>
#include "io/HtmlToOoxml.h"
#include "preview/JsRenderEngine.h"
#include "TestConfig.h"

// A tiny 200x100 SVG with intrinsic dimensions.
static QString testSvgData()
{
    return QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"100\" "
        "viewBox=\"0 0 200 100\">"
        "<rect x=\"0\" y=\"0\" width=\"200\" height=\"100\" fill=\"#3366cc\"/>"
        "</svg>");
}

// Build the data URI the DOCX export path produces for an embedded file image
// (see JsRenderEngine::embedImages): data:<mime>;base64,<bytes>.
static QString svgDataUri()
{
    return QStringLiteral("data:image/svg+xml;base64,")
        + QString::fromLatin1(testSvgData().toUtf8().toBase64());
}

// Extract the first <wp:extent cx=".." cy=".."> from OOXML body XML.
static QPair<int, int> firstExtent(const QString &bodyXml)
{
    QRegularExpression re(QStringLiteral("wp:extent cx=\"(\\d+)\" cy=\"(\\d+)\""));
    auto m = re.match(bodyXml);
    if (m.hasMatch())
        return {m.captured(1).toInt(), m.captured(2).toInt()};
    return {0, 0};
}

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
            "<p>Inline: <span class=\"katex\" data-tex=\"x^2\"></span></p>"
            "<p>Display:</p>"
            "<span class=\"katex-display\"><span class=\"katex\" data-tex=\""
            "\\\\frac{n!}{k!(n-k)!} = \\\\binom{n}{k}"
            "\"></span></span>"
            "<p>Chemistry: <span class=\"katex\" data-tex=\"\\\\ce{H2O}\"></span></p>"
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

TEST_F(DocxImageExportTest, KaTeXImageModeMarksCssPxDims)
{
    QUrl baseUrl = QUrl::fromLocalFile(QDir::currentPath() + "/");
    QString fullHtml = JsRenderEngine::buildFullHtmlForDocx(
        m_htmlBody, m_css, QStringLiteral("bw"), QStringLiteral("default"));
    QString renderedHtml = JsRenderEngine::renderSync(fullHtml, baseUrl.toString());

    EXPECT_TRUE(renderedHtml.contains(QRegularExpression("<img[^>]*\\bwidth=\"\\d+\"")))
        << "convertKatexToImages must set CSS-px width/height attributes on images";
    EXPECT_TRUE(renderedHtml.contains(QRegularExpression("<img[^>]*\\bheight=\"\\d+\"")))
        << "convertKatexToImages must set CSS-px width/height attributes on images";
}

TEST_F(DocxImageExportTest, SvgImageWithUpscaleDimension)
{
    // A 200px-natural SVG asked to render at 400px via the #400x suffix.
    // SVG is vector, so it must scale UP past its natural size. The OOXML
    // extent must match the requested 400 CSS px, not the natural 200.
    QString html = QStringLiteral(
        "<p><img src=\"%1\" alt=\"svg\" style=\"width: 400px\"></p>")
            .arg(svgDataUri());

    OoxmlResult result = HtmlToOoxml::convert(html);
    EXPECT_FALSE(result.bodyXml.isEmpty());

    // EMUs: 400 CSS px at 96 DPI, 914400 EMUs/inch, but the canvas is
    // rasterized at 2x -> kPxToEmu = 914400/192.
    static constexpr double kPxToEmu = 914400.0 / 192.0;
    int expectedW = qMax(1, static_cast<int>(400 * 2 * kPxToEmu));
    auto [cx, cy] = firstExtent(result.bodyXml);

    // The upscaled width must be substantially larger than the natural 200px
    // size, i.e. the SVG actually scaled up.
    int naturalW = qMax(1, static_cast<int>(200 * 2 * kPxToEmu));
    EXPECT_GT(cx, naturalW) << "SVG must scale up past natural 200px";
    EXPECT_NEAR(cx, expectedW, expectedW * 0.02)
        << "Upscaled SVG extent should match the #400x target width";
    EXPECT_GT(cy, 0);

    // The image must actually be registered (SVG rasterized to PNG)
    EXPECT_TRUE(result.images.isEmpty() == false)
        << "SVG image must be embedded as a rasterized PNG";
}

TEST_F(DocxImageExportTest, SvgImageWithoutDimensionKeepsNaturalSize)
{
    QString html = QStringLiteral(
        "<p><img src=\"%1\" alt=\"svg\"></p>").arg(svgDataUri());

    OoxmlResult result = HtmlToOoxml::convert(html);
    EXPECT_FALSE(result.bodyXml.isEmpty());

    static constexpr double kPxToEmu = 914400.0 / 192.0;
    int naturalW = qMax(1, static_cast<int>(200 * 2 * kPxToEmu));
    int naturalH = qMax(1, static_cast<int>(100 * 2 * kPxToEmu));
    auto [cx, cy] = firstExtent(result.bodyXml);

    EXPECT_NEAR(cx, naturalW, naturalW * 0.02)
        << "Natural-size SVG should keep its intrinsic width";
    EXPECT_NEAR(cy, naturalH, naturalH * 0.02)
        << "Natural-size SVG should keep its intrinsic height";
    EXPECT_FALSE(result.images.isEmpty());
}

TEST_F(DocxImageExportTest, SvgDataUriCarriesRawSvg)
{
    QString html = QStringLiteral("<p><img src=\"%1\" alt=\"svg\"></p>").arg(svgDataUri());
    OoxmlResult result = HtmlToOoxml::convert(html);

    ASSERT_FALSE(result.images.isEmpty());
    EXPECT_FALSE(result.images[0].svgData.isEmpty());
    EXPECT_TRUE(result.images[0].svgFileName.endsWith(".svg"));
    EXPECT_FALSE(result.images[0].svgRelId.isEmpty());

    // Vector extents are unchanged: natural 200px @96 DPI (equivalent to the
    // previous 2x@192 px/inch trade-off: 914400/96 == 2*(914400/192)).
    static constexpr double kPxToEmu = 914400.0 / 96.0;
    auto [cx, cy] = firstExtent(result.bodyXml);
    EXPECT_NEAR(cx, 200 * kPxToEmu, 200 * kPxToEmu * 0.01);
    EXPECT_NEAR(cy, 100 * kPxToEmu, 100 * kPxToEmu * 0.01);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();

    // Hide QWebEngine warnings about GPU/sandbox
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

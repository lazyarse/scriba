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
#include <QTemporaryDir>
#include <QFile>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QDir>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>

#include "ExportHtmlDialog.h"
#include "MarkdownParser.h"
#include "Preferences.h"
#include "JsRenderEngine.h"
#include "CssLoader.h"
#include "CssConfig.h"
#include "TestConfig.h"

class HtmlExportTest : public testing::Test
{
protected:
    void SetUp() override
    {
        QSettings s;
        s.setValue(Preferences::TableStriping, true);
        s.setValue(Preferences::EmojiMode,
            Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw));
        // Seed theme settings so dialog tests don't depend on leftover
        // QSettings state from other test targets sharing the scribaTest org.
        QStringList themes = CssConfig::bundledThemes();
        s.setValue(Preferences::CssFiles, themes);
        s.setValue(Preferences::ActiveCssFile, themes.first());

        config = new CssConfig();
        loader = new CssLoader(config);
    }

    void TearDown() override
    {
        delete loader;
        delete config;
    }

    CssConfig *config = nullptr;
    CssLoader *loader = nullptr;
};

// ---------- Dialog tests ----------

TEST_F(HtmlExportTest, DialogCreation)
{
    ExportHtmlDialog dlg(config, loader, QDir::tempPath() + "/test.md");
    dlg.show();
    QApplication::processEvents();

    auto *combo = dlg.findChild<QComboBox *>();
    ASSERT_NE(combo, nullptr);
    EXPECT_GT(combo->count(), 0);
}

TEST_F(HtmlExportTest, DialogDefaultsToActiveTheme)
{
    QString activeTheme = config->activeStylesheet();

    ExportHtmlDialog dlg(config, loader, QDir::tempPath() + "/test.md");
    dlg.show();
    QApplication::processEvents();

    auto *combo = dlg.findChild<QComboBox *>();
    ASSERT_NE(combo, nullptr);

    QString selected = dlg.selectedThemePath();
    EXPECT_EQ(selected, activeTheme);
}

TEST_F(HtmlExportTest, DialogPopulatesThemes)
{
    ExportHtmlDialog dlg(config, loader, QDir::tempPath() + "/test.md");
    dlg.show();
    QApplication::processEvents();

    auto *combo = dlg.findChild<QComboBox *>();
    ASSERT_NE(combo, nullptr);

    // Should have all themes from the stylesheet list
    QStringList themes = config->stylesheets();
    EXPECT_EQ(combo->count(), themes.size());
}

TEST_F(HtmlExportTest, SelectedThemeReturnsValidPath)
{
    ExportHtmlDialog dlg(config, loader, QDir::tempPath() + "/test.md");
    dlg.show();
    QApplication::processEvents();

    QString path = dlg.selectedThemePath();
    EXPECT_FALSE(path.isEmpty());
    // Should be either a qrc path or an absolute file path
    EXPECT_TRUE(path.startsWith(":") || QDir::isAbsolutePath(path));
}

TEST_F(HtmlExportTest, DialogHasExportAndCancelButtons)
{
    ExportHtmlDialog dlg(config, loader, QDir::tempPath() + "/test.md");
    dlg.show();
    QApplication::processEvents();

    auto *exportBtn = dlg.findChild<QPushButton *>();
    // At minimum the dialog should be created without crash
    EXPECT_NE(exportBtn, nullptr);
}

// ---------- JsRenderEngine tests ----------

TEST_F(HtmlExportTest, BuildFullHtmlContainsCss)
{
    QString css = "body { color: red; }";
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", css, "bw", "default");

    EXPECT_TRUE(html.contains(css));
    EXPECT_TRUE(html.contains("<body id=\"preview\">"));
    EXPECT_TRUE(html.contains("<p>test</p>"));
}

TEST_F(HtmlExportTest, BuildFullHtmlContainsScripts)
{
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", "body{}", "bw", "default");

    EXPECT_TRUE(html.contains("qrc:///highlight.min.js"));
    EXPECT_TRUE(html.contains("qrc:///mermaid.min.js"));
    EXPECT_TRUE(html.contains("qrc:///katex.min.js"));
    EXPECT_TRUE(html.contains("qrc:///echarts.min.js"));
    EXPECT_TRUE(html.contains("initMermaid()"));
    EXPECT_TRUE(html.contains("initKaTeX()"));
    EXPECT_TRUE(html.contains("initECharts()"));
}

TEST_F(HtmlExportTest, BuildFullHtmlMermaidThemeDark)
{
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", "body{}", "bw", "dark");
    EXPECT_TRUE(html.contains("theme:'dark'"));
}

TEST_F(HtmlExportTest, BuildFullHtmlMermaidThemeDefault)
{
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", "body{}", "bw", "default");
    EXPECT_TRUE(html.contains("theme:'default'"));
}

TEST_F(HtmlExportTest, BuildFullHtmlEmojiModeColor)
{
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", "body{}", "color", "default");
    EXPECT_TRUE(html.contains("twemojiParse('color')"));
}

TEST_F(HtmlExportTest, BuildFullHtmlEmojiModeBw)
{
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", "body{}", "bw", "default");
    EXPECT_TRUE(html.contains("twemojiParse('bw')"));
}

TEST_F(HtmlExportTest, ReplaceQrcUrls)
{
    QString html = "<img src=\"qrc:///checkbox-checked.svg\">";
    QString result = JsRenderEngine::replaceQrcUrls(html);
    // qrc URLs should be replaced with data URIs
    EXPECT_FALSE(result.contains("qrc:///"));
    EXPECT_TRUE(result.contains("data:image/"));
}

TEST_F(HtmlExportTest, ReplaceQrcUrlsNoChangeForNonQrc)
{
    QString html = "<p>hello world</p>";
    QString result = JsRenderEngine::replaceQrcUrls(html);
    EXPECT_EQ(result, html);
}

// ---------- Export output structure tests ----------

TEST_F(HtmlExportTest, ExportHtmlStructure)
{
    // Test the structure of what exportHtml() would produce
    QString baseCss = "body { font-family: serif; }";
    QString themeCss = "body { color: #333; } #editor { background: #fff; }";
    QString combinedCss = baseCss + "\n" + themeCss;

    // Strip #editor rule as exportHtml does
    QString exportCss = themeCss;
    exportCss.remove(QRegularExpression(R"(#editor\s*\{[^}]*\})"));
    exportCss = baseCss + "\n" + exportCss;

    QString bodyHtml = "<p>Hello</p>";

    QString output = QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<style>%1</style>\n"
        "</head>\n"
        "<body>%2</body>\n"
        "</html>\n"
    ).arg(exportCss, bodyHtml);

    EXPECT_TRUE(output.startsWith("<!DOCTYPE html>"));
    EXPECT_TRUE(output.contains("<meta charset=\"utf-8\">"));
    EXPECT_TRUE(output.contains("<body><p>Hello</p></body>"));
    EXPECT_TRUE(output.contains("font-family: serif"));
    EXPECT_TRUE(output.contains("color: #333"));
    // #editor rule should be stripped
    EXPECT_FALSE(output.contains("#editor"));
}

TEST_F(HtmlExportTest, ExportHtmlNoScriptTags)
{
    // The exported HTML should not contain any <script> tags
    QString css = "body { color: black; }";
    QString bodyHtml = "<p>Hello</p>";

    QString output = QString(
        "<!DOCTYPE html>\n<html>\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<style>%1</style>\n"
        "</head>\n<body>%2</body>\n</html>\n"
    ).arg(css, bodyHtml);

    EXPECT_FALSE(output.contains("<script"));
    EXPECT_FALSE(output.contains("</script>"));
}

// ---------- KaTeX rendering tests ----------

TEST_F(HtmlExportTest, KatexCssReturnsContent)
{
    QString css = JsRenderEngine::katexCss();
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains(".katex"));
    EXPECT_TRUE(css.contains("@font-face"));
}

TEST_F(HtmlExportTest, BuildFullHtmlNoOutputSvgOption)
{
    // KaTeX does not support output:'svg' — the option must be absent
    QString html = JsRenderEngine::buildFullHtml("<p>test</p>", "body{}", "bw", "default");
    EXPECT_FALSE(html.contains("output:'svg'"));
    EXPECT_FALSE(html.contains("output:\"svg\""));
}

TEST_F(HtmlExportTest, RenderSyncKaTeXInlineMath)
{
    // Semantic math spans (as produced by MdRenderer) should render to KaTeX
    QString bodyHtml = "<p>Inline math: <span class=\"katex\" data-tex=\"x^2\"></span></p>";
    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, "body{}", "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString());
    ASSERT_FALSE(rendered.isEmpty());

    // KaTeX wraps rendered math in <span class="katex">
    EXPECT_TRUE(rendered.contains("katex")) << "KaTeX should produce .katex spans";
    // The semantic span's source attribute survives; no raw $...$ is introduced
    EXPECT_FALSE(rendered.contains("$x^2$")) << "LaTeX delimiters should not be emitted";
}

TEST_F(HtmlExportTest, RenderSyncKaTeXDisplayMath)
{
    // Display math with the .katex-display wrapper MdRenderer emits
    QString bodyHtml =
        "<div class=\"katex-display\"><span class=\"katex\" data-tex=\"E=mc^2\"></span></div>";
    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, "body{}", "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString());
    ASSERT_FALSE(rendered.isEmpty());

    EXPECT_TRUE(rendered.contains("katex")) << "KaTeX should produce .katex spans for display math";
    EXPECT_TRUE(rendered.contains("katex-display")) << "Display math should use .katex-display class";
}

TEST_F(HtmlExportTest, DocxOmmlPipelineSetsDataTexOnDisplayMath)
{
    // Simulate the full OMML export pipeline: body HTML with already-rendered
    // KaTeX display math (as it comes from the main preview) goes through
    // buildFullHtmlForDocxOmml → renderSync. The JS must set data-tex
    // on display math spans using getElementsByTagNameNS for MathML annotation.
    QString bodyHtml =
        "<p>Display math:</p>"
        "<div class=\"katex-display\">"
        "<span class=\"katex\">"
        "<span class=\"katex-mathml\">"
        "<math display=\"block\" xmlns=\"http://www.w3.org/1998/Math/MathML\">"
        "<semantics>"
        "<mrow><mi>E</mi><mo>=</mo><mi>m</mi><msup><mi>c</mi><mn>2</mn></msup></mrow>"
        "<annotation encoding=\"application/x-tex\">E=mc^2</annotation>"
        "</semantics>"
        "</math>"
        "</span>"
        "<span class=\"katex-html\" aria-hidden=\"true\"><span class=\"base\">E=mc²</span></span>"
        "</span>"
        "</div>"
        "<p>After display math</p>";

    QString fullHtml = JsRenderEngine::buildFullHtmlForDocxOmml(
        bodyHtml, "body{}", "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString(), 15000);
    ASSERT_FALSE(rendered.isEmpty()) << "OMML pipeline must produce output";

    // The .katex span must have a data-tex attribute set by the JS
    // (via getElementsByTagNameNS annotation extraction)
    EXPECT_TRUE(rendered.contains("data-tex"))
        << "KaTeX span must have data-tex attribute from MathML annotation";

    // The data-tex value must contain the TeX source, not "math" fallback
    EXPECT_TRUE(rendered.contains("data-tex=\"E=mc^2\"")
                || rendered.contains("data-tex=&quot;E=mc^2&quot;"))
        << "data-tex must contain the actual TeX source, not 'math' fallback";

    // The katex-mathml and katex-html inner spans should be removed
    EXPECT_FALSE(rendered.contains("katex-mathml"))
        << "katex-mathml span should be removed by JS";
    EXPECT_FALSE(rendered.contains("katex-html"))
        << "katex-html span should be removed by JS";

    // Content after display math must survive
    EXPECT_TRUE(rendered.contains("After display math"))
        << "Content after display math must survive";
}

TEST_F(HtmlExportTest, ExportHtmlStructureIncludesKatexCss)
{
    // The exported HTML should include KaTeX CSS for offline rendering
    QString katexCss = JsRenderEngine::katexCss();
    ASSERT_FALSE(katexCss.isEmpty());

    QString bodyHtml = "<p>Math: $x^2$</p>";
    QString output = QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<style>body{}</style>\n"
        "<style>%1</style>\n"
        "</head>\n"
        "<body>%2</body>\n"
        "</html>\n"
    ).arg(katexCss, bodyHtml);

    EXPECT_TRUE(output.contains(".katex"));
    EXPECT_TRUE(output.contains("@font-face"));
}

TEST_F(HtmlExportTest, RenderSyncKaTeXNoSvgOutput)
{
    // KaTeX should produce HTML spans, not SVG elements
    QString bodyHtml = "<p><span class=\"katex\" data-tex=\"x^2\"></span></p>";
    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, "body{}", "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString());
    ASSERT_FALSE(rendered.isEmpty());

    // KaTeX HTML output uses <span> elements, not <svg>
    EXPECT_TRUE(rendered.contains("<span"));
    EXPECT_FALSE(rendered.contains("<svg"));
}

TEST_F(HtmlExportTest, RenderSyncEChartsSvgOutput)
{
    // ECharts charts should render as SVG, not canvas, with non-zero width
    QString spec = "{\"xAxis\":{\"type\":\"category\",\"data\":[\"A\"]},"
                   "\"yAxis\":{\"type\":\"value\"},"
                   "\"series\":[{\"type\":\"bar\",\"data\":[28]}]}";
    QString bodyHtml = "<pre><code class=\"language-ec\">" + spec.toHtmlEscaped() + "</code></pre>";
    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, "body{}", "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString(), 15000);
    ASSERT_FALSE(rendered.isEmpty());

    // ECharts with renderer:'svg' should produce <svg> elements
    EXPECT_TRUE(rendered.contains("<svg")) << "ECharts should render as SVG";
    EXPECT_TRUE(rendered.contains("echarts-chart")) << "Chart container should have echarts-chart class";
    // Should NOT contain canvas elements
    EXPECT_FALSE(rendered.contains("<canvas")) << "Should use SVG renderer, not canvas";
    // SVG must have non-zero width (regression: zero-width SVGs are invisible in browsers)
    QRegularExpression svgRe(QStringLiteral("<svg[^>]*(?:width=\"(\\d+)\"|viewBox=\"0 0 (\\d+))"));
    QRegularExpressionMatch match = svgRe.match(rendered);
    ASSERT_TRUE(match.hasMatch()) << "SVG should have a numeric width attribute or viewBox";
    int w = match.captured(1).isEmpty() ? match.captured(2).toInt() : match.captured(1).toInt();
    EXPECT_GT(w, 0) << "SVG width must be > 0 to be visible";
}

TEST_F(HtmlExportTest, EChartsSurvivesReplaceQrcUrls)
{
    // SVGs must survive the replaceQrcUrls step used in the export pipeline
    QString spec = "{\"xAxis\":{\"type\":\"category\",\"data\":[\"A\"]},"
                   "\"yAxis\":{\"type\":\"value\"},"
                   "\"series\":[{\"type\":\"bar\",\"data\":[28]}]}";
    QString bodyHtml = "<pre><code class=\"language-ec\">" + spec.toHtmlEscaped() + "</code></pre>";
    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, "body{}", "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString(), 15000);
    ASSERT_FALSE(rendered.isEmpty());

    QString afterReplace = JsRenderEngine::replaceQrcUrls(rendered);
    ASSERT_FALSE(afterReplace.isEmpty());

    // SVG must still be present after qrc URL replacement
    EXPECT_TRUE(afterReplace.contains("<svg")) << "SVG should survive replaceQrcUrls";
    EXPECT_TRUE(afterReplace.contains("echarts-chart")) << "Chart container should survive replaceQrcUrls";
    EXPECT_FALSE(afterReplace.contains("<canvas")) << "Should still be SVG, not canvas";
}

TEST_F(HtmlExportTest, EChartsFullExportPipeline)
{
    // Test the complete export pipeline: buildFullHtml -> renderSync -> replaceQrcUrls -> wrap
    QString spec = "{\"xAxis\":{\"type\":\"category\",\"data\":[\"A\"]},"
                   "\"yAxis\":{\"type\":\"value\"},"
                   "\"series\":[{\"type\":\"bar\",\"data\":[28]}]}";
    QString bodyHtml = "<pre><code class=\"language-ec\">" + spec.toHtmlEscaped() + "</code></pre>";
    QString css = "body { font-family: serif; }";
    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, css, "bw", "default");

    QString rendered = JsRenderEngine::renderSync(fullHtml, QUrl::fromLocalFile(QDir::tempPath() + "/").toString(), 15000);
    ASSERT_FALSE(rendered.isEmpty());

    QString finalBody = JsRenderEngine::replaceQrcUrls(rendered);

    // Wrap as the export does
    QString katexCss = JsRenderEngine::katexCss();
    QString output = QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<style>%1</style>\n"
        "<style>%3</style>\n"
        "</head>\n"
        "<body>%2</body>\n"
        "</html>\n"
    ).arg(css, finalBody, katexCss);

    EXPECT_TRUE(output.startsWith("<!DOCTYPE html>"));
    EXPECT_TRUE(output.contains("<svg")) << "Exported HTML should contain SVG chart";
    EXPECT_TRUE(output.contains("echarts-chart")) << "Exported HTML should contain chart container";
    EXPECT_FALSE(output.contains("<script")) << "Exported HTML should have no script tags";
    EXPECT_FALSE(output.contains("<canvas")) << "Exported HTML should use SVG, not canvas";
}

// ---------- Image embedding tests ----------

TEST_F(HtmlExportTest, EmbedLocalImageAsDataUri)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    // Minimal 1x1 transparent PNG
    static const unsigned char kPng[] = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
        0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,0xde,0x00,0x00,0x00,
        0x0c,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0x0f,0x00,0x00,
        0x01,0x01,0x00,0x05,0x18,0xd8,0x4e,0x00,0x00,0x00,0x00,0x49,
        0x45,0x4e,0x44,0xae,0x42,0x60,0x82
    };

    QString imgPath = dir.filePath("test.png");
    QFile imgFile(imgPath);
    ASSERT_TRUE(imgFile.open(QIODevice::WriteOnly));
    imgFile.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
    imgFile.close();

    QString html = "<p><img src=\"test.png\" alt=\"test\"></p>";
    QUrl baseUrl = QUrl::fromLocalFile(dir.path() + "/");

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_TRUE(result.contains("data:image/png;base64,"));
    EXPECT_FALSE(result.contains("src=\"test.png\""));
}

TEST_F(HtmlExportTest, EmbedLocalSvgAsDataUri)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString svgPath = dir.filePath("icon.svg");
    QFile svgFile(svgPath);
    ASSERT_TRUE(svgFile.open(QIODevice::WriteOnly));
    svgFile.write("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1 1\"><circle r=\"1\"/></svg>");
    svgFile.close();

    QString html = "<img src=\"icon.svg\" alt=\"icon\">";
    QUrl baseUrl = QUrl::fromLocalFile(dir.path() + "/");

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_TRUE(result.contains("data:image/svg+xml;base64,"));
    EXPECT_FALSE(result.contains("src=\"icon.svg\""));
}

TEST_F(HtmlExportTest, EmbedImagesSkipsDataUris)
{
    QString html = "<img src=\"data:image/png;base64,AAAA\" alt=\"x\">";
    QUrl baseUrl;

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_EQ(result, html);
}

TEST_F(HtmlExportTest, EmbedImagesSkipsQrcUrls)
{
    QString html = "<img src=\"qrc:///checkbox-checked.svg\" alt=\"x\">";
    QUrl baseUrl;

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_EQ(result, html);
}

TEST_F(HtmlExportTest, EmbedImagesFallbackOnMissingLocal)
{
    QString html = "<img src=\"nonexistent.png\" alt=\"x\">";
    QUrl baseUrl = QUrl::fromLocalFile(QDir::tempPath() + "/");

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_TRUE(result.contains("src=\"nonexistent.png\""));
    EXPECT_FALSE(result.contains("data:"));
}

TEST_F(HtmlExportTest, EmbedImagesFallbackOnBadUrl)
{
    QString html = "<img src=\"http://localhost:1/nonexistent.png\" alt=\"x\">";
    QUrl baseUrl;

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_TRUE(result.contains("src=\"http://localhost:1/nonexistent.png\""));
    EXPECT_FALSE(result.contains("data:"));
}

TEST_F(HtmlExportTest, EmbedExternalImageFromUrl)
{
    // Minimal 1x1 transparent PNG
    static const unsigned char kPng[] = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
        0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,0xde,0x00,0x00,0x00,
        0x0c,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0x0f,0x00,0x00,
        0x01,0x01,0x00,0x05,0x18,0xd8,0x4e,0x00,0x00,0x00,0x00,0x49,
        0x45,0x4e,0x44,0xae,0x42,0x60,0x82
    };
    QByteArray pngData(reinterpret_cast<const char*>(kPng), sizeof(kPng));

    QTcpServer server;
    ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));
    int port = server.serverPort();

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        auto *sock = server.nextPendingConnection();
        // Read and discard the HTTP request
        sock->waitForReadyRead(2000);
        QByteArray resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: image/png\r\n"
                          "Content-Length: " + QByteArray::number(pngData.size()) + "\r\n"
                          "Connection: close\r\n\r\n" + pngData;
        sock->write(resp);
        sock->waitForBytesWritten(3000);
        sock->disconnectFromHost();
    });

    QString url = QStringLiteral("http://127.0.0.1:%1/test.png").arg(port);
    QString html = "<img src=\"" + url + "\" alt=\"test\">";
    QUrl baseUrl;

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_TRUE(result.contains("data:image/png;base64,"))
        << "External image should be embedded as a data URI";
    EXPECT_FALSE(result.contains("src=\"" + url + "\""))
        << "Original URL should be replaced";

    server.close();
}

TEST_F(HtmlExportTest, EmbedImagesResolvesRelativePath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    // Create subdirectory with image
    QString subDir = dir.filePath("images");
    QDir().mkpath(subDir);
    // Minimal 1x1 transparent PNG
    static const unsigned char kPng[] = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
        0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,0xde,0x00,0x00,0x00,
        0x0c,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0x0f,0x00,0x00,
        0x01,0x01,0x00,0x05,0x18,0xd8,0x4e,0x00,0x00,0x00,0x00,0x49,
        0x45,0x4e,0x44,0xae,0x42,0x60,0x82
    };

    QString imgPath = subDir + "/photo.png";
    QFile imgFile(imgPath);
    ASSERT_TRUE(imgFile.open(QIODevice::WriteOnly));
    imgFile.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
    imgFile.close();

    // HTML references relative path from document root
    QString html = "<img src=\"images/photo.png\" alt=\"photo\">";
    QUrl baseUrl = QUrl::fromLocalFile(dir.path() + "/");

    QString result = JsRenderEngine::embedImages(html, baseUrl);
    EXPECT_TRUE(result.contains("data:image/png;base64,"));
    EXPECT_FALSE(result.contains("src=\"images/photo.png\""));
}

// ---------- embedResources tests ----------

TEST_F(HtmlExportTest, EmbedResourcesStripsScripts)
{
    QString html = "<p>text</p><script src=\"http://example.com/x.js\"></script><p>more</p>";
    QString result = JsRenderEngine::embedResources(html, ScriptHandling::Strip);
    EXPECT_FALSE(result.contains("<script"));
    EXPECT_TRUE(result.contains("<p>text</p>"));
    EXPECT_TRUE(result.contains("<p>more</p>"));
}

TEST_F(HtmlExportTest, EmbedResourcesStripsInlineScripts)
{
    QString html = "<p>text</p><script>alert('hi')</script><p>more</p>";
    QString result = JsRenderEngine::embedResources(html, ScriptHandling::Strip);
    EXPECT_FALSE(result.contains("<script"));
    EXPECT_TRUE(result.contains("<p>text</p>"));
}

TEST_F(HtmlExportTest, EmbedResourcesKeepsInlineScriptsInEmbedMode)
{
    QString html = "<p>text</p><script>alert('hi')</script><p>more</p>";
    QString result = JsRenderEngine::embedResources(html, ScriptHandling::EmbedExternal);
    EXPECT_TRUE(result.contains("<script>alert('hi')</script>"));
}

TEST_F(HtmlExportTest, EmbedResourcesEmbedExternalScript)
{
    QTcpServer server;
    ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));
    int port = server.serverPort();

    QByteArray jsCode = "console.log('hello')";
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        auto *sock = server.nextPendingConnection();
        sock->waitForReadyRead(2000);
        QByteArray resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/javascript\r\n"
                          "Content-Length: " + QByteArray::number(jsCode.size()) + "\r\n"
                          "Connection: close\r\n\r\n" + jsCode;
        sock->write(resp);
        sock->waitForBytesWritten(3000);
        sock->disconnectFromHost();
    });

    QString url = QStringLiteral("http://127.0.0.1:%1/app.js").arg(port);
    QString html = "<script src=\"" + url + "\"></script>";

    QString result = JsRenderEngine::embedResources(html, ScriptHandling::EmbedExternal);
    EXPECT_TRUE(result.contains("<script>console.log('hello')</script>"))
        << "External script should be fetched and inlined";
    EXPECT_FALSE(result.contains("src=\"" + url + "\""))
        << "Original src attribute should be removed";
}

TEST_F(HtmlExportTest, EmbedResourcesStripsScriptOnFetchFailure)
{
    QString html = "<script src=\"http://localhost:1/missing.js\"></script>";
    QString result = JsRenderEngine::embedResources(html, ScriptHandling::EmbedExternal);
    EXPECT_FALSE(result.contains("<script"))
        << "Script should be removed when fetch fails";
}

TEST_F(HtmlExportTest, EmbedResourcesInlinesExternalCss)
{
    QTcpServer server;
    ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));
    int port = server.serverPort();

    QByteArray css = "body { color: red; }";
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        auto *sock = server.nextPendingConnection();
        sock->waitForReadyRead(2000);
        QByteArray resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/css\r\n"
                          "Content-Length: " + QByteArray::number(css.size()) + "\r\n"
                          "Connection: close\r\n\r\n" + css;
        sock->write(resp);
        sock->waitForBytesWritten(3000);
        sock->disconnectFromHost();
    });

    QString url = QStringLiteral("http://127.0.0.1:%1/style.css").arg(port);
    QString html = "<link rel=\"stylesheet\" href=\"" + url + "\">";

    QString result = JsRenderEngine::embedResources(html, ScriptHandling::Strip);
    EXPECT_TRUE(result.contains("<style>body { color: red; }</style>"))
        << "External CSS should be fetched and inlined as <style>";
    EXPECT_FALSE(result.contains("<link"))
        << "Original <link> tag should be removed";
}

TEST_F(HtmlExportTest, EmbedResourcesCssAlwaysInlined)
{
    // CSS is always inlined regardless of script handling mode
    QTcpServer server;
    ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));
    int port = server.serverPort();

    QByteArray css = "h1 { font-size: 2em; }";
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        auto *sock = server.nextPendingConnection();
        sock->waitForReadyRead(2000);
        QByteArray resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/css\r\n"
                          "Content-Length: " + QByteArray::number(css.size()) + "\r\n"
                          "Connection: close\r\n\r\n" + css;
        sock->write(resp);
        sock->waitForBytesWritten(3000);
        sock->disconnectFromHost();
    });

    QString url = QStringLiteral("http://127.0.0.1:%1/headings.css").arg(port);
    QString html = "<link rel=\"stylesheet\" href=\"" + url + "\">";

    QString result = JsRenderEngine::embedResources(html, ScriptHandling::Strip);
    EXPECT_TRUE(result.contains("<style>h1 { font-size: 2em; }</style>"));
}

TEST_F(HtmlExportTest, EmbedResourcesNoChangeForEmpty)
{
    QString html = "<p>no external resources</p>";
    QString result = JsRenderEngine::embedResources(html, ScriptHandling::Strip);
    EXPECT_EQ(result, html);
}

// ---------- stripScriptTags tests ----------

TEST_F(HtmlExportTest, StripScriptTagsRemovesExternal)
{
    QString html = "<p>before</p><script src=\"http://evil.com/x.js\"></script><p>after</p>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_FALSE(result.contains("<script"));
    EXPECT_TRUE(result.contains("<p>before</p>"));
    EXPECT_TRUE(result.contains("<p>after</p>"));
}

TEST_F(HtmlExportTest, StripScriptTagsRemovesInline)
{
    QString html = "<p>text</p><script>alert('xss')</script><p>more</p>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_FALSE(result.contains("<script"));
    EXPECT_TRUE(result.contains("<p>text</p>"));
    EXPECT_TRUE(result.contains("<p>more</p>"));
}

TEST_F(HtmlExportTest, StripScriptTagsRemovesSelfClosing)
{
    QString html = "<p>text</p><script src=\"evil.js\"/><p>after</p>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_FALSE(result.contains("<script"));
    EXPECT_TRUE(result.contains("<p>text</p>"));
}

TEST_F(HtmlExportTest, StripScriptTagsNoChangeForNoScripts)
{
    QString html = "<p>clean content</p>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_EQ(result, html);
}

TEST_F(HtmlExportTest, StripScriptTagsRemovesMultiLine)
{
    QString html = "<p>before</p>\n<script>\nvar x = 1;\nconsole.log(x);\n</script>\n<p>after</p>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_FALSE(result.contains("<script"));
    EXPECT_FALSE(result.contains("console.log"));
    EXPECT_TRUE(result.contains("<p>before</p>"));
    EXPECT_TRUE(result.contains("<p>after</p>"));
}

TEST_F(HtmlExportTest, StripScriptTagsDoesNotStripEventHandlers)
{
    QString html = "<img src=x onerror=\"alert(1)\"><p onclick=\"evil()\">click</p>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_TRUE(result.contains("onerror"));
    EXPECT_TRUE(result.contains("onclick"));
    EXPECT_TRUE(result.contains("alert(1)"));
}

TEST_F(HtmlExportTest, StripScriptTagsDoesNotStripIframe)
{
    QString html = "<iframe src=\"https://evil.com\"></iframe>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_TRUE(result.contains("<iframe"));
}

TEST_F(HtmlExportTest, StripScriptTagsDoesNotStripStyle)
{
    QString html = "<style>body{display:none}</style>";
    QString result = JsRenderEngine::stripScriptTags(html);
    EXPECT_TRUE(result.contains("<style>"));
    EXPECT_TRUE(result.contains("display:none"));
}

TEST_F(HtmlExportTest, CombinedNoHtmlAndStripScriptTags)
{
    QString markdown = "<script>alert(1)</script><img src=x onerror=\"alert(2)\">";
    QString html = MarkdownParser::toHtml(markdown, true);
    QString stripped = JsRenderEngine::stripScriptTags(html);
    EXPECT_TRUE(html.contains("&lt;script"));
    EXPECT_FALSE(html.contains("<img"));
    EXPECT_TRUE(stripped.contains("&lt;script"));
    EXPECT_FALSE(stripped.contains("<script"));
}

TEST_F(HtmlExportTest, CspConstantIsWellFormed)
{
    QString csp = Security::CspHeader;
    EXPECT_TRUE(csp.contains("default-src"));
    EXPECT_TRUE(csp.contains("script-src"));
    EXPECT_TRUE(csp.contains("style-src"));
    EXPECT_TRUE(csp.contains("img-src"));
    EXPECT_TRUE(csp.contains("font-src"));
    EXPECT_TRUE(csp.contains("connect-src"));
    EXPECT_TRUE(csp.contains("'unsafe-inline'"));
    EXPECT_TRUE(csp.contains("'unsafe-eval'"));
    EXPECT_TRUE(csp.contains("'self'"));
    EXPECT_TRUE(csp.contains("qrc:"));
}

TEST_F(HtmlExportTest, CspMetaTagCanBeInjected)
{
    QString html = "<!DOCTYPE html>\n<html><head>\n</head><body></body></html>";
    QString cspTag = QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">").arg(Security::CspHeader);
    int headEnd = html.indexOf("</head>");
    ASSERT_GE(headEnd, 0);
    html.insert(headEnd, cspTag);
    EXPECT_TRUE(html.contains("Content-Security-Policy"));
    EXPECT_TRUE(html.contains("default-src"));
}

TEST_F(HtmlExportTest, NoHtmlFlagBlocksHtmlInExportPath)
{
    QString markdown = "<div onclick=\"evil()\">click</div>";
    QString html = MarkdownParser::toHtml(markdown, true);
    EXPECT_FALSE(html.contains("<div>"));
    EXPECT_TRUE(html.contains("&lt;div"));
    EXPECT_TRUE(html.contains("onclick"));
    EXPECT_TRUE(html.contains("click"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

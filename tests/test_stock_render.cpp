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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <QWebEnginePage>

#include "EditorTestHarness.h"
#include "mainwindow/MainWindow.h"
#include "prefs/Preferences.h"
#include "preview/Preview.h"
#include "TestConfig.h"

// Shared payload shape used by the StockChartDialog for every engine.
// ohlc entries are [open, close, low, high]; indicators hold precomputed
// C++ arrays (null entries = no value at that index).
static const char *const kLcBlock =
    "```lc\n"
    "scribaStockChart(\"lightweight\", {\"title\":\"LWC probe\",\"type\":\"candlestick\","
    "\"volume\":true,\"zoom\":false,\"animate\":false,\"ma\":[20],"
    "\"indicators\":{\"ma20\":[null,null,null,null,null,null,null,null,null,null,"
    "null,null,null,null,null,null,null,null,null,1.0,2.0,3.0,4.0,5.0]},"
    "\"dates\":[\"2026-01-02\",\"2026-01-05\",\"2026-01-06\",\"2026-01-07\",\"2026-01-08\","
    "\"2026-01-09\",\"2026-01-12\",\"2026-01-13\",\"2026-01-14\",\"2026-01-15\","
    "\"2026-01-16\",\"2026-01-19\",\"2026-01-20\",\"2026-01-21\",\"2026-01-22\","
    "\"2026-01-23\",\"2026-01-26\",\"2026-01-27\",\"2026-01-28\",\"2026-01-29\","
    "\"2026-01-30\",\"2026-02-02\",\"2026-02-03\",\"2026-02-04\",\"2026-02-05\"],"
    "\"ohlc\":[[100,102,101,103],[102,101,104,99],[101,105,103,108],[103,104,101,106],"
    "[104,107,106,110],[106,105,108,102],[105,109,107,112],[109,111,108,113],"
    "[111,110,113,107],[110,114,112,116],[114,116,113,119],[116,115,118,111],"
    "[115,119,117,121],[119,122,120,124],[122,120,123,117],[120,124,121,126],"
    "[124,127,125,129],[127,126,129,123],[126,130,128,132],[130,133,131,135],"
    "[133,131,134,128],[131,135,132,137],[135,138,136,140],[138,136,139,133],"
    "[136,140,137,142]],"
    "\"volumes\":[1000,1200,900,1100,1500,1300,1600,1400,1200,1700,1900,1500,1800,"
    "2100,1600,2000,2300,1900,2200,2500,2100,2400,2700,2300,2600]})\n"
    "```\n";

static const char *const kKcBlock =
    "```kc\n"
    "scribaStockChart(\"klinecharts\", {\"title\":\"KC probe\",\"type\":\"candlestick\","
    "\"volume\":false,\"zoom\":false,\"animate\":false,\"ma\":[5],"
    "\"indicators\":{\"ma5\":[null,null,null,null,102.2,103.6,104.8,106.2,108.4,109.8,"
    "111.4,112.6,114.0,115.8,117.2,119.0,120.6,122.4,124.0,125.8,127.6,129.2,131.0,"
    "132.8,134.6]},"
    "\"dates\":[\"2026-01-02\",\"2026-01-05\",\"2026-01-06\",\"2026-01-07\",\"2026-01-08\","
    "\"2026-01-09\",\"2026-01-12\",\"2026-01-13\",\"2026-01-14\",\"2026-01-15\","
    "\"2026-01-16\",\"2026-01-19\",\"2026-01-20\",\"2026-01-21\",\"2026-01-22\","
    "\"2026-01-23\",\"2026-01-26\",\"2026-01-27\",\"2026-01-28\",\"2026-01-29\","
    "\"2026-01-30\",\"2026-02-02\",\"2026-02-03\",\"2026-02-04\",\"2026-02-05\"],"
    "\"ohlc\":[[100,102,101,103],[102,101,104,99],[101,105,103,108],[103,104,101,106],"
    "[104,107,106,110],[106,105,108,102],[105,109,107,112],[109,111,108,113],"
    "[111,110,113,107],[110,114,112,116],[114,116,113,119],[116,115,118,111],"
    "[115,119,117,121],[119,122,120,124],[122,120,123,117],[120,124,121,126],"
    "[124,127,125,129],[127,126,129,123],[126,130,128,132],[130,133,131,135],"
    "[133,131,134,128],[131,135,132,137],[135,138,136,140],[138,136,139,133],"
    "[136,140,137,142]],\"volumes\":[]})\n"
    "```\n";

class StockRenderHarness : public EditorTestHarness
{
protected:
    void SetUp() override
    {
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.remove(Preferences::CssFiles);
        settings.remove(Preferences::ActiveCssFile);
        settings.setValue(Preferences::ReopenLastCorpus, false);
        settings.setValue(Preferences::PreviewState, 1);
        settings.setValue(Preferences::EmojiMode,
            Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw));
        QSettings().setValue(Preferences::FileAutoComplete, false);
        QSettings().setValue(Preferences::EmojiAutoComplete, false);
        QSettings().setValue(Preferences::LanguageAutoComplete, false);

        window = new MainWindow();
        window->show();
        QApplication::processEvents();
        editor = window->editor();
    }

    void TearDown() override
    {
        delete window;
        window = nullptr;
        editor = nullptr;
    }

    // Paste a fence (real paste path -> contentsChange -> preview update),
    // then poll the preview until the chart canvases exist. The poll absorbs
    // the update reload, the 1500ms heavy-JS pass, the engine script load and
    // the layout-poll loop. The result must hold across eight consecutive
    // polls (~2s): the first render can arrive from the initial load's
    // DOMContentLoaded pass and be wiped moments later by the debounced
    // update reload, whose own heavy pass re-renders it, so a single
    // observation is not enough.
    void renderFence(const QString &fence)
    {
        pasteText(fence);
        EXPECT_TRUE(waitForStableJs(
            "(function(){"
            "var wraps=document.querySelectorAll('.scriba-chart-wrap');"
            "if(!wraps.length)return false;"
            "return document.querySelectorAll('.scriba-chart-wrap canvas').length>0"
            "&&document.querySelectorAll('.scriba-chart-wrap .scriba-edit-btn').length>0;"
            "})()", 25000)) << "stock chart canvases/anchors never appeared in the preview";
    }

    bool waitForStableJs(const QString &probe, int timeoutMs = 15000)
    {
        int stable = 0;
        for (int elapsed = 0; elapsed < timeoutMs; elapsed += 250) {
            QVariant result;
            bool done = false;
            window->preview()->page()->runJavaScript(probe,
                [&](const QVariant &r) {
                    result = r;
                    done = true;
                });
            for (int i = 0; i < 10 && !done; ++i)
                QTest::qWait(25);
            if (done) {
                if (result.toBool())
                    stable++;
                else
                    stable = 0;
                if (stable >= 8)
                    return true;
            }
        }
        return false;
    }

    QJsonObject runJs(const QString &script)
    {
        QVariant result;
        bool done = false;
        window->preview()->page()->runJavaScript(script, [&](const QVariant &r) {
            result = r;
            done = true;
        });
        for (int i = 0; i < 200 && !done; ++i)
            QTest::qWait(50);
        EXPECT_TRUE(done) << "runJavaScript callback did not fire";
        return QJsonDocument::fromJson(result.toString().toUtf8()).object();
    }

    MainWindow *window = nullptr;
};

TEST_F(StockRenderHarness, LightweightChartsRendersCanvasesAndTitle)
{
    renderFence(QString::fromUtf8(kLcBlock));

    QJsonObject state = runJs(
        "(function(){"
        "var wraps=document.querySelectorAll('.scriba-chart-wrap');"
        "var cvs=document.querySelectorAll('.scriba-chart-wrap canvas');"
        "var engine=window._scribaStockEngines||{};"
        "var err=window._scribaJsErrors||[];"
        "return JSON.stringify({"
        "wraps:wraps.length,"
        "canvases:cvs.length,"
        "title:wraps.length?wraps[0].querySelector('.scriba-chart-title').textContent:'',"
        "engineLoaded:!!engine.lightweight,"
        "errors:err"
        "});"
        "})()");

    EXPECT_EQ(0, state.value("errors").toArray().size())
        << "JS errors on the page: " << state.value("errors").toArray().size() << " total";
    EXPECT_TRUE(state.value("engineLoaded").toBool()) << "lightweight-charts not loaded";
    EXPECT_EQ(1, state.value("wraps").toInt()) << "expected one chart wrap";
    EXPECT_EQ("LWC probe", state.value("title").toString());
    EXPECT_GE(state.value("canvases").toInt(), 2)
        << "expected main pane + volume pane canvases, got "
        << state.value("canvases").toInt();
}

TEST_F(StockRenderHarness, KlineChartsRendersCanvasesAndTitle)
{
    renderFence(QString::fromUtf8(kKcBlock));

    QJsonObject state = runJs(
        "(function(){"
        "var wraps=document.querySelectorAll('.scriba-chart-wrap');"
        "var cvs=document.querySelectorAll('.scriba-chart-wrap canvas');"
        "var engine=window._scribaStockEngines||{};"
        "var err=window._scribaJsErrors||[];"
        "return JSON.stringify({"
        "wraps:wraps.length,"
        "canvases:cvs.length,"
        "title:wraps.length?wraps[0].querySelector('.scriba-chart-title').textContent:'',"
        "engineLoaded:!!engine.klinecharts,"
        "errors:err"
        "});"
        "})()");

    EXPECT_EQ(0, state.value("errors").toArray().size())
        << "JS errors on the page: " << state.value("errors").toArray().size() << " total";
    EXPECT_TRUE(state.value("engineLoaded").toBool()) << "klinecharts not loaded";
    EXPECT_EQ(1, state.value("wraps").toInt()) << "expected one chart wrap";
    EXPECT_EQ("KC probe", state.value("title").toString());
    EXPECT_GE(state.value("canvases").toInt(), 1)
        << "expected at least one klinecharts canvas, got "
        << state.value("canvases").toInt();
}

TEST_F(StockRenderHarness, ConvertStockChartsToImagesRasterizesCanvases)
{
    renderFence(QString::fromUtf8(kLcBlock));

    QJsonObject state = runJs(
        "(function(){"
        "convertStockChartsToImages().then(function(){"
        "var imgs=document.querySelectorAll('.scriba-chart-wrap img');"
        "window._scribaRasterized=JSON.stringify({"
        "images:imgs.length,"
        "remainingCanvases:document.querySelectorAll('.scriba-chart-wrap canvas').length,"
        "firstSrc:imgs.length?imgs[0].src.substring(0,22):''"
        "});"
        "});"
        "return JSON.stringify({started:true});"
        "})()");

    EXPECT_TRUE(state.value("started").toBool());
    QJsonObject after;
    for (int i = 0; i < 200; ++i) {
        after = runJs("JSON.stringify(JSON.parse(window._scribaRasterized||'{\"images\":-1,\"remainingCanvases\":-1,\"firstSrc\":\"\"}'))");
        if (after.value("images").toInt() >= 0)
            break;
        QTest::qWait(50);
    }
    EXPECT_GE(after.value("images").toInt(), 2)
        << "expected the LWC canvases to become PNG imgs, got "
        << after.value("images").toInt();
    EXPECT_EQ(0, after.value("remainingCanvases").toInt());
    EXPECT_TRUE(after.value("firstSrc").toString().startsWith("data:image/png"))
        << "first rasterized image is not a PNG data URI: "
        << after.value("firstSrc").toString().toStdString();
}

static const char *const kEcBlock =
    "```ec\n"
    "{\"animation\":false,\"title\":{\"text\":\"EC probe\"},\"tooltip\":{\"trigger\":\"axis\",\"axisPointer\":{\"type\":\"cross\"}},\"grid\":[{\"left\":\"5%\",\"right\":\"5%\",\"top\":\"8%\",\"bottom\":\"12%\"}],\"xAxis\":[{\"type\":\"category\",\"data\":[\"2026-01-02\",\"2026-01-05\",\"2026-01-06\"],\"boundaryGap\":false}],\"yAxis\":[{\"type\":\"value\",\"scale\":true}],\"series\":[{\"name\":\"OHLC\",\"type\":\"candlestick\",\"data\":[[100,102,101,103],[102,101,104,99],[101,105,103,108]]}],\"legend\":{\"data\":[\"OHLC\"]}}\n"
    "```\n";

TEST_F(StockRenderHarness, AllEnginesRenderInOneDocument)
{
    renderFence(QString::fromUtf8(kEcBlock) + kLcBlock + kKcBlock);

    QJsonObject state = runJs(
        "(function(){"
        "var wraps=document.querySelectorAll('.scriba-chart-wrap');"
        "var lcKc=document.querySelectorAll('.scriba-chart-wrap .stock-chart canvas').length;"
        "var ec=document.querySelectorAll('.scriba-chart-wrap .echarts-chart svg').length;"
        "return JSON.stringify({"
        "wraps:wraps.length,"
        "canvases:lcKc,"
        "echartsSvg:ec,"
        "anchors:document.querySelectorAll('.scriba-chart-wrap .scriba-edit-btn').length"
        "});"
        "})()");

    EXPECT_EQ(3, state.value("wraps").toInt())
        << "expected one wrap per engine (ec + lc + kc), got "
        << state.value("wraps").toInt();
    EXPECT_GE(state.value("canvases").toInt(), 2)
        << "LWC and KlineCharts wraps must carry canvases, got "
        << state.value("canvases").toInt();
    EXPECT_GE(state.value("echartsSvg").toInt(), 1)
        << "ECharts wraps must carry an SVG (svg renderer), got "
        << state.value("echartsSvg").toInt();
    EXPECT_EQ(2, state.value("anchors").toInt())
        << "only lc/kc wraps get an edit anchor (ec has none), got "
        << state.value("anchors").toInt();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

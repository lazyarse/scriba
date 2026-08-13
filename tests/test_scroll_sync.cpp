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
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QSignalSpy>
#include <QTextCursor>
#include <QWebEnginePage>
#include <QSettings>
#include <QDir>
#include <QDialog>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "MainWindow.h"
#include "Editor.h"
#include "Gutter.h"
#include "Preview.h"
#include "StaticHelpers.h"
#include "Preferences.h"
#include "Corpus.h"
#include "TestConfig.h"

/* ========== Test A: Preview::scrollToPercent ========== */

class PreviewScrollTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_scroll_sync";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }
};

TEST_F(PreviewScrollTest, ScrollToPercentMovesScrollY) {
    Preview preview;
    preview.resize(800, 600);
    preview.show();
    QApplication::processEvents();

    preview.setHtmlContent(
        "<html><body><div style='height:10000px'>tall content</div></body></html>");

    QSignalSpy loadSpy(preview.page(), &QWebEnginePage::loadFinished);
    ASSERT_TRUE(loadSpy.wait(10000));
    ASSERT_TRUE(loadSpy.at(0).at(0).toBool());

    preview.scrollToPercent(0.5);

    double scrollY = -1;
    preview.page()->runJavaScript("window.scrollY", [&](const QVariant &r) {
        scrollY = r.toDouble();
    });
    QTest::qWait(2000);
    EXPECT_GT(scrollY, 1000);
    EXPECT_LT(scrollY, 9000);
}

TEST_F(PreviewScrollTest, ScrollToPercentZeroStaysAtTop) {
    Preview preview;
    preview.resize(800, 600);
    preview.show();
    QApplication::processEvents();

    preview.setHtmlContent(
        "<html><body><div style='height:10000px'>tall content</div></body></html>");

    QSignalSpy loadSpy(preview.page(), &QWebEnginePage::loadFinished);
    ASSERT_TRUE(loadSpy.wait(10000));
    ASSERT_TRUE(loadSpy.at(0).at(0).toBool());

    preview.scrollToPercent(0.0);

    double scrollY = -1;
    preview.page()->runJavaScript("window.scrollY", [&](const QVariant &r) {
        scrollY = r.toDouble();
    });
    QTest::qWait(2000);
    EXPECT_DOUBLE_EQ(scrollY, 0.0);
}

static double scrollbarFraction(QScrollBar *sb) {
    double range = sb->maximum() - sb->minimum();
    return range > 0 ? (sb->value() - sb->minimum()) / range : 0.0;
}

/* ========== Test: preview page background before first paint ========== */

class PreviewBackgroundTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_scroll_sync";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }
};

TEST_F(PreviewBackgroundTest, ThemeBackgroundPaintedOnPage) {
    Preview preview;
    preview.setThemeBackgroundColor(QColor("#282a36"));
    EXPECT_EQ(preview.page()->backgroundColor(), QColor("#282a36"));
}

TEST_F(PreviewBackgroundTest, InvalidBackgroundIsIgnored) {
    Preview preview;
    preview.setThemeBackgroundColor(QColor("#282a36"));
    QColor before = preview.page()->backgroundColor();
    preview.setThemeBackgroundColor(QColor::Invalid);
    EXPECT_EQ(preview.page()->backgroundColor(), before);
}

TEST_F(PreviewBackgroundTest, CreatePreviewViewMatchesTheme) {
    const QString darkTheme = QStringLiteral(
        "#editor{background-color:#282a36;color:#f8f8f2}\n"
        "body{background-color:#282a36;color:#f8f8f2}");
    QWebEngineView *view = createPreviewView(nullptr, darkTheme);
    EXPECT_EQ(view->page()->backgroundColor(), QColor("#282a36"));
    delete view;
}

TEST_F(PreviewBackgroundTest, CreatePreviewViewDefaultsNoTheme) {
    QWebEngineView *view = createPreviewView(nullptr);
    EXPECT_EQ(view->page()->backgroundColor(), QColor("#ffffff"));
    delete view;
}

/* ========== Test B: Editor scroll after cursor restore ========== */

class EditorScrollTest : public testing::Test {
protected:
    void SetUp() override {
        editor = new Editor();
        editor->resize(800, 600);
        editor->show();
        QApplication::processEvents();

        QString content;
        for (int i = 0; i < 200; ++i)
            content += QString("Line %1\n").arg(i, 3, 10, QChar('0'));
        editor->setPlainText(content);
        QApplication::processEvents();
    }

    void TearDown() override {
        delete editor;
    }

    Editor *editor = nullptr;
};

TEST_F(EditorScrollTest, CursorAtStartScrollbarAtTop) {
    auto *sb = editor->verticalScrollBar();

    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(cursor);
    editor->centerCursor();
    QApplication::processEvents();

    double pct = scrollbarFraction(sb);
    EXPECT_LT(pct, 0.2);
}

TEST_F(EditorScrollTest, CursorAtMiddleScrollbarNearMiddle) {
    auto *sb = editor->verticalScrollBar();

    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 100);
    editor->setTextCursor(cursor);
    editor->centerCursor();
    QApplication::processEvents();

    double pct = scrollbarFraction(sb);
    EXPECT_GT(pct, 0.15);
    EXPECT_LT(pct, 0.85);
}

/* ========== Test C: Full integration ========== */

static int s_argc = 1;
static char s_arg0[] = "test_scroll_sync";
static char *s_argv[] = { s_arg0, nullptr };

/* ========== Test H: Block-anchored scroll JS (real preview-script.js injected) ========== */

class PreviewAnchorJsTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(s_argc, s_argv);
    }

    void SetUp() override {
        m_preview = new Preview();
        m_preview->resize(800, 600);
        m_preview->show();
        QApplication::processEvents();
    }

    void TearDown() override {
        delete m_preview;
    }

    // Loads a page whose #scriba-content holds `contentHtml`, with the REAL
    // preview-script.js functions injected (reads them from the qrc resource,
    // so tests exercise exactly what the app runs). Heavy-render globals are
    // stubbed so the injected DOMContentLoaded handler cannot throw.
    void loadPage(const QString &contentHtml) {
        QString script = readResourceFile(":/preview-script.js");
        QString html = QStringLiteral(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
            "<script>"
            "window.mermaid={initialize:function(){},run:function(q){return Promise.resolve();}};"
            "window.hljs={registerAliases:function(){},highlightAll:function(){}};"
            "window.twemoji={parse:function(){}};window.katex={};window.echarts={};"
            "window.replaceEmoji=function(){};window.generateHeadingIds=function(){};"
            "</script>"
            "<script>%1</script>"
            "</head><body><div id=\"scriba-content\">%2</div></body></html>")
            .arg(script, contentHtml);
        QSignalSpy loadSpy(m_preview->page(), &QWebEnginePage::loadFinished);
        m_preview->setHtmlContent(html);
        bool loaded = false;
        while (!loaded) {
            if (!loadSpy.wait(8000)) break;
            if (loadSpy.last().at(0).toBool()) loaded = true;
        }
        ASSERT_TRUE(loaded);
        QTest::qWait(300);
        m_preview->page()->runJavaScript("scribaRebuildAnchorIndex()");
        QTest::qWait(200);
    }

    double evalD(const QString &js) {
        double v = -1;
        m_preview->page()->runJavaScript(js, [&](const QVariant &r) { v = r.toDouble(); });
        QTest::qWait(250);
        return v;
    }

    QPointF evalPoint(const QString &js) {
        QPointF p(-1, -1);
        m_preview->page()->runJavaScript(
            "JSON.stringify(" + js + ")",
            [&](const QVariant &r) {
                QJsonDocument d = QJsonDocument::fromJson(r.toString().toUtf8());
                QJsonArray a = d.array();
                p = QPointF(a.at(0).toDouble(), a.at(1).toDouble());
            });
        QTest::qWait(250);
        return p;
    }

    static const QString kMixedDom; // defined below
    Preview *m_preview = nullptr;
};

// heading(1), para(2), tall block(3, 2000px), list item(4)
const QString PreviewAnchorJsTest::kMixedDom = QStringLiteral(
    "<h1 data-line=\"1\">One</h1>"
    "<p data-line=\"2\">Para two.</p>"
    "<div data-line=\"3\" style=\"height:2000px;background:#e0e0e0\"></div>"
    "<li data-line=\"4\">Item</li>");

TEST_F(PreviewAnchorJsTest, ScrollToBlockLinePutsItsTopAtScrollY) {
    loadPage(kMixedDom);
    m_preview->scrollToSourceLine(3.0);
    double scrollY = evalD("window.scrollY");
    QPointF top = evalPoint(
        "(function(){var e=document.querySelector('[data-line=\"3\"]');"
        "return [e.getBoundingClientRect().top+window.scrollY, 0];})()");
    EXPECT_NEAR(scrollY, top.x(), 2.0);
}

TEST_F(PreviewAnchorJsTest, ScrollInterpolatesWithinBlockSpan) {
    loadPage(kMixedDom);
    m_preview->scrollToSourceLine(3.5); // midpoint between line 3 and line 4
    double scrollY = evalD("window.scrollY");
    QPointF a = evalPoint(
        "(function(){var e=document.querySelector('[data-line=\"3\"]');"
        "return [e.getBoundingClientRect().top+window.scrollY,0];})()");
    QPointF b = evalPoint(
        "(function(){var e=document.querySelector('[data-line=\"4\"]');"
        "return [e.getBoundingClientRect().top+window.scrollY,0];})()");
    EXPECT_NEAR(scrollY, a.x() + (b.x() - a.x()) / 2.0, 4.0);
}

TEST_F(PreviewAnchorJsTest, ScrollClampsToTopAndBottom) {
    loadPage(kMixedDom);
    // Above the first anchor -> top
    m_preview->scrollToSourceLine(0.5);
    EXPECT_DOUBLE_EQ(evalD("window.scrollY"), 0.0);
    // Past the last anchor -> bottom (scrollable; not 0)
    m_preview->scrollToSourceLine(9999.0);
    double bottom = evalD("window.scrollY");
    EXPECT_GT(bottom, 500);
}

TEST_F(PreviewAnchorJsTest, CaptureThenRestoreRoundTrips) {
    loadPage(kMixedDom);
    m_preview->scrollToSourceLine(3.5);
    QTest::qWait(300);
    double captured = evalD("scribaCaptureAnchorLine()");
    EXPECT_NEAR(captured, 3.5, 0.2);
    evalD("window.scrollTo(0, 900)");
    QTest::qWait(200);
    m_preview->scrollToSourceLine(captured);
    QTest::qWait(300);
    double yAfterRestore = evalD("window.scrollY");
    // Line 3.5 sits at the interpolated midpoint between the line-3 block's
    // top and the line-4 item's top, NOT at the line-3 block's top.
    QPointF a = evalPoint(
        "(function(){var e=document.querySelector('[data-line=\"3\"]');"
        "return [e.getBoundingClientRect().top+window.scrollY,0];})()");
    QPointF b = evalPoint(
        "(function(){var e=document.querySelector('[data-line=\"4\"]');"
        "return [e.getBoundingClientRect().top+window.scrollY,0];})()");
    EXPECT_NEAR(yAfterRestore, a.x() + (b.x() - a.x()) / 2.0, 2.0);
}

TEST_F(PreviewAnchorJsTest, EmptyDocumentScrollSyncNoOp) {
    loadPage(QString());
    m_preview->scrollToSourceLine(3.0);
    QTest::qWait(200);
    EXPECT_DOUBLE_EQ(evalD("window.scrollY"), 0.0);
    EXPECT_DOUBLE_EQ(evalD("scribaCaptureAnchorLine()"), 0.0);
}

TEST_F(PreviewAnchorJsTest, SingleBlockDocumentClampsToTop) {
    loadPage("<h1 data-line=\"1\">Only</h1>");
    m_preview->scrollToSourceLine(1.0);
    QTest::qWait(200);
    EXPECT_DOUBLE_EQ(evalD("window.scrollY"), 0.0);
    m_preview->scrollToSourceLine(9999.0);
    QTest::qWait(200);
    EXPECT_DOUBLE_EQ(evalD("window.scrollY"), 0.0);
}

TEST_F(PreviewAnchorJsTest, ConsecutiveDataLineElementsPreferred) {
    // li(3) with a nested div carrying the same data-line(3): the binary
    // search picks the LAST anchor with line <= target (stable sort keeps
    // equal lines in DOM order), so the nested div wins as the interpolation
    // base — a stable, documented winner.
    loadPage("<h1 data-line=\"1\">One</h1>"
             "<li data-line=\"3\">Item<div data-line=\"3\" style=\"height:1600px\"></div></li>"
             "<p data-line=\"4\">Next</p>");
    m_preview->scrollToSourceLine(3.0);
    QTest::qWait(200);
    double scrollY = evalD("window.scrollY");
    QPointF nestedTop = evalPoint(
        "(function(){var e=document.querySelectorAll('[data-line=\"3\"]')[1];"
        "return [e.getBoundingClientRect().top+window.scrollY, 0];})()");
    EXPECT_NEAR(scrollY, nestedTop.x(), 2.0);
}


/* ========== Test G: togglePreview without initialized preview ========== */

class TogglePreviewTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(s_argc, s_argv);
    }

    void SetUp() override {
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.setValue(Preferences::ReopenLastCorpus, false);
        settings.setValue(Preferences::PreviewState, 1);
    }

    void TearDown() override {
        delete window;
    }

    MainWindow *window = nullptr;
};

TEST_F(TogglePreviewTest, CycleThroughAllStatesWithoutCrash) {
    window = new MainWindow();
    window->resize(1200, 800);
    window->show();
    QApplication::processEvents();

    // Preview is not yet initialized (no file loaded)
    // Cycle through all 4 states via Ctrl+B — must not crash
    for (int i = 0; i < 4; ++i) {
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QApplication::processEvents();
    }
}

/* ========== Gutter theme colors through the real MainWindow init path ========== */

class GutterThemeInitTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(s_argc, s_argv);
    }

    void SetUp() override {
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.remove(Preferences::CssFiles);
        settings.remove(Preferences::ActiveCssFile);
        settings.remove(Preferences::PreviewShowPageBreaks);
        settings.setValue(Preferences::ReopenLastCorpus, false);
        settings.setValue(Preferences::PreviewState, 1);
    }

    void TearDown() override {
        delete window;
        QSettings settings;
        settings.remove(Preferences::CssFiles);
        settings.remove(Preferences::ActiveCssFile);
    }

    void createWindowWithTheme(const QString &bg, const QString &fg) {
        tmpTheme = new QTemporaryFile();
        ASSERT_TRUE(tmpTheme->open());
        qint64 written = tmpTheme->write(QString("#editor { background-color: %1; color: %2; }")
            .arg(bg, fg).toUtf8());
        ASSERT_GT(written, 0);
        tmpTheme->close();
        QSettings settings;
        settings.setValue(Preferences::CssFiles, QStringList() << tmpTheme->fileName());
        settings.setValue(Preferences::ActiveCssFile, tmpTheme->fileName());
        window = new MainWindow();
        window->resize(1200, 800);
        window->show();
        QApplication::processEvents();
    }

    QTemporaryFile *tmpTheme = nullptr;
    MainWindow *window = nullptr;
};

TEST_F(GutterThemeInitTest, DarkThemeGutterColorReachesPalette) {
    createWindowWithTheme("#282a36", "#f8f8f2");
    QString es = window->editor()->styleSheet();
    QColor gutterBg = QColor("#282a36").darker(120);
    QColor expected(gutterBg.red() + (0xf8 - gutterBg.red()) * 30 / 100,
                    gutterBg.green() + (0xf8 - gutterBg.green()) * 30 / 100,
                    gutterBg.blue() + (0xf2 - gutterBg.blue()) * 30 / 100);
    EXPECT_EQ(window->editor()->gutter()->palette().windowText().color(), expected);
}

TEST_F(GutterThemeInitTest, LightThemeGutterColorReachesPalette) {
    createWindowWithTheme("#ffffff", "#24292f");
    QColor gutterBg = QColor("#ffffff").darker(105);
    QColor expected(gutterBg.red() + (0x24 - gutterBg.red()) * 30 / 100,
                    gutterBg.green() + (0x29 - gutterBg.green()) * 30 / 100,
                    gutterBg.blue() + (0x2f - gutterBg.blue()) * 30 / 100);
    EXPECT_EQ(window->editor()->gutter()->palette().windowText().color(), expected);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

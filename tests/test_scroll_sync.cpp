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

class ScrollSyncIntegrationTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(s_argc, s_argv);
    }

    void SetUp() override {
        // Clear settings that MainWindow constructor reads to avoid
        // stale last-file path or blocking warning dialogs
        QSettings settings;
        settings.remove(Preferences::LastOpenedFile);
        settings.setValue(Preferences::ReopenLastCorpus, false);

        tmpFile = new QTemporaryFile();
        ASSERT_TRUE(tmpFile->open());
        for (int i = 0; i < 200; ++i)
            tmpFile->write(QString("%1\n\n").arg(i, 3, 10, QChar('0')).toUtf8());
        tmpFile->close();

        window = new MainWindow();
        window->resize(1200, 800);
        window->show();
        QApplication::processEvents();
    }

    void TearDown() override {
        delete window;
        delete tmpFile;
    }

    QTemporaryFile *tmpFile = nullptr;
    MainWindow *window = nullptr;
};

/* ========== Test I: end-to-end editor->preview anchor alignment ========== */

// Evaluates JS in the window's preview page and returns a double (waits for the
// async reply). Reused across the integration tests below.
static double evalPreview(MainWindow *w, const QString &js, int waitMs = 250)
{
    double v = -1;
    w->preview()->page()->runJavaScript(js, [&](const QVariant &r) { v = r.toDouble(); });
    QTest::qWait(waitMs);
    return v;
}

// Returns the block-start line nearest the preview's current viewport top:
// the largest data-line of any element whose top is at/below scrollY.
static int previewTopBlockLine(MainWindow *w)
{
    const QString js =
        "(function(){"
        "var sy=window.scrollY;"
        "var e=document.querySelectorAll('#scriba-content [data-line]');"
        "var out=0;"
        "for(var i=0;i<e.length;i++){var L=parseInt(e[i].getAttribute('data-line'),10);"
        "var t=e[i].getBoundingClientRect().top+window.scrollY;"
        "if(t<=sy+2&&L>out)out=L;}"
        "return out;})()";
    return static_cast<int>(evalPreview(w, js));
}

static int editorTopBlockLine(MainWindow *w)
{
    QTextCursor c = w->editor()->cursorForPosition(
        QPoint(w->editor()->viewport()->width() / 2, 1));
    return c.blockNumber() + 1;
}

TEST_F(ScrollSyncIntegrationTest, MixedContentPreviewTracksEditorBlock) {
    // doc = padding + mermaid fence + broken image + list, then more padding so
    // the editor can scroll deep enough to cross the fence.
    QStringList a;
    for (int i = 0; i < 40; ++i) { a << QString("Filler %1").arg(i) << QString(); }
    a << "```mermaid" << "flowchart LR" << "  A --> B" << "```" << QString();
    a << "![broken](/nonexistent/image.png)" << QString();
    a << "1. first" << "2. second" << "3. third" << QString();
    for (int i = 0; i < 40; ++i) { a << QString("Tail %1").arg(i) << QString(); }
    const int fenceLine = a.indexOf("```mermaid") + 1;
    const QString doc = a.join('\n');

    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    tmpFile->write(doc.toUtf8());
    tmpFile->close();
    window->loadFile(tmpFile->fileName());

    // wait for load + heavy render (mermaid) + index build + post-settle sync
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    QTest::qWait(2500);

    // The fence block must exist in the preview (either as the mermaid chart
    // wrap, or the <pre> before mermaid finishes).
    double fenceTop = evalPreview(window, QString(
        "(function(){var e=document.querySelector('[data-line=\"%1\"]');"
        "return e?e.getBoundingClientRect().top+window.scrollY:-1;})()").arg(fenceLine));
    EXPECT_GE(fenceTop, 0) << "preview must contain a block with data-line=" << fenceLine;

    // Scroll the editor to several positions; the preview's top block line
    // must track the editor's top source line (percentage sync violated this
    // around the chart/image/list regions).
    for (int pos = 0; pos < 4; ++pos) {
        int v = pos == 0 ? 0 : window->editor()->verticalScrollBar()->maximum() * pos / 3;
        window->editor()->verticalScrollBar()->setValue(v);
        QApplication::processEvents();
        QTest::qWait(700);
        int edLine = editorTopBlockLine(window);
        int pvLine = previewTopBlockLine(window);
        EXPECT_LE(qAbs(pvLine - edLine), 2)
            << "preview top block (line " << pvLine << ") diverged from editor top "
            << "(line " << edLine << ") at editor scroll " << v;
    }
}

TEST_F(ScrollSyncIntegrationTest, LargeDocumentPreviewRendersAsynchronously) {
    // > kLargeDocBlocks (4000): the md→HTML render is dispatched to the
    // background PreviewRenderWorker instead of running inline on the GUI
    // thread. The preview must still end up fully loaded — including the very
    // last heading of the document — proving the async result was committed.
    QStringList a;
    for (int i = 0; i < 4500; ++i) { a << QString("Heading %1").arg(i) << QString("Body %1").arg(i) << QString(); }
    const QString doc = a.join('\n');

    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    tmpFile->write(doc.toUtf8());
    tmpFile->close();

    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    window->loadFile(tmpFile->fileName());

    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded) << "large-document async preview render never loaded";

    // The tail of the large document must have been rendered and committed.
    double hasTail = evalPreview(window,
        "(function(){return document.body.textContent.indexOf('Heading 4499')>=0?1:0;})()", 1500);
    EXPECT_EQ(hasTail, 1) << "async render did not commit the full large document";
}

TEST_F(ScrollSyncIntegrationTest, PrintLayoutStillSyncsAfterRebuild) {
    // Tall document so the editor can scroll deep and the paginator must
    // split the preview into several page boxes.
    QStringList a;
    for (int i = 0; i < 120; ++i) { a << QString("Fill %1").arg(i) << QString(); }
    const QString doc = a.join('\n');

    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    tmpFile->write(doc.toUtf8());
    tmpFile->close();
    window->loadFile(tmpFile->fileName());

    // Enable print layout: forces the full setHtmlWithOverlay rebuild path
    // with the paginator script embedded; the preview script's own render
    // tails must run scribaPaginate and keep the anchor index usable.
    window->showPageBreaksAction()->setChecked(true);
    QTest::qWait(3000);

    // The paginator must have run on the initial paint (page separators).
    double separators = evalPreview(window,
        "(function(){return document.querySelectorAll('#scriba-content .scriba-pb').length;})()");
    EXPECT_GT(separators, 0) << "print layout must paginate the preview";

    // The anchor index must survive the paginated rebuild: an explicit scroll
    // to a mid-document source line lands on that block.
    window->preview()->page()->runJavaScript("scribaScrollToSourceLine(53.0);", [](const QVariant&){});
    QTest::qWait(250);
    double scrollY = evalPreview(window, "window.scrollY");
    double docTop53 = evalPreview(window,
        "(function(){var e=document.querySelector('[data-line=\"53\"]');"
        "return e?e.getBoundingClientRect().top+window.scrollY:-1;})()");
    EXPECT_NEAR(scrollY, docTop53, 2.0)
        << "print layout: anchored scroll must land on the target block";

    for (int pos = 1; pos <= 3; ++pos) {
        int v = window->editor()->verticalScrollBar()->maximum() * pos / 4;
        window->editor()->verticalScrollBar()->setValue(v);
        QApplication::processEvents();
        QTest::qWait(700);
        int edLine = editorTopBlockLine(window);
        int pvLine = previewTopBlockLine(window);
        EXPECT_LE(qAbs(pvLine - edLine), 2)
            << "print layout: preview top block (line " << pvLine << ") diverged from "
            << "editor top (line " << edLine << ") at editor scroll " << v;
    }

    // Page-break mode is a persisted setting; reset it so later windows in
    // this process start in normal mode (print layout skips the editor chrome
    // stylesheet in refreshPreviewCss, which breaks window-init theme tests).
    window->showPageBreaksAction()->setChecked(false);
    QSettings().remove(Preferences::PreviewShowPageBreaks);
}

TEST_F(ScrollSyncIntegrationTest, TabSwitchPreScrollAnchorsToNewEditorLine) {
    auto makeDoc = [](const QString &tag, int n) {
        QStringList a;
        for (int i = 0; i < n; ++i) a << QString("%1 %2").arg(tag).arg(i) << QString();
        return a.join('\n');
    };
    QString docA = makeDoc("ALPHA", 90);
    QString docB = makeDoc("BRAVO", 90);

    auto ta = new QTemporaryFile();
    ASSERT_TRUE(ta->open()); ta->write(docA.toUtf8()); ta->close();
    auto tb = new QTemporaryFile();
    ASSERT_TRUE(tb->open()); tb->write(docB.toUtf8()); tb->close();

    window->loadFile(ta->fileName()); // tab 0
    window->loadFile(tb->fileName()); // tab 1 (now current)

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_EQ(tabBar->count(), 2);

    auto scrollEditorMiddle = [this]() {
        auto *sb = window->editor()->verticalScrollBar();
        sb->setValue(sb->maximum() / 2);
        QApplication::processEvents();
        QTest::qWait(700);
    };

    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    QTest::qWait(1500);

    scrollEditorMiddle();                    // B mid
    tabBar->setCurrentIndex(0);              // -> A
    QTest::qWait(1800);
    scrollEditorMiddle();                    // A mid
    tabBar->setCurrentIndex(1);              // -> B
    QTest::qWait(2000);                      // render + scheduled re-assert (450ms) + marge

    int edLine = editorTopBlockLine(window);
    int pvLine = previewTopBlockLine(window);
    EXPECT_GT(edLine, 20) << "sanity: editor should be scrolled past the top";
    EXPECT_LE(qAbs(pvLine - edLine), 2)
        << "after switching to tab B, preview must anchor to B's editor top line";

    delete ta;
    delete tb;
}

TEST_F(ScrollSyncIntegrationTest, LiveEditRestoresAnchoredToSameBlock) {
    // doc with a long code block in the middle so the viewport sits well inside
    // the document; editing at the END must not move the preview's top block.
    QStringList a;
    for (int i = 0; i < 60; ++i) a << QString("Filler %1").arg(i) << QString();
    a << "```" << "const x = 1;" << "const y = 2;" << "```" << QString();
    for (int i = 0; i < 60; ++i) a << QString("Tail %1").arg(i) << QString();
    const QString doc = a.join('\n');

    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    tmpFile->write(doc.toUtf8());
    tmpFile->close();
    window->loadFile(tmpFile->fileName());

    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    QTest::qWait(1500);

    window->editor()->verticalScrollBar()->setValue(
        window->editor()->verticalScrollBar()->maximum() / 2);
    QApplication::processEvents();
    QTest::qWait(800);

    const double before = evalPreview(window, "scribaCaptureAnchorLine()");
    EXPECT_GT(before, 1);

    // append text at the very end -> debounced live re-render -> anchored restore
    QTextCursor cur(window->editor()->document());
    cur.movePosition(QTextCursor::End);
    cur.insertText("\n\nEND-MARKER");
    QApplication::processEvents();
    QTest::qWait(2800); // 80ms debounce + heavy render + image/chart settle

    const double after = evalPreview(window, "scribaCaptureAnchorLine()");
    double gone = evalPreview(window,
        "(function(){return document.getElementById('scriba-content')"
        "?(document.getElementById('scriba-content').innerText.indexOf('END-MARKER')>=0?1:0):0;})()");
    EXPECT_EQ(static_cast<int>(gone), 1) << "live edit must reach the preview";
    EXPECT_LE(qAbs(after - before), 2)
        << "after an append at the end, the preview must keep the same top block";
}

TEST_F(ScrollSyncIntegrationTest, EditorScrollbarCorrectAfterFileOpenAndCursorRestore) {
    window->loadFile(tmpFile->fileName());

    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 100);
    window->editor()->setTextCursor(cursor);
    window->editor()->centerCursor();
    QApplication::processEvents();

    double pct = scrollbarFraction(window->editor()->verticalScrollBar());
    EXPECT_GT(pct, 0.15);
    EXPECT_LT(pct, 0.85);
}

TEST_F(ScrollSyncIntegrationTest, PreviewScrollSyncsAfterDeferredUpdate) {
    window->loadFile(tmpFile->fileName());

    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 100);
    window->editor()->setTextCursor(cursor);
    window->editor()->centerCursor();
    QApplication::processEvents();

    // Wait for the 80ms deferred timer to fire, calling updatePreview() -> setHtml()
    // Create spy BEFORE qWait to capture any loadFinished signals that fire
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    QTest::qWait(200);

    // Scan all captured loadFinished signals for a successful one
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i) {
        if (loadSpy.at(i).at(0).toBool()) {
            loaded = true;
            break;
        }
    }

    // If no successful loadFinished yet, wait for more
    while (!loaded) {
        if (!loadSpy.wait(5000))
            break;
        if (loadSpy.last().at(0).toBool())
            loaded = true;
    }
    ASSERT_TRUE(loaded);

    // Allow scroll-to-percent JS (from syncPreviewScroll callback) to execute
    QTest::qWait(1000);

    // Verify the preview has rendered content and is scrolled
    double scrollY = 0;
    window->preview()->page()->runJavaScript(
        "document.body ? Math.round(window.scrollY) : 0",
        [&](const QVariant &r) { scrollY = r.toDouble(); });
    QTest::qWait(2000);
    EXPECT_GT(scrollY, 0);
}

/* ========== Regression: %N tokens in content must not break incremental preview ========== */
// themes.md's "What each element uses" table is full of literal %2..%20 tokens.
// The incremental updater built its scribaUpdate(...) JS with chained
// QString::arg() calls; the trailing .arg()s re-scan the whole string and
// clobber those %N tokens, leaving %5/%6 unfilled -> SyntaxError -> the
// preview silently stops updating. The initial full render (single multi-arg
// .arg) is unaffected, so the doc looks fine until you edit it.

TEST_F(ScrollSyncIntegrationTest, PreviewUpdatesDespitePercentPlaceholderTokens) {
    const QString marker = QStringLiteral("AFTER-EDIT-UNIQUE-TOKEN-9f8d");
    const char doc[] =
        "| Widget / element | Background | Text |\n"
        "|---|---|---|\n"
        "| QDialog | `%2` | |\n"
        "| QGroupBox | `%3` | `%4` |\n"
        "| QCheckBox | `%10` | disabled `%9` |\n";

    auto tf = new QTemporaryFile();
    ASSERT_TRUE(tf->open());
    tf->write(doc);
    tf->close();

    window->loadFile(tf->fileName());

    // wait for the initial full render to finish
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    QTest::qWait(200);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded);
    QTest::qWait(1000);

    // Append a marker line; the editor's deferred updatePreview pushes an
    // incremental scribaUpdate that (pre-fix) was corrupted into a SyntaxError.
    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n\n%1\n").arg(marker));
    QApplication::processEvents();
    QTest::qWait(500);

    QString bodyText;
    window->preview()->page()->runJavaScript(
        "document.getElementById('scriba-content') ? "
        "document.getElementById('scriba-content').innerText : ''",
        [&](const QVariant &r) { bodyText = r.toString(); });
    QTest::qWait(2000);

    EXPECT_TRUE(bodyText.contains(marker))
        << "preview should reflect live edits even when content contains %N tokens";
    delete tf;
}

/* ========== Test D: Table insertion scroll sync ========== */

TEST_F(ScrollSyncIntegrationTest, TableInsertScrollSyncsPreview) {
    window->loadFile(tmpFile->fileName());

    // Scroll editor to top so we can detect movement
    window->editor()->verticalScrollBar()->setValue(0);
    QApplication::processEvents();

    // Position cursor past the halfway point (doc is 399 lines: 200 + blank
    // lines between) so the table is inserted off-screen
    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 300);
    window->editor()->setTextCursor(cursor);
    QApplication::processEvents();

    // Auto-accept the table dialog with default settings (3 columns, with header)
    QTimer::singleShot(0, [&]() {
        auto *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        ASSERT_NE(dlg, nullptr);
        dlg->accept();
    });

    // Call the real method — blocks until the timer fires and dialog is accepted
    window->showTableInsert();
    QApplication::processEvents();

    // Verify editor scrolled to the cursor (centerCursor inside showTableInsert)
    double pct = scrollbarFraction(window->editor()->verticalScrollBar());
    EXPECT_GT(pct, 0.5);

    // Wait for the 80ms deferred timer + preview load
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    QTest::qWait(200);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i) {
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded);

    QTest::qWait(1000);

    // Verify preview scrolled down to show the table
    double scrollY = 0;
    window->preview()->page()->runJavaScript(
        "document.body ? Math.round(window.scrollY) : 0",
        [&](const QVariant &r) { scrollY = r.toDouble(); });
    QTest::qWait(2000);
    EXPECT_GT(scrollY, 0);
}

/* ========== Test E: Image insertion scroll sync ========== */

TEST_F(ScrollSyncIntegrationTest, ImageInsertScrollSyncsPreview) {
    window->loadFile(tmpFile->fileName());

    // Scroll editor to top so we can detect movement
    window->editor()->verticalScrollBar()->setValue(0);
    QApplication::processEvents();

    // Position cursor at line 150 so the image is inserted off-screen
    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 150);
    window->editor()->setTextCursor(cursor);
    QApplication::processEvents();

    // Insert image markdown (non-existent file triggers onerror immediately)
    window->editor()->textCursor().insertText("![](/nonexistent/image.png)");
    QApplication::processEvents();

    // Wait for the 80ms deferred timer + preview update + image loading
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    QTest::qWait(300);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i) {
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded);

    // Allow image onerror + promise resolution + syncPreviewScroll to execute
    QTest::qWait(2000);

    // Verify preview scrolled down to show the image
    double scrollY = 0;
    window->preview()->page()->runJavaScript(
        "document.body ? Math.round(window.scrollY) : 0",
        [&](const QVariant &r) { scrollY = r.toDouble(); });
    QTest::qWait(2000);
    EXPECT_GT(scrollY, 0);
}

/* ========== Test E2: async content update must not yank the preview ========== */

TEST_F(ScrollSyncIntegrationTest, AsyncContentUpdateDoesNotYankPreviewScroll) {
    // Rewrite the temp file with enough lines to make the preview scrollable,
    // plus async content (mermaid) so the deferred restore path runs.
    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    for (int i = 0; i < 300; ++i)
        tmpFile->write(QString("Line %1\n\n").arg(i, 3, 10, QChar('0')).toUtf8());
    tmpFile->write("```mermaid\nflowchart LR\n  A --> B\n```\n");
    tmpFile->close();

    window->loadFile(tmpFile->fileName());

    // Wait for load
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded);
    QTest::qWait(2000);

    // Scroll editor to the middle so sync re-asserts a known preview baseline
    window->editor()->verticalScrollBar()->setValue(
        window->editor()->verticalScrollBar()->maximum() / 2);
    QApplication::processEvents();
    QTest::qWait(800);

    double baseline = 0;
    window->preview()->page()->runJavaScript("window.scrollY",
        [&](const QVariant &r) { baseline = r.toDouble(); });
    QTest::qWait(500);
    EXPECT_GT(baseline, 1000);

    // Type a character -> debounced update -> scribaUpdate captures the baseline
    window->editor()->setFocus();
    QTest::keyClick(window->editor(), Qt::Key_Space);
    QApplication::processEvents();

    // Simulate the user scrolling the preview down during the 1500ms render window
    QTest::qWait(400);
    window->preview()->page()->runJavaScript("window.scrollTo(0, 20000)");
    QTest::qWait(300);
    double userY = 0;
    window->preview()->page()->runJavaScript("window.scrollY",
        [&](const QVariant &r) { userY = r.toDouble(); });
    QTest::qWait(100);

    // Wait well past the 1500ms heavy timer + async mermaid render
    QTest::qWait(4000);

    double finalY = 0;
    window->preview()->page()->runJavaScript("window.scrollY",
        [&](const QVariant &r) { finalY = r.toDouble(); });
    QTest::qWait(500);

    // The user-scrolled position must be preserved, not yanked back to the baseline
    EXPECT_GT(userY, baseline + 1000);
    EXPECT_GT(finalY, baseline + 1000);
}

/* ========== Test F: Single tab preview renders content ========== */

TEST_F(ScrollSyncIntegrationTest, InitialTabRendersPreviewContent) {
    window->editor()->setPlainText("# Hello Tab\n\nThis is the first tab.");
    QApplication::processEvents();

    // Wait for 80ms debounce timer + preview re-render + async JS
    QTest::qWait(5000);

    QString html;
    window->preview()->page()->toHtml([&](const QString &h) { html = h; });
    QTest::qWait(2000);

    EXPECT_TRUE(html.contains("Hello Tab"));
    EXPECT_TRUE(html.contains("first tab"));
}

TEST_F(ScrollSyncIntegrationTest, SingleFileTabRendersPreviewAfterOpen) {
    window->loadFile(tmpFile->fileName());

    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    QTest::qWait(300);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded);
    QTest::qWait(1000);

    QString html;
    window->preview()->page()->toHtml([&](const QString &h) { html = h; });
    QTest::qWait(2000);

    EXPECT_TRUE(html.contains("001"));
}

TEST_F(ScrollSyncIntegrationTest, PreviewTemplateHasRenderOverlayHiddenAfterRender) {
    window->loadFile(tmpFile->fileName());

    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    QTest::qWait(300);
    bool loaded = false;
    for (int i = 0; i < loadSpy.count(); ++i)
        if (loadSpy.at(i).at(0).toBool()) { loaded = true; break; }
    while (!loaded) {
        if (!loadSpy.wait(5000)) break;
        if (loadSpy.last().at(0).toBool()) loaded = true;
    }
    ASSERT_TRUE(loaded);
    QTest::qWait(1500);

    QString result;
    window->preview()->page()->runJavaScript(
        "(function(){var o=document.getElementById('scriba-rendering-overlay');"
        "return JSON.stringify({overlay:!!o,content:!!document.getElementById('scriba-content'),"
        "hidden:!!o&&getComputedStyle(o).display==='none',"
        "fns:(typeof scribaBeginRender==='function'&&typeof scribaEndRender==='function')});})()",
        [&](const QVariant &r) { result = r.toString(); });
    QTest::qWait(1500);

    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
    ASSERT_FALSE(doc.isNull()) << result.toStdString();
    QJsonObject obj = doc.object();
    EXPECT_TRUE(obj.value("overlay").toBool());
    EXPECT_TRUE(obj.value("content").toBool());
    EXPECT_TRUE(obj.value("hidden").toBool());
    EXPECT_TRUE(obj.value("fns").toBool());
}

TEST_F(ScrollSyncIntegrationTest, TabSwitchPreviewShowsOnlyCurrentDocument) {
    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    tmpFile->write("AAA-MARKER\n\nLine two.\n\nLine three.\n");
    tmpFile->close();
    window->loadFile(tmpFile->fileName());

    QTemporaryFile second;
    ASSERT_TRUE(second.open());
    second.write("BBB-MARKER\n\nOther line.\n\nMore lines.\n");
    second.close();
    window->loadFile(second.fileName());

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_EQ(tabBar->count(), 2);

    auto previewHtml = [this]() {
        QString html;
        window->preview()->page()->toHtml([&](const QString &h) { html = h; });
        QTest::qWait(500);
        return html;
    };
    auto previewSettlesOn = [&](const QString &present, const QString &absent) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            QString html = previewHtml();
            if (html.contains(present) && !html.contains(absent))
                return true;
            QTest::qWait(1000);
        }
        return false;
    };

    // Currently on tab B (the second file): only B's content may be visible
    EXPECT_TRUE(previewSettlesOn("BBB-MARKER", "AAA-MARKER"));

    // Switch back to tab A: stale B content must be gone once A renders
    tabBar->setCurrentIndex(0);
    EXPECT_TRUE(previewSettlesOn("AAA-MARKER", "BBB-MARKER"));

    // And forward to tab B again
    tabBar->setCurrentIndex(1);
    EXPECT_TRUE(previewSettlesOn("BBB-MARKER", "AAA-MARKER"));
}

TEST_F(ScrollSyncIntegrationTest, TabSwitchDoesNotReloadPreviewPage) {
    ASSERT_TRUE(tmpFile->open());
    tmpFile->resize(0);
    tmpFile->write("AAA-MARKER\n\nLine two.\n\nLine three.\n");
    tmpFile->close();
    window->loadFile(tmpFile->fileName());

    QTemporaryFile second;
    ASSERT_TRUE(second.open());
    second.write("BBB-MARKER\n\nOther line.\n\nMore lines.\n");
    second.close();
    window->loadFile(second.fileName());

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_EQ(tabBar->count(), 2);

    auto previewHtml = [this]() {
        QString html;
        window->preview()->page()->toHtml([&](const QString &h) { html = h; });
        QTest::qWait(500);
        return html;
    };
    auto previewSettlesOn = [&](const QString &present, const QString &absent) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            QString html = previewHtml();
            if (html.contains(present) && !html.contains(absent))
                return true;
            QTest::qWait(1000);
        }
        return false;
    };

    // Let the preview fully load tab B before counting page loads
    EXPECT_TRUE(previewSettlesOn("BBB-MARKER", "AAA-MARKER"));

    // A fast tab switch must reuse the live page: content swaps in place and
    // no full page load (loadFinished) is triggered.
    QSignalSpy loadSpy(window->preview()->page(), &QWebEnginePage::loadFinished);
    loadSpy.clear();
    tabBar->setCurrentIndex(0);
    EXPECT_TRUE(previewSettlesOn("AAA-MARKER", "BBB-MARKER"));
    EXPECT_EQ(loadSpy.count(), 0);

    // And back again
    tabBar->setCurrentIndex(1);
    EXPECT_TRUE(previewSettlesOn("BBB-MARKER", "AAA-MARKER"));
    EXPECT_EQ(loadSpy.count(), 0);
}

TEST_F(ScrollSyncIntegrationTest, TabSwitchUpdatesDocumentBaseUrl) {
    QTemporaryDir dirA;
    QTemporaryDir dirB;
    ASSERT_TRUE(dirA.isValid());
    ASSERT_TRUE(dirB.isValid());
    QString fileA = dirA.filePath("doc-a.md");
    QString fileB = dirB.filePath("doc-b.md");
    QFile fa(fileA);
    ASSERT_TRUE(fa.open(QIODevice::WriteOnly));
    fa.write("AAA-MARKER\n\n![img](pic.png)\n");
    fa.close();
    QFile fb(fileB);
    ASSERT_TRUE(fb.open(QIODevice::WriteOnly));
    fb.write("BBB-MARKER\n\n![img](pic.png)\n");
    fb.close();

    window->loadFile(fileA);
    window->loadFile(fileB);

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_EQ(tabBar->count(), 2);

    auto previewSettlesOn = [this](const QString &present, const QString &absent) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            QString html;
            window->preview()->page()->toHtml([&](const QString &h) { html = h; });
            QTest::qWait(500);
            if (html.contains(present) && !html.contains(absent))
                return true;
            QTest::qWait(1000);
        }
        return false;
    };
    auto evalStr = [this](const QString &js) {
        QString v;
        window->preview()->page()->runJavaScript(js, [&](const QVariant &r) { v = r.toString(); });
        QTest::qWait(300);
        return v;
    };

    // Let the preview fully load tab B first
    EXPECT_TRUE(previewSettlesOn("BBB-MARKER", "AAA-MARKER"));

    // Relative assets on tab A must resolve against dir A, not the page's
    // original load location.
    tabBar->setCurrentIndex(0);
    EXPECT_TRUE(previewSettlesOn("AAA-MARKER", "BBB-MARKER"));
    const QString expA = QUrl::fromLocalFile(dirA.path() + "/").toString();
    EXPECT_TRUE(evalStr("document.baseURI").startsWith(expA));
    EXPECT_TRUE(evalStr("var i=document.querySelector('img');i?i.src:''").startsWith(expA));

    // Switching to tab B redirects relative assets to dir B.
    tabBar->setCurrentIndex(1);
    EXPECT_TRUE(previewSettlesOn("BBB-MARKER", "AAA-MARKER"));
    const QString expB = QUrl::fromLocalFile(dirB.path() + "/").toString();
    EXPECT_TRUE(evalStr("document.baseURI").startsWith(expB));
    EXPECT_TRUE(evalStr("var i=document.querySelector('img');i?i.src:''").startsWith(expB));
}

/* ========== Regression: relative images must survive edits after a tab switch ========== */

TEST_F(ScrollSyncIntegrationTest, BaseUrlSurvivesEdits) {
    QTemporaryDir dirA;
    QTemporaryDir dirB;
    ASSERT_TRUE(dirA.isValid());
    ASSERT_TRUE(dirB.isValid());
    QString fileA = dirA.filePath("doc-a.md");
    QString fileB = dirB.filePath("doc-b.md");
    QFile fa(fileA);
    ASSERT_TRUE(fa.open(QIODevice::WriteOnly));
    fa.write("AAA-EDITED\n\n![img](pic.png)\n");
    fa.close();
    QFile fb(fileB);
    ASSERT_TRUE(fb.open(QIODevice::WriteOnly));
    fb.write("BBB-EDITED\n\n![img](pic.png)\n");
    fb.close();

    window->loadFile(fileA);
    window->loadFile(fileB);

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);

    auto previewSettlesOn = [this](const QString &present, const QString &absent) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            QString html;
            window->preview()->page()->toHtml([&](const QString &h) { html = h; });
            QTest::qWait(500);
            if (html.contains(present) && !html.contains(absent))
                return true;
            QTest::qWait(1000);
        }
        return false;
    };
    auto evalStr = [this](const QString &js) {
        QString v;
        window->preview()->page()->runJavaScript(js, [&](const QVariant &r) { v = r.toString(); });
        QTest::qWait(300);
        return v;
    };
    auto waitForBasePrefix = [&](const QString &prefix) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            if (evalStr("document.baseURI").startsWith(prefix)
                && evalStr("var i=document.querySelector('img');i?i.src:''").startsWith(prefix))
                return true;
            QTest::qWait(500);
        }
        return false;
    };

    // Load B, then switch to A: relative assets on A resolve against dir A.
    EXPECT_TRUE(previewSettlesOn("BBB-EDITED", "AAA-EDITED"));
    tabBar->setCurrentIndex(0);
    EXPECT_TRUE(previewSettlesOn("AAA-EDITED", "BBB-EDITED"));
    const QString expA = QUrl::fromLocalFile(dirA.path() + "/").toString();
    const QString expB = QUrl::fromLocalFile(dirB.path() + "/").toString();

    // Edit A. The shared page's base must survive the debounced re-render that
    // follows (previously scribaUpdate got an empty baseUrl on edits and
    // removed the <base> element, so images fell back to dir B's page URL).
    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\nEDITED AFTER SWITCH");
    EXPECT_TRUE(previewSettlesOn("EDITED AFTER SWITCH", "BBB-EDITED"));
    EXPECT_TRUE(waitForBasePrefix(expA));

    // Type further into A; still anchored on dir A.
    QTextCursor cursor2(window->editor()->document());
    cursor2.movePosition(QTextCursor::End);
    cursor2.insertText("\nMORE EDITS");
    EXPECT_TRUE(previewSettlesOn("MORE EDITS", "BBB-EDITED"));
    EXPECT_TRUE(waitForBasePrefix(expA));

    // Switch to B and edit it: the base must now follow dir B.
    tabBar->setCurrentIndex(1);
    EXPECT_TRUE(previewSettlesOn("BBB-EDITED", "MORE EDITS"));
    QTextCursor cursor3(window->editor()->document());
    cursor3.movePosition(QTextCursor::End);
    cursor3.insertText("\nEDITED IN B");
    EXPECT_TRUE(previewSettlesOn("EDITED IN B", "AAA-EDITED"));
    EXPECT_TRUE(waitForBasePrefix(expB));
}

/* ========== Untitled documents in a corpus resolve relative assets against the corpus root ========== */

TEST_F(ScrollSyncIntegrationTest, UntitledTabInCorpusUsesCorpusRootAsBase) {
    // Force Qt's widget file dialog so the test can drive it programmatically;
    // the GTK native dialog cannot be accepted from outside.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString corpusPath = dir.filePath("corpus.scriba");
    Corpus c;
    c.filePath = corpusPath;
    c.name = QStringLiteral("BaseTest");
    c.active = 0;
    c.documents = {
        CorpusDocument{ .path = QString(),
                        .content = QStringLiteral("CORPUS-UNTITLED\n\n![img](pic.png)\n") },
    };
    ASSERT_TRUE(c.save());

    window->openCorpusFile(corpusPath, /*skipPrompt=*/true);
    QApplication::processEvents();

    // openCorpusFile keeps the empty placeholder tab (index 0) and adds the
    // embedded document after it; m_corpus.active would select the placeholder.
    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_GE(tabBar->count(), 2);
    tabBar->setCurrentIndex(tabBar->count() - 1);
    QApplication::processEvents();

    auto evalStr = [this](const QString &js) {
        QString v;
        window->preview()->page()->runJavaScript(js, [&](const QVariant &r) { v = r.toString(); });
        QTest::qWait(300);
        return v;
    };
    auto waitForBasePrefix = [&](const QString &prefix) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            const QString base = evalStr("document.baseURI");
            const QString src = evalStr("var i=document.querySelector('img');i?i.src:''");
            if (base.startsWith(prefix) && src.startsWith(prefix))
                return true;
            QTest::qWait(500);
        }
        return false;
    };

    const QString corpusBase = QUrl::fromLocalFile(dir.path() + "/").toString();
    EXPECT_TRUE(waitForBasePrefix(corpusBase))
        << "Untitled corpus document must resolve relative assets against the corpus root";

    // Saving the document elsewhere moves the base to the saved file's directory.
    QTemporaryDir other;
    ASSERT_TRUE(other.isValid());
    const QString target = other.filePath("saved.md");
    QTimer::singleShot(0, [&]() {
        auto *dlg = qobject_cast<QFileDialog *>(qApp->activeModalWidget());
        if (!dlg)
            return;
        dlg->selectFile(target);
        QMetaObject::invokeMethod(dlg, "accept", Qt::QueuedConnection);
    });
    const auto actions = window->findChildren<QAction *>();
    bool triggered = false;
    for (QAction *a : actions) {
        if (a->text().contains(QStringLiteral("Save &As"))) {
            a->trigger();
            triggered = true;
            break;
        }
    }
    ASSERT_TRUE(triggered) << "Save As action must exist";

    const QString savedBase = QUrl::fromLocalFile(other.path() + "/").toString();
    EXPECT_TRUE(waitForBasePrefix(savedBase))
        << "After saving an untitled corpus document, relative assets must resolve "
           "against the saved file's directory";
    EXPECT_TRUE(QFile::exists(target));
}

/* ========== Regression: reordering tabs must move each tab's cached render with it ========== */

TEST_F(ScrollSyncIntegrationTest, MoveTabCarriesPreviewCacheToNewPosition) {
    const char docA[] = "AAA-REORDER\n\nLine two A.\n\nLine three A.\n";
    const char docB[] = "BBB-REORDER\n\nLine two B.\n\nLine three B.\n";

    auto ta = new QTemporaryFile();
    ASSERT_TRUE(ta->open());
    ta->write(docA);
    ta->close();
    window->loadFile(ta->fileName());   // tab 0, marker AAA

    auto tb = new QTemporaryFile();
    ASSERT_TRUE(tb->open());
    tb->write(docB);
    tb->close();
    window->loadFile(tb->fileName());   // tab 1, marker BBB

    auto *tabBar = window->findChild<QTabBar *>();
    ASSERT_NE(tabBar, nullptr);
    ASSERT_EQ(tabBar->count(), 2);

    auto previewSettlesOn = [&](const QString &present, const QString &absent) {
        for (int attempt = 0; attempt < 12; ++attempt) {
            QString html;
            window->preview()->page()->toHtml([&](const QString &h) { html = h; });
            QTest::qWait(500);
            if (html.contains(present) && !html.contains(absent))
                return true;
            QTest::qWait(1000);
        }
        return false;
    };

    // Currently on tab 1 (BBB), currently shown.
    EXPECT_TRUE(previewSettlesOn("BBB-REORDER", "AAA-REORDER"));

    // Reorder: move tab 0 (AAA) to position 1 -> order becomes [B, A].
    tabBar->moveTab(0, 1);
    QApplication::processEvents();

    // Now index 0 holds B. A's cached render must have followed A to index 1.
    tabBar->setCurrentIndex(0);
    EXPECT_TRUE(previewSettlesOn("BBB-REORDER", "AAA-REORDER"));
    tabBar->setCurrentIndex(1);
    EXPECT_TRUE(previewSettlesOn("AAA-REORDER", "BBB-REORDER"));

    // And back to index 0.
    tabBar->setCurrentIndex(0);
    EXPECT_TRUE(previewSettlesOn("BBB-REORDER", "AAA-REORDER"));

    delete ta;
    delete tb;
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

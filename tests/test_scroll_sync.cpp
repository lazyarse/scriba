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
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "MainWindow.h"
#include "Editor.h"
#include "Gutter.h"
#include "Preview.h"
#include "StaticHelpers.h"
#include "Preferences.h"
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
        settings.setValue(Preferences::ReopenLastSession, false);

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
        settings.setValue(Preferences::ReopenLastSession, false);
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
        settings.setValue(Preferences::ReopenLastSession, false);
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

#include <gtest/gtest.h>
#include <QApplication>
#include <QScrollBar>
#include <QTemporaryFile>
#include <QTest>
#include <QSignalSpy>
#include <QTextCursor>
#include <QWebEnginePage>
#include <QSettings>
#include <QDir>
#include <QDialog>
#include <QTimer>

#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "StaticHelpers.h"
#include "Preferences.h"

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
        settings.setValue(Preferences::ReopenLastFile, false);
        settings.setValue(Preferences::FirstRun, false);

        tmpFile = new QTemporaryFile();
        ASSERT_TRUE(tmpFile->open());
        for (int i = 0; i < 200; ++i)
            tmpFile->write(QString("%1\n").arg(i, 3, 10, QChar('0')).toUtf8());
        tmpFile->close();
    }

    void TearDown() override {
        delete window;
        delete tmpFile;
    }

    QTemporaryFile *tmpFile = nullptr;
    MainWindow *window = nullptr;
};

TEST_F(ScrollSyncIntegrationTest, EditorScrollbarCorrectAfterFileOpenAndCursorRestore) {
    window = new MainWindow();
    QApplication::processEvents();

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
    window = new MainWindow();
    QApplication::processEvents();

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
        if (!loadSpy.wait(1000))
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

/* ========== Test D: Table insertion scroll sync ========== */

TEST_F(ScrollSyncIntegrationTest, TableInsertScrollSyncsPreview) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());

    // Scroll editor to top so we can detect movement
    window->editor()->verticalScrollBar()->setValue(0);
    QApplication::processEvents();

    // Position cursor at line 150 so the table is inserted off-screen
    QTextCursor cursor(window->editor()->document());
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 150);
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
        if (!loadSpy.wait(1000)) break;
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

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("ScribaTest");
    app.setApplicationName("ScribaTest");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

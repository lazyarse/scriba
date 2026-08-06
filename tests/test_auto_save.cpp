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
#include <QElapsedTimer>
#include <QSettings>
#include <QTest>
#include <QTemporaryFile>
#include <QTextCursor>
#include <QTimer>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"
#include "TestConfig.h"

// Observes the unsaved-changes prompt at its call site instead of driving
// the real modal QMessageBox, which would block waiting for user input.
class TestMainWindow : public MainWindow
{
public:
    bool unsavedPromptShown = false;
    bool lastPromptHadUntitled = false;

protected:
    MainWindow::ClosePromptResult promptUnsavedChanges(bool hasUntitledDirty) override
    {
        unsavedPromptShown = true;
        lastPromptHadUntitled = hasUntitledDirty;
        return MainWindow::ClosePromptResult::Discard;
    }
};

class AutoSaveTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_auto_save";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override {
        QSettings s;
        s.remove(Preferences::LastOpenedFile);
        s.setValue(Preferences::ReopenLastSession, false);
        s.setValue(Preferences::AutoSaveOnExit, false);
        s.setValue(Preferences::AutoSaveInterval, 0);

        tmpFile = new QTemporaryFile();
        ASSERT_TRUE(tmpFile->open());
        tmpFile->write("original content\n");
        tmpFile->close();
    }

    void TearDown() override {
        delete window;
        delete tmpFile;
    }

    QTemporaryFile *tmpFile = nullptr;
    MainWindow *window = nullptr;
};

TEST_F(AutoSaveTest, NoFileLoadedDoesNothing) {
    window = new MainWindow();
    QApplication::processEvents();

    window->editor()->setPlainText("unsaved content");
    window->autoSave();
}

TEST_F(AutoSaveTest, SavesContentToLoadedFile) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("modified content");
    window->autoSave();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "modified content");
}

TEST_F(AutoSaveTest, SaveOnExitWritesFile) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, true);

    auto *testWindow = new TestMainWindow();
    window = testWindow;
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("exit content");
    window->close();
    QApplication::processEvents();

    EXPECT_FALSE(testWindow->unsavedPromptShown) << "autosave-on-exit must save without prompting";

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "exit content");
}

TEST_F(AutoSaveTest, SaveOnExitDoesNotWriteWhenDisabled) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, false);

    auto *testWindow = new TestMainWindow();
    window = testWindow;
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("should not be saved");
    window->close();
    QApplication::processEvents();

    EXPECT_TRUE(testWindow->unsavedPromptShown) << "close on a dirty tab must prompt";
    EXPECT_FALSE(testWindow->lastPromptHadUntitled) << "loaded tabs are not untitled";

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "original content\n");
}

TEST_F(AutoSaveTest, AcceptedPreferenceApplyKeepsEditorAppliesSynchronous) {
    window = new MainWindow();
    window->show();
    QApplication::processEvents();

    QSettings s;
    s.setValue(Preferences::EditorCaretWidth, 5);
    QApplication::processEvents();

    // The editor-affecting applies must be visible immediately: the dialog's
    // live preview keeps the editor in sync, and OK-ing must not revert it.
    window->applyAcceptedPreferences();
    EXPECT_EQ(window->editor()->cursorWidth(), 5)
        << "editor settings must apply synchronously on OK";

    // The WebEngine preview work must be deferred off the modal-close path. A
    // 50ms timer armed right after the call fires well before the ~5s renderer
    // stall that previously blocked the editor after OK-ing the preferences
    // dialog.
    bool fired = false;
    QTimer::singleShot(50, QCoreApplication::instance(), [&fired]() { fired = true; });
    QElapsedTimer t;
    t.start();
    while (!fired && t.elapsed() < 3000) {
        QApplication::processEvents();
        QTest::qWait(10);
    }
    EXPECT_TRUE(fired) << "preference-apply preview work blocked the GUI thread";
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

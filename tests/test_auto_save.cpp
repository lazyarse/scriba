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
#include <QTemporaryFile>
#include <QSettings>
#include <QTextCursor>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"
#include "TestConfig.h"

// Observes the unsaved-changes prompt at its call site instead of driving
// the real modal QMessageBox, which would block waiting for user input.
// Also stubs the Save-As dialog so tests can simulate cancel/success.
class TestMainWindow : public MainWindow
{
public:
    bool unsavedPromptShown = false;
    bool lastPromptHadUntitled = false;
    MainWindow::ClosePromptResult promptResult = MainWindow::ClosePromptResult::Discard;
    QString saveAsResult;

protected:
    MainWindow::ClosePromptResult promptUnsavedChanges(bool hasUntitledDirty) override
    {
        unsavedPromptShown = true;
        lastPromptHadUntitled = hasUntitledDirty;
        return promptResult;
    }

    QString saveAsDialogPath() override
    {
        return saveAsResult;
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
        s.setValue(Preferences::ReopenLastCorpus, false);
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
    // A real typed edit (select-all + insert) rather than setPlainText, which
    // resets QTextDocument's modified flag by design.
    QTextCursor c = window->editor()->textCursor();
    c.select(QTextCursor::Document);
    c.insertText("exit content");
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
    window->editor()->textCursor().insertText("should not be saved");
    window->close();
    QApplication::processEvents();

    EXPECT_TRUE(testWindow->unsavedPromptShown) << "close on a dirty tab must prompt";
    EXPECT_FALSE(testWindow->lastPromptHadUntitled) << "loaded tabs are not untitled";

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "original content\n");
}

TEST_F(AutoSaveTest, CancelSaveAsAbortsClose) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, false);

    auto *testWindow = new TestMainWindow();
    window = testWindow;
    QApplication::processEvents();

    window->editor()->textCursor().insertText("precious content");
    testWindow->promptResult = MainWindow::ClosePromptResult::Save;
    testWindow->saveAsResult = QString();

    EXPECT_FALSE(window->close()) << "cancelling the Save As dialog must abort the close";
    QApplication::processEvents();

    EXPECT_EQ(window->editor()->toPlainText(), "precious content");
}

TEST_F(AutoSaveTest, SaveAsWritesFileThenCloses) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, false);

    auto *testWindow = new TestMainWindow();
    window = testWindow;
    QApplication::processEvents();

    window->editor()->textCursor().insertText("precious content");
    testWindow->promptResult = MainWindow::ClosePromptResult::Save;
    testWindow->saveAsResult = tmpFile->fileName();

    EXPECT_TRUE(window->close()) << "a completed Save As must let the window close";
    QApplication::processEvents();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "precious content");
}

// Dirty-tracking behavior (fresh tab typing dirties, undo-to-saved clears,
// format-only preference applies never dirty) is covered in
// test_dirty_on_load.cpp.

TEST(MainWindowExtension, AppendsDefaultSuffixToBareName) {
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QStringLiteral("notes"), "md"),
              QStringLiteral("notes.md"));
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QStringLiteral("/tmp/book"), "scriba"),
              QStringLiteral("/tmp/book.scriba"));
}

TEST(MainWindowExtension, LeavesExistingSuffixUntouched) {
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QStringLiteral("notes.txt"), "md"),
              QStringLiteral("notes.txt"));
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QStringLiteral("notes.MD"), "md"),
              QStringLiteral("notes.MD"));
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QStringLiteral("book.json"), "scriba"),
              QStringLiteral("book.json"));
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QStringLiteral("book.scriba"), "scriba"),
              QStringLiteral("book.scriba"));
}

TEST(MainWindowExtension, EmptyPathIsUnchanged) {
    EXPECT_EQ(MainWindow::ensureDefaultSuffix(QString(), "md"), QString());
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

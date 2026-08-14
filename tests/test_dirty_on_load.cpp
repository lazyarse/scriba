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
#include <QDialog>
#include <QTemporaryFile>
#include <QSettings>
#include <QTabBar>
#include <QStackedWidget>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "mainwindow/MainWindow.h"
#include "editor/Editor.h"
#include "prefs/Preferences.h"
#include "TestConfig.h"

class DirtyOnLoadTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_dirty_on_load";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override {
        QSettings s;
        s.remove(Preferences::OnExitCorpusData);
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

    static QString tabText(MainWindow *w) {
        QTabBar *tabs = w->findChild<QTabBar *>();
        if (!tabs || tabs->count() == 0)
            return QString();
        return tabs->tabText(0);
    }

    static QString activeTabText(MainWindow *w) {
        QTabBar *tabs = w->findChild<QTabBar *>();
        if (!tabs || tabs->count() == 0)
            return QString();
        return tabs->tabText(tabs->currentIndex());
    }

    static bool anyTabDirty(MainWindow *w) {
        QTabBar *tabs = w->findChild<QTabBar *>();
        if (!tabs)
            return false;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i).contains('*'))
                return true;
        }
        return false;
    }

    QTemporaryFile *tmpFile = nullptr;
    MainWindow *window = nullptr;
};

TEST_F(DirtyOnLoadTest, LoadFileDoesNotMarkTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, ForceReloadDoesNotMarkTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->loadFile(tmpFile->fileName(), true);
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, UndoRightAfterLoadDoesNotMarkTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));

    window->editor()->undo();
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

// Dirty tracking compares the live text against the saved content hash
// (see MainWindow::setTabSaved / addTab's contentsChange handler): typing
// marks the tab dirty and undoing back to the saved state clears the
// asterisk again. QTextDocument::isModified() is asserted alongside only
// because it happens to agree in this clean-undo-stack scenario.
TEST_F(DirtyOnLoadTest, UndoBackToSavedStateClearsDirtyMarker) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
    ASSERT_FALSE(window->editor()->document()->isModified());

    window->editor()->textCursor().insertText("hello");
    QApplication::processEvents();
    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")));
    EXPECT_TRUE(window->editor()->document()->isModified());

    window->editor()->undo();
    QApplication::processEvents();
    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
    EXPECT_FALSE(window->editor()->document()->isModified());

    window->editor()->redo();
    QApplication::processEvents();
    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, UndoPastSavedStateStillDirtyAfterRetrying) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();

    window->editor()->textCursor().insertText("AAA");
    window->autoSave();
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));

    // Undo below the saved point, then type something different: the undo count
    // returns to the saved value, but the content differs, so it must stay dirty.
    window->editor()->undo();
    QApplication::processEvents();
    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")));

    window->editor()->textCursor().insertText("BBB");
    QApplication::processEvents();
    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")));
    EXPECT_TRUE(window->editor()->document()->isModified());
}

TEST_F(DirtyOnLoadTest, TypingAfterLoadMarksTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));

    window->editor()->textCursor().insertText("modified content");
    QApplication::processEvents();

    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, RecheckSpellingDoesNotMarkTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));

    window->editor()->recheckSpelling();
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

TEST_F(DirtyOnLoadTest, RecheckSpellingDoesNotDirtyAnyTab) {
    QTemporaryFile secondFile;
    ASSERT_TRUE(secondFile.open());
    secondFile.write("second file content\n");
    secondFile.close();

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->loadFile(secondFile.fileName());
    QApplication::processEvents();
    ASSERT_FALSE(anyTabDirty(window));

    if (auto *tabs = window->findChild<QTabBar *>()) {
        auto *stack = window->findChild<QStackedWidget *>();
        for (int i = 0; i < tabs->count(); ++i) {
            if (stack && i < stack->count()) {
                if (auto *ed = qobject_cast<Editor *>(stack->widget(i)))
                    ed->recheckSpelling();
            }
        }
    }
    QApplication::processEvents();

    EXPECT_FALSE(anyTabDirty(window));
}

TEST_F(DirtyOnLoadTest, AcceptingPreferencesDoesNotDirtyAnyTab) {
    QTemporaryFile secondFile;
    ASSERT_TRUE(secondFile.open());
    secondFile.write("second file content\n");
    secondFile.close();

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->loadFile(secondFile.fileName());
    QApplication::processEvents();
    ASSERT_FALSE(anyTabDirty(window));

    QTimer::singleShot(0, []() {
        if (auto *dlg = qobject_cast<QDialog *>(qApp->activeModalWidget()))
            dlg->accept();
    });
    QMetaObject::invokeMethod(window, "showPreferences");
    QApplication::processEvents();

    EXPECT_FALSE(anyTabDirty(window));
}

TEST_F(DirtyOnLoadTest, SessionRestoreDoesNotMarkTabDirty) {
    QSettings s;
    s.setValue(Preferences::ReopenLastCorpus, true);
    QJsonObject session;
    session["version"] = 1;
    QJsonObject doc;
    doc["path"] = tmpFile->fileName();
    QJsonObject cursor;
    cursor["block"] = 0;
    cursor["col"] = 0;
    doc["cursor"] = cursor;
    doc["scroll"] = 0;
    doc["folds"] = QJsonArray{};
    session["documents"] = QJsonArray{ doc };
    session["active"] = 0;
    s.setValue(Preferences::OnExitCorpusData,
               QString::fromUtf8(QJsonDocument(session).toJson(QJsonDocument::Compact)));

    window = new MainWindow();
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

// The line-height preference apply runs a document signal-blocked
// mergeBlockFormat (applyEditorLineHeight): that appends an undo command
// QTextDocument's modified flag would silently absorb, but the content hash
// is unchanged, so clean tabs must stay clean and dirty ones must stay dirty.
TEST_F(DirtyOnLoadTest, LineHeightPreferenceChangeDoesNotDirtyCleanTabs) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(anyTabDirty(window));

    QTimer::singleShot(0, []() {
        if (auto *dlg = qobject_cast<QDialog *>(qApp->activeModalWidget()))
            dlg->accept();
    });
    QMetaObject::invokeMethod(window, "showPreferences");
    QApplication::processEvents();

    EXPECT_FALSE(anyTabDirty(window));
}

TEST_F(DirtyOnLoadTest, LineHeightPreferenceChangePreservesDirtyAndUndoClears) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    window->editor()->textCursor().insertText("hello");
    QApplication::processEvents();
    ASSERT_TRUE(activeTabText(window).contains(QStringLiteral("*")));

    QTimer::singleShot(0, []() {
        if (auto *dlg = qobject_cast<QDialog *>(qApp->activeModalWidget()))
            dlg->accept();
    });
    QMetaObject::invokeMethod(window, "showPreferences");
    QApplication::processEvents();

    EXPECT_TRUE(activeTabText(window).contains(QStringLiteral("*")))
        << "a format-only preference apply must not clear the dirty marker";

    // The pref apply's format merge sits on top of the edit in the undo stack
    // (a no-op undo step if the merge was a no-op), so undo twice to reach the
    // saved content; undoing once would only pop the format op.
    window->editor()->undo();
    window->editor()->undo();
    QApplication::processEvents();
    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")))
        << "undoing the edit returns to the saved content even after a format op";
}

// The tab bar is hidden with a single tab unless the "Always show the tab bar"
// preference (tabBarAlwaysShow) is on; it becomes visible once a second file is
// opened. updateTabBarVisibility() is re-invoked on tab add/remove and on
// preferences accept, so both paths are exercised here.
TEST_F(DirtyOnLoadTest, TabBarHiddenWithSingleTabByDefault) {
    QSettings s;
    s.remove(Preferences::TabBarAlwaysShow);

    window = new MainWindow();
    QApplication::processEvents();
    window->show();
    QApplication::processEvents();

    auto *tabs = window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    EXPECT_FALSE(tabs->isVisible());

    QTemporaryFile secondFile;
    ASSERT_TRUE(secondFile.open());
    secondFile.write("second file content\n");
    secondFile.close();

    window->loadFile(tmpFile->fileName());
    window->loadFile(secondFile.fileName());
    QApplication::processEvents();
    EXPECT_TRUE(tabs->isVisible()) << "tab bar must appear with two tabs";
}

TEST_F(DirtyOnLoadTest, TabBarAlwaysShowKeepsTabBarVisibleWithSingleTab) {
    QSettings s;
    s.setValue(Preferences::TabBarAlwaysShow, true);

    window = new MainWindow();
    QApplication::processEvents();
    window->show();
    QApplication::processEvents();

    auto *tabs = window->findChild<QTabBar *>();
    ASSERT_NE(tabs, nullptr);
    EXPECT_TRUE(tabs->isVisible()) << "tab bar must stay visible with one tab when preferred";
}

TEST_F(DirtyOnLoadTest, CloseStoresSessionData) {
    QSettings s;
    s.remove(Preferences::OnExitCorpusData);
    s.setValue(Preferences::ReopenLastCorpus, false);

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();

    window->close();
    QApplication::processEvents();

    const QString raw = s.value(Preferences::OnExitCorpusData).toString();
    ASSERT_FALSE(raw.isEmpty()) << "closeEvent must persist the session snapshot";
    const QJsonObject corpus = QJsonDocument::fromJson(raw.toUtf8()).object();
    ASSERT_FALSE(corpus.isEmpty());
    const QJsonArray docs = corpus["documents"].toArray();
    ASSERT_EQ(docs.size(), 1);
    EXPECT_EQ(docs.at(0).toObject()["path"].toString(), tmpFile->fileName());
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

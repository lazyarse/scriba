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
#include <QTabWidget>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"
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
        s.remove(Preferences::SessionData);
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

    static QString tabText(MainWindow *w) {
        QTabWidget *tabs = w->findChild<QTabWidget *>();
        if (!tabs || tabs->count() == 0)
            return QString();
        return tabs->tabText(0);
    }

    static QString activeTabText(MainWindow *w) {
        QTabWidget *tabs = w->findChild<QTabWidget *>();
        if (!tabs || tabs->count() == 0)
            return QString();
        return tabs->tabText(tabs->currentIndex());
    }

    static bool anyTabDirty(MainWindow *w) {
        QTabWidget *tabs = w->findChild<QTabWidget *>();
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

TEST_F(DirtyOnLoadTest, TypingAfterLoadMarksTabDirty) {
    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    QApplication::processEvents();
    ASSERT_FALSE(activeTabText(window).contains(QStringLiteral("*")));

    window->editor()->setPlainText("modified content");
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

    if (auto *tabs = window->findChild<QTabWidget *>()) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (auto *ed = qobject_cast<Editor *>(tabs->widget(i)))
                ed->recheckSpelling();
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
    s.setValue(Preferences::ReopenLastSession, true);
    QJsonObject session;
    session["version"] = 1;
    session["files"] = QJsonArray{ tmpFile->fileName() };
    session["cursors"] = QJsonArray{};
    session["active"] = 0;
    s.setValue(Preferences::SessionData,
               QString::fromUtf8(QJsonDocument(session).toJson(QJsonDocument::Compact)));

    window = new MainWindow();
    QApplication::processEvents();

    EXPECT_FALSE(activeTabText(window).contains(QStringLiteral("*")));
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

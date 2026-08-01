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

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("exit content");
    window->close();
    QApplication::processEvents();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "exit content");
}

TEST_F(AutoSaveTest, SaveOnExitDoesNotWriteWhenDisabled) {
    QSettings s;
    s.setValue(Preferences::AutoSaveOnExit, false);

    window = new MainWindow();
    QApplication::processEvents();

    window->loadFile(tmpFile->fileName());
    window->editor()->setPlainText("should not be saved");
    window->close();
    QApplication::processEvents();

    QFile f(tmpFile->fileName());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString saved = QString::fromUtf8(f.readAll());
    EXPECT_EQ(saved, "original content\n");
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

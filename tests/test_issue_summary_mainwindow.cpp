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
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include "editor/Editor.h"
#include "editor/IssueSummaryPane.h"
#include "mainwindow/MainWindow.h"
#include "prefs/Preferences.h"
#include "spell/SpellChecker.h"
#include "TestConfig.h"

TEST(IssueSummaryMainWindowTest, OpensMdFileShowsPane)
{
    QTemporaryDir tmp;
    QFile mdFile(tmp.filePath(QStringLiteral("notes.md")));
    ASSERT_TRUE(mdFile.open(QIODevice::WriteOnly | QIODevice::Text));
    mdFile.write("helo world\n");
    mdFile.close();

    QSettings().setValue(Preferences::IssueSummaryEnabled, true);
    QSettings().setValue(Preferences::IssueSummaryTimeoutEnabled, false);
    QSettings().setValue(Preferences::IssueSummaryShowTypos, true);
    QSettings().setValue(Preferences::DictionaryLanguage, QStringLiteral("en_US"));
    SpellChecker::availableLanguages();

    MainWindow win;
    win.resize(800, 600);
    win.show();
    win.loadFile(mdFile.fileName());
    QTest::qWait(700);

    Editor *ed = win.editor();
    ASSERT_NE(ed, nullptr);
    IssueSummaryPane *pane = ed->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    EXPECT_TRUE(pane->isVisible());
}

TEST(IssueSummaryMainWindowTest, OpensTxtFileHidesPane)
{
    QTemporaryDir tmp;
    QFile txtFile(tmp.filePath(QStringLiteral("data.txt")));
    ASSERT_TRUE(txtFile.open(QIODevice::WriteOnly | QIODevice::Text));
    txtFile.write("helo world\n");
    txtFile.close();

    QSettings().setValue(Preferences::IssueSummaryEnabled, true);
    QSettings().setValue(Preferences::IssueSummaryTimeoutEnabled, false);
    QSettings().setValue(Preferences::IssueSummaryShowTypos, true);
    QSettings().setValue(Preferences::DictionaryLanguage, QStringLiteral("en_US"));
    SpellChecker::availableLanguages();

    MainWindow win;
    win.resize(800, 600);
    win.show();
    win.loadFile(txtFile.fileName());
    QTest::qWait(700);

    Editor *ed = win.editor();
    ASSERT_NE(ed, nullptr);
    IssueSummaryPane *pane = ed->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    EXPECT_FALSE(pane->isVisible());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
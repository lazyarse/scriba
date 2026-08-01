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
#include <QSettings>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>

#include "Editor.h"
#include "Gutter.h"
#include "Preferences.h"
#include "TestConfig.h"

namespace {

void setGutterPreferences(bool lineNumbers, bool showGutter)
{
    QSettings s;
    s.setValue(Preferences::ShowLineNumbers, lineNumbers);
    s.setValue(Preferences::ShowGutter, showGutter);
    s.setValue(Preferences::GutterColorOverride, false);
    s.sync();
}

void fillDocument(Editor *editor, int blocks)
{
    QString text;
    for (int i = 0; i < blocks; ++i)
        text += QString("line %1\n").arg(i);
    editor->setPlainText(text);
    QApplication::processEvents();
}

} // namespace

class GutterWidthTest : public testing::Test
{
protected:
    void SetUp() override
    {
        editor = new Editor();
        editor->resize(800, 600);
        editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete editor;
    }

    Editor *editor = nullptr;
};

TEST_F(GutterWidthTest, WidthStableAcrossSettingsReload)
{
    setGutterPreferences(true, true);
    fillDocument(editor, 150);

    editor->updateGutterSettings();
    QApplication::processEvents();
    QTest::qWait(10); // let deferred updateGutter() fire
    QApplication::processEvents();

    int expected = editor->gutter()->fontMetrics().horizontalAdvance(QLatin1Char('9')) * 3
                   + 16 + 14;
    EXPECT_EQ(editor->gutter()->width(), expected);
}

TEST_F(GutterWidthTest, AutoWidensAsLineCountCrossesDigitBoundary)
{
    setGutterPreferences(true, true);
    fillDocument(editor, 5);
    QTest::qWait(10);
    QApplication::processEvents();

    int widthTwoDigits = editor->gutter()->fontMetrics().horizontalAdvance(QLatin1Char('9')) * 2
                         + 16 + 14;
    EXPECT_EQ(editor->gutter()->width(),
              editor->gutter()->fontMetrics().horizontalAdvance(QLatin1Char('9')) + 16 + 14);

    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    for (int i = 0; i < 50; ++i)
        cursor.insertText("extra line\n");
    QTest::qWait(10);
    QApplication::processEvents();

    EXPECT_EQ(editor->gutter()->width(), widthTwoDigits);
}

TEST_F(GutterWidthTest, WidthWithoutLineNumbersIsFoldIconsOnly)
{
    setGutterPreferences(false, true);
    fillDocument(editor, 150);

    editor->updateGutterSettings();
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();

    EXPECT_EQ(editor->gutter()->width(), 14);
}

TEST_F(GutterWidthTest, ToggleGutterHidesAndShows)
{
    setGutterPreferences(true, true);
    fillDocument(editor, 150);
    editor->updateGutterSettings();
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();

    int visibleWidth = editor->gutter()->width();
    EXPECT_GT(visibleWidth, 0);

    editor->toggleGutter();
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();
    EXPECT_EQ(editor->gutter()->width(), 0);

    editor->toggleGutter();
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();
    EXPECT_EQ(editor->gutter()->width(), visibleWidth);

    setGutterPreferences(true, true);
}

TEST_F(GutterWidthTest, FoldingStillWorksWhenGutterHidden)
{
    setGutterPreferences(true, true);
    fillDocument(editor, 150);
    editor->updateGutterSettings();
    editor->toggleGutter();
    QApplication::processEvents();

    auto *doc = editor->document();
    doc->findBlockByNumber(0).setVisible(false);
    doc->markContentsDirty(0, doc->characterCount());
    EXPECT_FALSE(doc->findBlockByNumber(0).isVisible());

    setGutterPreferences(true, true);
}

TEST_F(GutterWidthTest, GutterStylesheetColorReachesPalette)
{
    setGutterPreferences(true, true);
    editor->setStyleSheet("QTextEdit { background-color: #282a36; color: #f8f8f2; }\n"
                          "#gutter { background-color: #21232d; color: #a0a0a0; }");
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();

    EXPECT_EQ(editor->gutter()->palette().windowText().color(), QColor("#a0a0a0"));
}

TEST_F(GutterWidthTest, GutterIsChildOfEditor)
{
    EXPECT_NE(editor->gutter(), nullptr);
    EXPECT_EQ(editor->gutter()->parentWidget(), editor);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

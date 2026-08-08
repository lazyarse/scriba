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
#include <QSettings>
#include <QTextBlock>
#include <QTextLayout>

#include "Editor.h"
#include "EditorTestHarness.h"
#include "Gutter.h"
#include "Preferences.h"
#include "TestConfig.h"

namespace {

void setWrapPrefs(bool enabled, const QString &mode, int column)
{
    QSettings s;
    s.setValue(Preferences::EditorWrapEnabled, enabled);
    s.setValue(Preferences::EditorWrapMode, mode);
    s.setValue(Preferences::EditorWrapColumn, column);
    s.sync();
}

} // namespace

TEST_F(EditorTestHarness, WindowWrapModeIsWidgetWidth)
{
    setWrapPrefs(true, QStringLiteral("window"), 80);
    editor->applyLineWrap();
    EXPECT_EQ(editor->lineWrapMode(), QTextEdit::WidgetWidth);
}

TEST_F(EditorTestHarness, WrapOffUsesNoWrapAndAllowsHorizontalScroll)
{
    setWrapPrefs(false, QStringLiteral("window"), 80);
    editor->applyLineWrap();
    EXPECT_EQ(editor->lineWrapMode(), QTextEdit::NoWrap);

    editor->setPlainText(QString(300, 'a'));
    QApplication::processEvents();
    QApplication::processEvents();
    EXPECT_GT(editor->horizontalScrollBar()->maximum(), 0);
}

TEST_F(EditorTestHarness, WrapAtWindowWidthKeepsLineWithinViewport)
{
    setWrapPrefs(true, QStringLiteral("window"), 80);
    editor->applyLineWrap();
    EXPECT_EQ(editor->lineWrapMode(), QTextEdit::WidgetWidth);

    editor->setPlainText(QString(300, 'a'));
    QApplication::processEvents();
    QApplication::processEvents();
    EXPECT_EQ(editor->horizontalScrollBar()->maximum(), 0);
}

TEST_F(EditorTestHarness, WrapAtColumnWrapsLongLines)
{
    setWrapPrefs(true, QStringLiteral("column"), 40);
    editor->applyLineWrap();
    EXPECT_EQ(editor->lineWrapMode(), QTextEdit::FixedColumnWidth);
    EXPECT_EQ(editor->lineWrapColumnOrWidth(), 40);

    editor->setPlainText(QString(100, 'a'));
    QApplication::processEvents();
    QApplication::processEvents();
    QTextBlock block = editor->document()->firstBlock();
    ASSERT_GT(block.layout()->lineCount(), 1);
}

TEST_F(EditorTestHarness, ColumnWrapOnKeepsWrapOffHorizontalScroll)
{
    // Swapping column wrap OFF must go back to the no-scroll window wrap.
    setWrapPrefs(true, QStringLiteral("column"), 40);
    editor->applyLineWrap();
    setWrapPrefs(false, QStringLiteral("column"), 40);
    editor->applyLineWrap();
    EXPECT_EQ(editor->lineWrapMode(), QTextEdit::NoWrap);
    editor->setPlainText(QString(300, 'a'));
    QApplication::processEvents();
    QApplication::processEvents();
    EXPECT_GT(editor->horizontalScrollBar()->maximum(), 0);
}

TEST_F(EditorTestHarness, ColumnWrapTakesOverCentredWidth)
{
    setWrapPrefs(true, QStringLiteral("column"), 40);
    editor->resize(1200, 600);
    editor->applyLineWrap();
    // The px content width must be overridden by the wrapped column width.
    editor->setCenterContent(true, 800);
    QApplication::processEvents();
    QApplication::processEvents();

    const QMargins margins = editor->contentMargins();
    const int gutterW = editor->gutter() ? editor->gutter()->width() : 0;
    // Still centred: left margin offsets the gutter, right margin matches.
    EXPECT_GT(margins.left(), gutterW);
    EXPECT_EQ(margins.left() - gutterW, margins.right());
    // Column took over: content narrower than the 800px px setting.
    const int contentWidth = editor->width() - margins.left() - margins.right();
    EXPECT_GT(contentWidth, 0);
    EXPECT_LT(contentWidth, 800);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
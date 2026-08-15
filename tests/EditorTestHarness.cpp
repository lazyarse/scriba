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
#include "EditorTestHarness.h"

#include <QApplication>
#include <QClipboard>
#include <QSettings>
#include <QTest>
#include <QTextCursor>

#include "editor/Editor.h"
#include "prefs/Preferences.h"

EditorTestHarness::EditorTestHarness(CompletionPrefs prefs)
    : m_prefs(prefs)
{
}

void EditorTestHarness::SetUp()
{
    QSettings().setValue(Preferences::FileAutoComplete, m_prefs.file);
    QSettings().setValue(Preferences::EmojiAutoComplete, m_prefs.emoji);
    QSettings().setValue(Preferences::LanguageAutoComplete, m_prefs.language);
    QSettings().setValue(Preferences::CommentAutoComplete, m_prefs.comment);

    editor = new Editor();
    editor->resize(800, 600);
    editor->show();
    QApplication::processEvents();
}

void EditorTestHarness::TearDown()
{
    delete editor;
    editor = nullptr;
}

void EditorTestHarness::typeText(const QString &text)
{
    QTest::keyClicks(editor, text);
    QApplication::processEvents();
}

void EditorTestHarness::press(Qt::Key key, Qt::KeyboardModifiers mods)
{
    QTest::keyClick(editor, key, mods);
    QApplication::processEvents();
}

void EditorTestHarness::enter()
{
    press(Qt::Key_Return);
}

void EditorTestHarness::typeLine(const QString &text)
{
    typeText(text);
    enter();
}

void EditorTestHarness::pasteText(const QString &text)
{
    QApplication::clipboard()->setText(text);
    editor->paste();
    QApplication::processEvents();
}

void EditorTestHarness::setContent(const QString &content)
{
    editor->setPlainText(content);
    QApplication::processEvents();
}

void EditorTestHarness::waitForFolds()
{
    QTest::qWait(400);
}

void EditorTestHarness::placeCursor(int block, int column)
{
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    editor->setTextCursor(cursor);
    QApplication::processEvents();
}

void EditorTestHarness::placeCursorAtEnd()
{
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    editor->setTextCursor(cursor);
    QApplication::processEvents();
}

void EditorTestHarness::selectLines(int firstBlock, int lastBlock)
{
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, firstBlock);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, lastBlock - firstBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    QApplication::processEvents();
}

QString EditorTestHarness::text() const
{
    return editor->toPlainText();
}

int EditorTestHarness::cursorBlock() const
{
    return editor->textCursor().blockNumber();
}

int EditorTestHarness::cursorColumn() const
{
    return editor->textCursor().positionInBlock();
}

void EditorTestHarness::assertCursor(int block, int column) const
{
    EXPECT_EQ(cursorBlock(), block) << "cursor block";
    EXPECT_EQ(cursorColumn(), column) << "cursor column";
}

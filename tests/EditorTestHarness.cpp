#include "EditorTestHarness.h"

#include <QApplication>
#include <QSettings>
#include <QTest>
#include <QTextCursor>

#include "Editor.h"
#include "Preferences.h"

void EditorTestHarness::SetUp()
{
    QSettings().setValue(Preferences::FileAutoComplete, false);
    QSettings().setValue(Preferences::EmojiAutoComplete, false);
    QSettings().setValue(Preferences::LanguageAutoComplete, false);

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

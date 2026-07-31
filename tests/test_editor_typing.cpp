#include <gtest/gtest.h>
#include <QApplication>
#include <QSettings>
#include <QTest>
#include <QTextCursor>

#include "Editor.h"
#include "Preferences.h"

class EditorTypingTest : public testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().setValue(Preferences::FileAutoComplete, false);
        QSettings().setValue(Preferences::EmojiAutoComplete, false);
        QSettings().setValue(Preferences::LanguageAutoComplete, false);

        editor = new Editor();
        editor->resize(800, 600);
        editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete editor;
    }

    void typeText(const QString &text)
    {
        QTest::keyClicks(editor, text);
        QApplication::processEvents();
    }

    void press(Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier)
    {
        QTest::keyClick(editor, key, mods);
        QApplication::processEvents();
    }

    void setContent(const QString &content)
    {
        editor->setPlainText(content);
        QApplication::processEvents();
    }

    void waitForFolds()
    {
        QTest::qWait(400);
    }

    void placeCursor(int block, int column)
    {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, block);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
        editor->setTextCursor(cursor);
        QApplication::processEvents();
    }

    void placeCursorAtEnd()
    {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        QApplication::processEvents();
    }

    void selectLines(int firstBlock, int lastBlock)
    {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, firstBlock);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, lastBlock - firstBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        QApplication::processEvents();
    }

    QString text() const { return editor->toPlainText(); }
    int cursorBlock() const { return editor->textCursor().blockNumber(); }
    int cursorColumn() const { return editor->textCursor().positionInBlock(); }

    Editor *editor = nullptr;
};

TEST_F(EditorTypingTest, TypingInsertsTextAndMovesCursor)
{
    typeText("Hello, world");
    EXPECT_EQ(text(), "Hello, world");
    EXPECT_EQ(cursorColumn(), 12);
}

TEST_F(EditorTypingTest, BackspaceDeletesCharacter)
{
    typeText("ab");
    press(Qt::Key_Backspace);
    EXPECT_EQ(text(), "a");
}

TEST_F(EditorTypingTest, PlainEnterInsertsNewline)
{
    typeText("abc");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "abc\n");
    EXPECT_EQ(cursorBlock(), 1);
}

TEST_F(EditorTypingTest, TabOnPlainTextInsertsFourSpaces)
{
    setContent("abc");
    placeCursor(0, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "    abc");
    EXPECT_EQ(cursorColumn(), 4);
}

TEST_F(EditorTypingTest, DashListContinuesOnEnter)
{
    typeText("- item");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "- item\n- ");
    EXPECT_EQ(cursorBlock(), 1);
}

TEST_F(EditorTypingTest, StarListContinuesOnEnter)
{
    typeText("* item");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "* item\n* ");
}

TEST_F(EditorTypingTest, PlusListContinuesOnEnter)
{
    typeText("+ item");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "+ item\n+ ");
}

TEST_F(EditorTypingTest, OrderedListIncrementsOnEnter)
{
    typeText("1. first");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "1. first\n2. ");
}

TEST_F(EditorTypingTest, OrderedListDoubleDigitIncrementsOnEnter)
{
    typeText("9. ninth");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "9. ninth\n10. ");
}

TEST_F(EditorTypingTest, IndentedListKeepsIndentOnEnter)
{
    typeText("  - nested");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "  - nested\n  - ");
}

TEST_F(EditorTypingTest, TaskListContinuesOnEnter)
{
    typeText("- [ ] todo");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "- [ ] todo\n- [ ] ");
}

TEST_F(EditorTypingTest, CheckedTaskResetsToUncheckedOnEnter)
{
    typeText("- [x] done");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "- [x] done\n- [ ] ");
}

TEST_F(EditorTypingTest, EmptyDashListItemClearsToNewline)
{
    typeText("- ");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "\n");
    EXPECT_EQ(cursorBlock(), 1);
    EXPECT_EQ(cursorColumn(), 0);
}

TEST_F(EditorTypingTest, EmptyOrderedListItemClearsToNewline)
{
    typeText("1. ");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "\n");
}

TEST_F(EditorTypingTest, EmptyTaskListItemClearsToNewline)
{
    typeText("- [ ] ");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "\n");
}

TEST_F(EditorTypingTest, FirstTableRowCreatesSeparatorAndDataRow)
{
    typeText("| a | b |");
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n|  |  |");
    EXPECT_EQ(cursorBlock(), 2);
    EXPECT_EQ(cursorColumn(), 2);
}

TEST_F(EditorTypingTest, TableDataRowContinuesOnEnter)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursorAtEnd();
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n| x | y |\n|  |  |");
    EXPECT_EQ(cursorBlock(), 3);
    EXPECT_EQ(cursorColumn(), 2);
}

TEST_F(EditorTypingTest, BlankTableRowExitsTable)
{
    setContent("| a | b |\n|---|---|\n|  |  |");
    placeCursorAtEnd();
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n\n");
    EXPECT_EQ(cursorBlock(), 3);
    EXPECT_EQ(cursorColumn(), 0);
}

TEST_F(EditorTypingTest, HtmlTableRowContinuesOnEnter)
{
    setContent("<tr><td>a</td><td>b</td></tr>");
    placeCursorAtEnd();
    press(Qt::Key_Return);
    EXPECT_EQ(text(), "<tr><td>a</td><td>b</td></tr>\n<tr><td></td><td></td></tr>");
    EXPECT_EQ(cursorBlock(), 1);
    EXPECT_EQ(cursorColumn(), 8);
}

TEST_F(EditorTypingTest, TabIndentsDashListItem)
{
    typeText("- item");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "  - item");
}

TEST_F(EditorTypingTest, ShiftTabOutdentsListItem)
{
    typeText("  - item");
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "- item");
}

TEST_F(EditorTypingTest, TabIndentsOrderedListItem)
{
    typeText("1. item");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "  1. item");
}

TEST_F(EditorTypingTest, TabIndentsSelectedLines)
{
    setContent("alpha\nbeta\ngamma");
    selectLines(0, 1);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "    alpha\n    beta\ngamma");
    EXPECT_TRUE(editor->textCursor().hasSelection());
}

TEST_F(EditorTypingTest, ShiftTabDedentsSelectedLines)
{
    setContent("    alpha\n    beta\ngamma");
    selectLines(0, 1);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "alpha\nbeta\ngamma");
}

TEST_F(EditorTypingTest, TabMovesToNextTableCell)
{
    setContent("|  |  |  |");
    placeCursor(0, 1);
    press(Qt::Key_Tab);
    EXPECT_EQ(cursorColumn(), 5);
}

TEST_F(EditorTypingTest, TabMovesThroughTableCells)
{
    setContent("|  |  |  |");
    placeCursor(0, 1);
    press(Qt::Key_Tab);
    EXPECT_EQ(cursorColumn(), 5);
    press(Qt::Key_Tab);
    EXPECT_EQ(cursorColumn(), 8);
}

TEST_F(EditorTypingTest, ShiftTabMovesToPreviousTableCell)
{
    setContent("|  |  |  |");
    placeCursor(0, 8);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(cursorColumn(), 5);
}

TEST_F(EditorTypingTest, TabFromLastCellJumpsToNextRow)
{
    setContent("|  |  |  |\n| x | y | z |");
    placeCursor(0, 8);
    press(Qt::Key_Tab);
    EXPECT_EQ(cursorBlock(), 1);
    EXPECT_EQ(cursorColumn(), 2);
}

TEST_F(EditorTypingTest, CtrlDDuplicatesCurrentLine)
{
    setContent("alpha\nbeta\ngamma");
    placeCursor(0, 2);
    press(Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(text(), "alpha\nalpha\nbeta\ngamma");
}

TEST_F(EditorTypingTest, CtrlDDuplicatesLastLine)
{
    setContent("alpha\nbeta");
    placeCursorAtEnd();
    press(Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(text(), "alpha\nbeta\nbeta");
}

TEST_F(EditorTypingTest, CtrlDDuplicatesSelection)
{
    setContent("alpha\nbeta\ngamma");
    selectLines(0, 1);
    press(Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(text(), "alpha\nbeta\nalpha\nbeta\ngamma");
}

TEST_F(EditorTypingTest, CtrlUpJumpsToPreviousHeader)
{
    setContent("# H1\nbody\n## H2\nmore");
    waitForFolds();
    placeCursor(1, 0);
    press(Qt::Key_Up, Qt::ControlModifier);
    EXPECT_EQ(cursorBlock(), 0);
}

TEST_F(EditorTypingTest, CtrlDownJumpsToNextHeader)
{
    setContent("# H1\nbody\n## H2\nmore");
    waitForFolds();
    placeCursor(1, 0);
    press(Qt::Key_Down, Qt::ControlModifier);
    EXPECT_EQ(cursorBlock(), 2);
}

TEST_F(EditorTypingTest, DownArrowAtLastLineMovesToEndOfBlock)
{
    setContent("hello");
    placeCursor(0, 2);
    press(Qt::Key_Down);
    EXPECT_EQ(cursorColumn(), 5);
}

TEST_F(EditorTypingTest, TypingCreatesUndoSteps)
{
    typeText("hello");
    int chars = editor->textCursor().position();
    ASSERT_EQ(chars, 5);

    for (int i = 0; i < chars; ++i)
        editor->undo();
    EXPECT_EQ(text(), "");

    for (int i = 0; i < chars; ++i)
        editor->redo();
    EXPECT_EQ(text(), "hello");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("scribaTest");
    app.setApplicationName("scribaTest");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

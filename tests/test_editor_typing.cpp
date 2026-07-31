#include <gtest/gtest.h>
#include <QApplication>

#include "Editor.h"
#include "EditorTestHarness.h"

TEST_F(EditorTestHarness, TypingInsertsTextAndMovesCursor)
{
    typeText("Hello, world");
    EXPECT_EQ(text(), "Hello, world");
    assertCursor(0, 12);
}

TEST_F(EditorTestHarness, BackspaceDeletesCharacter)
{
    typeText("ab");
    press(Qt::Key_Backspace);
    EXPECT_EQ(text(), "a");
}

TEST_F(EditorTestHarness, PlainEnterInsertsNewline)
{
    typeLine("abc");
    EXPECT_EQ(text(), "abc\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, TabOnPlainTextInsertsFourSpaces)
{
    setContent("abc");
    placeCursor(0, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "    abc");
    assertCursor(0, 4);
}

TEST_F(EditorTestHarness, DashListContinuesOnEnter)
{
    typeLine("- item");
    EXPECT_EQ(text(), "- item\n- ");
    assertCursor(1, 2);
}

TEST_F(EditorTestHarness, StarListContinuesOnEnter)
{
    typeLine("* item");
    EXPECT_EQ(text(), "* item\n* ");
}

TEST_F(EditorTestHarness, PlusListContinuesOnEnter)
{
    typeLine("+ item");
    EXPECT_EQ(text(), "+ item\n+ ");
}

TEST_F(EditorTestHarness, OrderedListIncrementsOnEnter)
{
    typeLine("1. first");
    EXPECT_EQ(text(), "1. first\n2. ");
}

TEST_F(EditorTestHarness, OrderedListDoubleDigitIncrementsOnEnter)
{
    typeLine("9. ninth");
    EXPECT_EQ(text(), "9. ninth\n10. ");
}

TEST_F(EditorTestHarness, IndentedListKeepsIndentOnEnter)
{
    typeLine("  - nested");
    EXPECT_EQ(text(), "  - nested\n  - ");
}

TEST_F(EditorTestHarness, TaskListContinuesOnEnter)
{
    typeLine("- [ ] todo");
    EXPECT_EQ(text(), "- [ ] todo\n- [ ] ");
}

TEST_F(EditorTestHarness, CheckedTaskResetsToUncheckedOnEnter)
{
    typeLine("- [x] done");
    EXPECT_EQ(text(), "- [x] done\n- [ ] ");
}

TEST_F(EditorTestHarness, EmptyDashListItemClearsToNewline)
{
    typeLine("- ");
    EXPECT_EQ(text(), "\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, EmptyOrderedListItemClearsToNewline)
{
    typeLine("1. ");
    EXPECT_EQ(text(), "\n");
}

TEST_F(EditorTestHarness, EmptyTaskListItemClearsToNewline)
{
    typeLine("- [ ] ");
    EXPECT_EQ(text(), "\n");
}

TEST_F(EditorTestHarness, FirstTableRowCreatesSeparatorAndDataRow)
{
    typeLine("| a | b |");
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n|  |  |");
    assertCursor(2, 2);
}

TEST_F(EditorTestHarness, TableDataRowContinuesOnEnter)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n| x | y |\n|  |  |");
    assertCursor(3, 2);
}

TEST_F(EditorTestHarness, BlankTableRowExitsTable)
{
    setContent("| a | b |\n|---|---|\n|  |  |");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n\n");
    assertCursor(3, 0);
}

TEST_F(EditorTestHarness, HtmlTableRowContinuesOnEnter)
{
    setContent("<tr><td>a</td><td>b</td></tr>");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "<tr><td>a</td><td>b</td></tr>\n<tr><td></td><td></td></tr>");
    assertCursor(1, 8);
}

TEST_F(EditorTestHarness, TabIndentsDashListItem)
{
    run("- item", Qt::Key_Tab);
    EXPECT_EQ(text(), "  - item");
}

TEST_F(EditorTestHarness, ShiftTabOutdentsListItem)
{
    typeText("  - item");
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "- item");
}

TEST_F(EditorTestHarness, TabIndentsOrderedListItem)
{
    run("1. item", Qt::Key_Tab);
    EXPECT_EQ(text(), "  1. item");
}

TEST_F(EditorTestHarness, TabIndentsSelectedLines)
{
    setContent("alpha\nbeta\ngamma");
    selectLines(0, 1);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "    alpha\n    beta\ngamma");
    EXPECT_TRUE(editor->textCursor().hasSelection());
}

TEST_F(EditorTestHarness, ShiftTabDedentsSelectedLines)
{
    setContent("    alpha\n    beta\ngamma");
    selectLines(0, 1);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "alpha\nbeta\ngamma");
}

TEST_F(EditorTestHarness, TabMovesToNextTableCell)
{
    setContent("|  |  |  |");
    placeCursor(0, 1);
    press(Qt::Key_Tab);
    assertCursor(0, 5);
}

TEST_F(EditorTestHarness, TabMovesThroughTableCells)
{
    setContent("|  |  |  |");
    placeCursor(0, 1);
    run(Qt::Key_Tab, Qt::Key_Tab);
    assertCursor(0, 8);
}

TEST_F(EditorTestHarness, ShiftTabMovesToPreviousTableCell)
{
    setContent("|  |  |  |");
    placeCursor(0, 8);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    assertCursor(0, 5);
}

TEST_F(EditorTestHarness, TabFromLastCellJumpsToNextRow)
{
    setContent("|  |  |  |\n| x | y | z |");
    placeCursor(0, 8);
    press(Qt::Key_Tab);
    assertCursor(1, 2);
}

TEST_F(EditorTestHarness, CtrlDDuplicatesCurrentLine)
{
    setContent("alpha\nbeta\ngamma");
    placeCursor(0, 2);
    press(Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(text(), "alpha\nalpha\nbeta\ngamma");
}

TEST_F(EditorTestHarness, CtrlDDuplicatesLastLine)
{
    setContent("alpha\nbeta");
    placeCursorAtEnd();
    press(Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(text(), "alpha\nbeta\nbeta");
}

TEST_F(EditorTestHarness, CtrlDDuplicatesSelection)
{
    setContent("alpha\nbeta\ngamma");
    selectLines(0, 1);
    press(Qt::Key_D, Qt::ControlModifier);
    EXPECT_EQ(text(), "alpha\nbeta\nalpha\nbeta\ngamma");
}

TEST_F(EditorTestHarness, CtrlUpJumpsToPreviousHeader)
{
    setContent("# H1\nbody\n## H2\nmore");
    waitForFolds();
    placeCursor(1, 0);
    press(Qt::Key_Up, Qt::ControlModifier);
    assertCursor(0, 0);
}

TEST_F(EditorTestHarness, CtrlDownJumpsToNextHeader)
{
    setContent("# H1\nbody\n## H2\nmore");
    waitForFolds();
    placeCursor(1, 0);
    press(Qt::Key_Down, Qt::ControlModifier);
    assertCursor(2, 0);
}

TEST_F(EditorTestHarness, DownArrowAtLastLineMovesToEndOfBlock)
{
    setContent("hello");
    placeCursor(0, 2);
    press(Qt::Key_Down);
    assertCursor(0, 5);
}

TEST_F(EditorTestHarness, TypingCreatesUndoSteps)
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

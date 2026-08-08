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
#include <QTextBlock>

#include "Editor.h"
#include "EditorTestHarness.h"
#include "Preferences.h"
#include "TestConfig.h"

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

TEST_F(EditorTestHarness, YearAtLineStartContinuesList)
{
    typeLine("2024. text");
    EXPECT_EQ(text(), "2024. text\n2025. ");
}

TEST_F(EditorTestHarness, OrderedListContinuesWhenPrecededByOne)
{
    setContent("1. first\n2. second");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "1. first\n2. second\n3. ");
}

TEST_F(EditorTestHarness, EnterMidYearCarriesListMarker)
{
    setContent("2024. report");
    placeCursor(0, 7);
    enter();
    EXPECT_EQ(text(), "2024. r\n2025. eport");
    assertCursor(1, 6);
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

TEST_F(EditorTestHarness, EnterAtStartOfListItemSplitsWithoutContinuation)
{
    setContent("- item1");
    placeCursor(0, 0);
    enter();
    EXPECT_EQ(text(), "\n- item1");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, EnterMidListItemContinuesList)
{
    setContent("- abcd");
    placeCursor(0, 4);
    enter();
    EXPECT_EQ(text(), "- ab\n- cd");
    assertCursor(1, 2);
}

TEST_F(EditorTestHarness, EnterMidOrderedListItemContinuesList)
{
    setContent("1. one two");
    placeCursor(0, 4);
    enter();
    EXPECT_EQ(text(), "1. o\n2. ne two");
    assertCursor(1, 3);
}

TEST_F(EditorTestHarness, EnterMidTaskListItemContinuesList)
{
    setContent("- [ ] todo now");
    placeCursor(0, 8);
    enter();
    EXPECT_EQ(text(), "- [ ] to\n- [ ] do now");
}

TEST_F(EditorTestHarness, EnterAtStartOfOrderedListItemSplitsWithoutContinuation)
{
    setContent("1. item1");
    placeCursor(0, 0);
    enter();
    EXPECT_EQ(text(), "\n1. item1");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, EnterAtStartOfTaskListItemSplitsWithoutContinuation)
{
    setContent("- [ ] todo");
    placeCursor(0, 0);
    enter();
    EXPECT_EQ(text(), "\n- [ ] todo");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, DoubleEnterAfterListItemsStopsListAutocomplete)
{
    typeLine("- item 1");
    typeLine("item2");
    EXPECT_EQ(text(), "- item 1\n- item2\n- ");
    assertCursor(2, 2);

    enter();
    EXPECT_EQ(text(), "- item 1\n- item2\n\n");
    assertCursor(3, 0);

    enter();
    EXPECT_EQ(text(), "- item 1\n- item2\n\n\n");
    assertCursor(4, 0);
}

TEST_F(EditorTestHarness, EnterAfterHeadingTwiceLeavesCursorTwoLinesBelow)
{
    typeLine("# Title");
    EXPECT_EQ(text(), "# Title\n");
    assertCursor(1, 0);

    enter();
    EXPECT_EQ(text(), "# Title\n\n");
    assertCursor(2, 0);

    enter();
    EXPECT_EQ(text(), "# Title\n\n\n");
    assertCursor(3, 0);
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

TEST_F(EditorTestHarness, ThematicBreakDoesNotContinueList)
{
    typeLine("---");
    EXPECT_EQ(text(), "---\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, StarThematicBreakDoesNotContinueList)
{
    typeLine("***");
    EXPECT_EQ(text(), "***\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, EnterAfterBoldTextDoesNotCreateListItem)
{
    setContent("**some bold text** followed by more text");
    placeCursor(0, 19);
    enter();
    EXPECT_EQ(text(), "**some bold text** \nfollowed by more text");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, EnterAtEndOfBoldTextDoesNotCreateListItem)
{
    setContent("**some bold text**");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "**some bold text**\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, UnderscoreThematicBreakDoesNotContinueList)
{
    typeLine("___");
    EXPECT_EQ(text(), "___\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, TabOnThematicBreakIsNoOp)
{
    setContent("---");
    placeCursorAtEnd();
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "---");
    assertCursor(0, 3);
}

TEST_F(EditorTestHarness, FirstTableRowCreatesSeparatorAndDataRow)
{
    typeLine("| a | b |");
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n|   |   |");
    assertCursor(2, 2);
}

TEST_F(EditorTestHarness, EditingTableThenLeavingAlignsColumns)
{
    setContent("| a | bb |\n|---|---|\n| ccc | d |\nend");
    placeCursor(2, 5);
    typeText("!");
    placeCursor(3, 0);
    EXPECT_EQ(text(), "| a    | bb |\n|------|----|\n| ccc! | d  |\nend");
}

TEST_F(EditorTestHarness, LeavingUntouchedTableDoesNotReformat)
{
    setContent("| a | bb |\n|---|---|\n| ccc | d |\nend");
    placeCursor(2, 4);
    placeCursor(3, 0);
    EXPECT_EQ(text(), "| a | bb |\n|---|---|\n| ccc | d |\nend");
}

TEST_F(EditorTestHarness, AutoAlignDisabledSkipsReformat)
{
    QSettings().setValue(Preferences::AutoAlignTables, false);
    setContent("| a | bb |\n|---|---|\n| ccc | d |\nend");
    placeCursor(2, 4);
    typeText("!");
    placeCursor(3, 0);
    EXPECT_EQ(text(), "| a | bb |\n|---|---|\n| cc!c | d |\nend");
    QSettings().setValue(Preferences::AutoAlignTables, true);
}

TEST_F(EditorTestHarness, TableSeparatorAlignmentFollowedOnReformat)
{
    setContent("| name | qty |\n|:-----|---:|\n| apple | 1 |\nend");
    placeCursor(2, 7);
    typeText("s");
    placeCursor(3, 0);
    EXPECT_EQ(text(), "| name   | qty |\n|:-------|----:|\n| apples |   1 |\nend");
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

TEST_F(EditorTestHarness, BlankTableRowExitsTableFromInsideCells)
{
    QSettings().setValue(Preferences::AutoAlignTables, false);
    setContent("| a | b |\n|---|---|\n|  |  |");
    placeCursor(2, 2);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n\n");
    assertCursor(3, 0);
    QSettings().setValue(Preferences::AutoAlignTables, true);
}

TEST_F(EditorTestHarness, BlankRowAfterAutocompleteExitsOnEnter)
{
    QSettings().setValue(Preferences::AutoAlignTables, false);
    typeLine("|h1|h2|h3|");
    assertCursor(2, 2);
    enter();
    EXPECT_EQ(text(), "|h1|h2|h3|\n|---|---|---|\n\n");
    assertCursor(3, 0);
    QSettings().setValue(Preferences::AutoAlignTables, true);
}

TEST_F(EditorTestHarness, EnterAtStartOfEmptyTableRowExitsTable)
{
    setContent("| a | b |\n|---|---|\n|  |  |");
    placeCursor(2, 0);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n\n");
    assertCursor(3, 0);
}

TEST_F(EditorTestHarness, EnterAtStartOfTableDataRowCreatesEmptyRow)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursor(2, 0);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n| x | y |\n|  |  |");
    assertCursor(3, 2);
}

TEST_F(EditorTestHarness, EnterMidCellInTableDataRowCreatesEmptyRow)
{
    setContent("| header1 | header2 | header3 |\n|---------|---------|---------|\n| left    | mid     | right   |");
    placeCursor(2, 13);
    enter();
    EXPECT_EQ(text(), "| header1 | header2 | header3 |\n|---------|---------|---------|\n| left    | mid     | right   |\n|  |  |  |");
    assertCursor(3, 2);
}

TEST_F(EditorTestHarness, EnterMidHeaderRowCreatesTable)
{
    setContent("| a | b |");
    placeCursor(0, 4);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n|   |   |");
    assertCursor(2, 2);
}

TEST_F(EditorTestHarness, EnterMidHeaderWithExistingTableJumpsToFirstDataRow)
{
    setContent("| h1 | h2 | h3 |\n|----|----|----|\n| a  | b  | c  |");
    placeCursor(0, 4);
    enter();
    EXPECT_EQ(text(), "| h1 | h2 | h3 |\n|----|----|----|\n| a  | b  | c  |");
    assertCursor(2, 2);
}

TEST_F(EditorTestHarness, EnterOnSeparatorRowJumpsToFirstDataRow)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursor(1, 3);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n| x | y |");
    assertCursor(2, 2);
    placeCursor(1, 7);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n| x | y |");
    assertCursor(2, 2);
}

TEST_F(EditorTestHarness, EnterOnSeparatorRowWithoutDataRowCreatesFirstDataRow)
{
    setContent("| a | b |\n|---|---|");
    placeCursor(1, 2);
    enter();
    EXPECT_EQ(text(), "| a | b |\n|---|---|\n|  |  |");
    assertCursor(2, 2);
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
    EXPECT_EQ(text(), "   1. item");
}

TEST_F(EditorTestHarness, TabNestsUnorderedListThreeLevels)
{
    setContent("- item one");
    placeCursorAtEnd();
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "  - item one");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "    - item one");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "      - item one");
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "    - item one");
}

TEST_F(EditorTestHarness, TabNestsOrderedListThreeLevels)
{
    setContent("1. item one");
    placeCursorAtEnd();
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "   1. item one");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "      1. item one");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "         1. item one");
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "      1. item one");
}

TEST_F(EditorTestHarness, TabRenumbersOrderedItemIntoNestedSublist)
{
    setContent("1. asdf\n2. asdf\n3. adsf\n4. asdf\n5. asdf\n6. asdf");
    placeCursor(2, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "1. asdf\n2. asdf\n   1. adsf\n4. asdf\n5. asdf\n6. asdf");
    placeCursor(3, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "1. asdf\n2. asdf\n   1. adsf\n   2. asdf\n5. asdf\n6. asdf");
}

TEST_F(EditorTestHarness, TabThenEnterContinuesNestedOrderedList)
{
    setContent("1. asdf\n2. asdf\n3. adsf");
    placeCursor(2, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "1. asdf\n2. asdf\n   1. adsf");
    enter();
    EXPECT_EQ(text(), "1. asdf\n2. asdf\n   1. adsf\n   2. ");
}

TEST_F(EditorTestHarness, TabRenumbersMultiDigitNestedItem)
{
    setContent("7. seventh\n10. tenth");
    placeCursor(1, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "7. seventh\n    1. tenth");
}

TEST_F(EditorTestHarness, ShiftTabRenumbersOutOfNestedList)
{
    setContent("1. first\n2. second\n   1. a\n   2. b\n   3. c\n4. foo\n5. bar");
    placeCursor(2, 0);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "1. first\n2. second\n3. a\n   2. b\n   3. c\n4. foo\n5. bar");
    placeCursor(3, 0);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "1. first\n2. second\n3. a\n4. b\n   3. c\n4. foo\n5. bar");
    placeCursor(4, 0);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "1. first\n2. second\n3. a\n4. b\n5. c\n6. foo\n7. bar");
}

TEST_F(EditorTestHarness, ShiftTabRenumbersEnterContinuationToOuterSequence)
{
    setContent("1. a\n2. b\n3. c\n4. d\n5. e\n   1. aa\n   2. bb\n   3. cc\n   4. dd\n   5. ee\n   6. ff\n   7. gg\n   8. hh");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "1. a\n2. b\n3. c\n4. d\n5. e\n   1. aa\n   2. bb\n   3. cc\n   4. dd\n   5. ee\n   6. ff\n   7. gg\n   8. hh\n   9. ");
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "1. a\n2. b\n3. c\n4. d\n5. e\n   1. aa\n   2. bb\n   3. cc\n   4. dd\n   5. ee\n   6. ff\n   7. gg\n   8. hh\n6. ");
    typeText("blah");
    EXPECT_EQ(text(), "1. a\n2. b\n3. c\n4. d\n5. e\n   1. aa\n   2. bb\n   3. cc\n   4. dd\n   5. ee\n   6. ff\n   7. gg\n   8. hh\n6. blah");
    enter();
    EXPECT_EQ(text(), "1. a\n2. b\n3. c\n4. d\n5. e\n   1. aa\n   2. bb\n   3. cc\n   4. dd\n   5. ee\n   6. ff\n   7. gg\n   8. hh\n6. blah\n7. ");
}

TEST_F(EditorTestHarness, ShiftTabPreservesIntentionalOuterStart)
{
    setContent("2024. a\n2025. b\n   1. c");
    placeCursor(2, 0);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "2024. a\n2025. b\n2026. c");
    placeCursor(0, 0);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(text(), "2024. a\n2025. b\n2026. c");
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

TEST_F(EditorTestHarness, TabMovesToNextCellInSeparatorRow)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursor(1, 1);
    press(Qt::Key_Tab);
    assertCursor(1, 5);
}

TEST_F(EditorTestHarness, ShiftTabMovesToPreviousCellInSeparatorRow)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursor(1, 5);
    press(Qt::Key_Tab, Qt::ShiftModifier);
    assertCursor(1, 1);
}

TEST_F(EditorTestHarness, TabFromLastSeparatorCellJumpsToFirstDataRow)
{
    setContent("| a | b |\n|---|---|\n| x | y |");
    placeCursor(1, 5);
    press(Qt::Key_Tab);
    assertCursor(2, 2);
}

TEST_F(EditorTestHarness, TabFromSeparatorLastCellWithoutDataRowCreatesRow)
{
    setContent("| a | b |\n|---|---|");
    placeCursor(1, 5);
    press(Qt::Key_Tab);
    assertCursor(2, 2);
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

TEST_F(EditorTestHarness, EnterOnFoldedHeaderInsertsBelowFoldToEOF)
{
    setContent("# Section\nline1\nline2");
    waitForFolds();
    editor->restoreFolds({0});
    placeCursor(0, 9);
    press(Qt::Key_Return);
    waitForFolds();

    EXPECT_EQ(text(), "# Section\nline1\nline2\n");
    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "blank below the fold is visible";
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({0}));
    assertCursor(3, 0);

    typeLine("new text");
    waitForFolds();
    EXPECT_EQ(text(), "# Section\nline1\nline2\nnew text\n");
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible()) << "typed text below the fold stays visible";
}

TEST_F(EditorTestHarness, EnterOnFoldedHeaderInsertsBelowTerminatingHeading)
{
    setContent("# S1\ncontent\n# S2\nmore");
    waitForFolds();
    editor->restoreFolds({0});
    placeCursor(0, 5);
    press(Qt::Key_Return);
    waitForFolds();

    EXPECT_EQ(text(), "# S1\ncontent\n# S2\n\nmore");
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible()) << "folded body still hidden";
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible()) << "terminating heading stays visible";
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "blank below terminating heading is visible";
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible());
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({0}));
    assertCursor(3, 0);

    typeLine("typed");
    waitForFolds();
    EXPECT_EQ(text(), "# S1\ncontent\n# S2\ntyped\n\nmore");
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "typed text stays visible";
}

TEST_F(EditorTestHarness, EnterMidFoldedHeaderSplitsNormally)
{
    setContent("# S1\ncontent\n# S2\nmore");
    waitForFolds();
    editor->restoreFolds({0});
    placeCursor(0, 2);
    press(Qt::Key_Return);
    waitForFolds();

    EXPECT_EQ(text(), "# \nS1\ncontent\n# S2\nmore");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, EnterInHiddenRegionRedirectsBelowFold)
{
    setContent("# S1\nline1\nline2\n# S2\nmore");
    waitForFolds();
    editor->restoreFolds({0});
    placeCursor(2, 0);
    press(Qt::Key_Return);
    waitForFolds();

    EXPECT_EQ(text(), "# S1\nline1\nline2\n# S2\n\nmore");
    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible()) << "blank below the fold is visible";
    assertCursor(4, 0);
}

TEST_F(EditorTestHarness, DeleteLineRemovesCurrentLineAndNewline)
{
    setContent("one\ntwo\nthree");
    placeCursor(1, 0);
    press(Qt::Key_Y, Qt::ControlModifier);

    EXPECT_EQ(text(), "one\nthree");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, DeleteLineClearsLastLine)
{
    setContent("one\ntwo\nthree");
    placeCursor(2, 3);
    press(Qt::Key_Y, Qt::ControlModifier);

    EXPECT_EQ(text(), "one\ntwo\n");
    assertCursor(2, 0);
}

TEST_F(EditorTestHarness, DeleteLineFirstLineMovesCursorToSlidUpLine)
{
    setContent("one\ntwo\nthree");
    placeCursor(0, 1);
    press(Qt::Key_Y, Qt::ControlModifier);

    EXPECT_EQ(text(), "two\nthree");
    assertCursor(0, 0);
}

TEST_F(EditorTestHarness, DeleteLineSpansSelection)
{
    setContent("one\ntwo\nthree\nfour");
    selectLines(1, 2);
    press(Qt::Key_Y, Qt::ControlModifier);

    EXPECT_EQ(text(), "one\nfour");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, DeleteLineEditorDoesNotBreakDuplicate)
{
    setContent("one\ntwo\nthree");
    placeCursor(1, 1);
    press(Qt::Key_D, Qt::ControlModifier);

    EXPECT_EQ(text(), "one\ntwo\ntwo\nthree");
}

TEST_F(EditorTestHarness, FenceByBodyFindsFencedBlockAndReadsBody)
{
    setContent("Before\n\n```mermaid\npie title Pets\n    \"Dogs\" : 10\n```\n\nAfter");
    waitForFolds();

    const QString body = "pie title Pets\n    \"Dogs\" : 10";
    EXPECT_EQ(editor->fenceBody(2), body);
    EXPECT_EQ(editor->findFenceByBody(body, 3), 2);
    // Tolerates the trailing newline the preview's <code> text carries.
    EXPECT_EQ(editor->findFenceByBody(body + "\n", 3), 2);
    // The hint line is only approximate; a lone match still resolves.
    EXPECT_EQ(editor->findFenceByBody(body, 50), 2);
    EXPECT_EQ(editor->findFenceByBody("no such chart", 3), -1);
    EXPECT_EQ(editor->findFenceByBody("", 3), -1);
}

TEST_F(EditorTestHarness, FenceByBodyPrefersNearestDuplicate)
{
    setContent("```mermaid\npie title Same\n```\n\ntext\n\n```mermaid\npie title Same\n```");
    waitForFolds();

    const QString body = "pie title Same";
    EXPECT_EQ(editor->findFenceByBody(body, 1), 0);
    EXPECT_EQ(editor->findFenceByBody(body, 8), 6);
}

TEST_F(EditorTestHarness, FenceLanguageReportsChartAndNonChartLanguages)
{
    setContent("Before\n\n```mermaid\npie title Pets\n```\n\n```ec\n{}\n```\n\n```python\nx=1\n```");
    waitForFolds();

    EXPECT_EQ(editor->fenceLanguage(2), "mermaid");
    EXPECT_EQ(editor->fenceLanguage(6), "ec");
    EXPECT_EQ(editor->fenceLanguage(10), "python");
    // A plain fence with no language token.
    setContent("```\nplain\n```");
    waitForFolds();
    EXPECT_EQ(editor->fenceLanguage(0), QString());
}

TEST_F(EditorTestHarness, MathByContentFindsInlineAndDisplayEquations)
{
    setContent("Use $x^2$ here.\n\n$$\\int f$$");
    waitForFolds();

    int out = -1;
    EXPECT_EQ(editor->findMathByContent("x^2", 0, 0, &out), 0);
    EXPECT_EQ(out, 0);
    EXPECT_EQ(editor->findMathByContent("\\int f", 2, 0, &out), 2);
    EXPECT_EQ(out, 0);
}

TEST_F(EditorTestHarness, MathByContentDistinguishesNthOccurrence)
{
    setContent("a $x$ b $y$ c");
    waitForFolds();

    int out = -1;
    EXPECT_EQ(editor->findMathByContent("x", 0, 0, &out), 0);
    EXPECT_EQ(out, 0);
    EXPECT_EQ(editor->findMathByContent("y", 0, 1, &out), 0);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(editor->findMathByContent("z", 0, 0, &out), -1);
}

TEST_F(EditorTestHarness, MathByContentPrefersExactIndexThenNearestLine)
{
    setContent("$x$\n\ntext\n\n$x$");
    waitForFolds();

    int out = -1;
    EXPECT_EQ(editor->findMathByContent("x", 1, 0, &out), 0);
    EXPECT_EQ(out, 0);
    EXPECT_EQ(editor->findMathByContent("x", 5, 0, &out), 4);
    EXPECT_EQ(out, 0);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

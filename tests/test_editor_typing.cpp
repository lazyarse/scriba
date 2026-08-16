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

#include "editor/Editor.h"
#include "EditorTestHarness.h"
#include "prefs/Preferences.h"
#include "TestConfig.h"

namespace {
QString escapeVisible(const QString &s)
{
    QString out;
    for (QChar c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c == ' ') out += QString(QChar(0x2423));
        else out += c;
    }
    return out;
}
QString qvs(const QStringList &l)
{
    return "[ " + l.join(" | ") + " ]";
}
}

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

TEST_F(EditorTestHarness, ColonDefContinuesOnEnter)
{
    typeLine(": def");
    EXPECT_EQ(text(), ": def\n: ");
    assertCursor(1, 2);
}

TEST_F(EditorTestHarness, TildeDefContinuesOnEnter)
{
    typeLine("~ def");
    EXPECT_EQ(text(), "~ def\n~ ");
    assertCursor(1, 2);
}

TEST_F(EditorTestHarness, EmptyDefLineClearsToNewline)
{
    typeLine(": ");
    EXPECT_EQ(text(), "\n");
    assertCursor(1, 0);
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

TEST_F(EditorTestHarness, EnterMidListItemTailAlreadyMarkedDoesNotDouble)
{
    setContent("- item1 is long - this is going to be a new item");
    placeCursor(0, 16);   // start of "- this is"
    enter();
    EXPECT_EQ(text(), "- item1 is long \n- this is going to be a new item");
    assertCursor(1, 0);
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

TEST_F(EditorTestHarness, IndentedLineAutoIndentsOnEnter)
{
    typeLine("    quote text");
    EXPECT_EQ(text(), "    quote text\n    ");
    assertCursor(1, 4);
}

TEST_F(EditorTestHarness, TabIndentThenEnterKeepsIndent)
{
    setContent("abc");
    placeCursor(0, 0);
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "    abc");
    placeCursorAtEnd();
    enter();
    EXPECT_EQ(text(), "    abc\n    ");
    assertCursor(1, 4);
}

TEST_F(EditorTestHarness, BlockquoteMarkerContinuesOnEnter)
{
    typeLine("> quote");
    EXPECT_EQ(text(), "> quote\n> ");
    assertCursor(1, 2);
}

TEST_F(EditorTestHarness, IndentedBlockquoteContinuesOnEnter)
{
    typeLine("    > quote");
    EXPECT_EQ(text(), "    > quote\n    > ");
    assertCursor(1, 6);
}

TEST_F(EditorTestHarness, NestedBlockquoteContinuesOnEnter)
{
    typeLine("> > quote");
    EXPECT_EQ(text(), "> > quote\n> > ");
    assertCursor(1, 4);
}

TEST_F(EditorTestHarness, SecondEnterRemovesAutoIndent)
{
    typeLine("    quote text");
    EXPECT_EQ(text(), "    quote text\n    ");
    enter();
    EXPECT_EQ(text(), "    quote text\n\n");
    assertCursor(2, 0);
}

TEST_F(EditorTestHarness, SecondEnterExitsBlockquote)
{
    typeLine("> quote");
    EXPECT_EQ(text(), "> quote\n> ");
    enter();
    EXPECT_EQ(text(), "> quote\n\n");
    assertCursor(2, 0);
}

TEST_F(EditorTestHarness, EnterAtStartOfIndentedLineSplitsPlainly)
{
    setContent("    text");
    placeCursor(0, 0);
    enter();
    EXPECT_EQ(text(), "\n    text");
    assertCursor(1, 0);
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

TEST_F(EditorTestHarness, PasteTableAlignsImmediately)
{
    pasteText("foo|bar\n--|--\nasdf | adsf\nasdf22 | adsads");
    EXPECT_EQ(text(), "foo    | bar   \n"
                      "------ | ------\n"
                      "asdf   | adsf  \n"
                      "asdf22 | adsads");
}

TEST_F(EditorTestHarness, PasteTableThenDoubleEnterAlignsRows)
{
    pasteText("foo|bar\n--|--\nasdf | adsf\nasdf22 | adsads");
    placeCursorAtEnd();
    enter();   // auto-continues a blank data row (borderless, padding 1)
    enter();   // blank-row exit leaves the table, realigning it
    EXPECT_EQ(text(), "foo    | bar   \n"
                      "------ | ------\n"
                      "asdf   | adsf  \n"
                      "asdf22 | adsads\n"
                      "\n");
    assertCursor(5, 0);
}

TEST_F(EditorTestHarness, DoubleEnterOnRightAlignedBorderlessTableAlignsPipes)
{
    // A borderless table whose first column is right-aligned cannot keep its
    // pipes aligned: the narrow last row would need four leading spaces and
    // md4c would drop it (indented code). The formatter upgrades the table to
    // bordered so every row's middle pipe lands on the same column.
    setContent("asdf | asdf \n----: | -----\ncell1 | cell2\ncell3 | cell4\n   a | aa ");
    placeCursorAtEnd();
    enter();   // auto-continues a blank data row (borderless, padding 1)
    enter();   // blank-row exit leaves the table, realigning it
    EXPECT_EQ(text(), "|  asdf | asdf  |\n"
                      "|------:|-------|\n"
                      "| cell1 | cell2 |\n"
                      "| cell3 | cell4 |\n"
                      "|     a | aa    |\n"
                      "\n");
    assertCursor(6, 0);
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

TEST_F(EditorTestHarness, InsertPageBreakIntoEmptyDocument)
{
    editor->insertTypesettingDirective(QStringLiteral("break"));
    EXPECT_EQ(text(), "<!-- break -->\n");
    assertCursor(1, 0);
}

TEST_F(EditorTestHarness, InsertKeepMidParagraphAddsSeparatingBlankLine)
{
    setContent("para one\npara two");
    placeCursor(1, 3);
    editor->insertTypesettingDirective(QStringLiteral("keep"));
    EXPECT_EQ(text(), "para one\n\n<!-- keep -->\npara two");
    assertCursor(3, 0);
}

TEST_F(EditorTestHarness, InsertKeepWithBlankLineAboveAddsNoBlank)
{
    setContent("para one\n\npara two");
    placeCursor(1, 3);
    editor->insertTypesettingDirective(QStringLiteral("keep"));
    EXPECT_EQ(text(), "para one\n\n<!-- keep -->\npara two");
    assertCursor(3, 0);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


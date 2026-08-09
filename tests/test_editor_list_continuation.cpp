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
#include "StaticHelpers.h"

TEST(ListContinuation, DashPrefix) {
    EXPECT_EQ(handleListReturn("- hello"), "- ");
}

TEST(ListContinuation, StarPrefix) {
    EXPECT_EQ(handleListReturn("* hello"), "* ");
}

TEST(ListContinuation, PlusPrefix) {
    EXPECT_EQ(handleListReturn("+ hello"), "+ ");
}

TEST(ListContinuation, OrderedIncrement) {
    EXPECT_EQ(handleListReturn("1. first"), "2. ");
}

TEST(ListContinuation, OrderedParenIncrement) {
    EXPECT_EQ(handleListReturn("1) first"), "2) ");
}

TEST(ListContinuation, OrderedParenIncrementDoubleDigit) {
    EXPECT_EQ(handleListReturn("9) ninth"), "10) ");
}

TEST(ListContinuation, OrderedIncrementDoubleDigit) {
    EXPECT_EQ(handleListReturn("9. ninth"), "10. ");
}

TEST(ListContinuation, IndentedUnordered) {
    EXPECT_EQ(handleListReturn("  - nested"), "  - ");
}

TEST(ListContinuation, IndentedOrdered) {
    EXPECT_EQ(handleListReturn("  1. nested"), "  2. ");
}

TEST(ListContinuation, IndentedOrderedParen) {
    EXPECT_EQ(handleListReturn("  1) nested"), "  2) ");
}

TEST(ListContinuation, EmptyUnorderedClears) {
    EXPECT_EQ(handleListReturn("-"), QString(clearSentinel));
}

TEST(ListContinuation, EmptyOrderedClears) {
    EXPECT_EQ(handleListReturn("1."), QString(clearSentinel));
}

TEST(ListContinuation, EmptyOrderedParenClears) {
    EXPECT_EQ(handleListReturn("1)"), QString(clearSentinel));
}

TEST(ListContinuation, EmptyIndentedClears) {
    EXPECT_EQ(handleListReturn("  -"), QString(clearSentinel));
}

TEST(ListContinuation, TaskUnchecked) {
    EXPECT_EQ(handleListReturn("- [ ] todo"), "- [ ] ");
}

TEST(ListContinuation, TaskChecked) {
    EXPECT_EQ(handleListReturn("- [x] done"), "- [ ] ");
}

TEST(ListContinuation, TaskCheckedCapital) {
    EXPECT_EQ(handleListReturn("- [X] done"), "- [ ] ");
}

TEST(ListContinuation, TaskStar) {
    EXPECT_EQ(handleListReturn("* [ ] todo"), "* [ ] ");
}

TEST(ListContinuation, TaskPlus) {
    EXPECT_EQ(handleListReturn("+ [ ] todo"), "+ [ ] ");
}

TEST(ListContinuation, TaskIndented) {
    EXPECT_EQ(handleListReturn("  - [ ] nested"), "  - [ ] ");
}

TEST(ListContinuation, TaskEmptyClears) {
    EXPECT_EQ(handleListReturn("- [ ]"), QString(clearSentinel));
}

TEST(ListContinuation, TaskCheckedEmptyClears) {
    EXPECT_EQ(handleListReturn("- [x]"), QString(clearSentinel));
}

TEST(ListContinuation, PlainTextReturnsEmpty) {
    EXPECT_EQ(handleListReturn("just text"), QString());
}

TEST(ListContinuation, EmptyLineReturnsEmpty) {
    EXPECT_EQ(handleListReturn(""), QString());
}

TEST(IndentListLine, IndentsDash) {
    EXPECT_EQ(indentListLine("- item"), "  - item");
}

TEST(IndentListLine, IndentsAlreadyIndented) {
    EXPECT_EQ(indentListLine("  - item"), "    - item");
}

TEST(IndentListLine, IndentsOrdered) {
    EXPECT_EQ(indentListLine("1. item"), "   1. item");
}

TEST(IndentListLine, IndentsOrderedParen) {
    EXPECT_EQ(indentListLine("1) item"), "   1) item");
}

TEST(IndentListLine, IndentsOrderedParenIndented) {
    EXPECT_EQ(indentListLine("  1) item"), "     1) item");
}

TEST(IndentListLine, IndentsMultiDigitOrdered) {
    EXPECT_EQ(indentListLine("10. item"), "    10. item");
}

TEST(IndentListLine, IndentsAlreadyIndentedOrdered) {
    EXPECT_EQ(indentListLine("   1. item"), "      1. item");
}

TEST(IndentListLine, NoChangeForPlainText) {
    EXPECT_EQ(indentListLine("just text"), "just text");
}

TEST(OutdentListLine, OutdentsDoubleSpace) {
    EXPECT_EQ(outdentListLine("  - item"), "- item");
}

TEST(OutdentListLine, OutdentsFourSpace) {
    EXPECT_EQ(outdentListLine("    - item"), "  - item");
}

TEST(OutdentListLine, OutdentsOrderedParen) {
    EXPECT_EQ(outdentListLine("  1) item"), "1) item");
}

TEST(OutdentListLine, OutdentsOrderedParenAtContentColumn) {
    EXPECT_EQ(outdentListLine("   1) item"), "1) item");
}

TEST(OutdentListLine, NoChangeForNoIndent) {
    EXPECT_EQ(outdentListLine("- item"), "- item");
}

TEST(OutdentListLine, NoChangeForPlainText) {
    EXPECT_EQ(outdentListLine("just text"), "just text");
}

TEST(ThematicBreak, MatchesDashes) {
    EXPECT_TRUE(isThematicBreak("---"));
}

TEST(ThematicBreak, MatchesStars) {
    EXPECT_TRUE(isThematicBreak("***"));
}

TEST(ThematicBreak, MatchesUnderscores) {
    EXPECT_TRUE(isThematicBreak("___"));
}

TEST(ThematicBreak, MatchesSpacedDashes) {
    EXPECT_TRUE(isThematicBreak("- - -"));
}

TEST(ThematicBreak, MatchesMoreThanThree) {
    EXPECT_TRUE(isThematicBreak("----"));
}

TEST(ThematicBreak, MatchesIndented) {
    EXPECT_TRUE(isThematicBreak("  ---"));
}

TEST(ThematicBreak, SingleDashIsNotBreak) {
    EXPECT_FALSE(isThematicBreak("-"));
}

TEST(ThematicBreak, TwoDashesAreNotBreak) {
    EXPECT_FALSE(isThematicBreak("--"));
}

TEST(ThematicBreak, MixedMarkersAreNotBreak) {
    EXPECT_FALSE(isThematicBreak("--*"));
}

TEST(ThematicBreak, ListItemIsNotBreak) {
    EXPECT_FALSE(isThematicBreak("- item"));
}

TEST(ThematicBreak, PlainTextIsNotBreak) {
    EXPECT_FALSE(isThematicBreak("hello"));
}

TEST(ThematicBreak, TwoSpacedDashesAreNotBreak) {
    EXPECT_FALSE(isThematicBreak("- -"));
}

TEST(HandleListReturn, ThematicBreakReturnsEmpty) {
    EXPECT_EQ(handleListReturn("---"), QString());
}

TEST(HandleListReturn, StarThematicBreakReturnsEmpty) {
    EXPECT_EQ(handleListReturn("***"), QString());
}

TEST(HandleListReturn, UnderscoreThematicBreakReturnsEmpty) {
    EXPECT_EQ(handleListReturn("___"), QString());
}

TEST(HandleListReturn, IndentedThematicBreakReturnsEmpty) {
    EXPECT_EQ(handleListReturn("  ---"), QString());
}

TEST(IndentListLine, ThematicBreakUnchanged) {
    EXPECT_EQ(indentListLine("---"), "---");
}

TEST(IndentListLine, SpacedThematicBreakUnchanged) {
    EXPECT_EQ(indentListLine("- - -"), "- - -");
}

TEST(OutdentListLine, ThematicBreakUnchanged) {
    EXPECT_EQ(outdentListLine("---"), "---");
}

TEST(HandleListReturn, SingleDashStillClears) {
    EXPECT_EQ(handleListReturn("-"), QString(clearSentinel));
}

TEST(HandleListReturn, DashItemStillContinues) {
    EXPECT_EQ(handleListReturn("- item"), "- ");
}

TEST(HandleListReturn, BoldMarkerDoesNotContinueList) {
    EXPECT_EQ(handleListReturn("**bold**"), QString());
}

TEST(HandleListReturn, BoldMarkerMidLineDoesNotContinueList) {
    EXPECT_EQ(handleListReturn("**bold** tail"), QString());
}

TEST(ListSplitReturn, MidLineContinuesDash) {
    EXPECT_EQ(handleListSplitReturn("- hello", 3), "- ");
}

TEST(ListSplitReturn, MidLineContinuesTask) {
    EXPECT_EQ(handleListSplitReturn("- [ ] todo", 8), "- [ ] ");
}

TEST(ListSplitReturn, MidLineContinuesOrdered) {
    EXPECT_EQ(handleListSplitReturn("1. first", 3), "2. ");
}

TEST(ListSplitReturn, MidLineContinuesIndented) {
    EXPECT_EQ(handleListSplitReturn("  - nested", 6), "  - ");
}

TEST(ListSplitReturn, CaretInsideMarkerSplitsNormally) {
    EXPECT_EQ(handleListSplitReturn("- hello", 1), QString());
}

TEST(ListSplitReturn, CaretAtStartSplitsNormally) {
    EXPECT_EQ(handleListSplitReturn("- hello", 0), QString());
}

TEST(ListSplitReturn, NothingAfterCaretSplitsNormally) {
    EXPECT_EQ(handleListSplitReturn("- hello", 7), QString());
}

TEST(ListSplitReturn, OnlyTrailingWhitespaceSplitsNormally) {
    EXPECT_EQ(handleListSplitReturn("- hi  ", 4), QString());
}

TEST(ListSplitReturn, PlainTextReturnsEmpty) {
    EXPECT_EQ(handleListSplitReturn("just text", 5), QString());
}

TEST(ListSplitReturn, BoldMarkerDoesNotContinueList) {
    EXPECT_EQ(handleListSplitReturn("**bold here** followed", 14), QString());
}

TEST(ListSplitReturn, BoldMarkerAtLineStartDoesNotContinueList) {
    EXPECT_EQ(handleListSplitReturn("**bold**", 2), QString());
}

TEST(ListSplitReturn, PlusMarkerDoesNotContinueList) {
    EXPECT_EQ(handleListSplitReturn("+clap+ trailing", 7), QString());
}

TEST(ListSplitReturn, DecimalWithDotDoesNotContinueList) {
    EXPECT_EQ(handleListSplitReturn("1.5 lbs", 3), QString());
}

TEST(ListSplitReturn, ThematicBreakReturnsEmpty) {
    EXPECT_EQ(handleListSplitReturn("---", 2), QString());
}


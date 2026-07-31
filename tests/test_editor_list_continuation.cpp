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

TEST(ListContinuation, OrderedIncrementDoubleDigit) {
    EXPECT_EQ(handleListReturn("9. ninth"), "10. ");
}

TEST(ListContinuation, IndentedUnordered) {
    EXPECT_EQ(handleListReturn("  - nested"), "  - ");
}

TEST(ListContinuation, IndentedOrdered) {
    EXPECT_EQ(handleListReturn("  1. nested"), "  2. ");
}

TEST(ListContinuation, EmptyUnorderedClears) {
    EXPECT_EQ(handleListReturn("-"), QString(clearSentinel));
}

TEST(ListContinuation, EmptyOrderedClears) {
    EXPECT_EQ(handleListReturn("1."), QString(clearSentinel));
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
    EXPECT_EQ(indentListLine("1. item"), "  1. item");
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

TEST(TableReturn, ContinuationReturnsRow) {
    EXPECT_EQ(handleTableReturn("| a | b |", "| x | y |"), "|  |  |");
}

TEST(TableReturn, OneColReturnsRow) {
    EXPECT_EQ(handleTableReturn("| foo |", "| z |"), "|  |");
}

TEST(TableReturn, FirstRowCreatesSeparator) {
    EXPECT_EQ(handleTableReturn("| a | b |", "not a table"), "|---|---|\n|  |  |");
}

TEST(TableReturn, SeparatorReturnsEmpty) {
    EXPECT_EQ(handleTableReturn("|---|---|", ""), QString());
}

TEST(TableReturn, BlankRowReturnsSentinel) {
    EXPECT_EQ(handleTableReturn("|  |  |", ""), QString(clearSentinel));
}

TEST(TableReturn, PlainTextReturnsEmpty) {
    EXPECT_EQ(handleTableReturn("hello", ""), QString());
}

TEST(TableReturn, HtmlReturnsRow) {
    EXPECT_EQ(handleTableReturn("<tr><td>a</td><td>b</td></tr>", ""),
              "<tr><td></td><td></td></tr>");
}

TEST(TableNav, ForwardWithinRow) {
    EXPECT_EQ(tableNavCell("|  |  |  |", 1, true), 5);
}

TEST(TableNav, ForwardFromMiddle) {
    EXPECT_EQ(tableNavCell("|  |  |  |", 4, true), 8);
}

TEST(TableNav, ForwardFromLastReturnsMinusOne) {
    EXPECT_EQ(tableNavCell("|  |  |  |", 7, true), -1);
}

TEST(TableNav, BackwardWithinRow) {
    EXPECT_EQ(tableNavCell("|  |  |  |", 7, false), 5);
}

TEST(TableNav, BackwardFromFirstReturnsMinusOne) {
    EXPECT_EQ(tableNavCell("|  |  |  |", 1, false), -1);
}

TEST(TableNav, BackwardFromSecondGoesToFirst) {
    EXPECT_EQ(tableNavCell("|  |  |  |", 4, false), 2);
}

TEST(TableNav, ForwardSingleCellRow) {
    EXPECT_EQ(tableNavCell("|  |", 1, true), -1);
}

TEST(TableReturn, HtmlBlankRowReturnsSentinel) {
    EXPECT_EQ(handleTableReturn("<tr><td></td><td></td></tr>", ""), QString(clearSentinel));
}

TEST(TableReturn, HtmlNonBlankRowReturnsRow) {
    EXPECT_EQ(handleTableReturn("<tr><td>a</td><td>b</td></tr>", ""),
              "<tr><td></td><td></td></tr>");
}

TEST(TableNavHtml, ForwardWithinRow) {
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td></tr>", 8, true), 17);
}

TEST(TableNavHtml, ForwardFromLastReturnsMinusOne) {
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td></tr>", 17, true), -1);
}

TEST(TableNavHtml, BackwardWithinRow) {
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td></tr>", 17, false), 8);
}

TEST(TableNavHtml, BackwardFromFirstReturnsMinusOne) {
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td></tr>", 8, false), -1);
}

TEST(TableNavHtml, ThreeCellsForward) {
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td><td></td></tr>", 8, true), 17);
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td><td></td></tr>", 17, true), 26);
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td><td></td></tr>", 26, true), -1);
}

TEST(TableNavHtml, ThreeCellsBackward) {
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td><td></td></tr>", 26, false), 17);
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td><td></td></tr>", 17, false), 8);
    EXPECT_EQ(tableNavHtmlCell("<tr><td></td><td></td><td></td></tr>", 8, false), -1);
}

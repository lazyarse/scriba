#include <gtest/gtest.h>
#include "StaticHelpers.h"

static const QChar clearSentinel(0x2412);

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

TEST(TableReturn, ContinuationReturnsRow) {
    EXPECT_EQ(handleTableReturn("| a | b |"), "|  |  |");
}

TEST(TableReturn, OneColReturnsRow) {
    EXPECT_EQ(handleTableReturn("| foo |"), "|  |");
}

TEST(TableReturn, SeparatorReturnsEmpty) {
    EXPECT_EQ(handleTableReturn("|---|---|"), QString());
}

TEST(TableReturn, BlankRowReturnsSentinel) {
    EXPECT_EQ(handleTableReturn("|  |  |"), QString(clearSentinel));
}

TEST(TableReturn, NonBlankReturnsRow) {
    EXPECT_EQ(handleTableReturn("| a | b |"), "|  |  |");
}

TEST(TableReturn, PlainTextReturnsEmpty) {
    EXPECT_EQ(handleTableReturn("hello"), QString());
}

TEST(TableReturn, HtmlReturnsRow) {
    EXPECT_EQ(handleTableReturn("<tr><td>a</td><td>b</td></tr>"),
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

TEST(TableNav, ForwardSingleCellRow) {
    EXPECT_EQ(tableNavCell("|  |", 1, true), -1);
}

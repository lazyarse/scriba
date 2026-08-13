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
#include "MdTable.h"
#include "StaticHelpers.h" // clearSentinel

using MdTable::MdRowStyle;
using MdTable::formatMdTable;
using MdTable::handleTableReturn;
using MdTable::makeEmptyTableRow;
using MdTable::makeTableSeparatorRow;
using MdTable::tableNavCell;
using MdTable::tableNavHtmlCell;

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

TEST(TableReturn, HtmlBlankRowReturnsSentinel) {
    EXPECT_EQ(handleTableReturn("<tr><td></td><td></td></tr>", ""), QString(clearSentinel));
}

TEST(TableReturn, HtmlNonBlankRowReturnsRow) {
    EXPECT_EQ(handleTableReturn("<tr><td>a</td><td>b</td></tr>", ""),
              "<tr><td></td><td></td></tr>");
}

TEST(TableReturn, PaddingScalesEmptyRow) {
    EXPECT_EQ(handleTableReturn("| a | b |", "| x | y |", 2), "|    |    |");
}

TEST(TableReturn, PaddingZeroPacksEmptyRow) {
    EXPECT_EQ(handleTableReturn("| a | b |", "| x | y |", 0), "|||");
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

TEST(FormatMdTableTest, AlignsColumnsWithDefaultAlignment) {
    QStringList rows = {"| a | bb |", "|---|---|", "| ccc | d |"};
    EXPECT_EQ(formatMdTable(rows),
        "| a   | bb |\n"
        "|-----|----|\n"
        "| ccc | d  |");
}

TEST(FormatMdTableTest, RespectsSeparatorAlignment) {
    QStringList rows = {
        "| name | qty |",
        "|:-----|---:|",
        "| apple | 1 |",
        "| grapefruit | 100 |"
    };
    EXPECT_EQ(formatMdTable(rows),
        "| name       | qty |\n"
        "|:-----------|----:|\n"
        "| apple      |   1 |\n"
        "| grapefruit | 100 |");
}

TEST(FormatMdTableTest, CenterAlignment) {
    QStringList rows = {
        "| h1 | h2 | h3 |",
        "|:---|:---:|---:|",
        "| a | bb | ccc |"
    };
    EXPECT_EQ(formatMdTable(rows),
        "| h1 | h2 |  h3 |\n"
        "|:---|:--:|----:|\n"
        "| a  | bb | ccc |");
}

TEST(FormatMdTableTest, LeftAlignmentFlushLeftAcrossRows) {
    QStringList rows = {
        "| a |",
        "|:--|",
        "| xyz |",
        "| longcontent |",
        "| m |"
    };
    EXPECT_EQ(formatMdTable(rows),
        "| a           |\n"
        "|:------------|\n"
        "| xyz         |\n"
        "| longcontent |\n"
        "| m           |");
}

TEST(FormatMdTableTest, RightAlignmentFlushRightAcrossRows) {
    QStringList rows = {
        "| a |",
        "|---:|",
        "| xyz |",
        "| longcontent |",
        "| m |"
    };
    EXPECT_EQ(formatMdTable(rows),
        "|           a |\n"
        "|------------:|\n"
        "|         xyz |\n"
        "| longcontent |\n"
        "|           m |");
}

TEST(FormatMdTableTest, CenterSingleCharInEvenColumn) {
    // A 1-char cell cannot be perfectly centred in an even-width column; the
    // extra space lands on the right so pipes stay aligned.
    QStringList rows = {
        "| a |",
        "|:--:|",
        "| bb |",
        "| c |"
    };
    EXPECT_EQ(formatMdTable(rows),
        "| a  |\n"
        "|:--:|\n"
        "| bb |\n"
        "| c  |");
}

TEST(FormatMdTableTest, EmptyCellsGetColumnWidth) {
    QStringList rows = {"| a | b |", "|---|---|", "|  |  |"};
    EXPECT_EQ(formatMdTable(rows),
        "| a | b |\n"
        "|---|---|\n"
        "|   |   |");
}

TEST(FormatMdTableTest, EscapedPipePreserved) {
    QStringList rows = {"| a\\|b | c |", "|---|---|", "| d | e |"};
    EXPECT_EQ(formatMdTable(rows),
        "| a\\|b | c |\n"
        "|------|---|\n"
        "| d    | e |");
}

TEST(FormatMdTableTest, NotATableReturnsEmpty) {
    QStringList rows = {"| a | b |", "| c | d |"};
    EXPECT_TRUE(formatMdTable(rows).isEmpty());
    EXPECT_TRUE(formatMdTable({"plain text"}).isEmpty());
    EXPECT_TRUE(formatMdTable({}).isEmpty());
}

TEST(FormatMdTableTest, RaggedRowsAreNormalized) {
    QStringList rows = {"| a | b |", "|---|---|", "| c |"};
    EXPECT_EQ(formatMdTable(rows),
        "| a | b |\n"
        "|---|---|\n"
        "| c |   |");
}

TEST(FormatMdTableTest, Idempotent) {
    QStringList rows = {"| a | bb |", "|---|---|", "| ccc | d |"};
    QString once = formatMdTable(rows);
    QString twice = formatMdTable(once.split('\n'));
    EXPECT_EQ(once, twice);
}

TEST(FormatMdTableTest, PaddingTwoSpacesEachSide) {
    QStringList rows = {"| a | bb |", "|---|---|", "| ccc | d |"};
    EXPECT_EQ(formatMdTable(rows, 2),
        "|  a    |  bb  |\n"
        "|-------|------|\n"
        "|  ccc  |  d   |");
}

TEST(FormatMdTableTest, PaddingZeroFloorsNarrowColumns) {
    // Padding 0 must not shrink a column below three chars: a narrower
    // separator cell would lose its dashes (`:` or `::`) and md4c would reject
    // the whole table. The body is floored instead.
    QStringList rows = {"| a | bb |", "|---|---|", "| ccc | d |"};
    EXPECT_EQ(formatMdTable(rows, 0),
        "|a  |bb |\n"
        "|---|---|\n"
        "|ccc|d  |");
}

TEST(FormatMdTableTest, PaddingZeroKeepsCenterColons) {
    // The floor-3 separator keeps at least one dash even for colon-aligned
    // narrow columns at padding 0, so the output stays a valid table.
    QStringList rows = {
        "| h | c |",
        "|:--:|:--:|",
        "| a | bb |"
    };
    EXPECT_EQ(formatMdTable(rows, 0),
        "| h | c |\n"
        "|:-:|:-:|\n"
        "| a |bb |");
}

TEST(FormatMdTableTest, PaddingKeepsAlignmentColons) {
    QStringList rows = {
        "| h1 | h2 |",
        "|:---|---:|",
        "| a | bb |"
    };
    EXPECT_EQ(formatMdTable(rows, 2),
        "|  h1  |  h2  |\n"
        "|:-----|-----:|\n"
        "|  a   |  bb  |");
}

TEST(FormatMdTableTest, BorderlessPaddingTwo) {
    QStringList rows = {"a | bb", "--- | ---", "ccc | d"};
    EXPECT_EQ(formatMdTable(rows, 2),
        "a    |  bb\n"
        "---  |  ---\n"
        "ccc  |  d ");
}

TEST(FormatMdTableTest, BorderlessRightAlignedFirstColumnUpgradesToBordered) {
    // md4c ends a borderless table at any row with four or more leading spaces
    // (indented-code check wins over table continuation), so a right-aligned
    // first column whose narrow cells would need that much leading padding
    // cannot stay borderless without either dropping the row from the render
    // or misaligning its first pipe (the cap). Upgrade to bordered instead:
    // the leading pipe anchors column zero, so the pipes line up and the
    // table keeps rendering.
    QStringList rows = {
        "asdf | asdf",
        "----: | -----",
        "cell1 | cell2",
        "cell3 | cell4",
        "a | aa"
    };
    EXPECT_EQ(formatMdTable(rows),
        "|  asdf | asdf  |\n"
        "|------:|-------|\n"
        "| cell1 | cell2 |\n"
        "| cell3 | cell4 |\n"
        "|     a | aa    |");
}

TEST(FormatMdTableTest, BorderlessRightAlignedFirstColumnStaysBorderlessWhenItFits) {
    // Right alignment that never needs four leading spaces keeps the bare
    // style: no gratuitous conversion for short, single-column tables.
    QStringList rows = {"a | b", "--: | --", "x | y"};
    EXPECT_EQ(formatMdTable(rows),
        "a | b\n"
        "--: | ---\n"
        "x | y");
}

TEST(FormatMdTableTest, BorderlessCenterAlignedFirstColumnUpgradesToBordered) {
    QStringList rows = {
        "abcdefghi | x",
        ":---: | ---",
        "a | y"
    };
    EXPECT_EQ(formatMdTable(rows),
        "| abcdefghi | x |\n"
        "|:---------:|---|\n"
        "|     a     | y |");
}

TEST(MakeEmptyTableRow, PaddingScalesBorderedCells) {
    EXPECT_EQ(makeEmptyTableRow(2, MdRowStyle{true, true}, 2), "|    |    |");
    EXPECT_EQ(makeEmptyTableRow(2, MdRowStyle{true, true}, 0), "|||");
}

TEST(MakeEmptyTableRow, PaddingScalesBorderlessCells) {
    // Borderless rows have no leading pipe to anchor column zero, and md4c
    // turns any table-continuation line with four or more leading spaces into
    // an indented code block — so the leading padding is capped at three.
    for (int p = 1; p <= 4; ++p) {
        const QString row = makeEmptyTableRow(2, MdRowStyle{false, false}, p);
        EXPECT_LE(row.indexOf('|'), 3) << "padding " << p;
        EXPECT_GT(row.size(), 0) << "padding " << p;
    }
    EXPECT_EQ(makeEmptyTableRow(2, MdRowStyle{false, false}, 2), "   |      ");
}

TEST(MakeSeparatorRow, PaddingScalesDashes) {
    EXPECT_EQ(makeTableSeparatorRow(2, MdRowStyle{true, true}, 2), "|-----|-----|");
    // Dash count is floored at three: fewer dashes than that is non-portable
    // markdown (and `-` alone can be misread), even at padding 0.
    EXPECT_EQ(makeTableSeparatorRow(2, MdRowStyle{true, true}, 0), "|---|---|");
}

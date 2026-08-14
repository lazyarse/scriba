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
#pragma once

#include <QString>
#include <QStringList>

// Pure markdown table string helpers shared between the editor's typing logic
// and the unit tests. All functions are stateless; `padding` is the number of
// spaces the formatters place around each cell's content (default 1, tunable
// via the Editor → Tables preference).

namespace MdTable {

// GFM tables make the leading/trailing `|` optional, so a table row may be
// bordered (`| a | b |`), borderless (`a | b`), or something in between.
// `MdRowStyle` records which of the two optional border pipes a row carries.
struct MdRowStyle {
    bool hasLeadingPipe = true;
    bool hasTrailingPipe = true;
};

// Returns the text for the new line(s) when Enter is pressed on `line` with
// `prevLine` above it: a new empty data row (matching `line`'s border style)
// when continuing a table, a separator + empty data row when the table starts
// on this line, the clear sentinel when the current row is blank (exits the
// table), an empty data row when `line` is an HTML table row, and an empty
// QString for non-table lines. `padding` is passed through to the row makers.
QString handleTableReturn(const QString &line, const QString &prevLine, int padding = 1);
// Returns whether `line` contains any unescaped `|`, i.e. could read as a
// markdown table row regardless of border style.
bool isMdTableLikeRow(const QString &line);
// Returns `line`'s leading/trailing border style (which of the optional pipes
// GFM allows are actually present).
MdRowStyle mdRowStyle(const QString &line);
// Splits a markdown table row into its cells, dropping the optional leading/
// trailing border cells. Escaped pipes (`\|`) are preserved and cells are
// trimmed. `style` (optional) receives the row's border style.
QStringList splitMdTableRow(const QString &line, MdRowStyle *style = nullptr);
// Position of the first cell's content in `line` (just past the leading pipe
// when bordered, else the line start, skipping any leading spaces).
int mdRowFirstCellPos(const QString &line);
// Position of the last cell's content in `line` (just past the second-last
// pipe when the row is trailing-bordered, else just past the last pipe).
int mdRowLastCellPos(const QString &line);
// 0-based index of the column containing `blockPos` in a markdown table row
// (clamped to the last column when the caret sits in the trailing border).
int mdRowColumnAt(const QString &line, int blockPos);
// Returns whether `line` is a markdown table separator row: pipes where every
// cell is made only of optional colons and at least one dash (spaces allowed
// around it). Matches narrow columns such as `|:--:|` or `|--:|` too, not just
// the spec's three-dash minimum, and matches borderless separators (`--- | ---`)
// as well. A bare `---` (thematic break / setext underline) is not a separator.
bool isMdSeparatorRow(const QString &line);
// Returns whether `line` is a markdown table data row whose cells are all
// empty (e.g. `|   |   |`). Separator rows are never blank.
bool isBlankMdTableRow(const QString &line);
// An empty markdown table data row of `cols` columns matching `style`'s border
// style (bordered rows carry leading/trailing pipes, borderless rows do not),
// with `padding` spaces on each side of every empty cell.
QString makeEmptyTableRow(int cols, const MdRowStyle &style, int padding = 1);
// A bare markdown table separator row of `cols` columns matching `style`, its
// dashes spanning `2 * padding + 1` characters like the padded data columns.
QString makeTableSeparatorRow(int cols, const MdRowStyle &style, int padding = 1);
QString makeEmptyTableRow(int cols);
QString makeEmptyHtmlTableRow(int cols);
// Returns block-relative cursor position of next/previous cell in a markdown
// table row. forward=true finds the next cell after cursorPos, false the
// previous cell before it. Returns -1 if at first cell (going backward) or
// last cell (going forward), signaling the caller should navigate to another
// row. Handles both bordered (`| a | b |`) and borderless (`a | b`) rows.
int tableNavCell(const QString &line, int cursorPos, bool forward);
int tableNavHtmlCell(const QString &line, int cursorPos, bool forward);
// Re-aligns a markdown table: takes the contiguous `|`-delimited rows of a
// table block (header, separator, and data rows) and returns the rows re-spaced
// so the column pipes line up. Per-column alignment follows the separator row
// (`:---` left, `---:` right, `:---:` center, `---` default/null). Returns an
// empty QString when `rows` is not a valid table (no separator row containing
// `---`). The separator row's border style (leading/trailing `|`) is preserved:
// a borderless table stays borderless. Cell content is trimmed; escaped pipes
// (`\|`) are preserved. `padding` is the number of spaces around each cell's
// content (default 1); the separator dashes span the padded column width so
// the pipes still line up.
QString formatMdTable(const QStringList &rows, int padding = 1);

} // namespace MdTable

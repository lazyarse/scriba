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
#include "MdTable.h"
#include "StaticHelpers.h" // clearSentinel
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QVector>
#include <algorithm>

namespace MdTable {

namespace {

enum class MdAlign { Default, Left, Right, Center };

MdAlign mdAlignFromSeparator(const QString &sep)
{
    const bool left = sep.startsWith(':');
    const bool right = sep.endsWith(':');
    if (left && right) return MdAlign::Center;
    if (right) return MdAlign::Right;
    if (left) return MdAlign::Left;
    return MdAlign::Default;
}

QString padMdCell(const QString &content, int width, MdAlign align)
{
    const int len = content.size();
    if (len >= width)
        return content;
    if (align == MdAlign::Right)
        return QString(width - len, ' ') + content;
    if (align == MdAlign::Center) {
        const int left = (width - len) / 2;
        const int right = width - len - left;
        return QString(left, ' ') + content + QString(right, ' ');
    }
    return content + QString(width - len, ' ');
}

// Separator cell for a bordered table: `span` characters wide (the padded
// content width), with the alignment colons, so the dashes reach the pipes.
QString mdSeparatorCell(MdAlign align, int width, int padding)
{
    const int span = qMax(width + 2 * padding, 3);
    switch (align) {
    case MdAlign::Left:    return ':' + QString(qMax(span - 1, 0), '-');
    case MdAlign::Right:   return QString(qMax(span - 1, 0), '-') + ':';
    case MdAlign::Center:  return ':' + QString(qMax(span - 2, 0), '-') + ':';
    case MdAlign::Default: return QString(span, '-');
    }
    return QString(span, '-');
}

// Separator cell for a borderless table: dashes spanning `width` (at least
// three) with the alignment colons, so aligning a borderless table never
// introduces leading/trailing pipes. Padding is carried by the join string in
// the caller, not by the dashes themselves.
QString bareSeparator(MdAlign align, int width)
{
    const int d = qMax(width, 3);
    switch (align) {
    case MdAlign::Left:    return ':' + QString(d - 1, '-');
    case MdAlign::Right:   return QString(d - 1, '-') + ':';
    case MdAlign::Center: {
        const int inner = qMax(d - 2, 1);
        const int l = inner / 2;
        return ':' + QString(l, '-') + QString(inner - l, '-') + ':';
    }
    default: return QString(d, '-');
    }
}

// An unescaped pipe is one not preceded by an odd number of backslashes; such
// `\|` is cell content, not a column separator.
bool pipeIsEscaped(const QString &line, int idx)
{
    int backslashes = 0;
    for (int i = idx - 1; i >= 0 && line[i] == '\\'; --i)
        ++backslashes;
    return (backslashes % 2) == 1;
}

// Indexes of the unescaped `|` characters in `line`.
QList<int> mdPipeIndexes(const QString &line)
{
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|' && !pipeIsEscaped(line, i))
            pipes.append(i);
    return pipes;
}

// md4c terminates a table at any continuation line with four or more leading
// spaces (its indented-code check runs before the table-continuation check;
// GFM keeps such rows in the table — see docs/gotchas.md). A borderless row
// has no leading pipe to anchor column zero, so padding-driven leading spaces
// can push a row past that limit. Cap the leading whitespace at three so the
// row stays in the table; the first pipe may sit one column left of aligned
// rows, a cosmetic cost of the cap.
QString capTableRowIndent(const QString &row)
{
    int lead = 0;
    while (lead < row.size() && row[lead] == ' ')
        ++lead;
    if (lead <= 3)
        return row;
    return QString(3, ' ') + row.mid(lead);
}

} // namespace

QString handleTableReturn(const QString &line, const QString &prevLine, int padding)
{
    if (isMdTableLikeRow(line)) {
        // The new row's pipes are written in the current line's own border
        // style, so a borderless table stays borderless.
        const MdRowStyle style = mdRowStyle(line);

        // Separator row → no continuation
        if (isMdSeparatorRow(line))
            return {};

        int cols = splitMdTableRow(line).size();
        if (cols <= 0) return {};

        // Blank row → exit table
        if (isBlankMdTableRow(line)) {
            return QString(clearSentinel);
        }

        // Previous line is also a table row → data row continuation
        if (isMdTableLikeRow(prevLine))
            return makeEmptyTableRow(cols, style, padding);

        // First row of a new table → separator + data row
        return makeTableSeparatorRow(cols, style, padding) + "\n" + makeEmptyTableRow(cols, style, padding);
    }

    if (line.contains("<tr>") && line.contains("<td>")) {
        int cols = line.count("<td>");
        if (cols <= 0) return {};

        // Blank HTML row → exit autocomplete
        {
            QXmlStreamReader xml(line);
            bool allEmpty = true;
            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isCharacters() && !xml.isWhitespace()) {
                    allEmpty = false;
                    break;
                }
            }
            if (allEmpty) {
                return QString(clearSentinel);
            }
        }

        return makeEmptyHtmlTableRow(cols);
    }

    return {};
}

bool isMdTableLikeRow(const QString &line)
{
    return !mdPipeIndexes(line).isEmpty();
}

MdRowStyle mdRowStyle(const QString &line)
{
    MdRowStyle style;
    const QList<int> pipes = mdPipeIndexes(line);
    int lead = 0;
    while (lead < line.size() && line[lead].isSpace())
        ++lead;
    const bool leading = lead < line.size() && line[lead] == '|'
                         && !pipeIsEscaped(line, lead);
    int trail = line.size() - 1;
    while (trail >= 0 && line[trail].isSpace())
        --trail;
    const bool trailing = trail >= 0 && line[trail] == '|'
                          && !pipeIsEscaped(line, trail);
    // A lone pipe (`a | b`, `    |    `, `--- | ---`) is a cell separator, not
    // a border: a bordered row always pairs its leading pipe with another pipe
    // after the first cell. Treating the single pipe of an empty borderless row
    // (`    |    ` — its leading spaces are the empty first cell, not indent) as
    // a border would misplace the cursor and mis-navigate it.
    style.hasLeadingPipe = leading && pipes.size() >= 2;
    style.hasTrailingPipe = trailing && pipes.size() >= 2;
    return style;
}

QStringList splitMdTableRow(const QString &line, MdRowStyle *style)
{
    if (style)
        *style = mdRowStyle(line);

    QStringList parts;
    QString cur;
    int backslashes = 0;
    for (const QChar &ch : line) {
        if (ch == '|' && (backslashes % 2) == 0) {
            parts << cur;
            cur.clear();
            backslashes = 0;
        } else {
            if (ch == '\\')
                ++backslashes;
            else
                backslashes = 0;
            cur += ch;
        }
    }
    parts << cur;

    if (!parts.isEmpty() && parts.first().isEmpty())
        parts.removeFirst();
    if (!parts.isEmpty() && parts.last().isEmpty())
        parts.removeLast();

    QStringList cells;
    cells.reserve(parts.size());
    for (const QString &p : parts)
        cells << p.trimmed();
    return cells;
}

int mdRowFirstCellPos(const QString &line)
{
    const MdRowStyle style = mdRowStyle(line);
    if (style.hasLeadingPipe) {
        int p = -1;
        for (int i = 0; i < line.size(); ++i) {
            if (line[i] == '|' && !pipeIsEscaped(line, i)) { p = i; break; }
        }
        ++p;
        if (p < line.size() && line[p] == ' ')
            ++p;
        return p;
    }
    int p = 0;
    while (p < line.size() && line[p].isSpace())
        ++p;
    // Borderless first cell made only of spaces (`    |    `): there is no
    // content to land on, so start at the row's own start — inside cell 1 —
    // instead of walking onto the separator pipe.
    if (p < line.size() && line[p] == '|')
        return 0;
    return p;
}

int mdRowLastCellPos(const QString &line)
{
    const QList<int> pipes = mdPipeIndexes(line);
    if (pipes.isEmpty())
        return 0;
    const MdRowStyle style = mdRowStyle(line);
    int pos;
    if (style.hasTrailingPipe) {
        if (pipes.size() < 2)
            return 0; // a lone trailing `|` leaves nothing to the left
        pos = pipes[pipes.size() - 2] + 1;
    } else {
        pos = pipes.last() + 1;
    }
    if (pos < line.size() && line[pos] == ' ')
        ++pos;
    return pos;
}

int mdRowColumnAt(const QString &line, int blockPos)
{
    const QStringList cells = splitMdTableRow(line);
    if (cells.isEmpty())
        return 0;
    const QList<int> pipes = mdPipeIndexes(line);
    const MdRowStyle style = mdRowStyle(line);
    int before = 0;
    for (int i = 0; i < pipes.size() && pipes[i] < blockPos; ++i)
        ++before;
    int col;
    if (style.hasLeadingPipe)
        col = (before > 0) ? before - 1 : 0; // the leading pipe is not a cell border
    else
        col = before;                        // borderless: a pipe before the care means a column before it
    return qBound(0, col, cells.size() - 1);
}

QString makeEmptyTableRow(int cols, const MdRowStyle &style, int padding)
{
    if (cols <= 0)
        return {};
    // Fully bordered rows keep the historic tight layout: cells of two spaces
    // with a pipe on each side (`|  |  |`), scaled by the padding preference.
    if (style.hasLeadingPipe && style.hasTrailingPipe)
        return "|" + (QString(2 * padding, ' ') + "|").repeated(cols);
    QStringList cells;
    cells.reserve(cols);
    for (int c = 0; c < cols; ++c)
        cells << QString(2 * padding, ' ');
    QString row = cells.join(QString(padding, ' ') + "|" + QString(padding, ' '));
    if (style.hasLeadingPipe) {
        row.prepend('|');
    } else {
        // Borderless rows have no leading pipe to anchor column zero; keep
        // their leading padding under md4c's four-space indented-code limit.
        row = capTableRowIndent(row);
    }
    if (style.hasTrailingPipe)
        row.append('|');
    return row;
}

QString makeTableSeparatorRow(int cols, const MdRowStyle &style, int padding)
{
    if (cols <= 0)
        return {};
    const QString dashes = QString(qMax(2 * padding + 1, 3), '-');
    if (style.hasLeadingPipe && style.hasTrailingPipe)
        return "|" + (dashes + "|").repeated(cols);
    QStringList cells;
    cells.reserve(cols);
    for (int c = 0; c < cols; ++c)
        cells << dashes;
    QString row = cells.join(QString(padding, ' ') + "|" + QString(padding, ' '));
    if (style.hasLeadingPipe)
        row.prepend('|');
    if (style.hasTrailingPipe)
        row.append('|');
    return row;
}

QString makeEmptyTableRow(int cols)
{
    return makeEmptyTableRow(cols, MdRowStyle{true, true});
}

QString makeEmptyHtmlTableRow(int cols)
{
    return "<tr>" + QString("<td></td>").repeated(cols) + "</tr>";
}

// A markdown table separator row: pipes where every cell is made only of
// optional colons and at least one dash (spaces allowed around it). This also
// matches narrow columns such as `|:--:|` or `|--:|` that a wide separator
// would not — the spec's three-dash minimum is not a real constraint here
// because the formatter itself can emit shorter separators for 1-2 char cells.
// The leading/trailing `|` GFM makes optional are allowed to be absent, so a
// borderless `--- | ---` separator counts too. A bare `---` (thematic break or
// setext underline) is not a separator: that line has no pipe at all.
bool isMdSeparatorRow(const QString &line)
{
    if (!isMdTableLikeRow(line))
        return false;
    const QStringList cells = splitMdTableRow(line);
    if (cells.isEmpty())
        return false;
    static const QRegularExpression sepCellRe(R"(^:?-+:?$)");
    for (const QString &cell : cells) {
        if (!sepCellRe.match(cell).hasMatch())
            return false;
    }
    return true;
}

bool isBlankMdTableRow(const QString &line)
{
    if (!isMdTableLikeRow(line))
        return false;
    if (isMdSeparatorRow(line))
        return false;
    const QStringList cells = splitMdTableRow(line);
    if (cells.isEmpty())
        return false;
    for (const QString &cell : cells) {
        if (!cell.trimmed().isEmpty())
            return false;
    }
    return true;
}

/* Returns block-relative cursor position of next/previous cell in a markdown table row.
   - forward=true:  find the next cell after cursorPos
   - forward=false: find the previous cell before cursorPos
   Returns -1 if at first cell (going backward) or last cell (going forward),
   signaling the caller should navigate to another row. Handles both bordered
   (`| a | b |`) and borderless (`a | b`) rows: pipes delimit cells in both,
   and a borderless row's first cell starts at the line's content start. */
int tableNavCell(const QString &line, int cursorPos, bool forward)
{
    const QList<int> pipes = mdPipeIndexes(line);
    if (pipes.isEmpty())
        return -1;

    const MdRowStyle style = mdRowStyle(line);

    if (forward) {
        int idx = -1;
        for (int i = 0; i < pipes.size(); ++i) {
            if (pipes[i] > cursorPos) { idx = i; break; }
        }
        if (idx == -1)
            return -1;  // past the last pipe: in the last cell
        if (style.hasTrailingPipe && idx >= pipes.size() - 1)
            return -1;  // next pipe is the trailing border: last cell
        int cellPos = pipes[idx] + 1;
        if (cellPos < line.size() && line[cellPos] == ' ')
            ++cellPos;
        return cellPos;
    }

    int idx = -1;
    for (int i = pipes.size() - 1; i >= 0; --i) {
        if (pipes[i] < cursorPos) { idx = i; break; }
    }
    if (idx == -1)
        return -1;  // before every pipe: first cell
    if (idx == 0) {
        if (style.hasLeadingPipe)
            return -1;  // inside the first cell
        return mdRowFirstCellPos(line);  // pipe after the first cell → back to it
    }
    int cellPos = pipes[idx - 1] + 1;
    if (cellPos < line.size() && line[cellPos] == ' ')
        ++cellPos;
    return cellPos;
}

int tableNavHtmlCell(const QString &line, int cursorPos, bool forward)
{
    QList<int> tdStarts;
    int idx = 0;
    while ((idx = line.indexOf("<td>", idx)) >= 0) {
        tdStarts.append(idx);
        idx += 4;
    }
    if (tdStarts.isEmpty())
        return -1;

    if (forward) {
        for (int i = 0; i < tdStarts.size(); ++i) {
            if (tdStarts[i] + 4 > cursorPos) {
                int contentStart = tdStarts[i] + 4;
                return contentStart;
            }
        }
        return -1;
    }

    for (int i = tdStarts.size() - 1; i >= 0; --i) {
        if (tdStarts[i] + 4 < cursorPos) {
            int contentStart = tdStarts[i] + 4;
            return contentStart;
        }
    }
    return -1;
}

QString formatMdTable(const QStringList &rows, int padding)
{
    // The separator row (the one containing dashes) defines the column count,
    // per-column alignment, and border style. Without one this is not a
    // markdown table.
    int sepIndex = -1;
    for (int i = 0; i < rows.size(); ++i) {
        if (isMdSeparatorRow(rows[i])) {
            sepIndex = i;
            break;
        }
    }
    if (sepIndex < 0)
        return {};

    const MdRowStyle style = mdRowStyle(rows[sepIndex]);
    const bool bordered = style.hasLeadingPipe && style.hasTrailingPipe;

    QList<QStringList> cellRows;
    cellRows.reserve(rows.size());
    int maxCols = 0;
    for (const QString &row : rows) {
        QStringList cells = splitMdTableRow(row);
        if (cells.size() > maxCols)
            maxCols = cells.size();
        cellRows << cells;
    }
    if (maxCols <= 0)
        return {};

    const QStringList &sepCells = cellRows[sepIndex];

    QVector<MdAlign> align(maxCols, MdAlign::Default);
    for (int c = 0; c < maxCols; ++c) {
        if (c < sepCells.size())
            align[c] = mdAlignFromSeparator(sepCells[c]);
    }

    QVector<int> width(maxCols, 0);
    for (int r = 0; r < cellRows.size(); ++r) {
        if (r == sepIndex)
            continue; // the separator's dashes must not drive column widths
        const QStringList &cells = cellRows[r];
        for (int c = 0; c < maxCols; ++c) {
            const int len = c < cells.size() ? cells[c].size() : 0;
            if (len > width[c])
                width[c] = len;
        }
    }
    for (int c = 0; c < maxCols; ++c)
        width[c] = qMax(width[c], 1);

    // Column body width including padding, floored at three chars so every
    // separator cell keeps at least one dash (`---`, `:--`, `--:`, `:-:`) and
    // data rows stay pipe-aligned with it even at padding 0.
    QVector<int> bodyWidth(maxCols);
    for (int c = 0; c < maxCols; ++c)
        bodyWidth[c] = qMax(width[c] + 2 * padding, 3);

    const QString pipeSpacing = QString(padding, ' ') + "|" + QString(padding, ' ');

    // A borderless row has no leading pipe to anchor column zero, so a
    // right/center-aligned first cell (or an empty default-aligned one) can
    // push its leading padding past md4c's four-space indented-code limit;
    // capTableRowIndent would then shift that row's first pipe off the aligned
    // column. When that would happen, upgrade the whole table to bordered so
    // the pipes line up and the table keeps rendering (see docs/gotchas.md).
    bool useBordered = bordered;
    if (!useBordered) {
        for (int r = 0; r < cellRows.size(); ++r) {
            if (r == sepIndex)
                continue;
            const QStringList &cells = cellRows[r];
            const QString content = cells.isEmpty() ? QString() : cells[0];
            const QString cell0 = padMdCell(content, width[0], align[0]);
            int lead = 0;
            while (lead < cell0.size() && cell0[lead] == ' ')
                ++lead;
            if (lead > 3) {
                useBordered = true;
                break;
            }
        }
    }

    QStringList out;
    out.reserve(rows.size());
    for (int r = 0; r < cellRows.size(); ++r) {
        if (useBordered) {
            // Existing behaviour: pipes on both sides of every row.
            if (r == sepIndex) {
                QString line = "|";
                for (int c = 0; c < maxCols; ++c)
                    line += mdSeparatorCell(align[c], bodyWidth[c], 0) + "|";
                out << line;
            } else {
                const QStringList &cells = cellRows[r];
                QString line = "|";
                for (int c = 0; c < maxCols; ++c) {
                    const QString content = c < cells.size() ? cells[c] : QString();
                    line += QString(padding, ' ')
                            + padMdCell(content, bodyWidth[c] - 2 * padding, align[c])
                            + QString(padding, ' ') + "|";
                }
                out << line;
            }
        } else {
            // Borderless: preserve the separator row's bare style — no leading
            // or trailing pipes, cells joined by ` | `.
            if (r == sepIndex) {
                QStringList cells;
                cells.reserve(maxCols);
                for (int c = 0; c < maxCols; ++c)
                    cells << bareSeparator(align[c], width[c]);
                out << cells.join(pipeSpacing);
            } else {
                const QStringList &cells = cellRows[r];
                QStringList lineCells;
                lineCells.reserve(maxCols);
                for (int c = 0; c < maxCols; ++c) {
                    const QString content = c < cells.size() ? cells[c] : QString();
                    lineCells << padMdCell(content, width[c], align[c]);
                }
                out << capTableRowIndent(lineCells.join(pipeSpacing));
            }
        }
    }
    return out.join('\n');
}

} // namespace MdTable

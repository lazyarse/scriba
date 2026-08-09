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
#include "StaticHelpers.h"
#include "Preferences.h"
#include <algorithm>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QAbstractButton>
#include <QPair>
#include <QVector>
#include <QSet>
#include <QSettings>
#include <QFont>
#include <QRect>

QString escapeJsString(const QString &s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("'", "\\'").replace("\"", "\\\"").replace("`", "\\`").replace("\n", "\\n").replace("\r", "\\r");
    return r;
}

bool isSafePreviewImage(const QString &filePath)
{
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile())
        return false;
    static const QStringList imageSuffixes{
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
        QStringLiteral("svg"), QStringLiteral("avif"), QStringLiteral("ico"),
        QStringLiteral("tif"), QStringLiteral("tiff"),
    };
    return imageSuffixes.contains(fi.suffix().toLower());
}

static const QRegularExpression &unorderedListRe()
{
    static QRegularExpression re(R"(^(\s*)([-*+])(?=\s|$))");
    return re;
}

static const QRegularExpression &orderedListRe()
{
    static QRegularExpression re(R"(^(\s*)(\d+)([.)])(?=\s|$))");
    return re;
}

bool isThematicBreak(const QString &line)
{
    static QRegularExpression re(R"(^\s*([-*_])(?:\s*\1){2,}\s*$)");
    return re.match(line).hasMatch();
}

// Returns the list-marker continuation prefix for a markdown list item line
// (e.g. "  - ", "2. ", "- [ ] "), or an empty QString when `line` isn't a list
// item or is a thematic break. Ordered lists are incremented; task checkboxes
// are reset to unchecked.
static QString listMarkerPrefix(const QString &line)
{
    if (isThematicBreak(line))
        return {};
    static QRegularExpression taskRe(R"(^(\s*)([-*+])\s+\[[ xX]\]\s?)");
    auto taskMatch = taskRe.match(line);
    if (taskMatch.hasMatch())
        return taskMatch.captured(1) + taskMatch.captured(2) + " [ ] ";
    auto match = unorderedListRe().match(line);
    if (match.hasMatch())
        return match.captured(1) + match.captured(2) + " ";
    match = orderedListRe().match(line);
    if (match.hasMatch())
        return match.captured(1) + QString::number(match.captured(2).toInt() + 1)
               + match.captured(3) + " ";
    return {};
}

QString handleListReturn(const QString &line)
{
    const QString prefix = listMarkerPrefix(line);
    if (prefix.isEmpty())
        return {};
    if (line.mid(prefix.length()).trimmed().isEmpty())
        return QString(clearSentinel);
    return prefix;
}

QString handleListSplitReturn(const QString &line, int caretPos)
{
    const QString prefix = listMarkerPrefix(line);
    if (prefix.isEmpty())
        return {};
    if (caretPos < prefix.length())
        return {};
    if (line.mid(caretPos).trimmed().isEmpty())
        return {};
    return prefix;
}

static int listIndentWidth(const QString &line)
{
    int spaces = 0;
    for (QChar c : line) {
        if (c == ' ') ++spaces;
        else break;
    }
    return spaces;
}

// How many spaces Tab adds / Shift+Tab removes when indenting a list item,
// matching the item's content column so the result nests correctly under
// CommonMark: 2 for bullets ("- ", "+ ", "* "), digits+delimiter+1 for
// ordered ("1." → 3, "10." → 4, "1)" → 3), 0 for non-list lines.
static int listIndentStep(const QString &line)
{
    auto match = unorderedListRe().match(line);
    if (match.hasMatch())
        return 2;
    match = orderedListRe().match(line);
    if (match.hasMatch())
        return match.captured(2).length() + match.captured(3).length() + 1;
    return 0;
}

QString handleTableReturn(const QString &line, const QString &prevLine)
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
            return makeEmptyTableRow(cols, style);

        // First row of a new table → separator + data row
        return makeTableSeparatorRow(cols, style) + "\n" + makeEmptyTableRow(cols, style);
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

// An unescaped pipe is one not preceded by an odd number of backslashes; such
// `\|` is cell content, not a column separator.
static bool pipeIsEscaped(const QString &line, int idx)
{
    int backslashes = 0;
    for (int i = idx - 1; i >= 0 && line[i] == '\\'; --i)
        ++backslashes;
    return (backslashes % 2) == 1;
}

// Indexes of the unescaped `|` characters in `line`.
static QList<int> mdPipeIndexes(const QString &line)
{
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|' && !pipeIsEscaped(line, i))
            pipes.append(i);
    return pipes;
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

QString makeEmptyTableRow(int cols, const MdRowStyle &style)
{
    if (cols <= 0)
        return {};
    // Fully bordered rows keep the historic tight layout: cells of two spaces
    // with a pipe on each side (`|  |  |`).
    if (style.hasLeadingPipe && style.hasTrailingPipe)
        return "|" + QString("  |").repeated(cols);
    QStringList cells;
    cells.reserve(cols);
    for (int c = 0; c < cols; ++c)
        cells << "  ";
    QString row = cells.join(" | ");
    if (style.hasLeadingPipe)
        row.prepend('|');
    if (style.hasTrailingPipe)
        row.append('|');
    return row;
}

QString makeTableSeparatorRow(int cols, const MdRowStyle &style)
{
    if (cols <= 0)
        return {};
    if (style.hasLeadingPipe && style.hasTrailingPipe)
        return "|" + QString("---|").repeated(cols);
    QStringList cells;
    cells.reserve(cols);
    for (int c = 0; c < cols; ++c)
        cells << "---";
    QString row = cells.join(" | ");
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

QString makeEmptyHtmlTableRow(int cols)
{
    return "<tr>" + QString("<td></td>").repeated(cols) + "</tr>";
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
            int contentStart = tdStarts[i] + 4;
            if (contentStart > cursorPos)
                return contentStart;
        }
        return -1;
    } else {
        for (int i = tdStarts.size() - 1; i >= 0; --i) {
            int contentStart = tdStarts[i] + 4;
            if (contentStart < cursorPos)
                return contentStart;
        }
        return -1;
    }
}

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

QString mdSeparatorCell(MdAlign align, int width)
{
    switch (align) {
    case MdAlign::Left:    return ':' + QString(width + 1, '-');
    case MdAlign::Right:   return QString(width + 1, '-') + ':';
    case MdAlign::Center:  return ':' + QString(width, '-') + ':';
    case MdAlign::Default: return QString(width + 2, '-');
    }
    return QString(width + 2, '-');
}

// Separator cell for a borderless table: dashes spanning `width` (at least
// three) with the alignment colons, so aligning a borderless table never
// introduces leading/trailing pipes.
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

} // namespace

QString formatMdTable(const QStringList &rows)
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

    QStringList out;
    out.reserve(rows.size());
    for (int r = 0; r < cellRows.size(); ++r) {
        if (bordered) {
            // Existing behaviour: pipes on both sides of every row.
            if (r == sepIndex) {
                QString line = "|";
                for (int c = 0; c < maxCols; ++c)
                    line += mdSeparatorCell(align[c], width[c]) + "|";
                out << line;
            } else {
                const QStringList &cells = cellRows[r];
                QString line = "|";
                for (int c = 0; c < maxCols; ++c) {
                    const QString content = c < cells.size() ? cells[c] : QString();
                    line += " " + padMdCell(content, width[c], align[c]) + " |";
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
                out << cells.join(" | ");
            } else {
                const QStringList &cells = cellRows[r];
                QStringList lineCells;
                lineCells.reserve(maxCols);
                for (int c = 0; c < maxCols; ++c) {
                    const QString content = c < cells.size() ? cells[c] : QString();
                    lineCells << padMdCell(content, width[c], align[c]);
                }
                out << lineCells.join(" | ");
            }
        }
    }
    return out.join('\n');
}

QString indentListLine(const QString &line)
{
    if (isThematicBreak(line))
        return line;
    const int step = listIndentStep(line);
    if (step == 0)
        return line;
    int indent = listIndentWidth(line);
    return line.left(indent) + QString(step, ' ') + line.mid(indent);
}

QString outdentListLine(const QString &line)
{
    if (isThematicBreak(line))
        return line;
    const int step = listIndentStep(line);
    if (step == 0)
        return line;
    int indent = listIndentWidth(line);
    int remove = qMin(indent, step);
    return line.mid(remove);
}

QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column)
{
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    return cursor;
}

QIcon themedIcon(const QString &svgPath, const QColor &color, int size)
{
    QFile f(svgPath);
    if (!f.open(QIODevice::ReadOnly))
        return QIcon(svgPath);
    QString svg = QString::fromUtf8(f.readAll());
    svg.replace("currentColor", color.name());
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    renderer.render(&painter);
    painter.end();
    return QIcon(pix);
}

void stripButtonIcon(QAbstractButton *btn)
{
    btn->setIcon(QIcon());
}

void stripButtonIcons(QDialogButtonBox *box)
{
    for (auto *btn : box->buttons())
        stripButtonIcon(btn);
}

QString readResourceFile(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return {};
}

QList<EmojiEntry> emojiCatalog()
{
    static QList<EmojiEntry> catalog;
    if (!catalog.isEmpty())
        return catalog;

    static const QRegularExpression re(R"('([a-z0-9_+\-]+)'\s*:\s*'([^']+)')");
    auto it = re.globalMatch(readResourceFile(":/emoji.js"));
    QSet<QString> seen;
    while (it.hasNext()) {
        auto match = it.next();
        EmojiEntry entry;
        entry.shortcode = match.captured(1);
        entry.unicode = match.captured(2);

        QStringList parts;
        for (uint cp : entry.unicode.toUcs4())
            parts.append(QString::number(cp, 16));
        QStringList stripped = parts;
        stripped.removeAll("fe0f");
        QString strippedStr = stripped.join("-");
        entry.codePoint = QFile::exists(QString(":/twemoji/svg/%1.svg").arg(strippedStr))
            ? strippedStr : parts.join("-");

        if (seen.contains(entry.shortcode))
            continue;
        seen.insert(entry.shortcode);
        catalog.append(entry);
    }
    std::sort(catalog.begin(), catalog.end(),
              [](const EmojiEntry &a, const EmojiEntry &b) { return a.shortcode < b.shortcode; });
    return catalog;
}

QString emojiTwemojiPath(const QString &unicode)
{
    QStringList parts;
    for (uint cp : unicode.toUcs4())
        parts.append(QString::number(cp, 16));
    QStringList stripped = parts;
    stripped.removeAll("fe0f");
    for (const QString &candidate : {stripped.join("-"), parts.join("-")}) {
        QString path = QString(":/twemoji/svg/%1.svg").arg(candidate);
        if (QFile::exists(path))
            return path;
    }
    return {};
}

QPixmap renderEmojiPixmap(const QString &unicode, int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    bool color = Preferences::emojiRenderingFromString(
        QSettings().value(Preferences::EmojiMode,
                          Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString())
        == Preferences::EmojiRendering::Color;

    if (color) {
        QString svgPath = emojiTwemojiPath(unicode);
        if (!svgPath.isEmpty()) {
            QSvgRenderer renderer(svgPath);
            renderer.render(&painter, QRectF(0, 0, size, size));
        } else {
            QFont font("Symbola");
            font.setPixelSize(size - 2);
            painter.setFont(font);
            painter.setPen(Qt::black);
            painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, unicode);
        }
    } else {
        QFont font("Symbola");
        font.setPixelSize(size - 2);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, unicode);
    }

    return pix;
}

QString duplicateCssFile(const QString &sourcePath, const QString &destDir, const QString &baseName)
{
    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString css = QString::fromUtf8(src.readAll());
    if (css.isEmpty())
        return {};

    QDir().mkpath(destDir);
    QString name = baseName.trimmed();
    if (name.isEmpty())
        name = QFileInfo(sourcePath).completeBaseName() + "-copy";
    name.replace(QRegularExpression("[\\\\/:*?\"<>|\\x00-\\x1f]"), "-");
    if (!name.endsWith(".css", Qt::CaseInsensitive))
        name += ".css";

    QString candidate = destDir + "/" + name;
    if (QFileInfo::exists(candidate))
        return {};

    QFile out(candidate);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    out.write(css.toUtf8());
    return candidate;
}

DebounceTimer::DebounceTimer(int interval, QObject *parent)
    : QTimer(parent)
{
    setSingleShot(true);
    setInterval(interval);
}

void DebounceTimer::arm()
{
    start();
}

bool extractLinkPath(const QString &line, int cursorPos, QString &partialPath)
{
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(cursorPos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return false;

    QString between = line.mid(parenPos, cursorPos - parenPos);
    if (between.contains(')'))
        return false;

    partialPath = between;
    return true;
}

int linkPathReplaceStart(const QString &line, int cursorPos)
{
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(cursorPos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return -1;

    QString between = line.mid(parenPos, cursorPos - parenPos);
    int lastSlash = between.lastIndexOf('/');
    return lastSlash >= 0 ? parenPos + lastSlash + 1 : parenPos;
}

bool extractHtmlPath(const QString &line, int cursorPos, QString &value)
{
    static const QRegularExpression re(R"((?:src|href)\s*=\s*["']([^"']*)$)");
    auto match = re.match(line.left(cursorPos));
    if (!match.hasMatch())
        return false;

    value = match.captured(1);
    return true;
}

int htmlPathReplaceStart(const QString &line, int cursorPos)
{
    static const QRegularExpression re(R"((?:src|href)\s*=\s*["']([^"']*)$)");
    auto match = re.match(line.left(cursorPos));
    if (!match.hasMatch())
        return -1;

    QString value = match.captured(1);
    int valueStart = cursorPos - value.length();
    int lastSlash = value.lastIndexOf('/');
    return lastSlash >= 0 ? valueStart + lastSlash + 1 : valueStart;
}

bool extractEmojiCode(const QString &line, int cursorPos, QString &partialCode)
{
    // Inside an open link path the file completion wins, not emoji
    QString ignored;
    if (extractLinkPath(line, cursorPos, ignored))
        return false;

    static const QRegularExpression re(R"(:([a-zA-Z0-9+-][a-zA-Z0-9_+-]*)$)");
    auto match = re.match(line.left(cursorPos));
    if (!match.hasMatch())
        return false;

    partialCode = match.captured(1);
    return true;
}

QString autoCorrectWord(const QString &line, int cursorPos, const QStringList &pairs,
                        bool separatorTyped, int *wordStart, int *wordLength)
{
    if (wordStart)
        *wordStart = -1;
    if (wordLength)
        *wordLength = 0;
    if (pairs.isEmpty() || cursorPos <= 0 || cursorPos > line.size())
        return {};

    // A word (apostrophes/hyphens included), optionally followed by the
    // separator(s) just typed. When separatorTyped, at least one trailing
    // non-letter must be present so an in-progress word is never corrected.
    static const QRegularExpression wordRe(R"(([A-Za-z]+(?:['-][A-Za-z]+)*)([^A-Za-z]*)$)");
    auto match = wordRe.match(line.left(cursorPos));
    if (!match.hasMatch())
        return {};
    const QString typed = match.captured(1);
    const QString trailing = match.captured(2);
    if (separatorTyped && trailing.isEmpty())
        return {};

    const int start = match.capturedStart(1);
    if (start > 0) {
        const QChar prev = line[start - 1];
        // Must not be glued to a larger token: letters/digits, or characters
        // that appear inside URLs, emails, paths, identifiers or code spans.
        if (prev.isLetterOrNumber() || prev == '@' || prev == '.'
            || prev == '/' || prev == '_' || prev == '-' || prev == '`' || prev == '\\')
            return {};
    }

    QString replacement;
    const QString lower = typed.toLower();
    for (const QString &entry : pairs) {
        const int eq = entry.indexOf('=');
        if (eq <= 0 || eq >= entry.size() - 1)
            continue;
        if (entry.left(eq).toLower() != lower)
            continue;
        replacement = entry.mid(eq + 1);
        break;
    }
    if (replacement.isEmpty() || replacement == typed)
        return {};

    // Preserve the case of what was typed.
    bool allUpper = typed.size() > 1;
    for (QChar c : typed) {
        if (c.isLower()) {
            allUpper = false;
            break;
        }
    }
    if (allUpper)
        replacement = replacement.toUpper();
    else if (typed[0].isUpper())
        replacement[0] = replacement[0].toUpper();

    if (wordStart)
        *wordStart = start;
    if (wordLength)
        *wordLength = typed.size();
    return replacement;
}

// Sequential (fuzzy) match: every character of the fragment appears in the
// entry in order, possibly with skipped characters in between. Subsumes plain
// substring containment, so "scrsvg" matches "scriba.svg". The score ranks
// matches: fewer skipped characters and an earlier first-match position make a
// match feel tighter (a prefix match scores 0/0 and always ranks first).
FuzzyScore fuzzyMatchScore(const QString &entry, const QString &fragment)
{
    if (fragment.isEmpty())
        return {true, 0, 0};
    int f = 0;
    int prev = -1;
    int gaps = 0;
    int first = -1;
    for (int i = 0; i < entry.size() && f < fragment.size(); ++i) {
        if (entry[i].toLower() == fragment[f].toLower()) {
            if (prev >= 0)
                gaps += i - prev - 1;
            else
                first = i;
            prev = i;
            ++f;
        }
    }
    if (f < fragment.size())
        return {false, 0, 0};
    return {true, gaps, first};
}

FileCompletionResult matchFileEntries(const QString &partialPath, const QDir &baseDir,
                                      int limit)
{
    QString normalized = partialPath;
    bool trailingSep = normalized.endsWith('/') || normalized.endsWith('\\');
    if (trailingSep)
        normalized.chop(1);
    QFileInfo pfi(normalized);
    QString dirPart = trailingSep ? pfi.filePath() : pfi.path();
    QString filePart = trailingSep ? QString() : pfi.fileName();

    QString searchDir = baseDir.absoluteFilePath(dirPart.isEmpty() ? "." : dirPart);
    QDir search(searchDir);
    if (!search.exists())
        return {};

    QVector<QPair<QString, FuzzyScore>> scored;
    QStringList all = search.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const QString &entry : all) {
        if (entry.startsWith('.') && !filePart.startsWith('.'))
            continue;
        FuzzyScore score = fuzzyMatchScore(entry, filePart);
        if (score.matched)
            scored.append({entry, score});
    }

    std::sort(scored.begin(), scored.end(),
        [](const QPair<QString, FuzzyScore> &a, const QPair<QString, FuzzyScore> &b) {
            if (a.second.gaps != b.second.gaps)
                return a.second.gaps < b.second.gaps;
            if (a.second.firstPos != b.second.firstPos)
                return a.second.firstPos < b.second.firstPos;
            return a.first < b.first;
        });

    QStringList entries;
    for (const auto &match : scored) {
        if (entries.size() >= limit)
            break;
        const QString &entry = match.first;
        if (QFileInfo(search, entry).isDir())
            entries.append(entry + "/");
        else
            entries.append(entry);
    }

    return {entries, filePart};
}



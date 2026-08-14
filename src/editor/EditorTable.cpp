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
#include "Editor.h"
#include "MdTable.h"
#include "prefs/Preferences.h"
#include <QRegularExpression>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

void Editor::onCursorPositionChanged()
{
    if (m_formattingTable)
        return;

    if (!QSettings().value(Preferences::AutoAlignTables, true).toBool())
        return;

    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        return;

    const QString line = cursor.block().text();
    const bool inTable = MdTable::isMdTableLikeRow(line);

    if (inTable) {
        if (m_trackTableStartBlock < 0) {
            // Entering a table: record its first block so edits while the
            // cursor is inside can be tracked and the table reformatted once
            // the cursor leaves.
            QTextBlock block = cursor.block();
            while (block.previous().isValid() && MdTable::isMdTableLikeRow(block.previous().text()))
                block = block.previous();
            m_trackTableStartBlock = block.blockNumber();
            m_tableDirty = false;
        }
        return;
    }

    if (m_trackTableStartBlock >= 0) {
        const int start = m_trackTableStartBlock;
        const bool dirty = m_tableDirty;
        m_trackTableStartBlock = -1;
        m_tableDirty = false;
        if (dirty)
            formatMdTableBlock(start);
    }
}

void Editor::formatTableAt(int documentPos)
{
    if (!QSettings().value(Preferences::AutoAlignTables, true).toBool())
        return;
    QTextBlock block = document()->findBlock(documentPos);
    if (block.isValid() && MdTable::isMdTableLikeRow(block.text()))
        formatMdTableBlock(block.blockNumber());
}

// Reads the configured table cell padding (Editor → Tables → Cell padding),
// clamped to the spin box's 1..4 range.
int Editor::tablePadding() const
{
    return qBound(1, QSettings().value(Preferences::TablePadding,
                                       Preferences::DefaultTablePadding).toInt(), 4);
}

void Editor::formatMdTableBlock(int startBlock)
{
    QTextDocument *doc = document();
    QTextBlock block = doc->findBlockByNumber(startBlock);
    if (!block.isValid() || !MdTable::isMdTableLikeRow(block.text()))
        return;

    QTextBlock first = block;
    while (first.previous().isValid() && MdTable::isMdTableLikeRow(first.previous().text()))
        first = first.previous();
    QTextBlock last = block;
    while (last.next().isValid() && MdTable::isMdTableLikeRow(last.next().text()))
        last = last.next();

    QStringList rows;
    for (QTextBlock b = first; ; b = b.next()) {
        rows << b.text();
        if (b == last)
            break;
    }

    // Only real markdown tables (those with a separator row) are aligned. Use
    // the canonical separator test rather than a `---` substring: isMdSeparatorRow
    // also accepts narrow separators such as `--|--`, `|:--:|` or `--:` (see
    // MdTable.cpp), which a `contains("---")` check would silently skip.
    bool hasSeparator = false;
    for (const QString &row : rows) {
        if (MdTable::isMdSeparatorRow(row)) {
            hasSeparator = true;
            break;
        }
    }
    if (!hasSeparator)
        return;

    const QString formatted = MdTable::formatMdTable(rows, tablePadding());
    if (formatted.isEmpty() || formatted == rows.join('\n'))
        return; // not a table, or already aligned — nothing to do

    // Preserve the cursor: block numbers don't change (row count is stable),
    // so restore the same block with the column clamped to the new width.
    const int cursorBlock = textCursor().blockNumber();
    const int cursorCol = textCursor().positionInBlock();

    m_formattingTable = true;
    QTextCursor tc(doc);
    tc.beginEditBlock();
    const int startPos = first.position();
    const int endPos = last.position() + last.length() - 1;
    tc.setPosition(startPos);
    tc.setPosition(endPos, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
    tc.insertText(formatted);
    tc.endEditBlock();
    m_formattingTable = false;

    QTextBlock newBlock = doc->findBlockByNumber(cursorBlock);
    if (newBlock.isValid()) {
        QTextCursor rc(doc);
        rc.setPosition(newBlock.position() + qMin(cursorCol, static_cast<int>(newBlock.length()) - 1));
        setTextCursor(rc);
    }
}

Editor::CursorContext Editor::detectCursorContext() const
{
    QString line = currentLineText();

    static const QRegularExpression listRe(R"(^\s*[-*+]\s|^\s*\d+[.)]\s)");
    if (listRe.match(line).hasMatch())
        return CursorContext::ListItem;

    if (MdTable::isMdTableLikeRow(line) && !MdTable::isMdSeparatorRow(line))
        return CursorContext::TableRow;

    if (isCursorInFencedCodeBlock())
        return CursorContext::CodeBlock;

    return CursorContext::None;
}

bool Editor::isCursorInFencedCodeBlock() const
{
    QTextCursor cursor = textCursor();
    int blockNum = cursor.blockNumber();
    int fenceCount = 0;
    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() <= blockNum) {
        QString text = block.text().trimmed();
        if (text.startsWith("```"))
            ++fenceCount;
        block = block.next();
    }
    return (fenceCount % 2) == 1;
}

void Editor::insertTableRow(bool above)
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!MdTable::isMdTableLikeRow(line))
        return;

    const MdTable::MdRowStyle style = MdTable::mdRowStyle(line);
    const int cols = MdTable::splitMdTableRow(line).size();
    if (cols <= 0)
        return;

    QString newRow = MdTable::makeEmptyTableRow(cols, style, tablePadding());
    if (!newRow.endsWith('\n'))
        newRow += '\n';

    if (above) {
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.insertText(newRow);
    } else {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        cursor.insertText("\n" + newRow);
    }
}

// Applies `transform` to every row of the table block containing `block`:
// consecutive lines that look like markdown table rows (any border style). The
// caret's current row determines the contiguous extent scanned above and below.
template <typename Fn>
static void forEachTableRow(QTextDocument *doc, const QTextBlock &block, Fn &&transform)
{
    QTextBlock b = block;
    while (b.previous().isValid() && MdTable::isMdTableLikeRow(b.previous().text()))
        b = b.previous();
    while (b.isValid() && MdTable::isMdTableLikeRow(b.text())) {
        QStringList cells = MdTable::splitMdTableRow(b.text());
        const bool separator = MdTable::isMdSeparatorRow(b.text());
        const MdTable::MdRowStyle style = MdTable::mdRowStyle(b.text());
        QString rebuilt = transform(cells, separator);
        if (!rebuilt.isNull()) {
            if (style.hasLeadingPipe)
                rebuilt.prepend('|');
            if (style.hasTrailingPipe)
                rebuilt.append('|');
            QTextCursor tc(doc);
            tc.setPosition(b.position());
            tc.setPosition(b.position() + b.length() - 1, QTextCursor::KeepAnchor);
            tc.removeSelectedText();
            tc.insertText(rebuilt);
        }
        b = doc->findBlockByNumber(b.blockNumber() + 1);
    }
}

void Editor::insertTableCol(bool left)
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!MdTable::isMdTableLikeRow(line))
        return;

    const QStringList cells = MdTable::splitMdTableRow(line);
    if (cells.isEmpty())
        return;
    const int insertIdx =
        qMin(left ? MdTable::mdRowColumnAt(line, cursor.positionInBlock())
                  : MdTable::mdRowColumnAt(line, cursor.positionInBlock()) + 1,
             cells.size());

    forEachTableRow(document(), cursor.block(), [insertIdx](QStringList cells, bool separator) {
        cells.insert(qMin(insertIdx, static_cast<int>(cells.size())),
                     separator ? QStringLiteral("---") : QString());
        return cells.join(" | ");
    });
}

void Editor::deleteTableRow()
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!MdTable::isMdTableLikeRow(line))
        return;

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    if (MdTable::isMdSeparatorRow(line)) {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else {
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
    setTextCursor(cursor);
}

void Editor::deleteTableCol()
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!MdTable::isMdTableLikeRow(line))
        return;

    // Refuse to delete the only column of the caret row's column count.
    if (MdTable::splitMdTableRow(line).size() <= 1)
        return;

    const int colIdx = MdTable::mdRowColumnAt(line, cursor.positionInBlock());

    forEachTableRow(document(), cursor.block(),
                    [colIdx](QStringList cells, bool) {
        if (colIdx >= cells.size())
            return QString(); // rows with fewer columns are left untouched
        cells.removeAt(colIdx);
        if (cells.isEmpty())
            return QString();
        return cells.join(" | ");
    });
}

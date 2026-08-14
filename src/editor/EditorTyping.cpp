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
#include "StaticHelpers.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

void Editor::insertParagraphWithLineHeight(QKeyEvent *event)
{
    // Qt's QTextControl swallows the paragraph separator on an empty block
    // with a non-default block format (its "second enter resets the
    // paragraph style" heuristic). The app stores the line-height setting
    // in every block's format, so normalize the current block to the
    // default format, let the default handler insert the new paragraph,
    // then restore the app's line-height on both blocks.
    // The key press must NOT run inside beginEditBlock()/endEditBlock():
    // QTextEdit then re-ranges its scrollbars while the document layout is
    // still transient, and a scrolled-down viewport collapses back to the
    // top. Only the format restores below are grouped.
    QTextBlockFormat lineHeightFmt;
    lineHeightFmt.setLineHeight(QSettings().value(Preferences::EditorLineHeight,
                                                  Preferences::DefaultEditorLineHeight).toInt(),
                                QTextBlockFormat::ProportionalHeight);
    QTextCursor edit = textCursor();
    edit.setBlockFormat(QTextBlockFormat());
    QTextEdit::keyPressEvent(event);
    edit = textCursor();
    edit.beginEditBlock();
    edit.mergeBlockFormat(lineHeightFmt);
    edit.movePosition(QTextCursor::PreviousBlock);
    edit.mergeBlockFormat(lineHeightFmt);
    edit.endEditBlock();
    // Re-applying the line height above grows the previous block and pushes
    // the caret's block down past the point Qt scrolled to during the key
    // press; a minimal ensureCursorVisible re-scroll keeps the caret fully on
    // screen (no-op when it already is, e.g. an Enter mid-document).
    ensureCursorVisible();
}

bool Editor::handleEscapeKey()
{
    m_completer->popup()->hide();
    return true;
}

bool Editor::handleEnterKey(QKeyEvent *event)
{
    if (m_completer && m_completer->popup()->isVisible()) {
        QAbstractItemModel *model = m_completer->completionModel();
        QModelIndex idx = m_completer->popup()->currentIndex();
        if (!idx.isValid() && model && model->rowCount() > 0)
            idx = model->index(0, 0);
        if (idx.isValid()) {
            acceptCompletion(model->data(idx).toString());
            return true;
        }
    }

    // Enter completes the word on the current line, so correct a finished
    // typo before the paragraph is split.
    applyAutoCorrect(false);

    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    // The fold whose hidden region holds the caret block, or -1. Inside a
    // folded region Enter redirects below the fold (headers and list items)
    // instead of auto-continuing a list marker into hidden text.
    int foldedRegion = -1;
    {
        int bn = cursor.blockNumber();
        for (auto it = m_foldedBlocks.constBegin(); it != m_foldedBlocks.constEnd(); ++it) {
            if (foldRegionContains(*it, bn) && (foldedRegion < 0 || *it > foldedRegion))
                foldedRegion = *it;
        }
    }
    // Only auto-continue list markers when the caret is at the end of the
    // block; pressing Enter mid-line (or at the start) should split normally.
    if (cursor.positionInBlock() == line.length()) {
        QString result = handleListReturn(line);
        if (!result.isEmpty() && foldedRegion < 0) {
            if (result == QString(clearSentinel)) {
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                insertParagraphWithLineHeight(event);
                return true;
            }
            QTextEdit::keyPressEvent(event);
            insertPlainText(result);
            return true;
        }
    } else {
        // Splitting a list item mid-line should carry the list marker onto
        // the new line instead of leaving a bare paragraph split.
        QString marker = handleListSplitReturn(line, cursor.positionInBlock());
        if (!marker.isEmpty() && foldedRegion < 0) {
            QTextEdit::keyPressEvent(event);
            insertPlainText(marker);
            return true;
        }
    }

    QTextBlock prevBlock = cursor.block().previous();
    // Auto-continue table rows when the caret is at the end of the block.
    // Enter mid-row normally splits the paragraph, but an empty data row
    // (e.g. the one just auto-completed below a header) exits the table
    // even when the caret sits inside a cell rather than at the row's end,
    // while Enter on a data/header row's content keeps working the table.
    if (MdTable::isMdTableLikeRow(line) && MdTable::isMdSeparatorRow(line)) {
        // Separator row: never split. Jump to the first data row below, or
        // create an empty one if the table has no data rows yet.
        QTextBlock block = cursor.block().next();
        while (block.isValid()) {
            QString t = block.text();
            if (MdTable::isMdTableLikeRow(t) && !MdTable::isMdSeparatorRow(t)) {
                QTextCursor tc = textCursor();
                tc.setPosition(block.position() + MdTable::mdRowFirstCellPos(t), QTextCursor::MoveAnchor);
                setTextCursor(tc);
                return true;
            }
            block = block.next();
        }
        const MdTable::MdRowStyle style = MdTable::mdRowStyle(line);
        int cols = qMax(MdTable::splitMdTableRow(line).size(), 1);
        QString newRow = MdTable::makeEmptyTableRow(cols, style, tablePadding());
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        cursor.insertText("\n" + newRow);
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, MdTable::mdRowFirstCellPos(newRow));
        setTextCursor(cursor);
        return true;
    }

    QString result;
    if (cursor.positionInBlock() == line.length()) {
        result = MdTable::handleTableReturn(line, prevBlock.isValid() ? prevBlock.text() : QString(), tablePadding());
    } else if (MdTable::isBlankMdTableRow(line)) {
        result = QString(clearSentinel);
    } else if (MdTable::isMdTableLikeRow(line)) {
        // Mid-cell Enter on a data or header row: let MdTable::handleTableReturn
        // decide — data rows continue with an empty row below, header rows
        // fall through to the header-skip block to jump to the first data
        // row (or create a fresh table).
        const QString r = MdTable::handleTableReturn(line, prevBlock.isValid() ? prevBlock.text() : QString(), tablePadding());
        if (!r.isEmpty() && r != QString(clearSentinel))
            result = r;
    }
    if (!result.isEmpty()) {
        if (result == QString(clearSentinel)) {
            if (line.contains("<tr>") && line.contains("<td>")) {
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                QTextCursor search = document()->find("</table>", cursor);
                if (!search.isNull()) {
                    search.movePosition(QTextCursor::EndOfLine);
                    setTextCursor(search);
                    insertPlainText("\n\n");
                    return true;
                }
            }
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            insertParagraphWithLineHeight(event);
            return true;
        }

        // Header row: skip to first data row below separator
        QTextBlock nextBlock = cursor.block().next();
        if (nextBlock.isValid() && MdTable::isMdSeparatorRow(nextBlock.text())) {
            QTextBlock block = nextBlock.next();
            while (block.isValid()) {
                QString t = block.text();
                if (MdTable::isMdTableLikeRow(t) && !MdTable::isMdSeparatorRow(t)) {
                    QTextCursor tc = textCursor();
                    tc.setPosition(block.position() + MdTable::mdRowFirstCellPos(t), QTextCursor::MoveAnchor);
                    setTextCursor(tc);
                    return true;
                }
                block = block.next();
            }
        }

        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        cursor.insertText("\n" + result);
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        // `result` is multi-line when Enter builds a whole table; the cursor
        // sits on its last line, so measure the first cell of that line.
        const QString lastLine = result.mid(result.lastIndexOf('\n') + 1);
        int cellPos = result.startsWith("<tr>")
            ? result.indexOf("<td>") + 4
            : MdTable::mdRowFirstCellPos(lastLine);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, cellPos);
        setTextCursor(cursor);
        // Typing a header row then Enter creates a fresh table
        // (separator + empty data row) — align it right away.
        if (result.contains("---")
            && QSettings().value(Preferences::AutoAlignTables, true).toBool())
            formatMdTableBlock(cursor.blockNumber());
        return true;
    }

    // Auto-indent indented lines and auto-continue '>' blockquote markers:
    // Enter at the end of a line whose leading prefix is whitespace and/or
    // blockquote markers carries that prefix onto the new line so the caret
    // stays inside the block. Enter on a line that is only that prefix clears
    // it — the second-Enter exit, mirroring lists and tables. Inside a folded
    // region the fold handling below redirects instead.
    if (cursor.positionInBlock() == line.length() && foldedRegion < 0) {
        const QString indent = handleIndentReturn(line);
        if (indent == QString(clearSentinel)) {
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            insertParagraphWithLineHeight(event);
            return true;
        }
        if (!indent.isEmpty()) {
            QTextEdit::keyPressEvent(event);
            insertPlainText(indent);
            return true;
        }
    }

    // Folded region: Enter at the end of a folded foldable line (or while the
    // cursor sits in the hidden body) inserts the new paragraph just below the
    // fold so typed text stays visible instead of vanishing into hidden blocks.
    int bn = cursor.blockNumber();
    int folded = -1;
    for (auto it = m_foldedBlocks.constBegin(); it != m_foldedBlocks.constEnd(); ++it) {
        if (*it <= bn && foldRegionContains(*it, bn) && (folded < 0 || *it > folded))
            folded = *it;
    }
    if (folded >= 0) {
        int end = foldEnd(folded);
        bool onStart = bn == folded;
        bool atStartEnd = onStart && cursor.positionInBlock() == cursor.block().text().length();
        if ((atStartEnd || (!onStart && bn < end)) && bn < end) {
            QTextCursor target = textCursor();
            if (end == document()->blockCount()
                && (m_headerLevel.contains(folded) || m_listItems.contains(folded))
                && !m_foldEndPins.contains(folded)) {
                // Fold runs to EOF: pin the bottom here so the new paragraph
                // (and anything typed below it) stays visible past the re-scan.
                target.movePosition(QTextCursor::End);
                m_foldEndPins.insert(folded, target.position());
            } else if (end == document()->blockCount()) {
                target.movePosition(QTextCursor::End);
            } else {
                QTextBlock eb = document()->findBlockByNumber(end);
                target.setPosition(eb.position() + eb.length() - 1);
            }
            setTextCursor(target);
            insertParagraphWithLineHeight(event);
            event->accept();
            return true;
        }
    }

    insertParagraphWithLineHeight(event);
    return true;
}

bool Editor::handleTabKey(QKeyEvent *event)
{
    bool shift = event->modifiers() & Qt::ShiftModifier;

    if (m_completer && m_completer->popup()->isVisible()) {
        QAbstractItemView *pv = m_completer->popup();
        QAbstractItemModel *model = m_completer->completionModel();
        int rows = model->rowCount();
        if (rows > 0) {
            QModelIndex cur = pv->currentIndex().isValid()
                ? pv->currentIndex() : model->index(0, 0);
            int next = shift ? cur.row() - 1 : cur.row() + 1;
            if (next < 0) next = rows - 1;
            if (next >= rows) next = 0;
            pv->setCurrentIndex(model->index(next, 0));
        }
        return true;
    }

    QTextCursor cursor = textCursor();

    // Block indent/dedent for multi-line selections
    if (cursor.hasSelection()) {
        int startPos = cursor.selectionStart();
        int endPos = cursor.selectionEnd();
        QTextBlock startBlock = document()->findBlock(startPos);
        QTextBlock endBlock = document()->findBlock(endPos);
        if (endBlock.position() == endPos && endBlock.blockNumber() > startBlock.blockNumber())
            endBlock = endBlock.previous();

        int startNum = startBlock.blockNumber();
        int endNum = endBlock.blockNumber();

        bool dedent = shift || event->key() == Qt::Key_Backtab;

        if (dedent) {
            for (int i = endNum; i >= startNum; --i) {
                QTextBlock block = document()->findBlockByNumber(i);
                QString text = block.text();
                int toRemove = 0;
                while (toRemove < 4 && toRemove < text.size() && text[toRemove] == ' ')
                    ++toRemove;
                if (toRemove > 0) {
                    QTextCursor tc = textCursor();
                    tc.setPosition(block.position());
                    tc.setPosition(block.position() + toRemove, QTextCursor::KeepAnchor);
                    tc.removeSelectedText();
                }
            }
        } else {
            for (int i = endNum; i >= startNum; --i) {
                QTextBlock block = document()->findBlockByNumber(i);
                QTextCursor tc = textCursor();
                tc.setPosition(block.position());
                tc.insertText("    ");
            }
        }

        // Reselect the modified range
        QTextBlock newStart = document()->findBlockByNumber(startNum);
        QTextBlock newEnd = document()->findBlockByNumber(endNum);
        cursor.setPosition(newStart.position());
        cursor.setPosition(newEnd.position() + newEnd.length() - 1, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
        return true;
    }

    QString line = cursor.block().text();

    // Table cell navigation
    if (MdTable::isMdTableLikeRow(line)) {
        int pos = cursor.positionInBlock();
        int cellPos = MdTable::tableNavCell(line, pos, !shift);
        if (cellPos >= 0) {
            cursor.setPosition(cursor.block().position() + cellPos, QTextCursor::MoveAnchor);
            setTextCursor(cursor);
            return true;
        }
        if (!shift) {
            QTextBlock block = cursor.block().next();
            while (block.isValid()) {
                QString t = block.text();
                if (MdTable::isMdTableLikeRow(t) && !MdTable::isMdSeparatorRow(t)) {
                    cursor.setPosition(block.position() + MdTable::mdRowFirstCellPos(t), QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                    return true;
                }
                block = block.next();
            }
            // No next row — create a new empty row
            const MdTable::MdRowStyle style = MdTable::mdRowStyle(line);
            int cols = MdTable::splitMdTableRow(line).size();
            if (cols > 0) {
                QString newRow = MdTable::makeEmptyTableRow(cols, style, tablePadding());
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
                bool hasSep = false;
                QTextBlock b = cursor.block();
                while (b.isValid() && MdTable::isMdTableLikeRow(b.text())) {
                    if (MdTable::isMdSeparatorRow(b.text())) { hasSep = true; break; }
                    b = b.previous();
                }
                QString sep = hasSep ? QString() : (MdTable::makeTableSeparatorRow(cols, style, tablePadding()) + "\n");
                cursor.insertText("\n" + sep + newRow);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, MdTable::mdRowFirstCellPos(newRow));
                setTextCursor(cursor);
                return true;
            }
        } else {
            QTextBlock block = cursor.block().previous();
            while (block.isValid()) {
                QString t = block.text();
                if (MdTable::isMdTableLikeRow(t) && !MdTable::isMdSeparatorRow(t)) {
                    cursor.setPosition(block.position() + MdTable::mdRowLastCellPos(t), QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                    return true;
                }
                block = block.previous();
            }
        }
    }

    // HTML table cell navigation
    if (line.contains("<tr>") && line.contains("<td>")) {
        int pos = cursor.positionInBlock();
        int cellPos = MdTable::tableNavHtmlCell(line, pos, !shift);
        if (cellPos >= 0) {
            cursor.setPosition(cursor.block().position() + cellPos, QTextCursor::MoveAnchor);
            setTextCursor(cursor);
            return true;
        }
        if (!shift) {
            QTextBlock block = cursor.block().next();
            while (block.isValid()) {
                QString t = block.text();
                if (t.contains("<tr>") && t.contains("<td>")) {
                    int p = t.indexOf("<td>") + 4;
                    cursor.setPosition(block.position() + p, QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                    return true;
                }
                block = block.next();
            }
            // No next row — create a new empty row
            int cols = line.count("<td>");
            if (cols > 0) {
                QString newRow = MdTable::makeEmptyHtmlTableRow(cols);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
                cursor.insertText("\n" + newRow);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newRow.indexOf("<td>") + 4);
                setTextCursor(cursor);
                return true;
            }
        }
    }

    static const QRegularExpression unorderedRe(R"(^\s*[-*+]\s?)");
    static const QRegularExpression orderedRe(R"(^\s*\d+[.)]\s?)");
    static const QRegularExpression definitionRe(R"(^\s*[:~]\s?)");
    auto matchUnordered = unorderedRe.match(line);
    auto matchOrdered = orderedRe.match(line);
    auto matchDefinition = definitionRe.match(line);
    bool isList = matchUnordered.hasMatch() || matchOrdered.hasMatch() || matchDefinition.hasMatch();

    if (event->key() == Qt::Key_Backtab || shift) {
        if (isList) {
            QString outdented = outdentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(outdented);
            // Only renumber when the outdent actually moved the item to a
            // shallower level — an already-top-level line stays untouched.
            int oldIndent = 0;
            for (QChar c : line) {
                if (c == ' ') ++oldIndent;
                else break;
            }
            int newIndent = 0;
            for (QChar c : outdented) {
                if (c == ' ') ++newIndent;
                else break;
            }
            if (newIndent < oldIndent)
                renumberOutdentedOrderedList(cursor.block().blockNumber());
            return true;
        }
    } else {
        if (isList) {
            QString indented = indentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(indented);
            renumberNestedOrderedList(cursor.block().blockNumber());
            return true;
        }
        insertPlainText("    ");
        return true;
    }

    // Backtab on a non-list line: nothing to outdent, so fall through to the
    // default handling in keyPressEvent.
    return false;
}

bool Editor::handleDownKey()
{
    QTextCursor cursor = textCursor();
    QTextCursor probe = cursor;
    if (!probe.movePosition(QTextCursor::Down)) {
        cursor.movePosition(QTextCursor::EndOfBlock);
        setTextCursor(cursor);
        return true;
    }
    return false;
}

bool Editor::handleCtrlAltScroll(int key)
{
    if (key == Qt::Key_Up)
        verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->singleStep());
    else
        verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->singleStep());
    return true;
}

bool Editor::handleHeaderJump(int direction)
{
    if (direction == Qt::Key_Up) {
        int target = findPrevHeader(textCursor().blockNumber());
        if (target >= 0) {
            QTextBlock block = document()->findBlockByNumber(target);
            QTextCursor cursor(block);
            setTextCursor(cursor);
            centerCursor();
        }
        return true;
    }
    int target = findNextHeader(textCursor().blockNumber());
    if (target >= 0) {
        QTextBlock block = document()->findBlockByNumber(target);
        QTextCursor cursor(block);
        setTextCursor(cursor);
        centerCursor();
    }
    return true;
}

bool Editor::handleZoom(int direction)
{
    if (direction == Qt::Key_Equal) {
        int bn = textCursor().blockNumber();
        int folded = -1;
        for (auto it = m_foldedBlocks.constBegin(); it != m_foldedBlocks.constEnd(); ++it) {
            if (foldRegionContains(*it, bn)) {
                if (folded < 0 || *it > folded)
                    folded = *it;
            }
        }
        if (folded >= 0)
            toggleFold(folded);
        return true;
    }
    int bn = textCursor().blockNumber();
    if (isFoldableBlock(bn) && !m_foldedBlocks.contains(bn)) {
        toggleFold(bn);
    }
    return true;
}

bool Editor::handleHardwrap()
{
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();
    bool hasSel = cursor.hasSelection();

    QTextBlock startBlock = hasSel
        ? doc->findBlock(cursor.selectionStart())
        : cursor.block();
    QTextBlock endBlock = hasSel
        ? doc->findBlock(cursor.selectionEnd())
        : startBlock;

    QStringList lines;
    QTextBlock b = startBlock;
    while (true) {
        lines << b.text();
        if (b == endBlock) break;
        b = b.next();
    }
    QString blockText = lines.join('\n');

    cursor.beginEditBlock();
    if (endBlock.blockNumber() == doc->blockCount() - 1) {
        cursor.movePosition(QTextCursor::End);
        cursor.insertText('\n' + blockText);
    } else {
        QTextBlock next = endBlock.next();
        cursor.setPosition(next.position());
        cursor.insertText(blockText + '\n');
    }
    cursor.endEditBlock();

    return true;
}

bool Editor::handleBackspaceDelete(QKeyEvent *event)
{
    QTextEdit::keyPressEvent(event);

    if (m_completer && m_completer->popup()->isVisible()) {
        QString partialCode;
        if (isInsideEmojiContext(textCursor(), partialCode) && QSettings().value(Preferences::EmojiAutoComplete, true).toBool()) {
            if (partialCode.isEmpty())
                m_completer->popup()->hide();
            else
                showEmojiCompletion(partialCode);
        } else {
            QString partialPath;
            if (isInsideLinkContext(textCursor(), partialPath)) {
                if (partialPath.isEmpty())
                    m_completer->popup()->hide();
                else
                    showFileCompletion(partialPath);
            } else {
                QString htmlPath;
                if (isInsideHtmlPathContext(textCursor(), htmlPath)) {
                    if (htmlPath.isEmpty())
                        m_completer->popup()->hide();
                    else
                        showFileCompletion(htmlPath);
                } else {
                    QString partialLang;
                    if (isInsideLanguageContext(textCursor(), partialLang) &&
                        QSettings().value(Preferences::LanguageAutoComplete, true).toBool()) {
                        if (partialLang.isEmpty())
                            m_completer->popup()->hide();
                        else
                            showLanguageCompletion(partialLang);
                    } else {
                        m_completer->popup()->hide();
                    }
                }
            }
        }
    }
    return true;
}

void Editor::applyAutoCorrect(bool separatorTyped)
{
    QSettings s;
    if (!s.value(Preferences::AutoCorrectEnabled, true).toBool())
        return;
    const QStringList pairs = s.value(Preferences::AutoCorrectPairs).toStringList();
    if (pairs.isEmpty())
        return;

    QTextCursor cursor = textCursor();
    const QString line = cursor.block().text();
    const int pos = cursor.positionInBlock();

    int wordStart = -1;
    int wordLength = 0;
    const QString replacement = autoCorrectWord(line, pos, pairs, separatorTyped, &wordStart, &wordLength);
    if (replacement.isEmpty() || wordStart < 0)
        return;

    // Never rewrite inside markdown link/HTML attribute paths, code spans or
    // emoji shortcodes.
    QString partial;
    if (isInsideLinkContext(cursor, partial) || isInsideHtmlPathContext(cursor, partial))
        return;
    if (isInsideEmojiContext(cursor, partial))
        return;
    if (insideFencedCode(cursor.blockNumber()) || isInsideInlineCode())
        return;

    // Replace the word (keeping any trailing separator so the caret stays put);
    // one edit block so a single undo restores the typo.
    const int blockStart = cursor.block().position();
    const QString trailing = line.mid(wordStart + wordLength, pos - wordStart - wordLength);
    cursor.beginEditBlock();
    cursor.setPosition(blockStart + wordStart, QTextCursor::MoveAnchor);
    cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(replacement + trailing);
    cursor.endEditBlock();
    setTextCursor(cursor);
}

QString Editor::currentLineText() const
{
    return textCursor().block().text();
}

void Editor::toggleCheckbox()
{
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    static const QRegularExpression checkRe(R"(^(\s*[-*+]\s)\[)([ xX])(\])");
    auto match = checkRe.match(line);
    if (!match.hasMatch())
        return;

    QString current = match.captured(2);
    QString toggled = (current == " ") ? "x" : " ";
    int offset = cursor.block().position() + match.capturedStart(2);
    cursor.setPosition(offset, QTextCursor::MoveAnchor);
    cursor.setPosition(offset + 1, QTextCursor::KeepAnchor);
    cursor.insertText(toggled);
    setTextCursor(cursor);
}

void Editor::deleteLine()
{
    QTextCursor cursor = textCursor();
    QTextDocument *doc = document();
    bool hasSel = cursor.hasSelection();

    QTextBlock startBlock = hasSel
        ? doc->findBlock(cursor.selectionStart())
        : cursor.block();
    QTextBlock endBlock = hasSel
        ? doc->findBlock(cursor.selectionEnd())
        : startBlock;

    int startPos = startBlock.position();
    QTextBlock next = endBlock.next();
    int endPos = next.isValid()
        ? next.position()                             // delete the line's trailing newline too
        : endBlock.position() + endBlock.length() - 1; // last line: clear its content

    cursor.beginEditBlock();
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();

    cursor.setPosition(startPos);
    setTextCursor(cursor);
}

void Editor::changeCodeLanguage()
{
    QTextCursor cursor = textCursor();
    int blockNum = cursor.blockNumber();

    QTextBlock fenceBlock;
    QTextBlock block = document()->firstBlock();
    bool foundOpen = false;
    while (block.isValid() && block.blockNumber() < blockNum) {
        if (block.text().trimmed().startsWith("```")) {
            if (!foundOpen) {
                fenceBlock = block;
                foundOpen = true;
            } else {
                foundOpen = false;
            }
        }
        block = block.next();
    }

    if (!foundOpen || !fenceBlock.isValid())
        return;

    QString fenceText = fenceBlock.text().trimmed();
    QString currentLang = fenceText.mid(3).trimmed();

    bool ok;
    QString lang = QInputDialog::getText(this, tr("Change Language"),
        tr("Language:"), QLineEdit::Normal, currentLang, &ok);
    if (!ok)
        return;

    QTextCursor tc(document());
    tc.setPosition(fenceBlock.position() + 3);
    tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
    if (!lang.isEmpty())
        tc.insertText(lang);
    setTextCursor(cursor);
}

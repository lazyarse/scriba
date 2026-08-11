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
#include "GrammarChecker.h"
#include "Gutter.h"
#include "StoppardEngine.h"
#include "Preferences.h"
#include "Corpus.h"
#include "MarkdownChecker.h"
#include "SpellChecker.h"
#include "StaticHelpers.h"
#include "MdTable.h"
#include <algorithm>
#include <climits>
#include <QAbstractItemView>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>
#include <QToolTip>
#include <QAbstractTextDocumentLayout>

namespace {

// Vertical gap (px) between the bottom of the caret/active line and the top of
// the autocompletion popup. Kept small so the suggestions hug the text.
constexpr int kCompletionPopupGap = 4;

// Normalized view of a line for list folding: quoteDepth counts leading '>'
// blockquote markers, indent is the content column after stripping them, and
// isList is true when a list marker (bullet, ordered, or task) begins the
// content. Used by both the fold scan and the list fold-range walk.
struct ListFoldInfo
{
    int quoteDepth = 0;
    int indent = 0;
    QString content;
    bool isList = false;
};

ListFoldInfo listFoldInfo(const QString &line)
{
    ListFoldInfo info;
    QString rem = line;
    static const QRegularExpression quotePrefixRe(R"(^(?:[ ]{0,3}>[ \t]?)+)");
    QRegularExpressionMatch m = quotePrefixRe.match(rem);
    if (m.hasMatch()) {
        info.quoteDepth = m.captured(0).count('>');
        rem = rem.mid(m.capturedLength());
    }
    int i = 0;
    while (i < rem.size()) {
        const QChar c = rem[i];
        if (c == ' ') {
            ++info.indent;
            ++i;
        } else if (c == '\t') {
            info.indent += 4;
            ++i;
        } else {
            break;
        }
    }
    info.content = rem.mid(i);
    static const QRegularExpression listMarkerRe(R"(^[-*+](?=\s|$)|\d+[.)](?=\s|$))");
    info.isList = !isThematicBreak(info.content) && listMarkerRe.match(info.content).hasMatch();
    return info;
}

// StoppardEngine is stateless and cheap to construct (no dictionary load), so
// each Editor tab gets its own instance.
GrammarChecker *sharedGrammarChecker()
{
    QSettings settings;
    const QString dialect = settings
        .value(Preferences::GrammarDialect, QStringLiteral("American"))
        .toString();
    return new StoppardEngine(dialect);
}

// Reads the per-check real-time markdown-consistency toggles from `settings`.
// When the master `MarkdownCheckEnabled` is off no checks run; otherwise each
// check is enabled unless its key is explicitly disabled.
QSet<MarkdownChecker::Check> markdownChecksFromSettings(const QSettings &settings,
                                                        bool masterEnabled)
{
    if (!masterEnabled)
        return {};
    auto on = [&settings](const char *key) {
        return settings.value(QLatin1String(key), true).toBool();
    };
    QSet<MarkdownChecker::Check> checks;
    if (on(Preferences::MarkdownCheckHeadingLevelSkip))
        checks.insert(MarkdownChecker::Check::HeadingLevelSkip);
    if (on(Preferences::MarkdownCheckDuplicateHeading))
        checks.insert(MarkdownChecker::Check::DuplicateHeading);
    if (on(Preferences::MarkdownCheckTrailingWhitespace))
        checks.insert(MarkdownChecker::Check::TrailingWhitespace);
    if (on(Preferences::MarkdownCheckConsecutiveBlankLines))
        checks.insert(MarkdownChecker::Check::ConsecutiveBlankLines);
    if (on(Preferences::MarkdownCheckOverlongLine))
        checks.insert(MarkdownChecker::Check::OverlongLine);
    if (on(Preferences::MarkdownCheckHashNoSpace))
        checks.insert(MarkdownChecker::Check::HashNoSpace);
    if (on(Preferences::MarkdownCheckFootnoteReference))
        checks.insert(MarkdownChecker::Check::FootnoteReference);
    return checks;
}

} // namespace

Editor::Editor(QWidget *parent)
    : QTextEdit(parent)
{
    setObjectName("scriba-editor");
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
    setFrameShape(QFrame::NoFrame);

    loadEmojiShortcodes();

    setupGutter();

    auto *foldTimer = new QTimer(this);
    foldTimer->setSingleShot(true);
    foldTimer->setInterval(Debounce::FoldScan);
    connect(foldTimer, &QTimer::timeout, this, &Editor::scanHeadersAndFolds);
    connect(document(), &QTextDocument::contentsChanged, this, [this, foldTimer]() {
        if (!m_updatingFolds)
            foldTimer->start();
    });

    m_spellChecker = std::make_unique<SpellChecker>();
    m_grammarChecker.reset(sharedGrammarChecker());
    m_spellHighlighter = new SpellHighlighter(document(), this);
    m_spellHighlighter->setChecker(m_spellChecker.get());
    m_spellHighlighter->setGrammarChecker(m_grammarChecker.get());
    applySpellSettings();
    applyLineWrap();

    m_underlineOverlay = new QWidget(viewport());
    m_underlineOverlay->setObjectName(QStringLiteral("underline-overlay"));
    m_underlineOverlay->setAttribute(Qt::WA_TranslucentBackground);
    m_underlineOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
    m_underlineOverlay->installEventFilter(this);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    connect(document(), &QTextDocument::contentsChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));
    // Spell underlines are painted from the spell-hit cache, which is
    // refreshed asynchronously (word-boundary or debounced check): repaint
    // the overlay when a check completes.
    connect(m_spellHighlighter, &SpellHighlighter::spellHitsChanged,
            m_underlineOverlay, QOverload<>::of(&QWidget::update));

    connect(this, &Editor::cursorPositionChanged,
            this, &Editor::onCursorPositionChanged);
    // Any text change while the cursor is inside a table marks it dirty so it
    // gets re-aligned when the cursor leaves. The formatting guard stops the
    // reformat from re-marking itself.
    connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        if (m_trackTableStartBlock >= 0 && !m_formattingTable)
            m_tableDirty = true;
    });
}

Editor::~Editor() = default;

void Editor::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    // Relative link targets resolve against this directory — re-check so the
    // broken-link underlines follow the new base.
    if (m_spellHighlighter)
        m_spellHighlighter->setCurrentFile(path);
}

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

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_completer && m_completer->popup()->isVisible()) {
            m_completer->popup()->hide();
            return;
        }
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_completer && m_completer->popup()->isVisible()) {
            QAbstractItemModel *model = m_completer->completionModel();
            QModelIndex idx = m_completer->popup()->currentIndex();
            if (!idx.isValid() && model && model->rowCount() > 0)
                idx = model->index(0, 0);
            if (idx.isValid()) {
                acceptCompletion(model->data(idx).toString());
                return;
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
                    return;
                }
                QTextEdit::keyPressEvent(event);
                insertPlainText(result);
                return;
            }
        } else {
            // Splitting a list item mid-line should carry the list marker onto
            // the new line instead of leaving a bare paragraph split.
            QString marker = handleListSplitReturn(line, cursor.positionInBlock());
            if (!marker.isEmpty() && foldedRegion < 0) {
                QTextEdit::keyPressEvent(event);
                insertPlainText(marker);
                return;
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
                    return;
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
            return;
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
                        return;
                    }
                }
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                insertParagraphWithLineHeight(event);
                return;
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
                        return;
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
            return;
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
                return;
            }
        }

        insertParagraphWithLineHeight(event);
        return;
    }

    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
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
            return;
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
            return;
        }

        QString line = cursor.block().text();

        // Table cell navigation
        if (MdTable::isMdTableLikeRow(line)) {
            int pos = cursor.positionInBlock();
            int cellPos = MdTable::tableNavCell(line, pos, !shift);
            if (cellPos >= 0) {
                cursor.setPosition(cursor.block().position() + cellPos, QTextCursor::MoveAnchor);
                setTextCursor(cursor);
                return;
            }
            if (!shift) {
                QTextBlock block = cursor.block().next();
                while (block.isValid()) {
                    QString t = block.text();
                    if (MdTable::isMdTableLikeRow(t) && !MdTable::isMdSeparatorRow(t)) {
                        cursor.setPosition(block.position() + MdTable::mdRowFirstCellPos(t), QTextCursor::MoveAnchor);
                        setTextCursor(cursor);
                        return;
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
                    return;
                }
            } else {
                QTextBlock block = cursor.block().previous();
                while (block.isValid()) {
                    QString t = block.text();
                    if (MdTable::isMdTableLikeRow(t) && !MdTable::isMdSeparatorRow(t)) {
                        cursor.setPosition(block.position() + MdTable::mdRowLastCellPos(t), QTextCursor::MoveAnchor);
                        setTextCursor(cursor);
                        return;
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
                return;
            }
            if (!shift) {
                QTextBlock block = cursor.block().next();
                while (block.isValid()) {
                    QString t = block.text();
                    if (t.contains("<tr>") && t.contains("<td>")) {
                        int p = t.indexOf("<td>") + 4;
                        cursor.setPosition(block.position() + p, QTextCursor::MoveAnchor);
                        setTextCursor(cursor);
                        return;
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
                    return;
                }
            }
        }

        static const QRegularExpression unorderedRe(R"(^\s*[-*+]\s?)");
        static const QRegularExpression orderedRe(R"(^\s*\d+[.)]\s?)");
        auto matchUnordered = unorderedRe.match(line);
        auto matchOrdered = orderedRe.match(line);
        bool isList = matchUnordered.hasMatch() || matchOrdered.hasMatch();

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
                return;
            }
        } else {
            if (isList) {
                QString indented = indentListLine(line);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                cursor.insertText(indented);
                renumberNestedOrderedList(cursor.block().blockNumber());
                return;
            }
            insertPlainText("    ");
            return;
        }
    }

    if (event->key() == Qt::Key_Down) {
        if (!m_completer || !m_completer->popup()->isVisible()) {
            QTextCursor cursor = textCursor();
            QTextCursor probe = cursor;
            if (!probe.movePosition(QTextCursor::Down)) {
                cursor.movePosition(QTextCursor::EndOfBlock);
                setTextCursor(cursor);
                return;
            }
        }
    }

    bool ctrl = event->modifiers() & Qt::ControlModifier;
    bool alt = event->modifiers() & Qt::AltModifier;

    // Ctrl+Alt+Up/Down: scroll viewport
    if (ctrl && alt && event->key() == Qt::Key_Up) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->singleStep());
        event->accept();
        return;
    }
    if (ctrl && alt && event->key() == Qt::Key_Down) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->singleStep());
        event->accept();
        return;
    }

    // Ctrl+Up/Down: jump to prev/next header
    if (ctrl && !alt && event->key() == Qt::Key_Up) {
        int target = findPrevHeader(textCursor().blockNumber());
        if (target >= 0) {
            QTextBlock block = document()->findBlockByNumber(target);
            QTextCursor cursor(block);
            setTextCursor(cursor);
            centerCursor();
        }
        event->accept();
        return;
    }
    if (ctrl && !alt && event->key() == Qt::Key_Down) {
        int target = findNextHeader(textCursor().blockNumber());
        if (target >= 0) {
            QTextBlock block = document()->findBlockByNumber(target);
            QTextCursor cursor(block);
            setTextCursor(cursor);
            centerCursor();
        }
        event->accept();
        return;
    }

    // Ctrl+= expand, Ctrl+- fold
    if (ctrl && !alt && event->key() == Qt::Key_Equal) {
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
        event->accept();
        return;
    }
    if (ctrl && !alt && event->key() == Qt::Key_Minus) {
        int bn = textCursor().blockNumber();
        if (isFoldableBlock(bn) && !m_foldedBlocks.contains(bn)) {
            toggleFold(bn);
        }
        event->accept();
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_D) {
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

        event->accept();
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Y) {
        deleteLine();
        event->accept();
        return;
    }

    QTextEdit::keyPressEvent(event);

    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
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
    } else if (!event->text().isEmpty()) {
        // A space or punctuation just completed the word before the cursor, so
        // apply any configured typo replacement before re-evaluating completions.
        applyAutoCorrect(true);
        QChar c = event->text()[0];
        bool shown = false;
        if (c.isLetterOrNumber() || c == '_' || c == ':' || c == '+' || c == '-' || c == '.' || c == '/') {
            QString partialPath;
            if (isInsideLinkContext(textCursor(), partialPath))
                shown = showFileCompletion(partialPath);
            else {
                QString htmlPath;
                if (isInsideHtmlPathContext(textCursor(), htmlPath))
                    shown = showFileCompletion(htmlPath);
            }
            QString partialCode;
            if (isInsideEmojiContext(textCursor(), partialCode) && QSettings().value(Preferences::EmojiAutoComplete, true).toBool())
                shown = showEmojiCompletion(partialCode) || shown;
            QString partialLang;
            if (isInsideLanguageContext(textCursor(), partialLang) && QSettings().value(Preferences::LanguageAutoComplete, true).toBool())
                shown = showLanguageCompletion(partialLang) || shown;
        }
        if (!shown && m_completer && m_completer->popup()->isVisible())
            m_completer->popup()->hide();
    }
}

void Editor::insertFromMimeData(const QMimeData *source)
{
    const int posBefore = textCursor().position();
    QTextEdit::insertFromMimeData(source);
    // A paste that drops a whole markdown table bypasses the leave-to-format
    // tracking in onCursorPositionChanged: the insert's contentsChanged fires
    // before the cursor lands inside the table, so the table is never marked
    // dirty and clicking away would not realign it. Align right away, like the
    // Insert Table dialog does. No-op for non-table content.
    formatTableAt(posBefore);
}

bool Editor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_underlineOverlay && event->type() == QEvent::Paint && m_spellHighlighter) {
        QPaintEvent *pe = static_cast<QPaintEvent*>(event);
        QPainter painter(m_underlineOverlay);
        painter.setClipRect(pe->rect());
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QFontMetrics fm(font());
        const int underlineY = fm.ascent() + fm.underlinePos() + kUnderlineDropPx;
        const QTextDocument *doc = document();
        for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
            if (!block.isVisible())
                continue;
            const int blockNumber = block.blockNumber();
            const QTextLayout *layout = block.layout();
            if (!layout || layout->lineCount() == 0)
                continue;
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->spellHitsInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::spellUnderlineColor(), fm, underlineY);
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->grammarIssuesInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::grammarUnderlineColor(), fm, underlineY);
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->linkIssuesInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::linkUnderlineColor(), fm, underlineY);
            for (const SpellHighlighter::GrammarHit &hit : m_spellHighlighter->markdownHitsInBlock(blockNumber))
                paintHitRange(painter, block, hit.start, hit.length, SpellHighlighter::markdownUnderlineColor(), fm, underlineY);
        }
        return false;
    }
    return QTextEdit::eventFilter(obj, event);
}

void Editor::paintHitRange(QPainter &painter, const QTextBlock &block, int start, int length,
                           const QColor &color, const QFontMetrics &fm, int underlineY)
{
    const QTextLayout *layout = block.layout();
    if (!layout || layout->lineCount() == 0)
        return;
    painter.setPen(QPen(color, kUnderlinePenWidthPx, Qt::SolidLine, Qt::FlatCap));
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        const int a = qMax(start, line.textStart());
        const int b = qMin(start + length, line.textStart() + line.textLength());
        if (a >= b)
            continue;
        QTextCursor cursor(document());
        cursor.setPosition(block.position() + a);
        const QRect leftRect = cursorRect(cursor);
        cursor.setPosition(block.position() + b);
        const QRect rightRect = cursorRect(cursor);
        const int y = leftRect.top() + underlineY;
        painter.drawLine(QPoint(leftRect.left(), y), QPoint(rightRect.left(), y));
    }
}

bool Editor::isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const
{
    return extractLinkPath(cursor.block().text(), cursor.positionInBlock(), partialPath);
}

bool Editor::isInsideHtmlPathContext(const QTextCursor &cursor, QString &partialPath) const
{
    return extractHtmlPath(cursor.block().text(), cursor.positionInBlock(), partialPath);
}

void Editor::positionCompletionPopup(const QRect &cursorRect)
{
    if (!m_completer || !m_completer->popup())
        return;
    QAbstractItemView *popup = m_completer->popup();
    QRect screen = viewport()->screen()->availableGeometry();

    QPoint caretBottom = viewport()->mapToGlobal(
        QPoint(cursorRect.x(), cursorRect.y() + cursorRect.height()));
    QPoint caretTop = viewport()->mapToGlobal(QPoint(cursorRect.x(), cursorRect.y()));
    int ph = popup->height();
    if (ph <= 0)
        ph = popup->sizeHint().height();

    // Prefer below the caret; flip above when the popup would fall off-screen.
    QPoint pos = caretBottom + QPoint(0, kCompletionPopupGap);
    if (pos.y() + ph > screen.bottom()) {
        int top = caretTop.y() - kCompletionPopupGap - ph;
        if (top < screen.top()) {
            top = screen.top();
            int h = caretTop.y() - kCompletionPopupGap - top;
            if (h > 0)
                popup->resize(popup->width(), h);
        }
        pos.setY(top);
    }
    pos.setX(qBound(screen.left(), pos.x(), screen.right() - qMin(popup->width(), screen.width())));
    popup->move(pos);
}

bool Editor::showFileCompletion(const QString &partialPath)
{
    if (m_currentFile.isEmpty() ||
        !QSettings().value(Preferences::FileAutoComplete, true).toBool())
        return false;

    QFileInfo fi(m_currentFile);
    QDir dir = fi.absoluteDir();

    FileCompletionResult result = matchFileEntries(partialPath, dir,
        QSettings().value(Preferences::FileCompletionLimit, 20).toInt());
    if (result.entries.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStringListModel *model = new QStringListModel(result.entries, m_completer);
    m_completer->setModel(model);
    m_completer->setCompletionPrefix(result.filePart);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : result.entries)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

void Editor::acceptCompletion(const QString &completion)
{
    if (m_completer)
        m_completer->popup()->hide();

    if (completion.startsWith(':') && completion.endsWith(':')) {
        acceptEmojiCompletion(completion);
        return;
    }

    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int pos = cursor.positionInBlock();
    int blockStart = cursor.block().position();

    // Code fence language context: replace text right after ```
    QString partialLang;
    if (isInsideLanguageContext(cursor, partialLang)) {
        cursor.setPosition(blockStart + 3, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);
        return;
    }

    // Markdown link context: [text](path) or ![text](path)
    int replaceStart = linkPathReplaceStart(line, pos);
    if (replaceStart >= 0) {
        cursor.setPosition(blockStart + replaceStart, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);

        if (!completion.endsWith('/')) {
            QTextCursor c = textCursor();
            QChar next = document()->characterAt(c.position());
            if (next != ')') {
                c.insertText(QStringLiteral(")"));
                setTextCursor(c);
            }
        }
        return;
    }

    // HTML attribute context: src="path" or href='path'
    replaceStart = htmlPathReplaceStart(line, pos);
    if (replaceStart >= 0) {
        cursor.setPosition(blockStart + replaceStart, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);
    }
}

void Editor::loadEmojiShortcodes()
{
    for (const EmojiEntry &entry : emojiCatalog()) {
        m_emojiShortcodes.append(entry.shortcode);
        m_emojiUnicode[entry.shortcode] = entry.unicode;
    }
    m_emojiShortcodes.removeDuplicates();
    m_emojiShortcodes.sort();
}

bool Editor::isInsideEmojiContext(const QTextCursor &cursor, QString &partialCode) const
{
    return extractEmojiCode(cursor.block().text(), cursor.positionInBlock(), partialCode);
}

bool Editor::showEmojiCompletion(const QString &partialCode)
{
    QVector<QPair<QString, FuzzyScore>> scored;
    for (const QString &sc : m_emojiShortcodes) {
        FuzzyScore score = fuzzyMatchScore(sc, partialCode);
        if (score.matched)
            scored.append({sc, score});
    }

    if (scored.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    std::sort(scored.begin(), scored.end(),
        [](const QPair<QString, FuzzyScore> &a, const QPair<QString, FuzzyScore> &b) {
            if (a.second.gaps != b.second.gaps)
                return a.second.gaps < b.second.gaps;
            if (a.second.firstPos != b.second.firstPos)
                return a.second.firstPos < b.second.firstPos;
            return a.first < b.first;
        });

    int limit = QSettings().value(Preferences::EmojiCompletionLimit, 100).toInt();
    if (scored.size() > limit)
        scored = scored.mid(0, limit);

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStandardItemModel *model = new QStandardItemModel(m_completer);
    for (const auto &match : scored) {
        auto *item = new QStandardItem(QString(":%1:").arg(match.first));
        item->setIcon(QIcon(renderEmojiIcon(m_emojiUnicode.value(match.first))));
        model->appendRow(item);
    }

    m_completer->setModel(model);
    m_completer->setCompletionPrefix(partialCode);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (int i = 0; i < model->rowCount(); ++i)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(model->item(i)->text()));
    cr.setWidth(maxWidth + 30 + 22);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

QPixmap Editor::renderEmojiIcon(const QString &emojiStr) const
{
    if (auto it = m_emojiIconCache.find(emojiStr); it != m_emojiIconCache.end())
        return *it;

    QPixmap pix = renderEmojiPixmap(emojiStr, 18);
    m_emojiIconCache[emojiStr] = pix;
    return pix;
}

void Editor::acceptEmojiCompletion(const QString &completion)
{
    if (m_completer)
        m_completer->popup()->hide();

    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int pos = cursor.positionInBlock();

    int colonPos = -1;
    static const QRegularExpression emojiRe(R"(:([a-zA-Z0-9_+-]*)$)");
    auto match = emojiRe.match(line.left(pos));
    if (match.hasMatch())
        colonPos = match.capturedStart(0);
    else
        return;

    int endPos = pos;
    if (pos < line.size() && line[pos] == ':')
        ++endPos;

    int blockStart = cursor.block().position();
    cursor.setPosition(blockStart + colonPos, QTextCursor::MoveAnchor);
    cursor.setPosition(blockStart + endPos, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(completion);
    setTextCursor(cursor);
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

bool Editor::isInsideInlineCode() const
{
    // Backtick parity before the caret on the current line (mirrors the spell
    // highlighter's per-line handling of inline code spans).
    const QString line = textCursor().block().text();
    const int pos = textCursor().positionInBlock();
    int backticks = 0;
    for (int i = 0; i < pos; ++i) {
        if (line[i] == '`' && (i == 0 || line[i - 1] != '\\'))
            ++backticks;
    }
    return backticks % 2 == 1;
}

namespace {
// Code fence languages bundled in resources/highlight.min.js (canonical → aliases).
// Keep in sync with the bundled highlight.js version.
const QHash<QString, QStringList> &codeLanguages()
{
    static const QHash<QString, QStringList> langs = {
        { "bash",        {"sh", "zsh", "shellscript"} },
        { "c",           {} },
        { "cpp",         {"c++", "cxx", "hpp", "hxx", "cc"} },
        { "csharp",      {"cs", "c#"} },
        { "css",         {} },
        { "diff",        {"patch"} },
        { "go",          {"golang"} },
        { "graphql",     {"gql"} },
        { "ini",         {"toml", "cfg", "conf"} },
        { "java",        {} },
        { "javascript",  {"js", "jsx", "mjs", "cjs"} },
        { "json",        {} },
        { "kotlin",      {"kt", "kts"} },
        { "less",        {} },
        { "lua",         {} },
        { "makefile",    {"make", "mk", "mak"} },
        { "markdown",    {"md", "mkdown", "mkd"} },
        { "mermaid",     {"mmd"} },
        { "objectivec",  {"objc", "obj-c", "mm"} },
        { "perl",        {"pl"} },
        { "php",         {} },
        { "plaintext",   {"text", "txt"} },
        { "python",      {"py", "gyp"} },
        { "r",           {} },
        { "ruby",        {"rb", "gemspec", "podspec", "thor", "irb"} },
        { "rust",        {"rs"} },
        { "scss",        {} },
        { "shell",       {"console", "shellsession"} },
        { "sql",         {} },
        { "swift",       {} },
        { "typescript",  {"ts", "tsx"} },
        { "vbnet",       {"vb", "vbs"} },
        { "ec",          {} },
        { "wasm",        {} },
        { "xml",         {"html", "xhtml", "svg", "rss", "atom", "xsl", "plist"} },
        { "yaml",        {"yml"} },
    };
    return langs;
}
}

bool Editor::isInsideLanguageContext(const QTextCursor &cursor, QString &partialLang) const
{
    QString text = cursor.block().text();
    int pos = cursor.positionInBlock();

    static const QRegularExpression fenceRe(R"(^```(\S*)$)");
    auto match = fenceRe.match(text.left(pos));
    if (!match.hasMatch())
        return false;

    // Must be an opening fence: an even number of fence blocks precede it.
    int fenceCount = 0;
    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() < cursor.blockNumber()) {
        if (block.text().trimmed().startsWith("```"))
            ++fenceCount;
        block = block.next();
    }
    if (fenceCount % 2 != 0)
        return false;

    partialLang = match.captured(1);
    return true;
}

bool Editor::showLanguageCompletion(const QString &partialLang)
{
    if (partialLang.isEmpty() ||
        !QSettings().value(Preferences::LanguageAutoComplete, true).toBool())
        return false;

    QStringList matches;
    const QHash<QString, QStringList> &langs = codeLanguages();
    for (auto it = langs.cbegin(); it != langs.cend(); ++it) {
        const QString &name = it.key();
        if (name.contains(partialLang, Qt::CaseInsensitive)) {
            matches.append(name);
            continue;
        }
        for (const QString &alias : it.value()) {
            if (alias.contains(partialLang, Qt::CaseInsensitive)) {
                matches.append(name);
                break;
            }
        }
    }
    if (matches.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    std::sort(matches.begin(), matches.end(),
        [&partialLang](const QString &a, const QString &b) {
            bool aPrefix = a.startsWith(partialLang, Qt::CaseInsensitive);
            bool bPrefix = b.startsWith(partialLang, Qt::CaseInsensitive);
            if (aPrefix != bPrefix)
                return aPrefix;
            return a < b;
        });

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStringListModel *model = new QStringListModel(matches, m_completer);
    m_completer->setModel(model);
    m_completer->setCompletionPrefix(partialLang);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : matches)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

void Editor::applySpellSettings()
{
    if (!m_spellChecker || !m_spellHighlighter)
        return;
    SpellHighlighter::reloadUnderlineColors();
    QSettings s;
    const bool spellEnabled = s.value(Preferences::SpellCheckEnabled, true).toBool();
    const bool grammarEnabled = s.value(Preferences::GrammarCheckEnabled, false).toBool();
    const bool linkEnabled = s.value(Preferences::LinkCheckEnabled, true).toBool();
    const bool markdownEnabled = s.value(Preferences::MarkdownCheckEnabled, false).toBool();

    // Empty = "follow dialect": the grammar dialect selects the base dictionary.
    const QString language = s.value(Preferences::DictionaryLanguage).toString();
    const QString dialect = s.value(Preferences::GrammarDialect, QStringLiteral("American")).toString();

    m_spellChecker->setDialect(dialect);

    bool loaded = false;
    if (spellEnabled) {
        const QString resolved = language.isEmpty()
            ? SpellChecker::defaultLanguageForDialect(dialect)
            : language;
        loaded = m_spellChecker->loadLanguage(resolved);
        if (!loaded) {
            for (const QString &lang : SpellChecker::availableLanguages()) {
                if (m_spellChecker->loadLanguage(lang)) {
                    loaded = true;
                    break;
                }
            }
        }
    }
    m_spellHighlighter->setSpellCheckingEnabled(spellEnabled && loaded);
    m_spellHighlighter->setGrammarCheckingEnabled(grammarEnabled);
    m_spellHighlighter->setLinkCheckingEnabled(linkEnabled);
    m_spellHighlighter->setMarkdownCheckingEnabled(markdownEnabled);
    m_spellHighlighter->setMarkdownChecks(markdownChecksFromSettings(s, markdownEnabled));

    if (auto *stoppard = dynamic_cast<StoppardEngine *>(m_grammarChecker.get()))
        stoppard->setDialect(dialect);

    m_spellHighlighter->refresh();
}

void Editor::recheckSpelling()
{
    applySpellSettings();
}

void Editor::applyCorpusDictionary(const CorpusDictionary &dict, bool merge)
{
    if (!m_spellChecker || !m_spellHighlighter)
        return;
    m_corpusActive = true;
    m_spellChecker->setCorpusMerge(merge);
    m_spellChecker->setCorpusWords(dict.customWords);
    m_spellChecker->setCorpusIgnored(dict.ignoredWords);

    // The corpus language/dialect override the global preferences when set;
    // empty fields fall back to the per-user settings, mirroring
    // applySpellSettings().
    const QSettings s;
    const QString language = dict.language.isEmpty()
        ? s.value(Preferences::DictionaryLanguage).toString()
        : dict.language;
    const QString dialect = dict.dialect.isEmpty()
        ? s.value(Preferences::GrammarDialect, QStringLiteral("American")).toString()
        : dict.dialect;
    m_spellChecker->setDialect(dialect);

    const bool spellEnabled = s.value(Preferences::SpellCheckEnabled, true).toBool();
    bool loaded = false;
    if (spellEnabled) {
        const QString resolved = language.isEmpty()
            ? SpellChecker::defaultLanguageForDialect(dialect)
            : language;
        loaded = m_spellChecker->loadLanguage(resolved);
        if (!loaded) {
            for (const QString &lang : SpellChecker::availableLanguages()) {
                if (m_spellChecker->loadLanguage(lang)) {
                    loaded = true;
                    break;
                }
            }
        }
    }
    m_spellHighlighter->setSpellCheckingEnabled(spellEnabled && loaded);
    m_spellHighlighter->refresh();
}

void Editor::refreshUnderlines()
{
    SpellHighlighter::reloadUnderlineColors();
    if (m_underlineOverlay)
        m_underlineOverlay->update();
}

void Editor::setSpellCheckHighlight(int blockNumber, int start, int length)
{
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;
    QTextCursor cursor(document());
    cursor.setPosition(block.position() + start);
    cursor.setPosition(block.position() + start + length, QTextCursor::KeepAnchor);
    setTextCursor(cursor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = cursor;
    QTextCharFormat fmt;
    fmt.setBackground(SpellHighlighter::spellHighlightColor());
    sel.format = fmt;
    setExtraSelections({sel});
}

void Editor::clearSpellCheckHighlight()
{
    setExtraSelections({});
}

SpellHighlighter::WordHit Editor::misspelledWordAt(const QTextCursor &cursor) const
{
    if (!m_spellChecker || !m_spellChecker->isLoaded())
        return {};
    if (isCursorInFencedCodeBlock())
        return {};
    const int pos = cursor.positionInBlock();
    const QString line = cursor.block().text();
    for (const SpellHighlighter::WordHit &word : SpellHighlighter::scanWords(line)) {
        if (pos >= word.start && pos <= word.start + word.length
            && !m_spellChecker->checkWord(word.text))
            return word;
    }
    return {};
}

QString Editor::explanationAt(int blockNumber, int positionInBlock)
{
    if (!m_spellHighlighter)
        return {};

    // A finding's span covers [start, start + length]. Zero-length findings
    // (e.g. the consecutive-blank-lines check) are treated as covering the
    // whole line so the message shows wherever the line is hovered.
    const auto hitAt = [positionInBlock](const QVector<SpellHighlighter::GrammarHit> &hits)
        -> const SpellHighlighter::GrammarHit * {
        for (const SpellHighlighter::GrammarHit &hit : hits) {
            const bool covers = hit.length == 0
                || (positionInBlock >= hit.start
                    && positionInBlock <= hit.start + hit.length);
            if (covers)
                return &hit;
        }
        return nullptr;
    };

    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->markdownHitsInBlock(blockNumber)))
        return hit->message;
    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->grammarIssuesInBlock(blockNumber)))
        return hit->message;
    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->linkIssuesInBlock(blockNumber)))
        return QStringLiteral("Broken link: %1").arg(hit->message);
    if (const SpellHighlighter::GrammarHit *hit
        = hitAt(m_spellHighlighter->spellHitsInBlock(blockNumber))) {
        const QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (block.isValid()) {
            const QString word = block.text().mid(hit->start, hit->length);
            if (!word.isEmpty())
                return QStringLiteral("Misspelled word: %1").arg(word);
        }
    }
    return {};
}

void Editor::mouseMoveEvent(QMouseEvent *event)
{
    const QTextCursor cursor = cursorForPosition(event->pos());
    const QString tip = explanationAt(cursor.block().blockNumber(),
                                      cursor.positionInBlock());
    if (tip != m_activeTooltip) {
        m_activeTooltip = tip;
        if (tip.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(mapToGlobal(event->pos()), tip, this);
    }
    QTextEdit::mouseMoveEvent(event);
}

void Editor::leaveEvent(QEvent *event)
{
    m_activeTooltip.clear();
    QToolTip::hideText();
    QTextEdit::leaveEvent(event);
}

void Editor::invalidateEmojiIconCache()
{
    m_emojiIconCache.clear();
}

void Editor::centerCursor()
{
    QRect cr = cursorRect();
    if (cr.isNull())
        return;
    int vh = viewport()->height();
    int cy = cr.center().y() + verticalScrollBar()->value();
    int target = cy - vh / 2;
    target = qBound(0, target, verticalScrollBar()->maximum());
    verticalScrollBar()->setValue(target);
}

void Editor::setCenterContent(bool enabled, int width)
{
    m_centerContent = enabled;
    m_centerContentWidth = width;
    updateViewportMargins();
}

void Editor::applyLineWrap()
{
    QSettings settings;
    const bool enabled = settings.value(Preferences::EditorWrapEnabled, true).toBool();
    if (!enabled) {
        setLineWrapMode(QTextEdit::NoWrap);
    } else {
        const QString mode = settings.value(Preferences::EditorWrapMode,
                                             QStringLiteral("window")).toString();
        if (mode == QLatin1String("column")) {
            setLineWrapMode(QTextEdit::FixedColumnWidth);
            setLineWrapColumnOrWidth(settings.value(Preferences::EditorWrapColumn,
                                                     Preferences::DefaultEditorWrapColumn).toInt());
        } else {
            setLineWrapMode(QTextEdit::WidgetWidth);
        }
    }
    updateViewportMargins();
}

QMargins Editor::contentMargins() const
{
    return viewportMargins();
}

void Editor::setInsertActions(const QList<QAction *> &actions)
{
    m_insertActions = actions;
}

void Editor::setMermaidAction(QAction *action)
{
    m_mermaidAction = action;
}

QString Editor::currentLineText() const
{
    return textCursor().block().text();
}

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

bool Editor::cursorHasUrlSelection() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return false;
    QString selected = cursor.selectedText();
    static const QRegularExpression urlRe(R"(^https?://\S+$)");
    return urlRe.match(selected).hasMatch();
}

bool Editor::cursorHasImagePathSelection() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return false;
    QString selected = cursor.selectedText();
    static const QRegularExpression imgRe(R"(^(\./|\.\.\/|/)?\S+\.(png|jpe?g|gif|svg|webp|bmp)$)", QRegularExpression::CaseInsensitiveOption);
    return imgRe.match(selected).hasMatch();
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

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // Right-click acts on the clicked position: move the caret there so the
    // spelling/grammar lookup and the context-sensitive actions below match
    // where the user clicked. A live selection is kept (for "Make Link").
    QTextCursor cursor = cursorForPosition(event->pos());
    if (textCursor().hasSelection())
        cursor = textCursor();
    else
        setTextCursor(cursor);

    const SpellHighlighter::WordHit misspelled = misspelledWordAt(cursor);
    if (!misspelled.text.isEmpty()) {
        QMenu *suggestions = menu.addMenu("Spelling: " + misspelled.text);
        const QStringList words = m_spellChecker->suggestions(misspelled.text);
        if (words.isEmpty()) {
            QAction *none = suggestions->addAction("No suggestions");
            none->setEnabled(false);
        } else {
            for (const QString &suggestion : words) {
                QAction *action = suggestions->addAction(suggestion);
                connect(action, &QAction::triggered, this, [this, suggestion, misspelled]() {
                    const QTextBlock block = textCursor().block();
                    QTextCursor replace(document());
                    replace.setPosition(block.position() + misspelled.start);
                    replace.setPosition(block.position() + misspelled.start + misspelled.length,
                                        QTextCursor::KeepAnchor);
                    replace.insertText(suggestion);
                });
            }
        }

        QAction *addAction = menu.addAction("Add to Dictionary: " + misspelled.text);
        connect(addAction, &QAction::triggered, this, [this, misspelled]() {
            if (m_corpusActive)
                m_spellChecker->addCorpusWord(misspelled.text);
            else
                m_spellChecker->addToUserDictionary(misspelled.text);
            m_spellHighlighter->refresh();
        });

        menu.addSeparator();
    }

    // Grammar issues under the cursor: each squiggle becomes a submenu whose
    // title states the justification for the error, with one-click fixes
    // inside. A rule is drawn underneath each submenu.
    for (const SpellHighlighter::GrammarHit &hit
         : m_spellHighlighter->grammarIssuesInBlock(cursor.block().blockNumber())) {
        if (cursor.positionInBlock() < hit.start
            || cursor.positionInBlock() > hit.start + hit.length)
            continue;

        QMenu *grammarMenu = menu.addMenu("Grammar: " + hit.message);
        const int blockNumber = cursor.block().blockNumber();

        if (hit.suggestions.isEmpty()) {
            QAction *none = grammarMenu->addAction("No suggestions");
            none->setEnabled(false);
        } else {
            for (const GrammarChecker::Issue::Suggestion &suggestion : hit.suggestions) {
                QString label;
                switch (suggestion.kind) {
                case GrammarChecker::Issue::SuggestionKind::Replace:
                    label = "Replace with '" + suggestion.text + "'";
                    break;
                case GrammarChecker::Issue::SuggestionKind::Remove:
                    label = "Remove";
                    break;
                case GrammarChecker::Issue::SuggestionKind::InsertAfter:
                    label = "Insert '" + suggestion.text + "' after";
                    break;
                }
                QAction *action = grammarMenu->addAction(label);
                connect(action, &QAction::triggered, this,
                        [this, blockNumber, hit, suggestion]() {
                            const QTextBlock block = document()->findBlockByNumber(blockNumber);
                            if (!block.isValid())
                                return;
                            QTextCursor fix(document());
                            fix.setPosition(block.position() + hit.start);
                            fix.setPosition(block.position() + hit.start + hit.length,
                                            QTextCursor::KeepAnchor);
                            if (suggestion.kind
                                == GrammarChecker::Issue::SuggestionKind::InsertAfter) {
                                fix.setPosition(block.position() + hit.start + hit.length);
                                fix.insertText(suggestion.text);
                            } else if (suggestion.kind
                                       == GrammarChecker::Issue::SuggestionKind::Remove) {
                                fix.removeSelectedText();
                            } else {
                                fix.insertText(suggestion.text);
                            }
                        });
            }
        }

        menu.addSeparator();
    }

    // Broken links under the cursor: an informational entry (there is no
    // automatic fix for a missing file or a malformed URL).
    for (const SpellHighlighter::GrammarHit &hit
         : m_spellHighlighter->linkIssuesInBlock(cursor.block().blockNumber())) {
        if (cursor.positionInBlock() < hit.start
            || cursor.positionInBlock() > hit.start + hit.length)
            continue;
        QAction *brokenLink = menu.addAction("Broken link: " + hit.message);
        brokenLink->setEnabled(false);
        menu.addSeparator();
    }

    // Markdown-consistency issues under the cursor (heading-level skips,
    // duplicate headings, trailing whitespace, ...): an informational entry,
    // same as broken links — there is no single automatic fix.
    for (const SpellHighlighter::GrammarHit &hit
         : m_spellHighlighter->markdownHitsInBlock(cursor.block().blockNumber())) {
        if (cursor.positionInBlock() < hit.start
            || cursor.positionInBlock() > hit.start + hit.length)
            continue;
        QAction *mdIssue = menu.addAction("Markdown: " + hit.message);
        mdIssue->setEnabled(false);
        menu.addSeparator();
    }

    for (QAction *action : m_insertActions)
        menu.addAction(action);

    if (m_mermaidAction)
        menu.addAction(m_mermaidAction);

    CursorContext ctx = detectCursorContext();

    if (ctx == CursorContext::ListItem) {
        menu.addSeparator();
        QAction *indentAction = menu.addAction("Increase Indent");
        connect(indentAction, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString line = currentLineText();
            QString indented = indentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(indented);
        });

        QAction *dedentAction = menu.addAction("Decrease Indent");
        connect(dedentAction, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString line = currentLineText();
            QString outdented = outdentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(outdented);
        });

        static const QRegularExpression taskRe(R"(^\s*[-*+]\s\[)");
        if (taskRe.match(currentLineText()).hasMatch()) {
            QAction *toggleAction = menu.addAction("Toggle Checkbox");
            connect(toggleAction, &QAction::triggered, this, &Editor::toggleCheckbox);
        }
    }

    if (ctx == CursorContext::TableRow) {
        menu.addSeparator();
        QAction *above = menu.addAction("Insert Row Above");
        connect(above, &QAction::triggered, this, [this]() { insertTableRow(true); });

        QAction *below = menu.addAction("Insert Row Below");
        connect(below, &QAction::triggered, this, [this]() { insertTableRow(false); });

        menu.addSeparator();

        QAction *colLeft = menu.addAction("Insert Column Left");
        connect(colLeft, &QAction::triggered, this, [this]() { insertTableCol(true); });

        QAction *colRight = menu.addAction("Insert Column Right");
        connect(colRight, &QAction::triggered, this, [this]() { insertTableCol(false); });

        menu.addSeparator();

        QAction *delRow = menu.addAction("Delete Row");
        connect(delRow, &QAction::triggered, this, &Editor::deleteTableRow);

        QAction *delCol = menu.addAction("Delete Column");
        connect(delCol, &QAction::triggered, this, &Editor::deleteTableCol);
    }

    if (ctx == CursorContext::CodeBlock) {
        menu.addSeparator();
        QAction *langAction = menu.addAction("Change Language...");
        connect(langAction, &QAction::triggered, this, &Editor::changeCodeLanguage);
    }

    if (cursorHasUrlSelection()) {
        menu.addSeparator();
        QAction *makeLink = menu.addAction("Make Link");
        connect(makeLink, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString url = cursor.selectedText();
            cursor.insertText("[](" + url + ")");
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, url.size() + 3);
            setTextCursor(cursor);
        });
    }

    if (cursorHasImagePathSelection()) {
        menu.addSeparator();
        QAction *insertImg = menu.addAction("Insert Image");
        connect(insertImg, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString path = cursor.selectedText();
            cursor.insertText("![](" + path + ")");
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, path.size() + 4);
            setTextCursor(cursor);
        });
    }

    if (!menu.isEmpty())
        menu.exec(event->globalPos());
}

void Editor::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    if (m_centerContent && !m_inResize) {
        m_inResize = true;
        updateViewportMargins();
        m_inResize = false;
    }
    updateGutter();
    if (m_underlineOverlay) {
        m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
        m_underlineOverlay->update();
    }
}

void Editor::scrollContentsBy(int dx, int dy)
{
    QTextEdit::scrollContentsBy(dx, dy);
    // QWidget::scroll() (used by the base implementation) moves child widgets
    // of the viewport along with the content, which would drift the underline
    // overlay out of sync with the text. Re-anchor it to the viewport origin.
    if (m_underlineOverlay) {
        m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
        m_underlineOverlay->update();
    }
}

void Editor::updateViewportMargins()
{
    int gutterW = m_gutter ? m_gutter->width() : 0;
    if (!m_centerContent) {
        setViewportMargins(gutterW, 0, 0, 0);
        return;
    }
    // When wrapping at a fixed column the column count takes over as the
    // editor's effective max width: the centred region is the wrapped width
    // rather than m_centerContentWidth.
    int contentWidth = m_centerContentWidth;
    if (wrapAtColumnActive())
        contentWidth = qRound(wrapColumnPx());
    int available = width() - 2 * frameWidth() - gutterW;
    int scrollbarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
    available -= scrollbarWidth;
    int margin = qMax(0, (available - contentWidth) / 2);
    setViewportMargins(margin + gutterW, 0, margin, 0);
}

bool Editor::wrapAtColumnActive() const
{
    QSettings settings;
    return settings.value(Preferences::EditorWrapEnabled, true).toBool()
        && settings.value(Preferences::EditorWrapMode,
                          QStringLiteral("window")).toString() == QLatin1String("column");
}

qreal Editor::wrapColumnPx() const
{
    const QFontMetrics fm = fontMetrics();
    const int col = QSettings().value(Preferences::EditorWrapColumn,
                                      Preferences::DefaultEditorWrapColumn).toInt();
    const qreal charWidth = fm.horizontalAdvance(QLatin1Char('M')) > 0
        ? fm.horizontalAdvance(QLatin1Char('M')) : fm.averageCharWidth();
    return charWidth * col;
}

void Editor::updateGutter()
{
    if (!m_gutter)
        return;
    int gutterW = m_gutter->width();
    m_gutter->setGeometry(0, 0, gutterW, viewport()->height() + height() - viewport()->height());
    if (m_underlineOverlay) {
        m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
        m_underlineOverlay->update();
    }
}

void Editor::updateGutterSettings()
{
    if (!m_gutter)
        return;
    QSettings s;
    m_gutter->setLineNumbersVisible(s.value(Preferences::ShowLineNumbers, true).toBool());
    applyGutterColors();
    updateGutterWidth();
}

void Editor::refreshGutter()
{
    if (m_gutter)
        m_gutter->update();
}

void Editor::updateGutterDelayed()
{
    QTimer::singleShot(0, this, &Editor::updateGutter);
}

void Editor::setupGutter()
{
    m_gutter = new Gutter(this);
    connect(m_gutter, &Gutter::foldToggled, this, &Editor::toggleFold);
    connect(m_gutter, &Gutter::chartEditRequested, this, &Editor::chartEditRequested);
    connect(this, &Editor::cursorPositionChanged, this, [this]() {
        if (m_gutter)
            m_gutter->update();
    });
    connect(document(), &QTextDocument::blockCountChanged, this, &Editor::updateGutterWidth);
    applyGutterColors();
    updateGutter();
    updateGutterWidth();
    scanHeadersAndFolds();
}

void Editor::updateGutterWidth()
{
    if (!m_gutter)
        return;
    m_gutter->updateWidth();
    updateViewportMargins();
    updateGutterDelayed();
}

void Editor::applyGutterColors()
{
    if (!m_gutter)
        return;
    QSettings s;
    bool colorOverride = s.value(Preferences::GutterColorOverride, false).toBool();
    QString css;
    if (colorOverride) {
        QString bg = s.value(Preferences::GutterBgColor, "#f0f0f0").toString();
        QString fg = s.value(Preferences::GutterTextColor, "#888888").toString();
        css = QString("background-color: %1; color: %2;").arg(bg, fg);
    } else {
        css.clear();
    }
    m_gutter->setStyleSheet(css);
}

void Editor::scanHeadersAndFolds()
{
    m_headerLevel.clear();
    m_codeFences.clear();
    m_chartFences.clear();
    m_mdTableSeparators.clear();
    m_htmlTables.clear();
    m_listItems.clear();

    QTextBlock block = document()->firstBlock();
    int codeDepth = 0;
    QRegularExpression atxRe("^(#{1,6})\\s");
    QRegularExpression setextUnderlineRe("^(={3,}|-{3,})\\s*$");
    QRegularExpression mdTableSepRe(
        "^\\s*\\|?\\s*:?-+:?\\s*(\\|\\s*:?-+:?\\s*)+\\|?\\s*$");

    // First pass: detect headers and fenced-code openings
    QTextBlock prevBlock;
    while (block.isValid()) {
        QString text = block.text();
        int bn = block.blockNumber();

        if (text.trimmed().startsWith("```")) {
            if (codeDepth == 0) {
                m_codeFences.insert(bn);
                // A chart block is a fence whose language is mermaid or ec;
                // the gutter pencil makes it editable without leaving the editor.
                const QString lang = text.trimmed().mid(3).trimmed()
                                         .section(QRegularExpression("[\\s{].*"), 0, 0)
                                         .toLower();
                if (lang == QLatin1String("mermaid") || lang == QLatin1String("ec"))
                    m_chartFences.insert(bn);
            }
            codeDepth ^= 1;
            prevBlock = block;
            block = block.next();
            continue;
        }

        if (codeDepth == 0) {
            auto atxMatch = atxRe.match(text);
            if (atxMatch.hasMatch()) {
                int level = atxMatch.captured(1).size();
                m_headerLevel[bn] = level;
                prevBlock = block;
                block = block.next();
                continue;
            }

            // Setext: check if PREVIOUS line was not a header and THIS line is === or ---
            if (prevBlock.isValid() && !m_headerLevel.contains(prevBlock.blockNumber())) {
                auto setextMatch = setextUnderlineRe.match(text.trimmed());
                if (setextMatch.hasMatch()) {
                    QChar ch = setextMatch.captured(1).at(0);
                    int level = (ch == '=') ? 1 : 2;
                    m_headerLevel[prevBlock.blockNumber()] = level;
                }
            }

            // Markdown table separator row (the fold anchor). Must be a line of
            // dashes/colons delimited by pipes, preceded by a non-blank header
            // row (so it is really a table delimiter, not a stray line).
            if (mdTableSepRe.match(text).hasMatch()
                && prevBlock.isValid() && !prevBlock.text().trimmed().isEmpty()) {
                m_mdTableSeparators.insert(bn);
            }

            // HTML table opening line: foldable only when multi-line, i.e. the
            // closing </table> is NOT on the same line.
            const QString t = text.trimmed();
            if (t.startsWith("<table") && !t.contains("</table>"))
                m_htmlTables.insert(bn);

            // List item marker line (bullet, ordered, task): a fold anchor whose
            // range runs to the next sibling/shallower item or the list end.
            // Leaf items whose fold would hide nothing get no anchor at all.
            if (listFoldInfo(text).isList && listAnchorHasContent(bn))
                m_listItems.insert(bn);
        }

        prevBlock = block;
        block = block.next();
    }

    // Update gutter with foldable info
    QSet<int> foldableSet;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it)
        foldableSet.insert(it.key());
    foldableSet.unite(m_codeFences);
    foldableSet.unite(m_mdTableSeparators);
    foldableSet.unite(m_htmlTables);
    foldableSet.unite(m_listItems);
    if (m_gutter) {
        m_gutter->setFoldableBlocks(foldableSet);
        m_gutter->setChartBlocks(m_chartFences);
    }

    // Re-apply folds
    m_updatingFolds = true;
    QSet<int> stillValid;
    for (int bn : m_foldedBlocks) {
        if (isFoldableBlock(bn)) {
            applyFold(bn, true);
            stillValid.insert(bn);
        }
    }
    m_foldedBlocks = stillValid;
    for (auto it = m_foldEndPins.begin(); it != m_foldEndPins.end();) {
        if ((!m_headerLevel.contains(it.key()) && !m_listItems.contains(it.key()))
            || !document()->findBlock(it.value()).isValid())
            it = m_foldEndPins.erase(it);
        else
            ++it;
    }
    if (m_gutter)
        m_gutter->setFoldedBlocks(m_foldedBlocks);
    m_updatingFolds = false;
}

bool Editor::isFoldableBlock(int blockNumber) const
{
    return m_headerLevel.contains(blockNumber) || m_codeFences.contains(blockNumber)
        || m_mdTableSeparators.contains(blockNumber) || m_htmlTables.contains(blockNumber)
        || m_listItems.contains(blockNumber);
}

bool Editor::foldRegionContains(int startBlock, int blockNumber) const
{
    return startBlock <= blockNumber && blockNumber < foldEnd(startBlock);
}

void Editor::applyFold(int startBlock, bool hide)
{
    int end = foldEnd(startBlock);
    QTextBlock block = document()->findBlockByNumber(startBlock + 1);
    while (block.isValid() && block.blockNumber() < end) {
        block.setVisible(!hide);
        block = block.next();
    }
}

int Editor::foldEnd(int startBlock) const
{
    if (m_codeFences.contains(startBlock)) {
        QTextBlock block = document()->findBlockByNumber(startBlock + 1);
        while (block.isValid()) {
            if (block.text().trimmed().startsWith("```"))
                return block.blockNumber() + 1;
            block = block.next();
        }
        return document()->blockCount();
    }

    // Markdown table: the separator row is the anchor; the table continues over
    // each following non-blank, non-block-start line (mirrors the GFM table
    // termination md4c now implements: the fold stops at the same boundaries
    // that end a rendered table).
    if (m_mdTableSeparators.contains(startBlock)) {
        QTextBlock block = document()->findBlockByNumber(startBlock + 1);
        static const QRegularExpression blockStartRe(
            "^\\s*(?:(?:#{1,6})\\s|```|~~~|[>\\-+*]\\s|\\d+[.)]\\s|</?[a-zA-Z#]|<!--|---+\\s*$|\\*{3,}\\s*$|_{3,}\\s*$)");
        while (block.isValid()) {
            const QString t = block.text();
            if (t.trimmed().isEmpty()
                || blockStartRe.match(t).hasMatch()
                || m_headerLevel.contains(block.blockNumber())
                || m_codeFences.contains(block.blockNumber())
                || m_htmlTables.contains(block.blockNumber()))
            {
                return block.blockNumber();
            }
            block = block.next();
        }
        return document()->blockCount();
    }

    // HTML table: fold to the matching </table> (if any), else to EOF.
    if (m_htmlTables.contains(startBlock)) {
        QTextBlock block = document()->findBlockByNumber(startBlock + 1);
        while (block.isValid()) {
            if (block.text().contains("</table>"))
                return block.blockNumber() + 1;
            block = block.next();
        }
        return document()->blockCount();
    }

    if (m_listItems.contains(startBlock)) {
        int end = listFoldEnd(startBlock);
        auto pinIt = m_foldEndPins.find(startBlock);
        if (pinIt != m_foldEndPins.end()) {
            QTextBlock pinned = document()->findBlock(pinIt.value());
            if (pinned.isValid() && pinned.blockNumber() > startBlock && pinned.blockNumber() <= end)
                end = pinned.blockNumber();
        }
        return end;
    }

    int end = sectionEndBlock(startBlock, m_headerLevel.value(startBlock));
    auto pinIt = m_foldEndPins.find(startBlock);
    if (pinIt != m_foldEndPins.end()) {
        QTextBlock pinned = document()->findBlock(pinIt.value());
        if (pinned.isValid() && pinned.blockNumber() > startBlock && pinned.blockNumber() <= end)
            end = pinned.blockNumber();
    }
    return end;
}

bool Editor::listItemIsFirst(int startBlock, int anchorQuoteDepth, int anchorIndent) const
{
    QTextBlock block = document()->findBlockByNumber(startBlock).previous();
    while (block.isValid()) {
        const QString text = block.text();
        if (text.trimmed().isEmpty()) {
            block = block.previous();
            continue;
        }
        const ListFoldInfo info = listFoldInfo(text);
        if (info.quoteDepth < anchorQuoteDepth)
            break;
        if (info.isList && info.quoteDepth == anchorQuoteDepth && info.indent <= anchorIndent)
            return false;
        if (!info.isList)
            break;
        block = block.previous();
    }
    return true;
}

bool Editor::listAnchorHasContent(int blockNumber) const
{
    const int end = listFoldEnd(blockNumber);
    if (end <= blockNumber + 1)
        return false;
    for (int i = blockNumber + 1; i < end; ++i) {
        if (!document()->findBlockByNumber(i).text().trimmed().isEmpty())
            return true;
    }
    return false;
}

// CommonMark only nests an *ordered* list whose first item is numbered 1 — a
// "nested" marker like 2. or 3. under an item is lazily-continued as paragraph
// text instead. After Tab indents an ordered item, renumber the consecutive
// run of same-indent ordered items starting at `startBlock` to 1, 2, 3... so
// the new sublist actually renders. Top-level lists (indent 0) are untouched,
// preserving intentional start numbers like "2024.".
void Editor::renumberNestedOrderedList(int startBlock)
{
    static const QRegularExpression orderedRe(R"(^(\s*)(\d+)([.)]))");
    QTextBlock start = document()->findBlockByNumber(startBlock);
    QRegularExpressionMatch head = orderedRe.match(start.text());
    if (!head.hasMatch())
        return;
    const int newIndent = head.captured(1).length();
    if (newIndent == 0)
        return;

    // Walk back to the head of the contiguous same-indent ordered run so the
    // nested list is numbered 1..n as a whole, not each freshly-tabbed line
    // independently.
    QTextBlock block = start;
    QTextBlock prev = block.previous();
    while (prev.isValid()) {
        const QRegularExpressionMatch m = orderedRe.match(prev.text());
        if (!m.hasMatch() || m.captured(1).length() != newIndent)
            break;
        block = prev;
        prev = prev.previous();
    }

    int counter = 1;
    while (block.isValid()) {
        const QRegularExpressionMatch m = orderedRe.match(block.text());
        if (!m.hasMatch() || m.captured(1).length() != newIndent)
            break;
        QTextCursor c(block);
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newIndent);
        c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m.captured(2).length());
        c.insertText(QString::number(counter));
        block = block.next();
        ++counter;
    }
}

// Mirror of renumberNestedOrderedList for Shift+Tab: when an ordered item is
// outdented it rejoins the list at a shallower level, so renumber it (and its
// contiguous same-indent ordered run) to continue the enclosing sequence. The
// base is the nearest preceding ordered item at the new indent + 1; when none
// precedes it (blank line, paragraph, new list context), the item's own number
// is preserved so intentional starts like "2024." survive.
void Editor::renumberOutdentedOrderedList(int startBlock)
{
    static const QRegularExpression orderedRe(R"(^(\s*)(\d+)([.)]))");
    QTextBlock start = document()->findBlockByNumber(startBlock);
    QRegularExpressionMatch head = orderedRe.match(start.text());
    if (!head.hasMatch())
        return;
    const int newIndent = head.captured(1).length();

    int base = head.captured(2).toInt();
    QTextBlock prev = start.previous();
    while (prev.isValid()) {
        const QString text = prev.text();
        if (text.trimmed().isEmpty()) {
            prev = prev.previous();
            continue;
        }
        int indent = 0;
        for (QChar c : text) {
            if (c == ' ') ++indent;
            else break;
        }
        const QRegularExpressionMatch m = orderedRe.match(text);
        if (indent <= newIndent && m.hasMatch()) {
            base = m.captured(2).toInt() + 1;
            break;
        }
        // At or above the target level but not an ordered item (paragraph,
        // bullet, heading, ...) — the outdented item starts its own context.
        if (indent <= newIndent)
            break;
        // Deeper-indented block: part of a sub-list, skip up toward the
        // enclosing (shallower) list the outdented item rejoins.
        prev = prev.previous();
    }

    QTextBlock block = start;
    int counter = base;
    while (block.isValid()) {
        const QRegularExpressionMatch m = orderedRe.match(block.text());
        if (!m.hasMatch() || m.captured(1).length() != newIndent)
            break;
        QTextCursor c(block);
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newIndent);
        c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m.captured(2).length());
        c.insertText(QString::number(counter));
        block = block.next();
        ++counter;
    }
}

int Editor::listFoldEnd(int startBlock) const
{
    const ListFoldInfo anchor = listFoldInfo(document()->findBlockByNumber(startBlock).text());
    const bool firstItem = listItemIsFirst(startBlock, anchor.quoteDepth, anchor.indent);
    const int contentColumn = anchor.indent + 2;
    auto terminatesList = [&](const ListFoldInfo &info) {
        const bool sameLevel = info.quoteDepth == anchor.quoteDepth
                               && info.indent <= anchor.indent;
        return sameLevel && (!firstItem || info.indent < anchor.indent);
    };
    QTextBlock block = document()->findBlockByNumber(startBlock + 1);
    while (block.isValid()) {
        const QString text = block.text();
        if (text.trimmed().isEmpty()) {
            QTextBlock next = block.next();
            while (next.isValid() && next.text().trimmed().isEmpty())
                next = next.next();
            if (!next.isValid())
                return document()->blockCount();
            const ListFoldInfo info = listFoldInfo(next.text());
            if (info.quoteDepth < anchor.quoteDepth || terminatesList(info)
                || (!info.isList && info.indent < contentColumn))
                return block.blockNumber();
            block = next;
            continue;
        }

        const ListFoldInfo info = listFoldInfo(text);
        if (info.quoteDepth < anchor.quoteDepth)
            return block.blockNumber();
        if (info.isList) {
            if (terminatesList(info))
                return block.blockNumber();
            block = block.next();
            continue;
        }
        if (info.indent < contentColumn) {
            static const QRegularExpression outerBlockStartRe(
                "^(?:#{1,6}\\s|```|~~~|>\\s|[-*+]\\s|\\d+[.)]\\s|</?[a-zA-Z#]|<!--|---+\\s*$|\\*{3,}\\s*$|_{3,}\\s*$)");
            if (outerBlockStartRe.match(info.content).hasMatch())
                return block.blockNumber();
        }
        block = block.next();
    }
    return document()->blockCount();
}

QPair<int, int> Editor::fencedCodeBlockRange(int blockNumber) const
{
    if (!m_codeFences.contains(blockNumber))
        return {-1, -1};
    return {blockNumber, foldEnd(blockNumber)};
}

QString Editor::blockRangeText(int firstBlock, int endExclusive) const
{
    QStringList parts;
    QTextBlock block = document()->findBlockByNumber(firstBlock);
    while (block.isValid() && block.blockNumber() < endExclusive) {
        parts.append(block.text());
        block = block.next();
    }
    return parts.join('\n');
}

void Editor::replaceBlockRange(int firstBlock, int endExclusive, const QString &text)
{
    QTextDocument *doc = document();
    if (firstBlock < 0 || firstBlock >= doc->blockCount())
        return;
    QTextCursor cur(doc);
    cur.beginEditBlock();
    QTextBlock first = doc->findBlockByNumber(firstBlock);
    cur.setPosition(first.position());
    cur.movePosition(QTextCursor::StartOfBlock);
    int last = endExclusive - 1;
    if (last >= doc->blockCount())
        last = doc->blockCount() - 1;
    if (last < firstBlock)
        last = firstBlock;
    QTextBlock endBlock = doc->findBlockByNumber(last);
    // Select through the last line's content but not its paragraph separator.
    cur.setPosition(endBlock.position() + qMax(0, endBlock.length() - 1),
                    QTextCursor::KeepAnchor);
    cur.removeSelectedText();
    cur.insertText(text);
    cur.endEditBlock();
    scanHeadersAndFolds();
}

// Matches `$$…$$` (display) before `$…$` (inline, single line, non-empty).
static const QRegularExpression &inlineMathRe()
{
    static const QRegularExpression re(
        QStringLiteral(R"(\$\$[\s\S]+?\$\$|\$[^$\n]+\$)"));
    return re;
}

int Editor::findNthInlineMath(int blockNumber, int index) const
{
    if (blockNumber < 0 || blockNumber >= document()->blockCount())
        return -1;
    const QString line = document()->findBlockByNumber(blockNumber).text();
    int i = 0;
    QRegularExpressionMatchIterator it = inlineMathRe().globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (i++ == index)
            return document()->findBlockByNumber(blockNumber).position() + m.capturedStart();
    }
    return -1;
}

void Editor::replaceInlineMath(int blockNumber, int index, const QString &replacement)
{
    if (blockNumber < 0 || blockNumber >= document()->blockCount())
        return;
    const QString line = document()->findBlockByNumber(blockNumber).text();
    int i = 0;
    QRegularExpressionMatchIterator it = inlineMathRe().globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (i++ == index) {
            QTextBlock block = document()->findBlockByNumber(blockNumber);
            QTextCursor cur(document());
            cur.setPosition(block.position() + m.capturedStart());
            cur.setPosition(block.position() + m.capturedEnd(), QTextCursor::KeepAnchor);
            cur.insertText(replacement);
            return;
        }
    }
}

QString Editor::fenceBody(int blockNumber) const
{
    const QPair<int, int> range = fencedCodeBlockRange(blockNumber);
    if (range.first < 0)
        return QString();
    QStringList lines = blockRangeText(range.first, range.second).split(QLatin1Char('\n'));
    if (lines.size() < 3)
        return QString();
    lines.removeFirst(); // opening fence
    lines.removeLast();  // closing fence
    return lines.join(QLatin1Char('\n'));
}

// The preview's `<code>` text ends with a line terminator that the editor's
// block range does not carry; tolerate that when comparing.
static bool chartBodiesMatch(const QString &a, const QString &b)
{
    return a == b || a == b + QLatin1Char('\n') || b == a + QLatin1Char('\n');
}

int Editor::findFenceByBody(const QString &body, int hintLine) const
{
    if (body.isEmpty())
        return -1;
    QList<int> fences = m_codeFences.values();
    std::sort(fences.begin(), fences.end());
    int best = -1;
    int bestDist = INT_MAX;
    for (int bn : fences) {
        if (!chartBodiesMatch(body, fenceBody(bn)))
            continue;
        const int dist = qAbs(bn - (hintLine - 1));
        if (dist < bestDist) {
            bestDist = dist;
            best = bn;
        }
    }
    return best;
}

QString Editor::fenceLanguage(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return QString();
    const QString text = block.text().trimmed();
    if (!text.startsWith(QLatin1String("```")))
        return QString();
    return text.mid(3).trimmed().section(QRegularExpression("[\\s{].*"), 0, 0).toLower();
}

// Strips the `$$` / `$` delimiters the regex captures, matching the inner
// text the preview's edit anchors carry.
static QString mathInnerText(const QString &match)
{
    if (match.startsWith(QLatin1String("$$")))
        return match.mid(2, match.size() - 4);
    if (match.size() >= 2)
        return match.mid(1, match.size() - 2);
    return QString();
}

int Editor::findMathByContent(const QString &innerTex, int hintLine, int index,
                              int *outIndex) const
{
    if (innerTex.isEmpty())
        return -1;
    int best = -1, bestOutIndex = -1, bestDist = INT_MAX;
    int exactBest = -1, exactOutIndex = -1, exactDist = INT_MAX;
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        const QString line = block.text();
        int i = 0;
        QRegularExpressionMatchIterator it = inlineMathRe().globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (mathInnerText(m.captured(0)) != innerTex) {
                ++i;
                continue;
            }
            const int dist = qAbs(block.blockNumber() - (hintLine - 1));
            if (i == index && dist < exactDist) {
                exactBest = block.blockNumber();
                exactOutIndex = i;
                exactDist = dist;
            }
            if (dist < bestDist) {
                best = block.blockNumber();
                bestOutIndex = i;
                bestDist = dist;
            }
            ++i;
        }
        block = block.next();
    }
    if (exactBest >= 0) {
        if (outIndex) *outIndex = exactOutIndex;
        return exactBest;
    }
    if (best >= 0 && outIndex) *outIndex = bestOutIndex;
    return best;
}

int Editor::sectionEndBlock(int fromBlock, int level) const
{
    QTextBlock block = document()->findBlockByNumber(fromBlock + 1);
    while (block.isValid()) {
        int bn = block.blockNumber();
        if (m_headerLevel.contains(bn)) {
            int otherLevel = m_headerLevel[bn];
            if (otherLevel <= level)
                return bn;
        }
        block = block.next();
    }
    return document()->blockCount();
}

void Editor::toggleFold(int blockNumber)
{
    if (!isFoldableBlock(blockNumber))
        return;

    m_updatingFolds = true;

    if (m_foldedBlocks.contains(blockNumber)) {
        // Unfold
        applyFold(blockNumber, false);
        m_foldedBlocks.remove(blockNumber);
        m_foldEndPins.remove(blockNumber);
    } else {
        // Fold
        applyFold(blockNumber, true);
        m_foldedBlocks.insert(blockNumber);
    }

    m_updatingFolds = false;

    if (m_gutter)
        m_gutter->setFoldedBlocks(m_foldedBlocks);

    document()->markContentsDirty(0, document()->characterCount());
    update();
}

int Editor::findPrevHeader(int fromBlock) const
{
    int best = -1;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it) {
        if (it.key() < fromBlock && (best < 0 || it.key() > best))
            best = it.key();
    }
    return best;
}

int Editor::findNextHeader(int fromBlock) const
{
    int best = -1;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it) {
        if (it.key() > fromBlock && (best < 0 || it.key() < best))
            best = it.key();
    }
    return best;
}

bool Editor::isHeaderBlock(int blockNumber) const
{
    return m_headerLevel.contains(blockNumber);
}

int Editor::headerLevelAt(int blockNumber) const
{
    auto it = m_headerLevel.find(blockNumber);
    return it != m_headerLevel.end() ? it.value() : 0;
}

bool Editor::insideFencedCode(int blockNumber) const
{
    int depth = 0;
    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() <= blockNumber) {
        if (block.text().trimmed().startsWith("```"))
            depth ^= 1;
        block = block.next();
    }
    return depth != 0;
}

void Editor::restoreFolds(const QList<int> &foldedBlocks)
{
    QSet<int> newFolds;
    for (int bn : foldedBlocks) {
        if (isFoldableBlock(bn))
            newFolds.insert(bn);
    }

    m_updatingFolds = true;
    // Unhide previously-folded blocks no longer in the set
    for (int bn : m_foldedBlocks) {
        if (!newFolds.contains(bn))
            applyFold(bn, false);
    }
    // Hide newly-folded blocks
    for (int bn : newFolds) {
        if (!m_foldedBlocks.contains(bn))
            applyFold(bn, true);
    }
    m_foldedBlocks = newFolds;
    for (auto it = m_foldEndPins.begin(); it != m_foldEndPins.end();) {
        if (!newFolds.contains(it.key()))
            it = m_foldEndPins.erase(it);
        else
            ++it;
    }
    m_updatingFolds = false;

    if (m_gutter)
        m_gutter->setFoldedBlocks(m_foldedBlocks);

    document()->markContentsDirty(0, document()->characterCount());
}

QList<int> Editor::foldedBlockNumbers() const
{
    QList<int> result;
    for (int bn : m_foldedBlocks)
        result.append(bn);
    std::sort(result.begin(), result.end());
    return result;
}

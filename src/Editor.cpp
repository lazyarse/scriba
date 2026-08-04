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
#include "SpellChecker.h"
#include "StaticHelpers.h"
#include <algorithm>
#include <QAbstractItemView>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>
#include <QAbstractTextDocumentLayout>

namespace {

// Vertical gap (px) between the bottom of the caret/active line and the top of
// the autocompletion popup. Kept small so the suggestions hug the text.
constexpr int kCompletionPopupGap = 4;

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

        QTextCursor cursor = textCursor();
        QString line = cursor.block().text();
        // Only auto-continue list markers when the caret is at the end of the
        // block; pressing Enter mid-line (or at the start) should split normally.
        if (cursor.positionInBlock() == line.length()) {
            QString result = handleListReturn(line);
            if (!result.isEmpty()) {
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
        }

        QTextBlock prevBlock = cursor.block().previous();
        // Only auto-continue table rows when the caret is at the end of the
        // block; pressing Enter mid-row (or at the start) should split normally.
        if (cursor.positionInBlock() == line.length()) {
            QString result = handleTableReturn(line, prevBlock.isValid() ? prevBlock.text() : QString());
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
                if (nextBlock.isValid() && nextBlock.text().contains("---")) {
                    QTextBlock block = nextBlock.next();
                    while (block.isValid()) {
                        QString t = block.text();
                        if (t.startsWith('|') && !t.contains("---")) {
                            QTextCursor tc = textCursor();
                            tc.setPosition(block.position() + 2, QTextCursor::MoveAnchor);
                            setTextCursor(tc);
                            return;
                        }
                        block = block.next();
                    }
                }

                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
                cursor.insertText("\n" + result);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                int cellPos = result.startsWith("<tr>") ? result.indexOf("<td>") + 4 : 2;
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, cellPos);
                setTextCursor(cursor);
                return;
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
                if (end == document()->blockCount() && m_headerLevel.contains(folded)
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
        if (line.startsWith('|') && !line.contains("---")) {
            int pos = cursor.positionInBlock();
            int cellPos = tableNavCell(line, pos, !shift);
            if (cellPos >= 0) {
                cursor.setPosition(cursor.block().position() + cellPos, QTextCursor::MoveAnchor);
                setTextCursor(cursor);
                return;
            }
            if (!shift) {
                QTextBlock block = cursor.block().next();
                while (block.isValid()) {
                    QString t = block.text();
                    if (t.startsWith('|') && !t.contains("---")) {
                        int p = 1;
                        if (p < t.size() && t[p] == ' ') ++p;
                        cursor.setPosition(block.position() + p, QTextCursor::MoveAnchor);
                        setTextCursor(cursor);
                        return;
                    }
                    block = block.next();
                }
                // No next row — create a new empty row
                int cols = line.count('|') - 1;
                if (cols > 0) {
                    QString newRow = makeEmptyTableRow(cols);
                    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
                    bool hasSep = false;
                    QTextBlock b = cursor.block().previous();
                    while (b.isValid() && b.text().startsWith('|')) {
                        if (b.text().contains("---")) { hasSep = true; break; }
                        b = b.previous();
                    }
                    QString sep = hasSep ? QString() : (QString("|") + QString("---|").repeated(cols) + "\n");
                    cursor.insertText("\n" + sep + newRow);
                    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
                    setTextCursor(cursor);
                    return;
                }
            } else {
                QTextBlock block = cursor.block().previous();
                while (block.isValid()) {
                    QString t = block.text();
                    if (t.startsWith('|') && !t.contains("---")) {
                        QList<int> pipes;
                        for (int i = 0; i < t.size(); ++i)
                            if (t[i] == '|') pipes.append(i);
                        if (pipes.size() >= 2) {
                            int p = pipes[pipes.size() - 2] + 1;
                            if (p < t.size() && t[p] == ' ') ++p;
                            cursor.setPosition(block.position() + p, QTextCursor::MoveAnchor);
                            setTextCursor(cursor);
                            return;
                        }
                    }
                    block = block.previous();
                }
            }
        }

        // HTML table cell navigation
        if (line.contains("<tr>") && line.contains("<td>")) {
            int pos = cursor.positionInBlock();
            int cellPos = tableNavHtmlCell(line, pos, !shift);
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
                    QString newRow = makeEmptyHtmlTableRow(cols);
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
        static const QRegularExpression orderedRe(R"(^\s*\d+\.\s?)");
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
                return;
            }
        } else {
            if (isList) {
                QString indented = indentListLine(line);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                cursor.insertText(indented);
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
    QFile file(":/emoji.js");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(file.readAll());
    static const QRegularExpression re(R"('([a-z0-9_+\-]+)'\s*:\s*'([^']+)')");
    auto it = re.globalMatch(content);
    while (it.hasNext()) {
        auto match = it.next();
        QString name = match.captured(1);
        QString unicode = match.captured(2);
        m_emojiShortcodes.append(name);
        m_emojiUnicode[name] = unicode;
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
    QStringList matchedCodes;
    for (const QString &sc : m_emojiShortcodes) {
        if (sc.contains(partialCode, Qt::CaseInsensitive))
            matchedCodes.append(sc);
    }

    if (matchedCodes.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    std::sort(matchedCodes.begin(), matchedCodes.end(),
        [&partialCode](const QString &a, const QString &b) {
            bool aPrefix = a.startsWith(partialCode, Qt::CaseInsensitive);
            bool bPrefix = b.startsWith(partialCode, Qt::CaseInsensitive);
            if (aPrefix != bPrefix)
                return aPrefix;
            return a < b;
        });

    int limit = QSettings().value(Preferences::EmojiCompletionLimit, 100).toInt();
    if (matchedCodes.size() > limit)
        matchedCodes = matchedCodes.mid(0, limit);

    QStringList matches;
    for (const QString &sc : matchedCodes)
        matches.append(QString(":%1:").arg(sc));

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
    for (const QString &sc : matchedCodes) {
        auto *item = new QStandardItem(QString(":%1:").arg(sc));
        item->setIcon(QIcon(renderEmojiIcon(m_emojiUnicode.value(sc))));
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

    QPixmap pix(18, 18);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    QString mode = QSettings().value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    if (mode == "color") {
        QStringList parts;
        for (int i = 0; i < emojiStr.size();) {
            uint code = emojiStr[i].unicode();
            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < emojiStr.size()) {
                uint low = emojiStr[i + 1].unicode();
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            } else {
                i += 1;
            }
            parts.append(QString::number(code, 16));
        }
        QStringList stripped = parts;
        stripped.removeAll("fe0f");
        QString svgPath;
        for (const QString &candidate : {stripped.join("-"), parts.join("-")}) {
            QString path = QString(":/twemoji/svg/%1.svg").arg(candidate);
            if (QFile::exists(path)) {
                svgPath = path;
                break;
            }
        }
        if (!svgPath.isEmpty()) {
            QSvgRenderer renderer(svgPath);
            renderer.render(&painter, QRectF(0, 0, 18, 18));
        } else {
            QFont font("Symbola");
            font.setPixelSize(16);
            painter.setFont(font);
            painter.setPen(Qt::black);
            painter.drawText(QRect(0, 0, 18, 18), Qt::AlignCenter, emojiStr);
        }
    } else {
        QFont font("Symbola");
        font.setPixelSize(16);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, 0, 18, 18), Qt::AlignCenter, emojiStr);
    }

    painter.end();
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
    QSettings s;
    const bool spellEnabled = s.value(Preferences::SpellCheckEnabled, true).toBool();
    const bool grammarEnabled = s.value(Preferences::GrammarCheckEnabled, false).toBool();
    const bool linkEnabled = s.value(Preferences::LinkCheckEnabled, true).toBool();
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

    if (auto *stoppard = dynamic_cast<StoppardEngine *>(m_grammarChecker.get()))
        stoppard->setDialect(dialect);

    m_spellHighlighter->refresh();
}

void Editor::recheckSpelling()
{
    applySpellSettings();
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

Editor::CursorContext Editor::detectCursorContext() const
{
    QString line = currentLineText();

    static const QRegularExpression listRe(R"(^\s*[-*+]\s|^\s*\d+\.\s)");
    if (listRe.match(line).hasMatch())
        return CursorContext::ListItem;

    if (line.startsWith('|') && !line.contains("---"))
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
    if (!line.startsWith('|'))
        return;

    int pipes = line.count('|');
    QString newRow;
    for (int i = 0; i < pipes; ++i) {
        newRow += "| ";
    }
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

void Editor::insertTableCol(bool left)
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!line.startsWith('|'))
        return;

    int pos = cursor.positionInBlock();
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|') pipes.append(i);

    int insertAfterPipe = -1;
    for (int i = 0; i < pipes.size() - 1; ++i) {
        if (pos >= pipes[i] && pos <= pipes[i + 1]) {
            insertAfterPipe = i;
            break;
        }
    }
    if (insertAfterPipe < 0)
        return;

    int colOffset = pipes[insertAfterPipe] + 1;

    QTextBlock block = cursor.block();
    while (block.isValid() && block.text().startsWith('|')) {
        QString text = block.text();
        QList<int> bPipes;
        for (int i = 0; i < text.size(); ++i)
            if (text[i] == '|') bPipes.append(i);

        if (insertAfterPipe < bPipes.size() - 1) {
            int insertPos = bPipes[insertAfterPipe] + 1;
            bool isSep = text.contains("---");
            QString insert = isSep ? " --- " : "  ";
            QTextCursor tc(document());
            tc.setPosition(block.position() + insertPos);
            tc.insertText(insert);
        }
        block = block.next();
    }
}

void Editor::deleteTableRow()
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!line.startsWith('|'))
        return;

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    if (line.contains("---")) {
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
    if (!line.startsWith('|'))
        return;

    int pos = cursor.positionInBlock();
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|') pipes.append(i);

    int colIdx = -1;
    for (int i = 0; i < pipes.size() - 1; ++i) {
        if (pos >= pipes[i] && pos <= pipes[i + 1]) {
            colIdx = i;
            break;
        }
    }
    if (colIdx < 0)
        return;

    QTextBlock b = document()->firstBlock();
    while (b.isValid()) {
        if (b.text().startsWith('|')) {
            QList<int> bPipes;
            QString text = b.text();
            for (int i = 0; i < text.size(); ++i)
                if (text[i] == '|') bPipes.append(i);

            if (colIdx < bPipes.size() - 1) {
                int start = bPipes[colIdx];
                int end = (colIdx + 1 < bPipes.size()) ? bPipes[colIdx + 1] : text.size();
                if (bPipes[colIdx + 1] == bPipes.last() && colIdx + 1 == bPipes.size() - 1) {
                    end = text.size() - 1;
                }
                QTextCursor tc(document());
                tc.setPosition(b.position() + start);
                tc.setPosition(b.position() + end, QTextCursor::KeepAnchor);
                tc.removeSelectedText();
            }
        }
        b = b.next();
    }
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

        menu.addSeparator();
        QAction *addAction = menu.addAction("Add to Dictionary");
        connect(addAction, &QAction::triggered, this, [this, misspelled]() {
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
    int available = width() - 2 * frameWidth() - gutterW;
    int scrollbarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
    available -= scrollbarWidth;
    int margin = qMax(0, (available - m_centerContentWidth) / 2);
    setViewportMargins(margin + gutterW, 0, margin, 0);
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
    bool showGutter = s.value(Preferences::ShowGutter, true).toBool();
    m_gutter->setLineNumbersVisible(s.value(Preferences::ShowLineNumbers, true).toBool());
    applyGutterColors();
    if (showGutter) {
        updateGutterWidth();
    } else {
        m_gutter->setFixedWidth(0);
        updateViewportMargins();
    }
}

void Editor::toggleGutter()
{
    QSettings s;
    bool show = !s.value(Preferences::ShowGutter, true).toBool();
    s.setValue(Preferences::ShowGutter, show);
    s.sync();
    updateGutterSettings();
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
    QSettings s;
    if (s.value(Preferences::ShowGutter, true).toBool())
        m_gutter->updateWidth();
    else
        m_gutter->setFixedWidth(0);
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

    QTextBlock block = document()->firstBlock();
    int codeDepth = 0;
    QRegularExpression atxRe("^(#{1,6})\\s");
    QRegularExpression setextUnderlineRe("^(={3,}|-{3,})\\s*$");

    // First pass: detect headers and fenced-code openings
    QTextBlock prevBlock;
    while (block.isValid()) {
        QString text = block.text();
        int bn = block.blockNumber();

        if (text.trimmed().startsWith("```")) {
            if (codeDepth == 0)
                m_codeFences.insert(bn);
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
        }

        prevBlock = block;
        block = block.next();
    }

    // Update gutter with foldable info
    QSet<int> foldableSet;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it)
        foldableSet.insert(it.key());
    foldableSet.unite(m_codeFences);
    if (m_gutter) {
        m_gutter->setFoldableBlocks(foldableSet);
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
        if (!m_headerLevel.contains(it.key()) || !document()->findBlock(it.value()).isValid())
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
    return m_headerLevel.contains(blockNumber) || m_codeFences.contains(blockNumber);
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

    int end = sectionEndBlock(startBlock, m_headerLevel.value(startBlock));
    auto pinIt = m_foldEndPins.find(startBlock);
    if (pinIt != m_foldEndPins.end()) {
        QTextBlock pinned = document()->findBlock(pinIt.value());
        if (pinned.isValid() && pinned.blockNumber() > startBlock && pinned.blockNumber() <= end)
            end = pinned.blockNumber();
    }
    return end;
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

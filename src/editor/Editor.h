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

#include <QHash>
#include <QPixmap>
#include <QTextEdit>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <memory>
#include "spell/SpellHighlighter.h"

class QCompleter;
class QAction;
class QContextMenuEvent;
class QFontMetrics;
class QKeyEvent;
class Gutter;
class SpellChecker;
class GrammarChecker;
struct CorpusDictionary;

class Editor : public QTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;
    void setCurrentFile(const QString &path);
    void setCenterContent(bool enabled, int width);
    // Reads the EditorWrap* settings and applies the editor's line wrap mode
    // (no-wrap / window width / fixed column count). Also re-computes the
    // centred content width when wrapping at a column. Call after a wrap
    // preference change.
    void applyLineWrap();
    QMargins contentMargins() const;
    void centerCursor();
    void invalidateEmojiIconCache();
    QCompleter *completer() const { return m_completer; }
    void setInsertActions(const QList<QAction *> &actions);
    void setMermaidAction(QAction *action);
    void recheckSpelling();
    // Push an active corpus's dictionary (word sets, language, dialect and the
    // override/merge mode) onto this editor's spell checker and rehighlight.
    void applyCorpusDictionary(const CorpusDictionary &dict, bool merge);
    // Reloads the underline colors from settings and repaints the squiggle
    // overlay. Call after a preference change that doesn't need a full
    // re-check (e.g. the underline color swatches).
    void refreshUnderlines();
    // Re-aligns the markdown table whose block contains `documentPos` (used
    // right after the Insert Table dialog drops a table into the document).
    // No-op when the block isn't a table row or the auto-align preference is
    // off.
    void formatTableAt(int documentPos);

    SpellChecker *spellChecker() const { return m_spellChecker.get(); }
    SpellHighlighter *spellHighlighter() const { return m_spellHighlighter; }

    // Helpers for the preview "edit chart/equation" feature: locate a fenced
    // code block (` ``` `) by its opening-fence block number and read or
    // replace it, and locate/replace inline math (`$…$` / `$$…$$`) occurrences
    // within a single block.
    QPair<int, int> fencedCodeBlockRange(int blockNumber) const;
    QString blockRangeText(int firstBlock, int endExclusive) const;
    void replaceBlockRange(int firstBlock, int endExclusive, const QString &text);
    int findNthInlineMath(int blockNumber, int index) const;
    void replaceInlineMath(int blockNumber, int index, const QString &replacement);

    // Preview "edit" bridge helpers. The preview's data-line attributes are
    // only approximate (see MdRenderer), so the anchor fragment carries the
    // chart source / equation text and these resolve it back to a real block:
    //  - fenceBody(): the code block's inner text (fence lines stripped).
    //  - findFenceByBody(): the fenced block whose inner text matches `body`
    //    (trailing-newline tolerant), preferring the one nearest `hintLine`
    //    (1-based) when several match; returns -1 when nothing matches.
    //  - findMathByContent(): the block containing an inline/display math
    //    span whose inner text equals `innerTex`; prefers a match whose index
    //    on the line equals `index` and, among ties, the one nearest
    //    `hintLine`. On success sets *outIndex to the span's index on the
    //    returned block and returns its block number; otherwise -1.
    QString fenceBody(int blockNumber) const;
    int findFenceByBody(const QString &body, int hintLine) const;
    // The language token of the fenced block whose opening fence is
    // `blockNumber` (e.g. "mermaid", "ec"); empty when not a fence.
    QString fenceLanguage(int blockNumber) const;
    int findMathByContent(const QString &innerTex, int hintLine, int index,
                          int *outIndex = nullptr) const;

    // Highlights the given range with a background highlight (not an
    // underline) and moves the cursor there — used by the Check Spelling
    // dialog to point at the current error. Cleared on document edits.
    void setSpellCheckHighlight(int blockNumber, int start, int length);
    void clearSpellCheckHighlight();

    // The lint explanation shown when the mouse hovers the given block/offset:
    // the markdown, grammar, broken-link or misspelled-word message under it,
    // or an empty string when nothing is flagged there. Pure (no GUI side
    // effects) so it is unit-testable.
    QString explanationAt(int blockNumber, int positionInBlock);

    static constexpr int kUnderlineDropPx = 2;     // extra px below fm.underlinePos()
    static constexpr int kUnderlinePenWidthPx = 3; // thickness of painted underlines

signals:
    // The gutter's pencil for a chart block (```mermaid/```ec) was clicked.
    // `blockNumber` is the opening-fence block of that chart.
    void chartEditRequested(int blockNumber);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void insertParagraphWithLineHeight(QKeyEvent *event);
    bool isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const;
    bool isInsideHtmlPathContext(const QTextCursor &cursor, QString &partialPath) const;
    void applyAutoCorrect(bool separatorTyped);
    bool handleEscapeKey();                              // completer popup dismiss
    bool handleEnterKey(QKeyEvent *event);               // list/table/fold return
    bool handleTabKey(QKeyEvent *event);                 // indent/dedent incl. Backtab
    bool handleDownKey();                                // fold expand
    bool handleCtrlAltScroll(int key);                   // viewport scroll
    bool handleHeaderJump(int direction);                // Ctrl+Up/Down
    bool handleZoom(int direction);                      // Ctrl+=/-
    bool handleHardwrap();                               // Ctrl+D
    bool handleBackspaceDelete(QKeyEvent *event);        // Backspace/Delete list ops
    bool isInsideInlineCode() const;
    bool showFileCompletion(const QString &partialPath);
    void acceptCompletion(const QString &completion);
    void positionCompletionPopup(const QRect &cursorRect);

    void loadEmojiShortcodes();
    bool isInsideEmojiContext(const QTextCursor &cursor, QString &partialCode) const;
    bool showEmojiCompletion(const QString &partialCode);
    void acceptEmojiCompletion(const QString &completion);
    QPixmap renderEmojiIcon(const QString &emojiStr) const;

    bool isInsideLanguageContext(const QTextCursor &cursor, QString &partialLang) const;
    bool showLanguageCompletion(const QString &partialLang);

    void applySpellSettings();
    // Push an active corpus's dictionary (word sets, language, dialect and the
    // override/merge mode) onto this editor's spell checker and rehighlight.
    SpellHighlighter::WordHit misspelledWordAt(const QTextCursor &cursor) const;
    void paintHitRange(QPainter &painter, const QTextBlock &block, int start, int length,
                       const QColor &color, const QFontMetrics &fm, int underlineY);

    void updateViewportMargins();
    // True when line wrapping is on and configured to wrap at a fixed column
    // count. In that mode the column width takes over as the editor's
    // effective max content width (see updateViewportMargins()).
    bool wrapAtColumnActive() const;
    // Pixel width of the configured wrap column measured in the editor's
    // current font (the width of `column` widest characters). Only meaningful
    // when wrapAtColumnActive().
    qreal wrapColumnPx() const;

    enum class CursorContext { None, ListItem, TableRow, CodeBlock };
    CursorContext detectCursorContext() const;
    bool isCursorInFencedCodeBlock() const;
    bool cursorHasUrlSelection() const;
    bool cursorHasImagePathSelection() const;
    void toggleCheckbox();
    void insertTableRow(bool above);
    void insertTableCol(bool left);
    void deleteTableRow();
    void deleteTableCol();
    void deleteLine();
    void changeCodeLanguage();
    QString currentLineText() const;

    // Auto-alignment of markdown table source (see Preferences::AutoAlignTables)
    void onCursorPositionChanged();
    void formatMdTableBlock(int startBlock);
    // Configured cell padding (spaces around each cell's content) for table
    // formatting and new-row creation; clamped to the 0..4 spin-box range.
    int tablePadding() const;

    QString m_currentFile;
    QCompleter *m_completer = nullptr;
    QStringList m_emojiShortcodes;
    QHash<QString, QString> m_emojiUnicode;
    mutable QHash<QString, QPixmap> m_emojiIconCache;
    bool m_centerContent = false;
    int m_centerContentWidth = 800;
    bool m_inResize = false;
    QList<QAction *> m_insertActions;
    QAction *m_mermaidAction = nullptr;

    // Table auto-alignment state. While the cursor sits in a `|`-delimited
    // table row, m_trackTableStartBlock is the first block of that table and
    // m_tableDirty records whether its text changed. When the cursor moves
    // off a dirty table, it gets reformatted (see onCursorPositionChanged).
    int m_trackTableStartBlock = -1;
    bool m_tableDirty = false;
    bool m_formattingTable = false;

    std::unique_ptr<SpellChecker> m_spellChecker;
    // Shared ownership with the grammar-lint worker: the worker keeps its own
    // shared_ptr for the duration of an in-flight check, so this editor can be
    // destroyed (tab closed) without the checker dangling.
    std::shared_ptr<GrammarChecker> m_grammarChecker;
    SpellHighlighter *m_spellHighlighter = nullptr;
    QWidget *m_underlineOverlay = nullptr;
    // True once a corpus dictionary has been applied; routes the "Add to
    // Dictionary" context action to the corpus word set instead of the global
    // user.dic. Sticky for the editor's lifetime (a corpus, once opened, stays
    // active until the app closes or another corpus supersedes it).
    bool m_corpusActive = false;
    // The explanation currently shown in the hover tooltip, so identical
    // hovers do not re-trigger QToolTip::showText on every mouse move.
    QString m_activeTooltip;

    // Gutter + folding
    Gutter *m_gutter = nullptr;
    QMap<int, int> m_headerLevel; // blockNumber → heading level 1-6
    QSet<int> m_codeFences;        // blockNumber of each opening ``` fence (foldable)
    QSet<int> m_chartFences;       // blockNumber of each opening ```mermaid/```ec fence
    QSet<int> m_mdTableSeparators; // blockNumber of each markdown table separator row (foldable)
    QSet<int> m_htmlTables;        // blockNumber of each line opening a multi-line <table…</table>
    QSet<int> m_listItems;         // blockNumber of each list-item marker line (foldable)
    QSet<int> m_foldedBlocks;
    QMap<int, qsizetype> m_foldEndPins; // folded header blockNumber → pinned fold-bottom character position
    bool m_updatingFolds = false;

    void setupGutter();
    void updateGutterWidth();
    void applyGutterColors();
    void scanHeadersAndFolds();
    void applyFold(int startBlock, bool hide);
    bool isFoldableBlock(int blockNumber) const;
    int foldEnd(int startBlock) const;
    int listFoldEnd(int startBlock) const;
    bool listItemIsFirst(int startBlock, int quoteDepth, int indent) const;
    bool listAnchorHasContent(int blockNumber) const;
    void renumberNestedOrderedList(int startBlock);
    void renumberOutdentedOrderedList(int startBlock);
    bool foldRegionContains(int startBlock, int blockNumber) const;
    void toggleFold(int blockNumber);
    int findPrevHeader(int fromBlock) const;
    int findNextHeader(int fromBlock) const;
    bool isHeaderBlock(int blockNumber) const;
    int headerLevelAt(int blockNumber) const;
    bool insideFencedCode(int blockNumber) const;
    int sectionEndBlock(int fromBlock, int level) const;

    void updateGutterDelayed();

public:
    void restoreFolds(const QList<int> &foldedBlocks);
    QList<int> foldedBlockNumbers() const;
    void updateGutter();
    void updateGutterSettings();
    void refreshGutter();
    Gutter *gutter() const { return m_gutter; }
};


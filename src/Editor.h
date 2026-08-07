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
#include "SpellHighlighter.h"

class QCompleter;
class QAction;
class QContextMenuEvent;
class QFontMetrics;
class Gutter;
class SpellChecker;
class GrammarChecker;

class Editor : public QTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;
    void setCurrentFile(const QString &path);
    void setCenterContent(bool enabled, int width);
    QMargins contentMargins() const;
    void centerCursor();
    void invalidateEmojiIconCache();
    QCompleter *completer() const { return m_completer; }
    void setInsertActions(const QList<QAction *> &actions);
    void setMermaidAction(QAction *action);
    void recheckSpelling();
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
    SpellHighlighter::WordHit misspelledWordAt(const QTextCursor &cursor) const;
    void paintHitRange(QPainter &painter, const QTextBlock &block, int start, int length,
                       const QColor &color, const QFontMetrics &fm, int underlineY);

    void updateViewportMargins();

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
    std::unique_ptr<GrammarChecker> m_grammarChecker;
    SpellHighlighter *m_spellHighlighter = nullptr;
    QWidget *m_underlineOverlay = nullptr;
    // The explanation currently shown in the hover tooltip, so identical
    // hovers do not re-trigger QToolTip::showText on every mouse move.
    QString m_activeTooltip;

    // Gutter + folding
    Gutter *m_gutter = nullptr;
    QMap<int, int> m_headerLevel; // blockNumber → heading level 1-6
    QSet<int> m_codeFences;        // blockNumber of each opening ``` fence (foldable)
    QSet<int> m_chartFences;       // blockNumber of each opening ```mermaid/```ec fence
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
    void toggleGutter();
    Gutter *gutter() const { return m_gutter; }
};


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

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void insertParagraphWithLineHeight(QKeyEvent *event);
    bool isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const;
    bool isInsideHtmlPathContext(const QTextCursor &cursor, QString &partialPath) const;
    void showFileCompletion(const QString &partialPath);
    void acceptCompletion(const QString &completion);

    void loadEmojiShortcodes();
    bool isInsideEmojiContext(const QTextCursor &cursor, QString &partialCode) const;
    void showEmojiCompletion(const QString &partialCode);
    void acceptEmojiCompletion(const QString &completion);
    QPixmap renderEmojiIcon(const QString &emojiStr) const;

    bool isInsideLanguageContext(const QTextCursor &cursor, QString &partialLang) const;
    void showLanguageCompletion(const QString &partialLang);

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
    void changeCodeLanguage();
    QString currentLineText() const;

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

    std::unique_ptr<SpellChecker> m_spellChecker;
    GrammarChecker *m_grammarChecker = nullptr;
    SpellHighlighter *m_spellHighlighter = nullptr;
    QWidget *m_underlineOverlay = nullptr;

    // Gutter + folding
    Gutter *m_gutter = nullptr;
    QMap<int, int> m_headerLevel; // blockNumber → heading level 1-6
    QSet<int> m_foldedHeaders;
    bool m_updatingFolds = false;

    void setupGutter();
    void updateGutterWidth();
    void applyGutterColors();
    void scanHeadersAndFolds();
    void applyFoldForHeader(int blockNumber, int level, bool hide);
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


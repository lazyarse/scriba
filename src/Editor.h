#pragma once

#include <QHash>
#include <QPixmap>
#include <QTextEdit>
#include <QStringList>
#include <QMap>
#include <QSet>

class QCompleter;
class QAction;
class QContextMenuEvent;
class Gutter;

class Editor : public QTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    void setCurrentFile(const QString &path);
    void setCenterContent(bool enabled, int width);
    QMargins contentMargins() const;
    void centerCursor();
    void invalidateEmojiIconCache();
    QCompleter *completer() const { return m_completer; }
    void setInsertActions(const QList<QAction *> &actions);
    void setMermaidAction(QAction *action);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
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
};


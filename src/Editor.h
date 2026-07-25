#ifndef EDITOR_H
#define EDITOR_H

#include <QHash>
#include <QPixmap>
#include <QTextEdit>
#include <QPoint>
#include <QStringList>

class QCompleter;
class QAction;
class QContextMenuEvent;

class Editor : public QTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    void setCurrentFile(const QString &path);
    void setCenterContent(bool enabled, int width);
    void centerCursor();
    QCompleter *completer() const { return m_completer; }
    void setInsertActions(const QList<QAction *> &actions);
    void setMermaidActions(const QList<QAction *> &actions);

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

    void repositionPopup();
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
    QList<QAction *> m_mermaidActions;
};

#endif

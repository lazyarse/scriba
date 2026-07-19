#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QPoint>
#include <QStringList>

class QCompleter;

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    void setCurrentFile(const QString &path);
    QCompleter *completer() const { return m_completer; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const;
    void showFileCompletion(const QString &partialPath);
    void acceptCompletion(const QString &completion);

    void loadEmojiShortcodes();
    bool isInsideEmojiContext(const QTextCursor &cursor, QString &partialCode) const;
    void showEmojiCompletion(const QString &partialCode);
    void acceptEmojiCompletion(const QString &completion);

    void repositionPopup();

    QString m_currentFile;
    QCompleter *m_completer = nullptr;
    QStringList m_emojiShortcodes;
};

#endif

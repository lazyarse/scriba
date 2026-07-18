#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class QCompleter;

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    void setCurrentFile(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const;
    void showFileCompletion(const QString &partialPath);
    void acceptCompletion(const QString &completion);

    QString m_currentFile;
    QCompleter *m_completer = nullptr;
};

#endif

#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    void applyStylesheet(const QString &css);

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

inline Editor* getEditorWidget() { return nullptr; } // placeholder

#endif

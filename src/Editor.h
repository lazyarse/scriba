#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QColor>

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    void applyFontSettings(const QString &family, int size, const QColor &color);
    void loadSettings();
    int firstVisibleLineNumber() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

#endif

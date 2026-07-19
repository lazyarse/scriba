#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QDialog>
#include <QTextEdit>

class LogWindow : public QDialog
{
    Q_OBJECT

public:
    enum Level { Info, Warning, Error };
    Q_ENUM(Level)

    explicit LogWindow(QWidget *parent = nullptr);
    ~LogWindow() override;

    void addEntry(Level level, const QString &source, const QString &message);

private:
    QTextEdit *m_output;
};

#endif

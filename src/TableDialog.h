#ifndef TABLEDIALOG_H
#define TABLEDIALOG_H

#include <QDialog>

class QSpinBox;
class QCheckBox;

class TableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TableDialog(QWidget *parent = nullptr);

    QString generateTable() const;
    bool hasHeader() const;

private:
    QSpinBox *m_columns;
    QCheckBox *m_includeHeader;
};

#endif

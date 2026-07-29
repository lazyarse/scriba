#pragma once

#include <QDialog>

class QSpinBox;
class QCheckBox;
class QRadioButton;

class TableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TableDialog(QWidget *parent = nullptr);

    QString generateTable() const;
    bool hasHeader() const;
    bool isHtml() const;

private:
    QSpinBox *m_columns;
    QCheckBox *m_includeHeader;
    QRadioButton *m_markdownRadio;
    QRadioButton *m_htmlRadio;
    QWidget *m_formatWidget;
};


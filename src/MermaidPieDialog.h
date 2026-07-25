#ifndef MERMAIDPIEDIALOG_H
#define MERMAIDPIEDIALOG_H

#include "MermaidDialogBase.h"

class QTableWidget;
class QLineEdit;

class MermaidPieDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidPieDialog(const QString &themeCss, QWidget *parent = nullptr);

protected:
    QString buildDiagram() const override;

private:
    void setupUi();

    QLineEdit *m_titleEdit;
    QTableWidget *m_table;
};

#endif

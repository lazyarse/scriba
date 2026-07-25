#ifndef MERMAIDQUADRANTDIALOG_H
#define MERMAIDQUADRANTDIALOG_H

#include "MermaidDialogBase.h"

class QTableWidget;
class QLineEdit;

class MermaidQuadrantDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidQuadrantDialog(const QString &themeCss, QWidget *parent = nullptr);

protected:
    QString buildDiagram() const override;

private:
    void setupUi();

    QLineEdit *m_titleEdit;
    QLineEdit *m_xAxisEdit, *m_xAxisRightEdit;
    QLineEdit *m_yAxisEdit, *m_yAxisTopEdit;
    QLineEdit *m_q1Edit, *m_q2Edit, *m_q3Edit, *m_q4Edit;
    QTableWidget *m_table;
};

#endif

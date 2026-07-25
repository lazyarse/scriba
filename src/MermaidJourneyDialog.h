#ifndef MERMAIDJOURNEYDIALOG_H
#define MERMAIDJOURNEYDIALOG_H

#include "MermaidDialogBase.h"

class QTableWidget;
class QLineEdit;

class MermaidJourneyDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidJourneyDialog(const QString &themeCss, QWidget *parent = nullptr);

protected:
    QString buildDiagram() const override;

private:
    void setupUi();

    QLineEdit *m_titleEdit;
    QTableWidget *m_table;
};

#endif

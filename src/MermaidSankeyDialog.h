#ifndef MERMAIDSANKEYDIALOG_H
#define MERMAIDSANKEYDIALOG_H

#include "MermaidDialogBase.h"

class QTableWidget;

class MermaidSankeyDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidSankeyDialog(const QString &themeCss, QWidget *parent = nullptr);

protected:
    QString buildDiagram() const override;

private:
    void setupUi();

    QTableWidget *m_table;
};

#endif

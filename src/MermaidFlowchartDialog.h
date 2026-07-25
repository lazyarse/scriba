#ifndef MERMAIDFLOWCHARTDIALOG_H
#define MERMAIDFLOWCHARTDIALOG_H

#include "MermaidDialogBase.h"

class QComboBox;
class QTableWidget;

class MermaidFlowchartDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidFlowchartDialog(const QString &themeCss, QWidget *parent = nullptr);

private slots:
    void onNodeChanged();

private:
    void setupUi();
    void refreshEdgeNodeCombos();
    QString buildDiagram() const override;
    static QString shapeToMermaid(const QString &shape, const QString &text);
    static QString renderEdge(const QString &from, const QString &to,
                              const QString &label, const QString &arrowType);

    QComboBox *m_directionCombo;
    QTableWidget *m_nodeTable;
    QTableWidget *m_edgeTable;
};

#endif

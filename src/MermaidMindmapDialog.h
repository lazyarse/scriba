#pragma once

#include "MermaidDialogBase.h"

class QTreeWidget;
class QTreeWidgetItem;

class MermaidMindmapDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidMindmapDialog(const QString &themeCss, QWidget *parent = nullptr);

private slots:
    void addChild();
    void addSibling();
    void deleteNode();

protected:
    QString buildDiagram() const override;

private:
    QString buildNode(QTreeWidgetItem *item, int depth) const;

    QTreeWidget *m_tree;
};

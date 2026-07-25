#pragma once

#include "MermaidDialogBase.h"
#include <QMap>

class QTableWidget;
class QComboBox;

class MermaidErDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidErDialog(const QString &themeCss, QWidget *parent = nullptr);

private slots:
    void updateAttributeTable();
    void refreshRelationCombos();
    void addEntity();
    void removeEntity();
    void addAttribute();
    void removeAttribute();
    void addRelation();
    void removeRelation();

protected:
    QString buildDiagram() const override;

private:
    void saveCurrentAttributes();
    void loadAttributes(int entityRow);
    void setupDefaultData();

    QTableWidget *m_entitiesTable;
    QTableWidget *m_attributesTable;
    QTableWidget *m_relationsTable;

    QMap<int, QList<QMap<QString, QString>>> m_entityAttributes;
    int m_lastEntityRow = -1;
};

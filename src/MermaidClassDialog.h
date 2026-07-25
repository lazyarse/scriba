#pragma once

#include "MermaidDialogBase.h"
#include <QMap>

class QTableWidget;
class QComboBox;

class MermaidClassDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidClassDialog(const QString &themeCss, QWidget *parent = nullptr);

private slots:
    void updateFieldTable();
    void updateMethodTable();
    void refreshRelationCombos();
    void addClass();
    void removeClass();
    void addField();
    void removeField();
    void addMethod();
    void removeMethod();
    void addRelation();
    void removeRelation();

protected:
    QString buildDiagram() const override;

private:
    void saveCurrentClassData();
    void loadClassData(int classRow);
    void setupDefaultData();

    QTableWidget *m_classesTable;
    QTableWidget *m_fieldsTable;
    QTableWidget *m_methodsTable;
    QTableWidget *m_relationsTable;

    struct ClassData {
        QList<QMap<QString, QString>> fields;
        QList<QMap<QString, QString>> methods;
    };
    QMap<int, ClassData> m_classData;
    int m_lastClassRow = -1;
};

#pragma once

#include <QDialog>
#include <QMap>

class QTableWidget;
class QComboBox;
class QWebEngineView;
class QTimer;

class MermaidClassDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidClassDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void updatePreview();
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

private:
    void schedulePreviewUpdate();
    void saveCurrentClassData();
    void loadClassData(int classRow);
    void setupDefaultData();

    QTableWidget *m_classesTable;
    QTableWidget *m_fieldsTable;
    QTableWidget *m_methodsTable;
    QTableWidget *m_relationsTable;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;

    struct ClassData {
        QList<QMap<QString, QString>> fields;
        QList<QMap<QString, QString>> methods;
    };
    QMap<int, ClassData> m_classData;
    int m_lastClassRow = -1;
};

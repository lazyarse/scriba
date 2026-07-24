#pragma once

#include <QDialog>
#include <QMap>

class QTableWidget;
class QComboBox;
class QWebEngineView;
class QTimer;

class MermaidErDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidErDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void updatePreview();
    void updateAttributeTable();
    void refreshRelationCombos();
    void addEntity();
    void removeEntity();
    void addAttribute();
    void removeAttribute();
    void addRelation();
    void removeRelation();

private:
    void schedulePreviewUpdate();
    void saveCurrentAttributes();
    void loadAttributes(int entityRow);
    void setupDefaultData();

    QTableWidget *m_entitiesTable;
    QTableWidget *m_attributesTable;
    QTableWidget *m_relationsTable;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;

    QMap<int, QList<QMap<QString, QString>>> m_entityAttributes;
    int m_lastEntityRow = -1;
};

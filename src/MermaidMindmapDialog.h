#pragma once

#include <QDialog>

class QTreeWidget;
class QTreeWidgetItem;
class QWebEngineView;
class QTimer;

class MermaidMindmapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidMindmapDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void updatePreview();
    void addChild();
    void addSibling();
    void deleteNode();

private:
    void schedulePreviewUpdate();
    QString buildDiagram() const;
    QString buildNode(QTreeWidgetItem *item, int depth) const;

    QTreeWidget *m_tree;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

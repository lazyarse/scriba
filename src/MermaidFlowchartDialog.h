#ifndef MERMAIDFLOWCHARTDIALOG_H
#define MERMAIDFLOWCHARTDIALOG_H

#include <QDialog>

class QComboBox;
class QTableWidget;
class QWebEngineView;
class QTimer;

class MermaidFlowchartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidFlowchartDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void onNodeChanged();
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    void refreshEdgeNodeCombos();
    QString buildDiagram() const;
    static QString shapeToMermaid(const QString &shape, const QString &text);
    static QString renderEdge(const QString &from, const QString &to,
                              const QString &label, const QString &arrowType);

    QComboBox *m_directionCombo;
    QTableWidget *m_nodeTable;
    QTableWidget *m_edgeTable;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif
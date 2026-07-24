#ifndef MERMAIDSANKEYDIALOG_H
#define MERMAIDSANKEYDIALOG_H

#include <QDialog>

class QTableWidget;
class QWebEngineView;
class QTimer;

class MermaidSankeyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidSankeyDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    QString buildDiagram() const;

    QTableWidget *m_table;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif
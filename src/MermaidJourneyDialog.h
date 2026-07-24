#ifndef MERMAIDJOURNEYDIALOG_H
#define MERMAIDJOURNEYDIALOG_H

#include <QDialog>

class QTableWidget;
class QWebEngineView;
class QLineEdit;
class QTimer;

class MermaidJourneyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidJourneyDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    QString buildDiagram() const;

    QLineEdit *m_titleEdit;
    QTableWidget *m_table;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif

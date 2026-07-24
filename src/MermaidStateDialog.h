#ifndef MERMAIDSTATEDIALOG_H
#define MERMAIDSTATEDIALOG_H

#include <QDialog>

class QComboBox;
class QTableWidget;
class QWebEngineView;
class QTimer;

class MermaidStateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidStateDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void onStateChanged();
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    void refreshTransitionCombos();
    QString buildDiagram() const;

    QTableWidget *m_stateTable;
    QTableWidget *m_transitionTable;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif

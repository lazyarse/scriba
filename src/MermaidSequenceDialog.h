#ifndef MERMAIDSEQUENCEDIALOG_H
#define MERMAIDSEQUENCEDIALOG_H

#include <QDialog>

class QComboBox;
class QTableWidget;
class QWebEngineView;
class QTimer;

class MermaidSequenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidSequenceDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void onParticipantChanged();
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    void refreshMessageCombos();
    QString buildDiagram() const;

    QTableWidget *m_participantTable;
    QTableWidget *m_messageTable;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif

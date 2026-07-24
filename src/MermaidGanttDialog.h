#ifndef MERMAIDGANTTDIALOG_H
#define MERMAIDGANTTDIALOG_H

#include <QDialog>

class QComboBox;
class QTableWidget;
class QWebEngineView;
class QLineEdit;
class QCheckBox;
class QTimer;

class MermaidGanttDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidGanttDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    QString buildDiagram() const;

    QLineEdit *m_titleEdit;
    QComboBox *m_dateFormatCombo;
    QCheckBox *m_excludeWeekendsCheck;
    QTableWidget *m_taskTable;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif

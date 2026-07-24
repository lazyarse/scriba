#ifndef MERMAIDQUADRANTDIALOG_H
#define MERMAIDQUADRANTDIALOG_H

#include <QDialog>

class QTableWidget;
class QWebEngineView;
class QLineEdit;
class QTimer;

class MermaidQuadrantDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidQuadrantDialog(QWidget *parent = nullptr);
    QString generatedDiagram() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    QString buildDiagram() const;

    QLineEdit *m_titleEdit;
    QLineEdit *m_xAxisEdit, *m_xAxisRightEdit;
    QLineEdit *m_yAxisEdit, *m_yAxisTopEdit;
    QLineEdit *m_q1Edit, *m_q2Edit, *m_q3Edit, *m_q4Edit;
    QTableWidget *m_table;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};

#endif
#ifndef KATEXHELPERDIALOG_H
#define KATEXHELPERDIALOG_H

#include <QDialog>

class QWebEngineView;
class QPlainTextEdit;
class QRadioButton;
class QTimer;

class KatexHelperDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KatexHelperDialog(QWidget *parent = nullptr);
    QString generatedLatex() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
    QWidget *createSymbolPalette(QWidget *parent);
    QWidget *createCheatSheet(QWidget *parent);
    void insertAtCursor(const QString &text);

    QWebEngineView *m_preview;
    QPlainTextEdit *m_input;
    QRadioButton *m_inlineRadio;
    QRadioButton *m_blockRadio;
    QTimer *m_previewTimer;
};

#endif

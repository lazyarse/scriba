#ifndef KATEXHELPERDIALOG_H
#define KATEXHELPERDIALOG_H

#include <QDialog>
#include <QColor>

class QWebEngineView;
class QPlainTextEdit;
class QRadioButton;
class QTimer;

class KatexHelperDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KatexHelperDialog(const QString &themeCss = QString(), QWidget *parent = nullptr);
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
    QColor m_themeBg;
    QColor m_themeTxt;
};

#endif

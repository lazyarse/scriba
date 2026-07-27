#pragma once

#include <QDialog>
#include <QColor>

class QWebEngineView;
class QPlainTextEdit;
class QRadioButton;
class QTimer;

class MchemHelperDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MchemHelperDialog(const QString &themeCss = QString(), QWidget *parent = nullptr);
    QString generatedNotation() const;

private slots:
    void schedulePreviewUpdate();
    void updatePreview();

private:
    void setupUi();
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

#ifndef EXPORTPDFDIALOG_H
#define EXPORTPDFDIALOG_H

#include <QDialog>
#include <QByteArray>
#include <QScopedPointer>
#include <QProcess>

class QWebEngineView;
class QCheckBox;
class QRadioButton;
class QPushButton;
class QPlainTextEdit;
class QLabel;
class QTemporaryFile;
class CssLoader;

class ExportPdfDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportPdfDialog(const QString &html, const QString &defaultFilePath,
                             CssLoader *loader, QWidget *parent = nullptr);
    ~ExportPdfDialog();

    static QString findChromiumBinary();

private slots:
    void onCssModeChanged();
    void browseCustomCss();
    void onPageLoaded(bool ok);
    void reloadPdfPreview();

private:
    friend class PrintExportAccess;
    static QMarginsF parsePageMargins(const QString &css);
    void setupUi();
    QString buildFullHtml(const QString &printCss) const;
    QString buildHeaderFooterCss() const;
    QString loadCustomCss() const;
    void generatePdfViaChromium(const QString &printCss);
    void accept() override;

    CssLoader *m_loader;
    QString m_html;
    QString m_defaultFilePath;
    QWebEngineView *m_preview;
    QWebEngineView *m_hiddenEngine;
    QRadioButton *m_defaultRadio;
    QRadioButton *m_customRadio;
    QPushButton *m_browseBtn;
    QLabel *m_pathLabel;
    QString m_customCssPath;
    QString m_currentPrintCss;
    QString m_currentFullHtml;
    QString m_baseUrl;
    QByteArray m_pdfData;
    QScopedPointer<QTemporaryFile> m_tempFile;
    QString m_chromiumBinary;
    QProcess *m_pdfProcess = nullptr;
    QCheckBox *m_showPdfToolbar = nullptr;
    QCheckBox *m_showHeader = nullptr;
    QPlainTextEdit *m_headerLeft = nullptr;
    QPlainTextEdit *m_headerCenter = nullptr;
    QPlainTextEdit *m_headerRight = nullptr;
    QPlainTextEdit *m_footerLeft = nullptr;
    QPlainTextEdit *m_footerCenter = nullptr;
    QPlainTextEdit *m_footerRight = nullptr;
    QPushButton *m_regenerateBtn = nullptr;
    QString m_pdfUrl;
    int m_generationId = 0;
};

#endif

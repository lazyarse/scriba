#ifndef EXPORTPDFDIALOG_H
#define EXPORTPDFDIALOG_H

#include <QDialog>

class QWebEngineView;
class QRadioButton;
class QPushButton;
class QLabel;
class CssLoader;

class ExportPdfDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportPdfDialog(const QString &html, CssLoader *loader, QWidget *parent = nullptr);
    QString selectedPrintCss() const;

private slots:
    void onCssModeChanged();
    void browseCustomCss();

private:
    void setupUi();
    void loadPreview(const QString &printCss);
    QString loadCustomCss() const;

    CssLoader *m_loader;
    QString m_html;
    QWebEngineView *m_preview;
    QRadioButton *m_defaultRadio;
    QRadioButton *m_customRadio;
    QPushButton *m_browseBtn;
    QLabel *m_pathLabel;
    QString m_customCssPath;
    QString m_cachedPrintCss;
};

#endif

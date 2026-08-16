// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

#include <QDialog>
#include <QByteArray>
#include <QScopedPointer>
#include <QProcess>
#include <QSharedPointer>
#include <functional>

#include "preview/PrintOptions.h"

class QWebEngineView;
class QVBoxLayout;
class QCheckBox;
class QRadioButton;
class QPushButton;
class QPlainTextEdit;
class QComboBox;
class QLineEdit;
class QLabel;
class QTemporaryFile;
class QTemporaryDir;
class QPrinter;
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
    void editPrintBaseCss();
    void onPageLoaded(bool ok);
    void reloadPdfPreview();
    void printDocument();

private:
    friend class PrintExportAccess;
    static QMarginsF parsePageMargins(const QString &css);
    static QSizeF parsePageSize(const QString &css);
    void setupUi();
    QVBoxLayout *setupLayout(QVBoxLayout *mainLayout);
    void setupCssModeSection(QVBoxLayout *leftLayout);
    void setupTypesettingSection(QVBoxLayout *leftLayout);
    void setupConnections();
    QString buildFullHtml(const QString &printCss) const;
    QString buildMergedPrintCss(const QString &printCss) const;
    QString buildHeaderFooterCss() const;
    QString loadCustomCss() const;
    void generatePdfViaChromium(const QString &printCss);
    void extractPdfBodyForChromium(int genId, const QString &printCss);
    // In-process Qt printToPdf: no external browser. Used when no chromium
    // binary exists and as the fallback when chromium keeps failing.
    void generatePdfViaQtPage(int genId, const QString &css);
    // Launches the headless chromium for one attempt; on failure retries up to
    // MaxChromiumAttempts total launches, then falls back to generatePdfViaQtPage.
    void launchChromiumPdf(int genId, const QString &htmlPath,
                           const QString &pdfPath,
                           QSharedPointer<QTemporaryDir> dir, int attempt);
    // Fresh QProcess wired with the stderr capture connection; a process that
    // failed to start can never be restarted, so every retry needs a new one.
    QProcess *createPdfProcess();
    // Polls a JS boolean flag on the hidden page (promises can't be awaited
    // through runJavaScript) and runs `continuation` when it reads true or
    // the poll budget is exhausted.
    void pollForStockFlag(const QString &probe, std::function<void()> continuation);
    void pollStockStep();
    void accept() override;
    void syncPrintOptionsFromUi();
    void applyPrintOptionsToUi(const PrintOptions::Options &o);

    CssLoader *m_loader;
    QString m_html;
    QString m_defaultFilePath;
    QWebEngineView *m_preview;
    QWebEngineView *m_hiddenEngine;
    QRadioButton *m_defaultRadio;
    QRadioButton *m_customRadio;
    QPushButton *m_browseBtn;
    QPushButton *m_editPrintCssBtn = nullptr;
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
    QComboBox *m_codeSplitCombo = nullptr;
    QCheckBox *m_keepTables = nullptr;
    QCheckBox *m_keepHeadings = nullptr;
    QCheckBox *m_keepFigures = nullptr;
    QCheckBox *m_orphanControl = nullptr;
    QLineEdit *m_marginEdit = nullptr;
    QLineEdit *m_sizeEdit = nullptr;
    QPushButton *m_resetTypesettingBtn = nullptr;
    QString m_pdfUrl;
    int m_generationId = 0;
    // Chromium launch retries before falling back to the in-process Qt path.
    static constexpr int MaxChromiumAttempts = 3;
    // Cap for the captured chromium stderr tail (diagnostics only).
    static constexpr int MaxChromiumStderr = 64 * 1024;
    QByteArray m_pdfStderr;
    PrintOptions::Options m_printOptions;
    QTimer *m_stockPollTimer = nullptr;
    int m_stockPollAttempt = 0;
    QString m_stockPollProbe;
    std::function<void()> m_stockPollContinuation;
};


#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "MarkdownParser.h"
#include "CssManager.h"
#include "PreferencesDialog.h"
#include "FindDialog.h"
#include "ExportPdfDialog.h"
#include "Preferences.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QStatusBar>
#include <QSettings>
#include <QScrollBar>
#include <QTextDocument>
#include <QColor>
#include <QRegularExpression>
#include <QPageLayout>
#include <QPageSize>
#include <QApplication>

static QString escapeJsString(const QString &s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n");
    return r;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser())
    , m_cssManager(new CssManager())
    , m_cssWatcher(new QFileSystemWatcher(this))
{
    setupUi();
    setupMenuBar();

    refreshPreviewCss();
    updatePreview();

    setWindowTitle("Scriba");
    showMaximized();

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(80);
    connect(timer, &QTimer::timeout, this, &MainWindow::updatePreview);
    connect(m_editor, &QPlainTextEdit::textChanged, timer, qOverload<>(&QTimer::start));

    connect(m_cssWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onCssFileChanged);
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onEditorScroll);
    syncCssWatcher();

    QSettings settings;
    if (settings.value(Preferences::FirstRun, true).toBool()) {
        settings.setValue(Preferences::FirstRun, false);
        QFile sample(":/sample.md");
        if (sample.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_editor->setPlainText(QString::fromUtf8(sample.readAll()));
            m_currentFile.clear();
            m_preview->setDocumentPath(QString());
            setWindowTitle("Scriba - Sample");
        }
    }
    if (settings.value(Preferences::ReopenLastFile, true).toBool()) {
        QString lastFile = settings.value(Preferences::LastOpenedFile).toString();
        if (!lastFile.isEmpty()) {
            loadFile(lastFile);
        }
    }
}

void MainWindow::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    m_editor = new Editor();
    m_preview = new Preview();

    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    {
        QSettings settings;
        if (!settings.value(Preferences::EditorOnLeft, true).toBool()) {
            m_splitter->insertWidget(0, m_preview);
        }
    }
    m_splitter->setSizes({600, 600});
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *newAction = fileMenu->addAction("&New");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        m_editor->clear();
        m_currentFile.clear();
        m_preview->setDocumentPath(QString());
        setWindowTitle("Scriba - Untitled");
    });

    QAction *openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Open Markdown File", QString(), "Markdown Files (*.md *.markdown *.txt);;All Files (*)");
        if (!file.isEmpty()) loadFile(file);
    });

    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        if (m_currentFile.isEmpty()) {
            QString file = QFileDialog::getSaveFileName(this, "Save Markdown File", QString(), "Markdown Files (*.md);;All Files (*)");
            if (!file.isEmpty()) saveFile(file);
        } else {
            saveFile(m_currentFile);
        }
    });

    QAction *saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "Save Markdown File As", QString(), "Markdown Files (*.md);;All Files (*)");
        if (!file.isEmpty()) saveFile(file);
    });

    fileMenu->addSeparator();

    QAction *exportPdfAction = fileMenu->addAction("Export &PDF...");
    exportPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::exportPdf);

    fileMenu->addSeparator();

    QAction *prefsAction = fileMenu->addAction("&Preferences...");
    prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P));
    connect(prefsAction, &QAction::triggered, this, &MainWindow::showPreferences);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QAction *findAction = new QAction("&Find", this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::showFindDialog);
    addAction(findAction);
}

QString MainWindow::deriveChromeCss(const QString &themeCss) const
{
    auto extractBg = [&](const QString &selector) {
        QRegularExpression re(
            R"(\b)" + selector + R"(\s*\{[^}]*background(?:-color)?\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    auto extractColor = [&](const QString &selector) {
        QRegularExpression re(
            R"(\b)" + selector + R"(\s*\{(?:[^}]*;\s*)?\bcolor\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    QString bgStr = extractBg("#editor");
    if (bgStr.isEmpty())
        bgStr = extractBg("body");

    QString txtStr = extractColor("#editor");
    if (txtStr.isEmpty())
        txtStr = extractColor("body");

    QColor bg(QStringLiteral("#ffffff"));
    if (!bgStr.isEmpty()) {
        QColor parsed(bgStr);
        if (parsed.isValid())
            bg = parsed;
    }

    bool dark = bg.lightness() < 128;
    QColor track, thumb, hover, selBg, txt, selTxt;
    if (dark) {
        track = bg.lighter(160);
        thumb = bg.lighter(220);
        hover = bg.lighter(250);
        selBg = hover;
        txt = QColor(QStringLiteral("#f0f0f0"));
        selTxt = QColor(QStringLiteral("#ffffff"));
    } else {
        track = bg.darker(115);
        thumb = bg.darker(160);
        hover = bg.darker(180);
        selBg = hover;
        txt = QColor(QStringLiteral("#333333"));
        selTxt = QColor(QStringLiteral("#000000"));
    }

    return QStringLiteral(
        "QDialog { background-color: %2; }\n"
        "QGroupBox { color: %3; font-weight: bold; border: 1px solid %4; margin-top: 8px; }\n"
        "QGroupBox::title { color: %3; font-weight: bold; }\n"
        "QCheckBox { color: %3; spacing: 6px; }\n"
        "QCheckBox::indicator { width: 14px; height: 14px; background-color: %2; border: 1px solid %4; }\n"
        "QCheckBox::indicator:checked { background-color: %5; border: 1px solid %5; image: url(:/checkbox-checked.svg); }\n"
        "QRadioButton { color: %3; spacing: 6px; }\n"
        "QRadioButton::indicator { width: 14px; height: 14px; background-color: %2; border: 1px solid %4; border-radius: 7px; }\n"
        "QRadioButton::indicator:checked { background-color: %5; border: 1px solid %5; }\n"
        "QListWidget { background-color: %2; color: %3; border: none; }\n"
        "QListWidget::item:selected { background-color: %5; color: %6; }\n"
        "QListWidget::item:hover { background-color: %1; }\n"
        "QPushButton { background-color: %4; color: %3; border: 1px solid %4; padding: 4px 12px; }\n"
        "QPushButton:hover { background-color: %5; }\n"
        "QLabel { color: %3; }\n"
        "#scriba-editor { padding: 0 !important; margin: 0 !important; border: none !important; background-color: %7 !important; color: %8 !important; }\n"
        "QSplitter::handle { background-color: %4; width: 1px; }\n"
        "QSplitter::handle:hover { background-color: %5; }\n"
        "QScrollBar:vertical { background: %2; width: 12px; }\n"
        "QScrollBar::handle:vertical { background: %4; border-radius: 6px; min-height: 30px; }\n"
        "QScrollBar::handle:vertical:hover { background: %5; }\n"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }\n"
        "QScrollBar:horizontal { background: %2; height: 12px; }\n"
        "QScrollBar::handle:horizontal { background: %4; border-radius: 6px; min-width: 30px; }\n"
        "QScrollBar::handle:horizontal:hover { background: %5; }\n"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }\n"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }\n"
        
        "::-webkit-scrollbar { width: 12px; height: 12px; }\n"
        "::-webkit-scrollbar-track { background: %2; }\n"
        "::-webkit-scrollbar-thumb { background: %4; border-radius: 6px; }\n"
        "::-webkit-scrollbar-thumb:hover { background: %5; }\n"
    ).arg(
        track.name(),   // %1 — splitter handle, hover bg
        track.name(),   // %2 — dialog/bg, menus, scrollbar track
        txt.name(),     // %3 — text color
        thumb.name(),   // %4 — button bg, scrollbar handle
        hover.name(),   // %5 — hover/selected bg
        selTxt.name(),  // %6 — selected text color
        bg.name(),       // %7 — editor background
        txtStr.isEmpty() ? txt.name() : txtStr  // %8 — editor text color from theme
    );
}

void MainWindow::refreshPreviewCss()
{
    QString rawThemeCss = m_cssManager->themeCss();
    QString chromeCss = deriveChromeCss(rawThemeCss);
    QString previewCss = chromeCss + rawThemeCss;
    QString fullCss = chromeCss + m_cssManager->editorBaseCss();
    QString previewBaseCss = m_cssManager->previewBaseCss();

    bool needPreviewUpdate = (previewCss != m_cachedPreviewCss);
    bool needChromeUpdate = (fullCss != m_cachedFullCss);
    bool needBaseUpdate = (previewBaseCss != m_cachedPreviewBaseCss);

    if (!needPreviewUpdate && !needChromeUpdate && !needBaseUpdate)
        return;

    if (needPreviewUpdate) {
        m_cachedPreviewCss = previewCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('theme-css').textContent = '%1';")
                .arg(escapeJsString(previewCss));
            m_preview->page()->runJavaScript(js);
        }
    }

    if (needBaseUpdate) {
        m_cachedPreviewBaseCss = previewBaseCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('base-css').textContent = '%1';")
                .arg(escapeJsString(previewBaseCss));
            m_preview->page()->runJavaScript(js);
        }
    }

    if (needChromeUpdate) {
        m_cachedFullCss = fullCss;
        m_editor->setStyleSheet(fullCss);
        if (!m_chromeUpdateScheduled) {
            m_chromeUpdateScheduled = true;
            QTimer::singleShot(0, this, [this, fullCss]() {
                m_chromeUpdateScheduled = false;
                qApp->setStyleSheet(fullCss);
            });
        }
    }
}

void MainWindow::updatePreview()
{
    QString markdown = m_editor->toPlainText();
    QString html = m_parser->toHtml(markdown);

    QString rawThemeCss = m_cssManager->themeCss();
    QString baseCss = m_cssManager->previewBaseCss();
    QString chromeCss = deriveChromeCss(rawThemeCss);
    QString previewCss = chromeCss + rawThemeCss;

    bool cssChanged = (previewCss != m_cachedPreviewCss);
    if (cssChanged) {
        m_cachedPreviewCss = previewCss;
    }

    QUrl baseUrl;
    QString docPath = m_preview->documentPath();
    if (!docPath.isEmpty()) {
        baseUrl = QUrl::fromLocalFile(QFileInfo(docPath).absolutePath() + "/");
    }

    static const QString mermaidInitJs = QStringLiteral(
        "function initMermaid(){"
        "var els=document.querySelectorAll('code.language-mermaid');"
        "if(!els.length)return;"
        "els.forEach(function(el){"
        "var div=document.createElement('div');"
        "div.className='mermaid';"
        "div.textContent=el.textContent;"
        "el.parentElement.parentElement.replaceChild(div,el.parentElement);"
        "});"
        "mermaid.run({querySelector:'.mermaid'});"
        "}"
    );

    if (!m_previewInitialized) {
        m_cachedPreviewBaseCss = baseCss;
        QString printCss = m_cssManager->printCss();
        QString fullHtml = QString(
            "<html><head>"
            "<style id=\"base-css\">%1</style>"
            "<style id=\"theme-css\">%2</style>"
            "<style id=\"print-css\">@media print { %4 }</style>"
            "<script src=\"qrc:///mermaid.min.js\"></script>"
            "<script>" + mermaidInitJs + "document.addEventListener('DOMContentLoaded',function(){mermaid.initialize({startOnLoad:false,theme:'default'});initMermaid();});</script>"
            "</head><body id=\"preview\">%3</body></html>"
        ).arg(baseCss, previewCss, html, printCss);
        m_preview->setHtml(fullHtml, baseUrl);
        m_previewInitialized = true;
    } else {
        QString escapedHtml = escapeJsString(html);

        QString js;
        if (cssChanged) {
            QString escapedCss = escapeJsString(previewCss);
            js = QString(
                "var sy = window.scrollY;"
                "document.getElementById('theme-css').textContent = '%1';"
                "document.body.innerHTML = '%2';"
                "initMermaid();"
                "window.scrollTo(0, sy);"
            ).arg(escapedCss, escapedHtml);
        } else {
            js = QString(
                "var sy = window.scrollY;"
                "document.body.innerHTML = '%1';"
                "initMermaid();"
                "window.scrollTo(0, sy);"
            ).arg(escapedHtml);
        }
        m_preview->page()->runJavaScript(js);
    }

    syncPreviewScroll();
}

void MainWindow::syncPreviewScroll()
{
    QSettings settings;
    if (!settings.value(Preferences::SyncScroll, true).toBool())
        return;
    auto *sb = m_editor->verticalScrollBar();
    double range = sb->maximum() - sb->minimum();
    double pct = range > 0 ? static_cast<double>(sb->value() - sb->minimum()) / range : 0.0;
    m_preview->scrollToPercent(pct);
}

void MainWindow::showPreferences()
{
    PreferencesDialog dlg(m_cssManager, this);
    auto updateAll = [this]() {
        syncCssWatcher();
        refreshPreviewCss();
    };
    connect(&dlg, &PreferencesDialog::stylesheetChanged, this, updateAll);
    dlg.exec();
    updateAll();
    {
        QSettings settings;
        bool editorOnLeft = settings.value(Preferences::EditorOnLeft, true).toBool();
        bool currentlyOnLeft = m_splitter->indexOf(m_editor) == 0;
        if (editorOnLeft != currentlyOnLeft) {
            QList<int> sizes = m_splitter->sizes();
            m_splitter->insertWidget(editorOnLeft ? 0 : 1, m_editor);
            m_splitter->setSizes(sizes);
        }
    }
}

void MainWindow::showFindDialog()
{
    FindDialog dlg(m_editor);
    if (dlg.exec() == QDialog::Accepted) {
        QString term = dlg.searchTerm();
        if (term.isEmpty()) return;

        QTextDocument::FindFlags flags;
        if (dlg.caseSensitive())
            flags |= QTextDocument::FindCaseSensitively;

        if (dlg.regexEnabled())
            m_editor->find(QRegularExpression(term), flags);
        else
            m_editor->find(term, flags);
    }
}

void MainWindow::syncCssWatcher()
{
    QStringList watched = m_cssWatcher->files();
    if (!watched.isEmpty())
        m_cssWatcher->removePaths(watched);

    QString active = m_cssManager->activeStylesheet();
    if (!active.isEmpty() && QFile::exists(active))
        m_cssWatcher->addPath(active);
}

void MainWindow::onCssFileChanged()
{
    m_cssManager->invalidateCache();
    refreshPreviewCss();
    syncCssWatcher();
}

void MainWindow::onEditorScroll()
{
    syncPreviewScroll();
}

void MainWindow::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filePath);
        return;
    }

    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    m_currentFile = filePath;
    setWindowTitle("Scriba - " + filePath);
    m_preview->setDocumentPath(filePath);
    m_previewInitialized = false;

    QSettings settings;
    settings.setValue(Preferences::LastOpenedFile, filePath);
}

void MainWindow::saveFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file: " + filePath);
        return;
    }

    file.write(m_editor->toPlainText().toUtf8());
    m_currentFile = filePath;
    setWindowTitle("Scriba - " + filePath);
    m_preview->setDocumentPath(filePath);
    statusBar()->showMessage("Saved", 2000);

    QSettings settings;
    settings.setValue(Preferences::LastOpenedFile, filePath);
}

void MainWindow::exportPdf()
{
    QString markdown = m_editor->toPlainText();
    QString html = m_parser->toHtml(markdown);

    ExportPdfDialog dlg(html, m_cssManager, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString printCss = dlg.selectedPrintCss();

    QString defaultName = "Untitled.pdf";
    if (!m_currentFile.isEmpty()) {
        QFileInfo fi(m_currentFile);
        defaultName = fi.absolutePath() + "/" + fi.completeBaseName() + ".pdf";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export PDF", defaultName, "PDF Files (*.pdf)");
    if (filePath.isEmpty()) return;

    auto printWithCss = [this, filePath, printCss](const QString &origBase, const QString &origTheme) {
        QString injectJs = QString(
            "document.getElementById('base-css').textContent = '';"
            "document.getElementById('theme-css').textContent = '%1';"
        ).arg(escapeJsString(printCss));

        m_preview->page()->runJavaScript(injectJs, [this, filePath, origBase, origTheme](const QVariant &) {
            QTimer::singleShot(150, this, [this, filePath, origBase, origTheme]() {
                QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                   QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
                m_preview->page()->printToPdf([this, filePath, origBase, origTheme](const QByteArray &data) {
                    QFile f(filePath);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(data);
                        statusBar()->showMessage("Exported to " + filePath, 5000);
                    }
                    QString restore = QString(
                        "document.getElementById('base-css').textContent = '%1';"
                        "document.getElementById('theme-css').textContent = '%2';"
                    ).arg(escapeJsString(origBase), escapeJsString(origTheme));
                    m_preview->page()->runJavaScript(restore);
                }, layout);
            });
        });
    };

    m_preview->page()->runJavaScript(
        "document.getElementById('base-css').textContent",
        [this, printWithCss](const QVariant &baseResult) {
            QString origBase = baseResult.toString();
            m_preview->page()->runJavaScript(
                "document.getElementById('theme-css').textContent",
                [origBase, printWithCss](const QVariant &themeResult) {
                    printWithCss(origBase, themeResult.toString());
                });
        });
}

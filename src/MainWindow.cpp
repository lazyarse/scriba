#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "MarkdownParser.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "CssUtils.h"
#include "PreferencesDialog.h"
#include "FindDialog.h"
#include "ExportPdfDialog.h"
#include "Preferences.h"
#include "StaticHelpers.h"
#include "VegaLiteDialog.h"

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
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QToolButton>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser())
    , m_cssConfig(new CssConfig())
    , m_cssLoader(new CssLoader(m_cssConfig))
    , m_cssWatcher(new QFileSystemWatcher(this))
{
    m_previewState = QSettings().value(Preferences::PreviewState, 1).toInt();
    if (m_previewState < 0 || m_previewState > 2) m_previewState = 1;

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
        loadSample();
    }
    if (settings.value(Preferences::ReopenLastFile, true).toBool()) {
        QString lastFile = settings.value(Preferences::LastOpenedFile).toString();
        if (!lastFile.isEmpty()) {
            QFile testFile(lastFile);
            if (testFile.exists()) {
                loadFile(lastFile);

                int block = settings.value(Preferences::LastCursorBlock, 0).toInt();
                int column = settings.value(Preferences::LastCursorColumn, 0).toInt();
                m_editor->setTextCursor(restoreCursorPosition(m_editor->document(), block, column));
                m_editor->centerCursor();
            } else {
                showCenteredWarning("File Not Found",
                    "Could not find: " + lastFile,
                    "Loading default sample instead.");
                loadSample();
            }
        }
    }
}

void MainWindow::loadSample()
{
    QFile sample(":/sample.md");
    if (sample.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_editor->setPlainText(QString::fromUtf8(sample.readAll()));
        m_currentFile.clear();
        m_preview->setDocumentPath(QString());
        setWindowTitle("Scriba - Sample");
    }
}

void MainWindow::showCenteredWarning(const QString &title, const QString &text, const QString &informative)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setInformativeText(informative);
    msgBox.move(QGuiApplication::primaryScreen()->geometry().center() - msgBox.rect().center());
    msgBox.exec();
}

void MainWindow::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    m_editor = new Editor();
    m_preview = new Preview();

    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);

    if (m_previewState == 0) {
        m_splitter->addWidget(m_editor);
        m_preview->setVisible(false);
    } else if (m_previewState == 1) {
        m_splitter->addWidget(m_editor);
        m_splitter->addWidget(m_preview);
    } else {
        m_splitter->addWidget(m_preview);
        m_splitter->addWidget(m_editor);
    }

    m_splitter->setSizes({600, 600});
    m_splitter->setStretchFactor(0, 1);
    if (m_previewState != 0) {
        m_splitter->setStretchFactor(1, 1);
        m_splitter->handle(1)->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    // Corner buttons in menu bar
    m_fullscreenBtn = new QToolButton();
    m_fullscreenBtn->setIcon(QIcon(":/icons/fullscreen.svg"));
    m_fullscreenBtn->setToolTip("Toggle Fullscreen (F11)");
    m_fullscreenBtn->setAutoRaise(true);
    m_fullscreenBtn->setFixedSize(28, 28);
    connect(m_fullscreenBtn, &QToolButton::clicked, this, &MainWindow::toggleFullscreen);

    m_previewBtn = new QToolButton();
    m_previewBtn->setIcon(QIcon(":/icons/preview.svg"));
    m_previewBtn->setToolTip("Toggle Preview (hidden → right → left)");
    m_previewBtn->setAutoRaise(true);
    m_previewBtn->setFixedSize(28, 28);
    connect(m_previewBtn, &QToolButton::clicked, this, &MainWindow::togglePreview);

    m_chartBtn = new QToolButton();
    m_chartBtn->setIcon(QIcon(":/icons/chart.svg"));
    m_chartBtn->setToolTip("Chart Builder");
    m_chartBtn->setAutoRaise(true);
    m_chartBtn->setFixedSize(28, 28);
    connect(m_chartBtn, &QToolButton::clicked, this, &MainWindow::showChartBuilder);

    QWidget *cornerWidget = new QWidget();
    QHBoxLayout *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 4, 0);
    cornerLayout->setSpacing(2);
    cornerLayout->addWidget(m_chartBtn);
    cornerLayout->addWidget(m_previewBtn);
    cornerLayout->addWidget(m_fullscreenBtn);
    menuBar()->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    m_statsLabel = new QLabel();
    m_statsLabel->setObjectName("stats-label");
    m_statsLabel->setAlignment(Qt::AlignCenter);
    m_statsLabel->setContentsMargins(0, 0, 0, 0);
    statusBar()->addWidget(m_statsLabel, 1);
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

    QAction *reloadAction = fileMenu->addAction("&Reload");
    reloadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(reloadAction, &QAction::triggered, this, [this]() {
        if (!m_currentFile.isEmpty()) loadFile(m_currentFile);
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

    QAction *fullscreenAction = new QAction("Toggle &Fullscreen", this);
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    addAction(fullscreenAction);

    QAction *previewAction = new QAction("Toggle &Preview", this);
    previewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(previewAction, &QAction::triggered, this, &MainWindow::togglePreview);
    addAction(previewAction);
}

void MainWindow::refreshPreviewCss()
{
    QString rawThemeCss = m_cssLoader->themeCss();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss);
    QString previewCss = chromeCss + rawThemeCss;
    QString fullCss = chromeCss + m_cssLoader->editorBaseCss();
    QString previewBaseCss = m_cssLoader->previewBaseCss();

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

void MainWindow::applyStripeSetting()
{
    if (!m_previewInitialized)
        return;
    QSettings settings;
    bool enabled = settings.value(Preferences::TableStriping, true).toBool();
    QString css = enabled ? QString()
        : QLatin1String(Preferences::TableStripeCss);
    QString js = QString(
        "var e=document.getElementById('stripe-css');"
        "if(e)e.textContent='%1';"
    ).arg(escapeJsString(css));
    m_preview->page()->runJavaScript(js);
}

void MainWindow::updatePreview()
{
    QString markdown = m_editor->toPlainText();
    updateStats();
    QString html = m_parser->toHtml(markdown);

    QString rawThemeCss = m_cssLoader->themeCss();
    QString baseCss = m_cssLoader->previewBaseCss();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss);
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

    static const QString headingIdJs = QStringLiteral(
        "function generateHeadingIds(){"
        "document.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(function(h){"
        "if(!h.id){"
        "h.id=h.textContent.toLowerCase().replace(/[^\\w\\s-]/g,'').replace(/\\s+/g,'-').replace(/^-+|-+$/g,'');"
        "}"
        "});"
        "}"
    );

    static const QString katexInitJs = QStringLiteral(
        "function initKaTeX(){"
        "if(typeof renderMathInElement==='function')"
        "renderMathInElement(document.body,{"
        "delimiters:["
        "{left:'$$',right:'$$',display:true},"
        "{left:'$',right:'$',display:false}"
        "]"
        "});"
        "}"
    );

    static const QString vegaLiteInitJs = QStringLiteral(
        "function initVegaLite(){"
        "var els=document.querySelectorAll('code.language-vl');"
        "if(!els.length)return;"
        "els.forEach(function(el){"
        "try{"
        "var spec=JSON.parse(el.textContent);"
        "var container=el.parentElement;"
        "var div=document.createElement('div');"
        "div.className='vega-lite-chart';"
        "div.style.width='100%';"
        "div.style.minHeight='300px';"
        "div.style.overflow='visible';"
        "container.parentElement.replaceChild(div,container);"
        "vegaEmbed(div,spec,{actions:false}).catch(function(e){});"
        "}"
        "catch(e){}"
        "});"
        "}"
    );

    if (!m_previewInitialized) {
        m_cachedPreviewBaseCss = baseCss;
        QString printCss = m_cssLoader->printCss();
        QSettings prefs;
        bool striping = prefs.value(Preferences::TableStriping, true).toBool();
        QString stripeInit = striping ? QString()
            : QLatin1String(Preferences::TableStripeCss);
        QString fullHtml = QString(
            "<!DOCTYPE html><html><head>"
            "<style id=\"base-css\">%1</style>"
            "<style id=\"theme-css\">%2</style>"
            "<style id=\"print-css\">@media print { %4 }</style>"
            "<style id=\"stripe-css\">%5</style>"
            "<script src=\"qrc:///highlight.min.js\"></script>"
            "<script src=\"qrc:///mermaid.min.js\"></script>"
            "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
            "<script src=\"qrc:///katex.min.js\"></script>"
            "<script src=\"qrc:///contrib/auto-render.min.js\"></script>"
            "<script src=\"qrc:///vega.min.js\"></script>"
            "<script src=\"qrc:///vega-lite.min.js\"></script>"
            "<script src=\"qrc:///vega-embed.min.js\"></script>"
            "<script>" + mermaidInitJs + headingIdJs + katexInitJs + vegaLiteInitJs + "document.addEventListener('DOMContentLoaded',function(){mermaid.initialize({startOnLoad:false,theme:'default'});initMermaid();hljs.highlightAll();generateHeadingIds();initKaTeX();initVegaLite();});</script>"
            "</head><body id=\"preview\">%3</body></html>"
        ).arg(baseCss, previewCss, html, printCss, stripeInit);
        m_preview->setHtml(fullHtml, baseUrl);
        m_previewInitialized = true;
    } else {
        QString escapedHtml = escapeJsString(html);

        QString js;
        if (cssChanged) {
            QString escapedCss = escapeJsString(previewCss);
            js = QString(
                "if(!document.body){}"
                "else{"
                "var sy = window.scrollY;"
                "document.getElementById('theme-css').textContent = '%1';"
                "document.body.innerHTML = '%2';"
                "initMermaid();"
                "initKaTeX();"
                "initVegaLite();"
                "hljs.highlightAll();"
                "generateHeadingIds();"
                "window.scrollTo(0, sy);"
                "}"
            ).arg(escapedCss, escapedHtml);
        } else {
            js = QString(
                "if(!document.body){}"
                "else{"
                "var sy = window.scrollY;"
                "document.body.innerHTML = '%1';"
                "initMermaid();"
                "initKaTeX();"
                "initVegaLite();"
                "hljs.highlightAll();"
                "generateHeadingIds();"
                "window.scrollTo(0, sy);"
                "}"
            ).arg(escapedHtml);
        }
        m_preview->page()->runJavaScript(js);
    }
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
    PreferencesDialog dlg(m_cssConfig, m_cssLoader, this);
    auto updateAll = [this]() {
        syncCssWatcher();
        refreshPreviewCss();
        applyStripeSetting();
    };
    connect(&dlg, &PreferencesDialog::stylesheetChanged, this, updateAll);
    dlg.exec();
    updateAll();
    applyStripeSetting();
}

void MainWindow::showChartBuilder()
{
    VegaLiteDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString spec = dlg.generatedSpec();
        if (!spec.isEmpty()) {
            QString block = "\n```vl\n" + spec + "\n```\n";
            m_editor->insertPlainText(block);
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

    QString active = m_cssConfig->activeStylesheet();
    if (!active.isEmpty() && QFile::exists(active))
        m_cssWatcher->addPath(active);
}

void MainWindow::onCssFileChanged()
{
    m_cssLoader->invalidateCache();
    refreshPreviewCss();
    syncCssWatcher();
}

void MainWindow::onEditorScroll()
{
    syncPreviewScroll();
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
        showMaximized();
    else
        showFullScreen();
}

void MainWindow::togglePreview()
{
    // Cycle: 0 (hidden) → 1 (right) → 2 (left) → 0
    int oldState = m_previewState;
    m_previewState = (m_previewState + 1) % 3;
    QSettings().setValue(Preferences::PreviewState, m_previewState);

    if (m_previewState == 0) {
        m_preview->setVisible(false);
        m_editor->setVisible(true);
    } else if (m_previewState == 1) {
        // editor | preview
        m_splitter->insertWidget(0, m_editor);
        m_splitter->insertWidget(1, m_preview);
        m_editor->setVisible(true);
        m_preview->setVisible(true);
    } else {
        // preview | editor
        m_splitter->insertWidget(0, m_preview);
        m_splitter->insertWidget(1, m_editor);
        m_preview->setVisible(true);
        m_editor->setVisible(true);
    }

    int w = m_splitter->width();
    if (w <= 0) w = 1200;
    m_splitter->setSizes({w / 2, w / 2});
}

void MainWindow::updateStats()
{
    QString text = m_editor->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_statsLabel->clear();
        return;
    }
    QStringList words = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    int wordCount = words.size();
    int sentences = countSentences(text);
    int totalSyllables = 0;
    for (const QString &w : words)
        totalSyllables += estimateSyllables(w);
    int minutes = (wordCount + 199) / 200;
    double grade = fleschKincaidGrade(wordCount, sentences, totalSyllables);
    int age = qMax(static_cast<int>(grade) + 5, 5);
    m_statsLabel->setText(QStringLiteral("%1 sentence%2 · %3 word%4 · ~%5 min read · Age %6+")
        .arg(sentences).arg(sentences == 1 ? "" : "s")
        .arg(wordCount).arg(wordCount == 1 ? "" : "s")
        .arg(minutes).arg(age));
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

    ExportPdfDialog dlg(html, m_cssLoader, this);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_currentFile.isEmpty()) {
        QTextCursor cursor = m_editor->textCursor();
        QSettings settings;
        settings.setValue(Preferences::LastCursorBlock, cursor.blockNumber());
        settings.setValue(Preferences::LastCursorColumn, cursor.columnNumber());
    }
    QMainWindow::closeEvent(event);
}

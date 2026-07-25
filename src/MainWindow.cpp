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
#include "TableDialog.h"
#include "VegaLiteDialog.h"
#include "EmojiDialog.h"
#include "AboutDialog.h"
#include "LogWindow.h"
#include "MermaidPieDialog.h"
#include "MermaidFlowchartDialog.h"
#include "MermaidSequenceDialog.h"
#include "MermaidGanttDialog.h"
#include "MermaidClassDialog.h"
#include "MermaidErDialog.h"
#include "MermaidStateDialog.h"
#include "MermaidMindmapDialog.h"
#include "MermaidTimelineDialog.h"
#include "MermaidJourneyDialog.h"
#include "MermaidQuadrantDialog.h"
#include "MermaidSankeyDialog.h"

#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDialogButtonBox>
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
#include <QSvgRenderer>
#include <QFontDatabase>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QPainter>
#include <QRegularExpression>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QToolButton>
#include <QCloseEvent>

static QIcon themedIcon(const QString &svgPath, const QColor &color, int size = 28)
{
    QFile f(svgPath);
    if (!f.open(QIODevice::ReadOnly))
        return QIcon(svgPath);
    QString svg = QString::fromUtf8(f.readAll());
    svg.replace("currentColor", color.name());
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    renderer.render(&painter);
    painter.end();
    return QIcon(pix);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser())
    , m_cssConfig(new CssConfig())
    , m_cssLoader(new CssLoader(m_cssConfig))
    , m_cssWatcher(new QFileSystemWatcher(this))
{
    m_previewState = QSettings().value(Preferences::PreviewState, 1).toInt();
    if (m_previewState < 0 || m_previewState > 3) m_previewState = 1;

    setupUi();
    setupMenuBar();

    QFontDatabase::addApplicationFont(":/fonts/Symbola.ttf");

    connect(m_preview->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok && m_previewInitialized)
            syncPreviewScroll();
    });

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

    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
    int asInterval = settings.value(Preferences::AutoSaveInterval, 0).toInt();
    if (asInterval > 0)
        m_autoSaveTimer->start(asInterval * 60000);

    if (m_cssConfig->stylesheets().isEmpty()) {
        m_cssConfig->setStylesheets(CssConfig::bundledThemes());
        if (m_cssConfig->activeStylesheet().isEmpty())
            m_cssConfig->setActiveStylesheet(":/themes/github-light.css");
        m_cssLoader->invalidateCache();
        refreshPreviewCss();
        applyStripeSetting();
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
                int scrollTop = settings.value(Preferences::LastScrollTop, 0).toInt();
                QTimer::singleShot(0, [this, scrollTop]() {
                    m_editor->verticalScrollBar()->setValue(scrollTop);
                });
            } else {
                showCenteredWarning("File Not Found",
                    "Could not find: " + lastFile,
                    "The file may have been moved or deleted.");
            }
        }
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
    m_preview = new Preview(this);

    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);

    if (m_previewState == 0) {
        m_splitter->addWidget(m_editor);
        m_preview->setVisible(false);
    } else if (m_previewState == 3) {
        m_splitter->addWidget(m_preview);
        m_editor->setVisible(false);
    } else if (m_previewState == 1) {
        m_splitter->addWidget(m_editor);
        m_splitter->addWidget(m_preview);
    } else {
        m_splitter->addWidget(m_preview);
        m_splitter->addWidget(m_editor);
    }

    m_splitter->setSizes({600, 600});
    m_splitter->setStretchFactor(0, 1);
    if (m_previewState != 0 && m_previewState != 3) {
        m_splitter->setStretchFactor(1, 1);
        m_splitter->handle(1)->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    // Apply initial single-view centering
    if (m_previewState == 0 || m_previewState == 3) {
        QSettings settings;
        bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
        int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();
        if (m_previewState == 0)
            m_editor->setCenterContent(centre, centreWidth);
    }

    // Corner buttons in menu bar
    QColor iconColor = palette().color(QPalette::WindowText);
    m_fullscreenBtn = new QToolButton();
    m_fullscreenBtn->setIcon(themedIcon(":/icons/fullscreen.svg", iconColor));
    m_fullscreenBtn->setToolTip("Toggle Fullscreen (F11)");
    m_fullscreenBtn->setAutoRaise(true);
    m_fullscreenBtn->setFixedSize(28, 28);
    connect(m_fullscreenBtn, &QToolButton::clicked, this, &MainWindow::toggleFullscreen);

    m_previewBtn = new QToolButton();
    m_previewBtn->setIcon(themedIcon(":/icons/preview.svg", iconColor));
    m_previewBtn->setToolTip("Toggle Preview (editor only → right → left → preview only)");
    m_previewBtn->setAutoRaise(true);
    m_previewBtn->setFixedSize(28, 28);
    connect(m_previewBtn, &QToolButton::clicked, this, &MainWindow::togglePreview);

    QWidget *cornerWidget = new QWidget();
    QHBoxLayout *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 4, 0);
    cornerLayout->setSpacing(2);
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
        m_editor->setCurrentFile(QString());
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

    QMenu *editMenu = menuBar()->addMenu("&Edit");

    QAction *findAction = editMenu->addAction("Find / &Replace...");
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::toggleFindDialog);

    QAction *findNextAction = editMenu->addAction("Find &Next");
    findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(findNextAction, &QAction::triggered, this, &MainWindow::onFindNext);

    QAction *findPrevAction = editMenu->addAction("Find &Previous");
    findPrevAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(findPrevAction, &QAction::triggered, this, &MainWindow::onFindPrev);

    QAction *fullscreenAction = new QAction("Toggle &Fullscreen", this);
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    addAction(fullscreenAction);

    QAction *previewAction = new QAction("Toggle &Preview", this);
    previewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(previewAction, &QAction::triggered, this, &MainWindow::togglePreview);
    addAction(previewAction);

    QMenu *toolsMenu = menuBar()->addMenu("&Tools");

    QAction *tableAction = toolsMenu->addAction("&Table Insert...");
    tableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(tableAction, &QAction::triggered, this, &MainWindow::showTableInsert);

    QAction *emojiAction = toolsMenu->addAction("&Emoji Picker...");
    emojiAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(emojiAction, &QAction::triggered, this, [this]() {
        EmojiDialog dlg(this);
        connect(&dlg, &EmojiDialog::emojiChosen, this, [this](const QString &sc) {
            m_editor->insertPlainText(":" + sc + ":");
        });
        dlg.exec();
    });

    toolsMenu->addSeparator();

    QAction *chartAction = toolsMenu->addAction("Vega-Lite &Charts");
    chartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(chartAction, &QAction::triggered, this, &MainWindow::showChartBuilder);

    QMenu *mermaidMenu = toolsMenu->addMenu("Mermaid &Charts");
    QAction *pieAction = mermaidMenu->addAction("&Pie Chart...");
    connect(pieAction, &QAction::triggered, this, &MainWindow::showMermaidPie);
    QAction *flowchartAction = mermaidMenu->addAction("&Flowchart...");
    connect(flowchartAction, &QAction::triggered, this, &MainWindow::showMermaidFlowchart);
    QAction *sequenceAction = mermaidMenu->addAction("&Sequence Diagram...");
    connect(sequenceAction, &QAction::triggered, this, &MainWindow::showMermaidSequence);
    QAction *ganttAction = mermaidMenu->addAction("&Gantt Chart...");
    connect(ganttAction, &QAction::triggered, this, &MainWindow::showMermaidGantt);
    QAction *classAction = mermaidMenu->addAction("&Class Diagram...");
    connect(classAction, &QAction::triggered, this, &MainWindow::showMermaidClass);
    QAction *erAction = mermaidMenu->addAction("&ER Diagram...");
    connect(erAction, &QAction::triggered, this, &MainWindow::showMermaidEr);
    mermaidMenu->addSeparator();
    QAction *stateAction = mermaidMenu->addAction("S&tate Diagram...");
    connect(stateAction, &QAction::triggered, this, &MainWindow::showMermaidState);
    QAction *mindmapAction = mermaidMenu->addAction("&Mind Map...");
    connect(mindmapAction, &QAction::triggered, this, &MainWindow::showMermaidMindmap);
    mermaidMenu->addSeparator();
    QAction *timelineAction = mermaidMenu->addAction("&Timeline...");
    connect(timelineAction, &QAction::triggered, this, &MainWindow::showMermaidTimeline);
    QAction *journeyAction = mermaidMenu->addAction("User &Journey...");
    connect(journeyAction, &QAction::triggered, this, &MainWindow::showMermaidJourney);
    QAction *quadrantAction = mermaidMenu->addAction("&Quadrant Chart...");
    connect(quadrantAction, &QAction::triggered, this, &MainWindow::showMermaidQuadrant);
    QAction *sankeyAction = mermaidMenu->addAction("&Sankey...");
    connect(sankeyAction, &QAction::triggered, this, &MainWindow::showMermaidSankey);

    toolsMenu->addSeparator();

    QAction *logAction = toolsMenu->addAction("&Debug Log");
    logAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(logAction, &QAction::triggered, this, &MainWindow::showLogWindow);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About Scriba...");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    QAction *shortcutsAction = helpMenu->addAction("&Keyboard Shortcuts...");
    connect(shortcutsAction, &QAction::triggered, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Keyboard Shortcuts");
        dlg.resize(520, 420);
        auto *layout = new QVBoxLayout(&dlg);
        auto *browser = new QTextBrowser();
        browser->setReadOnly(true);
        QFile f(":/shortcuts.html");
        if (f.open(QIODevice::ReadOnly))
            browser->setHtml(f.readAll());
        layout->addWidget(browser);
        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
        for (auto *btn : btnBox->buttons()) btn->setIcon(QIcon());
        connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::close);
        layout->addWidget(btnBox);
        dlg.exec();
    });
}

void MainWindow::refreshPreviewCss()
{
    QString rawThemeCss = m_cssLoader->themeCss();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss);
    QString previewCss = chromeCss + rawThemeCss;
    QString previewBaseCss = m_cssLoader->previewBaseCss();

    bool needPreviewUpdate = (previewCss != m_cachedPreviewCss);
    bool needChromeUpdate = (chromeCss != m_cachedFullCss);
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
        m_cachedFullCss = chromeCss;
        QString paddingCss = applyEditorSettings();
        m_editor->setStyleSheet(chromeCss + paddingCss);
        if (!m_chromeUpdateScheduled) {
            m_chromeUpdateScheduled = true;
            QTimer::singleShot(0, this, [this, chromeCss]() {
                m_chromeUpdateScheduled = false;
                qApp->setStyleSheet(chromeCss);
            });
        }
        QColor iconColor = CssUtils::chromeTextColor(rawThemeCss);
        m_fullscreenBtn->setIcon(themedIcon(":/icons/fullscreen.svg", iconColor));
        m_previewBtn->setIcon(themedIcon(":/icons/preview.svg", iconColor));
    }
}

QString MainWindow::applyEditorSettings()
{
    QSettings settings;
    QString family = settings.value(Preferences::EditorFontFamily,
        "'Consolas', 'Monaco', 'Courier New', monospace").toString();
    int size = settings.value(Preferences::EditorFontSize, 18).toInt();
    int lineHeight = settings.value(Preferences::EditorLineHeight, 240).toInt();
    int padding = settings.value(Preferences::EditorPadding, 12).toInt();

    applyEditorSettings(family, size, lineHeight, padding);
    return QString("#scriba-editor { padding: %1px; }").arg(padding);
}

void MainWindow::applyEditorSettings(const QString &fontFamily, int fontSize, int lineHeight, int padding)
{
    QFont font;
    font.setFamily(fontFamily);
    font.setPointSize(fontSize);
    m_editor->setFont(font);

    QTextBlockFormat fmt;
    fmt.setLineHeight(lineHeight, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(m_editor->document());
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
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
        "return mermaid.run({querySelector:'.mermaid'});"
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
        "if(!els.length)return Promise.resolve();"
        "return Promise.all(Array.from(els).map(function(el){"
        "try{"
        "var spec=JSON.parse(el.textContent);"
        "var container=el.parentElement;"
        "var div=document.createElement('div');"
        "div.className='vega-lite-chart';"
        "div.style.width='100%';"
        "div.style.minHeight='300px';"
        "div.style.overflow='visible';"
        "container.parentElement.replaceChild(div,container);"
        "return vegaEmbed(div,spec,{actions:false}).catch(function(){});"
        "}"
        "catch(e){return Promise.resolve();}"
        "}));"
        "}"
    );

    static const QString setImgTitlesJs = QStringLiteral(
        "function setImgTitles(){"
        "document.querySelectorAll('img:not([title])').forEach(function(img){"
        "if(img.alt)img.title=img.alt;"
        "});"
        "}"
    );

    QString emojiMode = QSettings().value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();
    if (!m_previewInitialized) {
        m_cachedPreviewBaseCss = baseCss;
        QSettings prefs;
        bool striping = prefs.value(Preferences::TableStriping, true).toBool();
        QString stripeInit = striping ? QString()
            : QLatin1String(Preferences::TableStripeCss);
        QString centerCss;
        if (m_previewState == 3) {
            bool centre = prefs.value(Preferences::CentreSingleViewContent, true).toBool();
            int centreWidth = prefs.value(Preferences::CentreSingleViewWidth, 800).toInt();
            if (centre)
                centerCss = QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth);
        }
        QString fullHtml = QString(
            "<!DOCTYPE html><html><head>"
            "<style id=\"base-css\">%1</style>"
            "<style id=\"theme-css\">%2</style>"
            "<style id=\"stripe-css\">%4</style>"
            "<style id=\"center-css\">%5</style>"
            "<style>#preview .emoji-char{font-family:'Symbola',monospace}.emoji{height:1em;width:1em;vertical-align:-0.1em;display:inline-block}</style>"
            "<script src=\"qrc:///highlight.min.js\"></script>"
            "<script src=\"qrc:///mermaid.min.js\"></script>"
            "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
            "<script src=\"qrc:///katex.min.js\"></script>"
            "<script src=\"qrc:///contrib/auto-render.min.js\"></script>"
            "<script src=\"qrc:///vega.min.js\"></script>"
            "<script src=\"qrc:///vega-lite.min.js\"></script>"
            "<script src=\"qrc:///vega-embed.min.js\"></script>"
            "<script src=\"qrc:///twemoji.min.js\"></script>"
            "<script src=\"qrc:///emoji.js\"></script>"
            "<script>" + mermaidInitJs + headingIdJs + katexInitJs + vegaLiteInitJs + setImgTitlesJs + "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}document.addEventListener('DOMContentLoaded',function(){mermaid.initialize({startOnLoad:false,theme:'default'});initMermaid();hljs.highlightAll();generateHeadingIds();initKaTeX();initVegaLite();setImgTitles();replaceEmoji(document.body);twemojiParse('" + emojiMode + "');});</script>"
            "</head><body id=\"preview\">%3</body></html>"
        ).arg(baseCss, previewCss, html, stripeInit, centerCss);
        m_preview->setHtml(fullHtml, baseUrl);
        m_previewInitialized = true;
    } else {
        QString escapedHtml = escapeJsString(html);

        QString js;
        if (cssChanged) {
            QString escapedCss = escapeJsString(previewCss);
            js = QString(
                "if(!document.body){false}"
                "else{"
                "var sy = window.scrollY;"
                "document.getElementById('theme-css').textContent = '%1';"
                "document.body.innerHTML = '%2';"
                "var mermaidPromise=initMermaid();"
                "initKaTeX();"
                "var vlPromise=initVegaLite();"
                "hljs.highlightAll();"
                "generateHeadingIds();"
                "setImgTitles();"
                "replaceEmoji(document.body);"
                "twemojiParse('" + emojiMode + "');"
                "window.scrollTo(0, sy);"
                "(function(){"
                "var p=[];"
                "if(typeof mermaidPromise!=='undefined')p.push(mermaidPromise);"
                "if(typeof vlPromise!=='undefined')p.push(vlPromise);"
                "var imgs=document.querySelectorAll('img:not(.emoji)');"
                "if(imgs.length>0){"
                "p.push(new Promise(function(r){"
                "var n=0,t=imgs.length;"
                "function c(){n++;if(n>=t)r();}"
                "for(var i=0;i<imgs.length;i++){"
                "if(imgs[i].complete)c();"
                "else{imgs[i].onload=c;imgs[i].onerror=c;}"
                "}"
                "}));"
                "}"
                "if(p.length===0)return true;"
                "return Promise.all(p).then(function(){return true;});"
                "})()}"
            ).arg(escapedCss, escapedHtml);
        } else {
            js = QString(
                "if(!document.body){false}"
                "else{"
                "var sy = window.scrollY;"
                "document.body.innerHTML = '%1';"
                "var mermaidPromise=initMermaid();"
                "initKaTeX();"
                "var vlPromise=initVegaLite();"
                "hljs.highlightAll();"
                "generateHeadingIds();"
                "setImgTitles();"
                "replaceEmoji(document.body);"
                "twemojiParse('" + emojiMode + "');"
                "window.scrollTo(0, sy);"
                "(function(){"
                "var p=[];"
                "if(typeof mermaidPromise!=='undefined')p.push(mermaidPromise);"
                "if(typeof vlPromise!=='undefined')p.push(vlPromise);"
                "var imgs=document.querySelectorAll('img:not(.emoji)');"
                "if(imgs.length>0){"
                "p.push(new Promise(function(r){"
                "var n=0,t=imgs.length;"
                "function c(){n++;if(n>=t)r();}"
                "for(var i=0;i<imgs.length;i++){"
                "if(imgs[i].complete)c();"
                "else{imgs[i].onload=c;imgs[i].onerror=c;}"
                "}"
                "}));"
                "}"
                "if(p.length===0)return true;"
                "return Promise.all(p).then(function(){return true;});"
                "})()}"
            ).arg(escapedHtml);
        }
        m_preview->page()->runJavaScript(js, [this](const QVariant &result) {
            if (result.toBool())
                syncPreviewScroll();
        });
    }
}

void MainWindow::syncPreviewScroll()
{
    if (!m_previewInitialized)
        return;
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
    connect(&dlg, &PreferencesDialog::editorSettingsChanged, this,
        [this](const QString &f, int s, int lh, int p) {
            applyEditorSettings(f, s, lh, p);
        });
    dlg.exec();
    updateAll();
    applyStripeSetting();
    m_previewInitialized = false;
    updatePreview();

    QSettings s;
    int interval = s.value(Preferences::AutoSaveInterval, 0).toInt();
    if (interval > 0)
        m_autoSaveTimer->start(interval * 60000);
    else
        m_autoSaveTimer->stop();
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

void MainWindow::showMermaidPie()
{
    MermaidPieDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidFlowchart()
{
    MermaidFlowchartDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidSequence()
{
    MermaidSequenceDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidGantt()
{
    MermaidGanttDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidClass()
{
    MermaidClassDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidEr()
{
    MermaidErDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidState()
{
    MermaidStateDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidMindmap()
{
    MermaidMindmapDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidTimeline()
{
    MermaidTimelineDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidJourney()
{
    MermaidJourneyDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidQuadrant()
{
    MermaidQuadrantDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showMermaidSankey()
{
    MermaidSankeyDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString diagram = dlg.generatedDiagram();
        if (!diagram.isEmpty()) {
            QString block = "\n```mermaid\n" + diagram + "\n```\n";
            m_editor->insertPlainText(block);
        }
    }
}

void MainWindow::showTableInsert()
{
    TableDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString table = dlg.generateTable();
        QTextCursor cursor = m_editor->textCursor();
        int insertPos = cursor.position();
        cursor.insertText(table);
        int offset = dlg.hasHeader() ? 2 : 16;
        cursor.setPosition(insertPos + offset, QTextCursor::MoveAnchor);
        m_editor->setTextCursor(cursor);
        m_editor->centerCursor();
    }
}

void MainWindow::showLogWindow()
{
    if (!m_logWindow) {
        m_logWindow = new LogWindow(this);
        m_logWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_logWindow->show();
    m_logWindow->raise();
    m_logWindow->activateWindow();
}

void MainWindow::toggleFindDialog()
{
    if (!m_findDialog) {
        m_findDialog = new FindDialog(this);
        connect(m_findDialog, &FindDialog::findNextRequested, this, [this](const QString &text, bool useRegex, bool caseSensitive) {
            findText(text, false, useRegex, caseSensitive);
        });
        connect(m_findDialog, &FindDialog::findPrevRequested, this, [this](const QString &text, bool useRegex, bool caseSensitive) {
            findText(text, true, useRegex, caseSensitive);
        });
        connect(m_findDialog, &FindDialog::replaceRequested, this, &MainWindow::onReplace);
        connect(m_findDialog, &FindDialog::replaceAllRequested, this, &MainWindow::onReplaceAll);
        connect(m_findDialog, &FindDialog::searchTextChanged, this, [this](const QString &text, bool useRegex, bool caseSensitive) {
            int count = countMatches(text, useRegex, caseSensitive);
            m_findDialog->setMatchCount(count);
        });
    }

    if (!m_findDialog->searchTerm().isEmpty())
        m_findDialog->setMatchCount(countMatches(m_findDialog->searchTerm(), m_findDialog->regexEnabled(), m_findDialog->caseSensitive()));
    else
        m_findDialog->setMatchCount(0);

    m_findDialog->show();
    m_findDialog->raise();
    m_findDialog->activateWindow();
    m_findDialog->focusSearchInput();
}

void MainWindow::onFindNext()
{
    if (!m_findDialog) {
        toggleFindDialog();
        return;
    }
    QString text = m_findDialog->searchTerm();
    if (text.isEmpty()) {
        toggleFindDialog();
        return;
    }
    findText(text, false, m_findDialog->regexEnabled(), m_findDialog->caseSensitive());
}

void MainWindow::onFindPrev()
{
    if (!m_findDialog) {
        toggleFindDialog();
        return;
    }
    QString text = m_findDialog->searchTerm();
    if (text.isEmpty()) {
        toggleFindDialog();
        return;
    }
    findText(text, true, m_findDialog->regexEnabled(), m_findDialog->caseSensitive());
}

bool MainWindow::findText(const QString &text, bool backward, bool useRegex, bool caseSensitive)
{
    if (text.isEmpty()) return false;

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    if (backward)
        flags |= QTextDocument::FindBackward;

    bool found;
    if (useRegex)
        found = m_editor->find(QRegularExpression(text), flags);
    else
        found = m_editor->find(text, flags);

    if (!found) {
        QTextCursor c = m_editor->textCursor();
        c.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_editor->setTextCursor(c);

        if (useRegex)
            found = m_editor->find(QRegularExpression(text), flags);
        else
            found = m_editor->find(text, flags);

        if (found)
            statusBar()->showMessage("Search wrapped around", 3000);
    }

    if (!found)
        statusBar()->showMessage("No matches found", 3000);

    return found;
}

int MainWindow::countMatches(const QString &text, bool useRegex, bool caseSensitive) const
{
    if (text.isEmpty()) return 0;

    QTextDocument *doc = m_editor->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;

    int count = 0;

    if (useRegex) {
        auto opts = caseSensitive ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(text, opts);
        while (true) {
            QTextCursor match = doc->find(regex, cursor, flags);
            if (match.isNull()) break;
            count++;
            cursor = match;
        }
    } else {
        while (true) {
            QTextCursor match = doc->find(text, cursor, flags);
            if (match.isNull()) break;
            count++;
            cursor = match;
        }
    }

    return count;
}

void MainWindow::onReplace(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive)
{
    if (search.isEmpty()) return;

    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString sel = cursor.selectedText();
        if (useRegex) {
            auto opts = caseSensitive ? QRegularExpression::NoPatternOption
                                       : QRegularExpression::CaseInsensitiveOption;
            QRegularExpression regex(search, opts);
            QString replaced = sel;
            if (replaced.contains(regex)) {
                replaced.replace(regex, replacement);
                cursor.insertText(replaced);
                findText(search, false, useRegex, caseSensitive);
                return;
            }
        } else {
            if (sel == search) {
                cursor.insertText(replacement);
                findText(search, false, useRegex, caseSensitive);
                return;
            }
        }
    }
    findText(search, false, useRegex, caseSensitive);
}

void MainWindow::onReplaceAll(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive)
{
    if (search.isEmpty()) return;

    QTextDocument *doc = m_editor->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;

    int count = 0;
    cursor.beginEditBlock();

    if (useRegex) {
        auto opts = caseSensitive ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(search, opts);
        while (true) {
            QTextCursor match = doc->find(regex, cursor, flags);
            if (match.isNull()) break;
            QString replaced = match.selectedText();
            replaced.replace(regex, replacement);
            match.insertText(replaced);
            count++;
            cursor = match;
        }
    } else {
        while (true) {
            QTextCursor match = doc->find(search, cursor, flags);
            if (match.isNull()) break;
            match.insertText(replacement);
            count++;
            cursor = match;
        }
    }

    cursor.endEditBlock();

    if (count > 0)
        statusBar()->showMessage(QString("Replaced %1 occurrence%2").arg(count).arg(count == 1 ? "" : "s"), 5000);
    else
        statusBar()->showMessage("No matches found", 3000);
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
    // Cycle: 0 (editor only) → 1 (editor|preview) → 2 (preview|editor) → 3 (preview only) → 0
    m_previewState = (m_previewState + 1) % 4;
    QSettings().setValue(Preferences::PreviewState, m_previewState);

    QSettings settings;
    bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
    int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();

    if (m_previewState == 0) {
        m_preview->setVisible(false);
        m_editor->setVisible(true);
        m_editor->setCenterContent(centre, centreWidth);
    } else if (m_previewState == 3) {
        m_editor->setVisible(false);
        m_preview->setVisible(true);
        m_editor->setCenterContent(false, 0);
        if (m_previewInitialized) {
            QString css = centre
                ? QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth)
                : QString();
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent='%1'").arg(css));
        }
    } else if (m_previewState == 1) {
        // editor | preview
        m_splitter->insertWidget(0, m_editor);
        m_splitter->insertWidget(1, m_preview);
        m_editor->setVisible(true);
        m_preview->setVisible(true);
        m_editor->setCenterContent(false, 0);
        if (m_previewInitialized)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
    } else {
        // preview | editor
        m_splitter->insertWidget(0, m_preview);
        m_splitter->insertWidget(1, m_editor);
        m_preview->setVisible(true);
        m_editor->setVisible(true);
        m_editor->setCenterContent(false, 0);
        if (m_previewInitialized)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
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
    m_editor->setCurrentFile(filePath);
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
    m_editor->setCurrentFile(filePath);
    setWindowTitle("Scriba - " + filePath);
    m_preview->setDocumentPath(filePath);
    statusBar()->showMessage("Saved", 2000);

    QSettings settings;
    settings.setValue(Preferences::LastOpenedFile, filePath);
}

void MainWindow::autoSave()
{
    if (m_currentFile.isEmpty())
        return;
    QFile file(m_currentFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        file.write(m_editor->toPlainText().toUtf8());
}

void MainWindow::exportPdf()
{
    QString markdown = m_editor->toPlainText();
    QString html = m_parser->toHtml(markdown);

    ExportPdfDialog dlg(html, m_currentFile, m_cssLoader, this);
    dlg.exec();
}



void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s;
    if (!m_currentFile.isEmpty()) {
        if (s.value(Preferences::AutoSaveOnExit, false).toBool())
            autoSave();
        QTextCursor cursor = m_editor->textCursor();
        s.setValue(Preferences::LastCursorBlock, cursor.blockNumber());
        s.setValue(Preferences::LastCursorColumn, cursor.positionInBlock());
        s.setValue(Preferences::LastScrollTop, m_editor->verticalScrollBar()->value());
    }
    QMainWindow::closeEvent(event);
}

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
#include "JsSnippets.h"
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
#include "KatexHelperDialog.h"

static constexpr const char *kMdFilter = "Markdown Files (*.md);;All Files (*)";
static constexpr const char *kOpenMdFilter = "Markdown Files (*.md *.markdown *.txt);;All Files (*)";
static constexpr int kMsPerMinute = 60000;

namespace {

template<typename T>
void showMermaidDialog(MainWindow *win, CssLoader *loader)
{
    T dlg(loader->themeCss(), win);
    if (dlg.exec() == QDialog::Accepted) {
        QString block = dlg.mermaidBlock();
        Editor *ed = win->editor();
        if (!block.isEmpty() && ed)
            ed->insertPlainText(block);
    }
}

}

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
#include <QDesktopServices>
#include <QTabBar>
#include <QJsonDocument>
#include <QJsonArray>
#include <QInputDialog>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent, bool skipSessionRestore)
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
        if (ok) {
            m_previewInitialized = true;
            syncPreviewScroll();
        }
    });

    connect(m_preview->page(), &QWebEnginePage::urlChanged, this, [this](const QUrl &url) {
        QString frag = url.fragment(QUrl::FullyDecoded);
        if (frag.startsWith("scriba-open:")) {
            QUrl target(frag.mid(12));
            if (target.isLocalFile()) {
                QString localPath = target.toLocalFile();
                QFileInfo fi(localPath);
                if (fi.suffix().compare("md", Qt::CaseInsensitive) == 0) {
                    loadFile(localPath);
                } else {
                    QDesktopServices::openUrl(target);
                }
            } else {
                QDesktopServices::openUrl(target);
            }
            m_preview->page()->runJavaScript("window.location.hash=''");
        }
    });

    refreshPreviewCss();

    setWindowTitle("Scriba");
    showMaximized();

    m_updateTimer = new DebounceTimer(80, this);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updatePreview);

    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);

    connect(m_cssWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onCssFileChanged);
    syncCssWatcher();

    if (m_cssConfig->stylesheets().isEmpty()) {
        m_cssConfig->setStylesheets(CssConfig::bundledThemes());
        if (m_cssConfig->activeStylesheet().isEmpty())
            m_cssConfig->setActiveStylesheet(":/themes/github-light.css");
        m_cssLoader->invalidateCache();
        refreshPreviewCss();
        applyStripeSetting();
    }

    QSettings settings;
    int asInterval = settings.value(Preferences::AutoSaveInterval, 0).toInt();
    if (asInterval > 0)
        m_autoSaveTimer->start(asInterval * kMsPerMinute);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    addTab();

    if (!skipSessionRestore) {
        bool reopen = settings.value(Preferences::ReopenLastSession, true).toBool();
        if (reopen) {
            QString raw = settings.value(Preferences::SessionData).toString();
            if (!raw.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
                if (doc.isObject())
                    restoreSession(doc.object());
            }
        }
    }

    connectActiveEditor();
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

    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setMovable(true);

    m_preview = new Preview(this);

    if (m_previewState == 0) {
        m_splitter->addWidget(m_tabWidget);
        m_preview->setVisible(false);
    } else if (m_previewState == 3) {
        m_splitter->addWidget(m_preview);
        m_tabWidget->setVisible(false);
    } else if (m_previewState == 1) {
        m_splitter->addWidget(m_tabWidget);
        m_splitter->addWidget(m_preview);
    } else {
        m_splitter->addWidget(m_preview);
        m_splitter->addWidget(m_tabWidget);
    }

    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    m_splitter->setSizes({600, 600});
    m_splitter->setStretchFactor(0, 1);
    if (m_previewState != 0 && m_previewState != 3) {
        m_splitter->setStretchFactor(1, 1);
        m_splitter->handle(1)->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    if (m_previewState == 0 || m_previewState == 3) {
        QSettings settings;
        bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
        int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();
        if (m_previewState == 0)
            ; // will setCenterContent on the active editor later
    }

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

int MainWindow::addTab(const QString &filePath)
{
    int existing = findTabByPath(filePath);
    if (existing >= 0) {
        m_tabWidget->setCurrentIndex(existing);
        return existing;
    }

    auto *editor = new Editor();
    editor->setInsertActions(m_insertActions);
    editor->setMermaidActions(m_mermaidActions);

    QString label = filePath.isEmpty() ? QStringLiteral("Untitled")
                                       : QFileInfo(filePath).fileName();

    // Populate m_tabs BEFORE QTabWidget::addTab so currentChanged handlers
    // (which fire synchronously for the first tab) can find the TabInfo.
    int idx = m_tabs.size();
    m_tabs.append({editor, filePath, false});
    m_tabWidget->addTab(editor, label);
    m_tabWidget->setCurrentIndex(idx);
    m_tabWidget->setTabToolTip(idx, filePath.isEmpty() ? QString() : filePath);

    connect(editor, &QTextEdit::textChanged, this, [this, editor]() {
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].editor == editor && !m_tabs[i].dirty) {
                setTabDirty(i, true);
                break;
            }
        }
    });

    updateTabBarVisibility();
    return idx;
}

void MainWindow::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size() || m_tabs.size() <= 1)
        return;

    disconnectTabEditor(index);
    m_connectedTabIndex = -1;

    Editor *editor = m_tabs[index].editor;
    m_tabs.removeAt(index);
    m_tabWidget->removeTab(index);
    delete editor;
    updateTabBarVisibility();
}

int MainWindow::findTabByPath(const QString &filePath) const
{
    if (filePath.isEmpty())
        return -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == filePath)
            return i;
    }
    return -1;
}

void MainWindow::connectTabEditor(int index)
{
    if (m_connectedTabIndex == index)
        return;

    disconnectActiveEditor();
    m_connectedTabIndex = index;

    if (index < 0 || index >= m_tabs.size())
        return;

    Editor *editor = m_tabs[index].editor;
    if (!editor)
        return;

    if (m_updateTimer)
        connect(editor, &QTextEdit::textChanged, m_updateTimer, qOverload<>(&QTimer::start));

    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::onEditorScroll);
}

void MainWindow::disconnectTabEditor(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    Editor *editor = m_tabs[index].editor;
    if (!editor)
        return;

    if (m_updateTimer)
        disconnect(editor, &QTextEdit::textChanged, m_updateTimer, nullptr);

    disconnect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
               this, &MainWindow::onEditorScroll);
}

void MainWindow::disconnectActiveEditor()
{
    disconnectTabEditor(m_connectedTabIndex);
    m_connectedTabIndex = -1;
}

void MainWindow::connectActiveEditor()
{
    int idx = m_tabWidget->currentIndex();
    connectTabEditor(idx);
}

Editor *MainWindow::currentEditor() const
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 0 || idx >= m_tabs.size())
        return nullptr;
    return m_tabs[idx].editor;
}

TabInfo *MainWindow::activeTabInfo()
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 0 || idx >= m_tabs.size())
        return nullptr;
    return &m_tabs[idx];
}

void MainWindow::updateTabLabel(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    const TabInfo &info = m_tabs[index];
    QString name = info.filePath.isEmpty() ? QStringLiteral("Untitled")
                                           : QFileInfo(info.filePath).fileName();
    if (info.dirty)
        name += QStringLiteral(" *");
    m_tabWidget->setTabText(index, name);
}

void MainWindow::setTabDirty(int index, bool dirty)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    if (m_tabs[index].dirty == dirty)
        return;
    m_tabs[index].dirty = dirty;
    updateTabLabel(index);
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    connectActiveEditor();

    TabInfo *info = activeTabInfo();
    if (info) {
        setWindowTitle(info->filePath.isEmpty()
            ? QStringLiteral("Scriba - Untitled")
            : QStringLiteral("Scriba - ") + info->filePath);
        m_preview->setDocumentPath(info->filePath);
        m_previewInitialized = false;
        updatePreview();

        if (m_previewState == 0) {
            QSettings settings;
            bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
            int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();
            info->editor->setCenterContent(centre, centreWidth);
        }
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    if (m_tabs.size() <= 1) {
        showSaveDiscardDialog(index);
        if (m_tabs.size() == 1 && !m_tabs[0].dirty) {
            m_tabs[0].filePath.clear();
            m_tabs[0].editor->clear();
            m_tabs[0].dirty = false;
            updateTabLabel(0);
            setWindowTitle("Scriba - Untitled");
            m_preview->setDocumentPath(QString());
            m_previewInitialized = false;
            updatePreview();
        }
        return;
    }

    showSaveDiscardDialog(index);
}

void MainWindow::showSaveDiscardDialog(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    TabInfo &info = m_tabs[index];
    if (!info.dirty) {
        removeTab(index);
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("Unsaved Changes");
    msgBox.setText(QString("Do you want to save changes to \"%1\"?")
        .arg(info.filePath.isEmpty() ? QStringLiteral("Untitled")
                                     : QFileInfo(info.filePath).fileName()));
    msgBox.setInformativeText("Your changes will be lost if you don't save them.");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);

    int ret = msgBox.exec();
    if (ret == QMessageBox::Save) {
        if (info.filePath.isEmpty()) {
                        QString file = QFileDialog::getSaveFileName(this, "Save File", QString(), kMdFilter);
            if (file.isEmpty())
                return;
            info.filePath = file;
        }
        saveFile(info.filePath);
        removeTab(index);
    } else if (ret == QMessageBox::Discard) {
        removeTab(index);
    }
}

void MainWindow::closeCurrentTab()
{
    int idx = m_tabWidget->currentIndex();
    onTabCloseRequested(idx);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *newAction = fileMenu->addAction("&New Tab");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        addTab();
    });

    QAction *openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Open Markdown File(s)", QString(), kOpenMdFilter);
        for (const QString &file : files)
            loadFile(file);
    });

    QAction *reloadAction = fileMenu->addAction("&Reload");
    reloadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(reloadAction, &QAction::triggered, this, [this]() {
        TabInfo *info = activeTabInfo();
        if (info && !info->filePath.isEmpty())
            loadFile(info->filePath);
    });

    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        TabInfo *info = activeTabInfo();
        if (!info) return;
        if (info->filePath.isEmpty()) {
            QString file = QFileDialog::getSaveFileName(this, "Save Markdown File", QString(), kMdFilter);
            if (!file.isEmpty()) saveFile(file);
        } else {
            saveFile(info->filePath);
        }
    });

    QAction *saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "Save Markdown File As", QString(), kMdFilter);
        if (!file.isEmpty()) saveFile(file);
    });

    fileMenu->addSeparator();

    QAction *closeTabAction = fileMenu->addAction("&Close Tab");
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    fileMenu->addSeparator();

    QAction *exportPdfAction = fileMenu->addAction("&Print / Export PDF...");
    exportPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::exportPdf);

    fileMenu->addSeparator();

    QMenu *sessionMenu = fileMenu->addMenu("&Session");
    QAction *saveSessionAction = sessionMenu->addAction("&Save Session As...");
    connect(saveSessionAction, &QAction::triggered, this, &MainWindow::saveSessionAsAction);

    QAction *loadSessionAction = sessionMenu->addAction("&Load Session...");
    connect(loadSessionAction, &QAction::triggered, this, &MainWindow::loadSessionAction);

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

    QMenu *viewMenu = menuBar()->addMenu("&View");

    QAction *nextTabAction = viewMenu->addAction("&Next Tab");
    nextTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(nextTabAction, &QAction::triggered, this, [this]() {
        int count = m_tabWidget->count();
        if (count > 1)
            m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() + 1) % count);
    });

    QAction *prevTabAction = viewMenu->addAction("&Previous Tab");
    prevTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTabAction, &QAction::triggered, this, [this]() {
        int count = m_tabWidget->count();
        if (count > 1)
            m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() - 1 + count) % count);
    });

    for (int i = 1; i <= 10; ++i) {
        Qt::Key key = (i == 10) ? Qt::Key_0
                                : static_cast<Qt::Key>(Qt::Key_0 + i);
        QAction *tabAction = new QAction(QString("Switch to Tab %1").arg(i), this);
        tabAction->setShortcut(QKeySequence(Qt::ALT | key));
        connect(tabAction, &QAction::triggered, this, [this, i]() {
            if (i - 1 < m_tabWidget->count())
                m_tabWidget->setCurrentIndex(i - 1);
        });
        addAction(tabAction);
    }

    QMenu *toolsMenu = menuBar()->addMenu("&Tools");

    QAction *tableAction = toolsMenu->addAction("&Table Insert...");
    tableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(tableAction, &QAction::triggered, this, &MainWindow::showTableInsert);

    QAction *emojiAction = toolsMenu->addAction("&Emoji Picker...");
    emojiAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(emojiAction, &QAction::triggered, this, [this]() {
        EmojiDialog dlg(this);
        Editor *ed = currentEditor();
        if (!ed) return;
        connect(&dlg, &EmojiDialog::emojiChosen, this, [this, ed](const QString &sc) {
            ed->insertPlainText(":" + sc + ":");
        });
        dlg.exec();
    });

    QAction *katexAction = toolsMenu->addAction("KaTeX &Equation...");
    katexAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(katexAction, &QAction::triggered, this, &MainWindow::showKatexHelper);

    toolsMenu->addSeparator();

    QAction *chartAction = toolsMenu->addAction("Vega-Lite &Charts");
    chartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(chartAction, &QAction::triggered, this, &MainWindow::showChartBuilder);

    QMenu *mermaidMenu = toolsMenu->addMenu("Mermaid &Charts");
    QAction *pieAction = mermaidMenu->addAction("&Pie Chart...");
    connect(pieAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidPieDialog>(this, m_cssLoader); });
    QAction *flowchartAction = mermaidMenu->addAction("&Flowchart...");
    connect(flowchartAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidFlowchartDialog>(this, m_cssLoader); });
    QAction *sequenceAction = mermaidMenu->addAction("&Sequence Diagram...");
    connect(sequenceAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidSequenceDialog>(this, m_cssLoader); });
    QAction *ganttAction = mermaidMenu->addAction("&Gantt Chart...");
    connect(ganttAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidGanttDialog>(this, m_cssLoader); });
    QAction *classAction = mermaidMenu->addAction("&Class Diagram...");
    connect(classAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidClassDialog>(this, m_cssLoader); });
    QAction *erAction = mermaidMenu->addAction("&ER Diagram...");
    connect(erAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidErDialog>(this, m_cssLoader); });
    mermaidMenu->addSeparator();
    QAction *stateAction = mermaidMenu->addAction("S&tate Diagram...");
    connect(stateAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidStateDialog>(this, m_cssLoader); });
    QAction *mindmapAction = mermaidMenu->addAction("&Mind Map...");
    connect(mindmapAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidMindmapDialog>(this, m_cssLoader); });
    mermaidMenu->addSeparator();
    QAction *timelineAction = mermaidMenu->addAction("&Timeline...");
    connect(timelineAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidTimelineDialog>(this, m_cssLoader); });
    QAction *journeyAction = mermaidMenu->addAction("User &Journey...");
    connect(journeyAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidJourneyDialog>(this, m_cssLoader); });
    QAction *quadrantAction = mermaidMenu->addAction("&Quadrant Chart...");
    connect(quadrantAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidQuadrantDialog>(this, m_cssLoader); });
    QAction *sankeyAction = mermaidMenu->addAction("&Sankey...");
    connect(sankeyAction, &QAction::triggered, this, [this]() { showMermaidDialog<MermaidSankeyDialog>(this, m_cssLoader); });

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
        btnBox->button(QDialogButtonBox::Close)->setText(tr("&Close"));
        stripButtonIcons(btnBox);
        connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::close);
        layout->addWidget(btnBox);
        dlg.exec();
    });

    m_insertActions = {tableAction, emojiAction, katexAction, chartAction};
    m_mermaidActions = {pieAction, flowchartAction, sequenceAction, ganttAction,
                  classAction, erAction, stateAction, mindmapAction,
                  timelineAction, journeyAction, quadrantAction, sankeyAction};

    for (TabInfo &info : m_tabs) {
        if (info.editor) {
            info.editor->setInsertActions(m_insertActions);
            info.editor->setMermaidActions(m_mermaidActions);
        }
    }
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
        Editor *ed = currentEditor();
        if (ed) {
            ed->setStyleSheet(chromeCss + applyEditorSettings());
            ed->update();
        }
        QSettings s;
        applyEditorLineHeight(s.value(Preferences::EditorLineHeight, 240).toInt());
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
    int padding = settings.value(Preferences::EditorPadding, 12).toInt();

    return applyEditorSettings(family, size, padding);
}

QString MainWindow::applyEditorSettings(const QString &fontFamily, int fontSize, int padding)
{
    QString css = QString("#scriba-editor { padding: %1px; font-family: %2; font-size: %3pt; }")
        .arg(padding).arg(fontFamily).arg(fontSize);
    QSettings s;
    if (s.value(Preferences::EditorColorOverride, false).toBool()) {
        QString bg = s.value(Preferences::EditorBgColor).toString();
        QString fg = s.value(Preferences::EditorFontColor).toString();
        if (!bg.isEmpty() || !fg.isEmpty()) {
            css += "#scriba-editor {";
            if (!bg.isEmpty()) css += " background-color: " + bg + " !important;";
            if (!fg.isEmpty()) css += " color: " + fg + " !important;";
            css += " }";
        }
        if (!bg.isEmpty()) {
            QColor bgColor(bg);
            if (bgColor.isValid()) {
                bool dark = bgColor.lightness() < 128;
                QColor track = dark ? bgColor.lighter(110) : bgColor.darker(105);
                QColor thumb = dark ? bgColor.lighter(130) : bgColor.darker(125);
                QColor hover = dark ? bgColor.lighter(160) : bgColor.darker(145);
                css += "#scriba-editor QScrollBar:vertical { background: " + track.name() + "; width: 12px; }";
                css += "#scriba-editor QScrollBar::handle:vertical { background: " + thumb.name() + "; border-radius: 6px; min-height: 30px; }";
                css += "#scriba-editor QScrollBar::handle:vertical:hover { background: " + hover.name() + "; }";
                css += "#scriba-editor QScrollBar::add-line:vertical, #scriba-editor QScrollBar::sub-line:vertical { height: 0; }";
                css += "#scriba-editor QScrollBar::add-page:vertical, #scriba-editor QScrollBar::sub-page:vertical { background: none; }";
            }
        }
    }
    return css;
}

void MainWindow::applyEditorLineHeight(int lineHeight)
{
    Editor *ed = currentEditor();
    if (!ed) return;
    QTextBlockFormat fmt;
    fmt.setLineHeight(lineHeight, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(ed->document());
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
    Editor *ed = currentEditor();
    if (!ed) return;

    QString markdown = ed->toPlainText();
    updateStats();
    QString html = m_parser->toHtml(markdown);

    QString rawThemeCss = m_cssLoader->themeCss();
    QString baseCss = m_cssLoader->previewBaseCss();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss);
    QString previewCss = chromeCss + rawThemeCss;
    QString mermaidTheme = CssUtils::isDarkTheme(rawThemeCss)
        ? QStringLiteral("dark") : QStringLiteral("default");

    bool cssChanged = (previewCss != m_cachedPreviewCss);
    if (cssChanged) {
        m_cachedPreviewCss = previewCss;
    }

    QUrl baseUrl;
    TabInfo *info = activeTabInfo();
    QString docPath = info ? info->filePath : QString();
    if (!docPath.isEmpty()) {
        baseUrl = QUrl::fromLocalFile(QFileInfo(docPath).absolutePath() + "/");
    }



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
            "<script>" + mermaidInitJs + headingIdJs + katexInitJs + vegaLiteInitJs + setImgTitlesJs + "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}document.addEventListener('DOMContentLoaded',function(){mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});window.mermaidReady=initMermaid();hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();generateHeadingIds();initKaTeX();window.vegaLiteReady=initVegaLite();setImgTitles();replaceEmoji(document.body);twemojiParse('" + emojiMode + "');});</script>"
            "</head><body id=\"preview\">%3"
            "<script>document.addEventListener('click',function(e){"
            "var l=e.target.closest('a');if(!l)return;"
            "e.preventDefault();"
            "window.location.hash='scriba-open:'+encodeURIComponent(l.href)"
            "})</script>"
            "</body></html>"
        ).arg(baseCss, previewCss, html, stripeInit, centerCss);
        m_preview->setHtml(fullHtml, baseUrl);
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
				"mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});"
				"var mermaidPromise=initMermaid();"
				"initKaTeX();"
				"var vlPromise=initVegaLite();"
				"hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();"
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
                "mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});"
                "var mermaidPromise=initMermaid();"
                "initKaTeX();"
                "var vlPromise=initVegaLite();"
				"hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();"
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
    Editor *ed = currentEditor();
    if (!ed) return;
    auto *sb = ed->verticalScrollBar();
    double range = sb->maximum() - sb->minimum();
    double pct = range > 0 ? static_cast<double>(sb->value() - sb->minimum()) / range : 0.0;
    m_preview->scrollToPercent(pct);
}

void MainWindow::showPreferences()
{
    QString oldStylesheet = m_cssConfig->activeStylesheet();
    CssUtils::ThemeColors tc = CssUtils::themeColors(m_cssLoader->themeCss());
    PreferencesDialog dlg(m_cssConfig, m_cssLoader, this,
        tc.background.name(), tc.text.name());
    auto updateAll = [this]() {
        syncCssWatcher();
        refreshPreviewCss();
        applyStripeSetting();
    };
    connect(&dlg, &PreferencesDialog::stylesheetChanged, this, updateAll);
    connect(&dlg, &PreferencesDialog::editorSettingsChanged, this,
        [this](const QString &f, int s, int lh, int p) {
            Editor *ed = currentEditor();
            if (ed) {
                ed->setStyleSheet(m_cachedFullCss + applyEditorSettings(f, s, p));
                ed->update();
            }
            applyEditorLineHeight(lh);
        });
    if (dlg.exec() == QDialog::Rejected) {
        m_cssConfig->setActiveStylesheet(oldStylesheet);
        m_cssLoader->invalidateCache();
    }
    Editor *ed = currentEditor();
    if (ed) {
        ed->setStyleSheet(m_cachedFullCss + applyEditorSettings());
        ed->update();
    }
    QSettings s;
    applyEditorLineHeight(s.value(Preferences::EditorLineHeight, 240).toInt());
    updateAll();
    applyStripeSetting();
    for (const auto &tab : m_tabs) {
        if (tab.editor)
            tab.editor->invalidateEmojiIconCache();
    }
    m_previewInitialized = false;
    updatePreview();

    int interval = s.value(Preferences::AutoSaveInterval, 0).toInt();
    if (interval > 0)
        m_autoSaveTimer->start(interval * kMsPerMinute);
    else
        m_autoSaveTimer->stop();
}

void MainWindow::showChartBuilder()
{
    VegaLiteDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString spec = dlg.generatedSpec();
        Editor *ed = currentEditor();
        if (!spec.isEmpty() && ed)
            ed->insertPlainText(spec);
    }
}

void MainWindow::showKatexHelper()
{
    KatexHelperDialog dlg(m_cssLoader->themeCss(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString latex = dlg.generatedLatex();
        Editor *ed = currentEditor();
        if (!latex.isEmpty() && ed)
            ed->insertPlainText(latex);
    }
}

void MainWindow::showTableInsert()
{
    TableDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Editor *ed = currentEditor();
        if (!ed) return;
        QString table = dlg.generateTable();
        QTextCursor cursor = ed->textCursor();
        int insertPos = cursor.position();
        cursor.insertText(table);
        int offset = dlg.hasHeader() ? 2 : 16;
        cursor.setPosition(insertPos + offset, QTextCursor::MoveAnchor);
        ed->setTextCursor(cursor);
        ed->centerCursor();
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

    Editor *ed = currentEditor();
    if (!ed) return false;

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    if (backward)
        flags |= QTextDocument::FindBackward;

    bool found;
    if (useRegex)
        found = ed->find(QRegularExpression(text), flags);
    else
        found = ed->find(text, flags);

    if (!found) {
        QTextCursor c = ed->textCursor();
        c.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        ed->setTextCursor(c);

        if (useRegex)
            found = ed->find(QRegularExpression(text), flags);
        else
            found = ed->find(text, flags);

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

    Editor *ed = currentEditor();
    if (!ed) return 0;

    QTextDocument *doc = ed->document();
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
    Editor *ed = currentEditor();
    if (!ed) return;

    QTextCursor cursor = ed->textCursor();
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
    Editor *ed = currentEditor();
    if (!ed) return;

    QTextDocument *doc = ed->document();
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
    m_previewState = (m_previewState + 1) % 4;
    QSettings().setValue(Preferences::PreviewState, m_previewState);

    QSettings settings;
    bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
    int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();

    Editor *ed = currentEditor();

    if (m_previewState == 0) {
        m_preview->setVisible(false);
        m_tabWidget->setVisible(true);
        if (ed) ed->setCenterContent(centre, centreWidth);
    } else if (m_previewState == 3) {
        m_tabWidget->setVisible(false);
        m_preview->setVisible(true);
        if (ed) ed->setCenterContent(false, 0);
        if (m_previewInitialized) {
            QString css = centre
                ? QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth)
                : QString();
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent='%1'").arg(css));
        }
    } else if (m_previewState == 1) {
        m_splitter->insertWidget(0, m_tabWidget);
        m_splitter->insertWidget(1, m_preview);
        m_tabWidget->setVisible(true);
        m_preview->setVisible(true);
        if (ed) ed->setCenterContent(false, 0);
        if (m_previewInitialized)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
    } else {
        m_splitter->insertWidget(0, m_preview);
        m_splitter->insertWidget(1, m_tabWidget);
        m_preview->setVisible(true);
        m_tabWidget->setVisible(true);
        if (ed) ed->setCenterContent(false, 0);
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
    Editor *ed = currentEditor();
    if (!ed) {
        m_statsLabel->clear();
        return;
    }
    QString text = ed->toPlainText().trimmed();
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

    int existing = findTabByPath(filePath);
    if (existing >= 0) {
        m_tabWidget->setCurrentIndex(existing);
        file.close();
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    TabInfo *info = nullptr;
    int idx = m_tabWidget->currentIndex();
    bool replacedUntitled = false;

    if (idx >= 0 && idx < m_tabs.size() && m_tabs[idx].filePath.isEmpty() && !m_tabs[idx].dirty && m_tabs[idx].editor->toPlainText().isEmpty()) {
        m_tabs[idx].filePath = filePath;
        m_tabs[idx].editor->setPlainText(content);
        m_tabs[idx].dirty = false;
        info = &m_tabs[idx];
        updateTabLabel(idx);
        m_tabWidget->setTabToolTip(idx, filePath);
        replacedUntitled = true;
    } else {
        idx = addTab(filePath);
        m_tabs[idx].editor->setPlainText(content);
        m_tabs[idx].dirty = false;
        updateTabLabel(idx);
        info = &m_tabs[idx];
    }

    setWindowTitle("Scriba - " + filePath);
    m_preview->setDocumentPath(filePath);
    m_previewInitialized = false;
    updatePreview();

    QSettings settings;
    if (!replacedUntitled)
        settings.setValue(Preferences::LastSessionName, filePath);
}

void MainWindow::saveFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file: " + filePath);
        return;
    }

    Editor *ed = currentEditor();
    if (!ed) return;

    file.write(ed->toPlainText().toUtf8());
    file.close();

    TabInfo *info = activeTabInfo();
    if (!info) return;

    bool pathChanged = (info->filePath != filePath);
    info->filePath = filePath;
    info->dirty = false;

    int idx = m_tabWidget->currentIndex();
    updateTabLabel(idx);
    m_tabWidget->setTabToolTip(idx, filePath);

    setWindowTitle("Scriba - " + filePath);
    m_preview->setDocumentPath(filePath);
    statusBar()->showMessage("Saved", 2000);

    QSettings settings;
    if (pathChanged)
        settings.setValue(Preferences::LastSessionName, filePath);
}

void MainWindow::autoSave()
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        TabInfo &info = m_tabs[i];
        if (info.filePath.isEmpty())
            continue;
        QFile file(info.filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(info.editor->toPlainText().toUtf8());
            info.dirty = false;
            updateTabLabel(i);
        }
    }
}

void MainWindow::exportPdf()
{
    Editor *ed = currentEditor();
    if (!ed) return;
    TabInfo *info = activeTabInfo();
    if (!info) return;

    QString markdown = ed->toPlainText();
    QString html = m_parser->toHtml(markdown);

    ExportPdfDialog dlg(html, info->filePath, m_cssLoader, this);
    dlg.exec();
}

QJsonObject MainWindow::serializeSession() const
{
    QJsonObject root;
    root["version"] = 1;
    QJsonArray files;
    QJsonArray cursors;

    int rawActive = m_tabWidget->currentIndex();
    int fileActive = 0;
    int activeIndex = -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath.isEmpty()) continue;
        if (i == rawActive) { activeIndex = fileActive; break; }
        ++fileActive;
    }
    root["active"] = activeIndex;

    for (int i = 0; i < m_tabs.size(); ++i) {
        const TabInfo &info = m_tabs[i];
        if (info.filePath.isEmpty())
            continue;

        files.append(info.filePath);

        QJsonObject cursor;
        cursor["block"] = info.editor->textCursor().blockNumber();
        cursor["col"] = info.editor->textCursor().positionInBlock();
        cursor["scroll"] = info.editor->verticalScrollBar()->value();
        cursors.append(cursor);
    }

    root["files"] = files;
    root["cursors"] = cursors;
    return root;
}

void MainWindow::restoreSession(const QJsonObject &session)
{
    int version = session["version"].toInt();
    if (version != 1) return;

    QJsonArray files = session["files"].toArray();
    QJsonArray cursors = session["cursors"].toArray();
    int active = session["active"].toInt(0);

    if (files.isEmpty()) return;

    bool firstTabIsEmpty = (m_tabs.size() == 1 && m_tabs[0].filePath.isEmpty()
                            && !m_tabs[0].dirty && m_tabs[0].editor->toPlainText().isEmpty());

    if (firstTabIsEmpty) {
        int idx = m_tabWidget->currentIndex();
        disconnectTabEditor(idx);
        Editor *ed = m_tabs[idx].editor;
        m_tabs.removeAt(idx);
        m_tabWidget->removeTab(idx);
        delete ed;
    }

    for (int i = 0; i < files.size(); ++i) {
        QString path = files[i].toString();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QString content = QString::fromUtf8(f.readAll());
        f.close();

        int idx = addTab(path);
        m_tabs[idx].editor->setPlainText(content);
        m_tabs[idx].dirty = false;
        updateTabLabel(idx);

        if (i < cursors.size()) {
            QJsonObject c = cursors[i].toObject();
            int block = c["block"].toInt();
            int col = c["col"].toInt();
            int scroll = c["scroll"].toInt();
            QTextCursor tc = restoreCursorPosition(m_tabs[idx].editor->document(), block, col);
            m_tabs[idx].editor->setTextCursor(tc);
            QTimer::singleShot(0, [this, idx, scroll]() {
                if (idx < m_tabs.size() && m_tabs[idx].editor)
                    m_tabs[idx].editor->verticalScrollBar()->setValue(scroll);
            });
        }
    }

    if (active >= 0 && active < m_tabWidget->count())
        m_tabWidget->setCurrentIndex(active);

    updateTabBarVisibility();
    if (auto *ed = currentEditor())
        ed->setFocus();
}

void MainWindow::saveSessionAction()
{
    QSettings settings;
    QString name = settings.value(Preferences::LastSessionName).toString();
    if (name.isEmpty())
        saveSessionAsAction();
    else {
        QJsonObject session = serializeSession();
        QSettings s;
        s.setValue(Preferences::SessionData, QString::fromUtf8(QJsonDocument(session).toJson(QJsonDocument::Compact)));
        s.setValue(Preferences::LastSessionName, name);
        statusBar()->showMessage("Session saved", 2000);
    }
}

void MainWindow::saveSessionAsAction()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Save Session As",
        "Session name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty())
        return;

    QJsonObject session = serializeSession();
    QSettings s;
    s.setValue(Preferences::SessionData, QString::fromUtf8(QJsonDocument(session).toJson(QJsonDocument::Compact)));
    s.setValue(Preferences::LastSessionName, name);
    statusBar()->showMessage("Session saved as \"" + name + "\"", 2000);
}

void MainWindow::loadSessionAction()
{
    QSettings s;
    QString raw = s.value(Preferences::SessionData).toString();
    if (raw.isEmpty()) {
        statusBar()->showMessage("No saved session found", 3000);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isObject()) {
        statusBar()->showMessage("Invalid session data", 3000);
        return;
    }

    while (m_tabs.size() > 1) {
        int idx = m_tabWidget->currentIndex();
        removeTab(idx);
    }

    if (m_tabs.size() == 1) {
        int idx = 0;
        disconnectTabEditor(idx);
        m_tabs[0].editor->clear();
        m_tabs[0].filePath.clear();
        m_tabs[0].dirty = false;
    }

    restoreSession(doc.object());
    statusBar()->showMessage("Session loaded", 2000);
}

void MainWindow::updateTabBarVisibility()
{
    m_tabWidget->tabBar()->setVisible(m_tabs.size() > 1);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s;

    bool autoSave = s.value(Preferences::AutoSaveOnExit, false).toBool();
    bool hasUntitledDirty = false;

    for (const TabInfo &info : m_tabs) {
        if (info.dirty) {
            if (info.filePath.isEmpty()) {
                hasUntitledDirty = true;
            }
        }
    }

    if (hasUntitledDirty) {
        auto ret = QMessageBox::question(this, "Unsaved Changes",
            "There are unsaved changes in untitled tabs.\n"
            "Save all before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (ret == QMessageBox::Cancel) {
            event->ignore();
            return;
        }

        if (ret == QMessageBox::Save) {
            for (TabInfo &info : m_tabs) {
                if (info.dirty) {
                    if (info.filePath.isEmpty()) {
            QString file = QFileDialog::getSaveFileName(this, "Save File", QString(), kMdFilter);
                        if (file.isEmpty()) continue;
                        info.filePath = file;
                    }
                    QFile file(info.filePath);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        file.write(info.editor->toPlainText().toUtf8());
                        info.dirty = false;
                    }
                }
            }
        }
    } else if (autoSave) {
        for (TabInfo &info : m_tabs) {
            if (info.dirty && !info.filePath.isEmpty()) {
                QFile file(info.filePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.write(info.editor->toPlainText().toUtf8());
                    info.dirty = false;
                }
            }
        }
    }

    QJsonObject session = serializeSession();

    if (session["files"].toArray().isEmpty()) {
        s.remove(Preferences::SessionData);
    } else {
        s.setValue(Preferences::SessionData, QString::fromUtf8(QJsonDocument(session).toJson(QJsonDocument::Compact)));
    }

    QMainWindow::closeEvent(event);
}

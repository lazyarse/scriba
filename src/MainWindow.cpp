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
#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "Corpus.h"
#include "CorpusIndex.h"
#include "CorpusWatcher.h"
#include "LinkFixer.h"
#include "PreviewBridge.h"
#include "MarkdownParser.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "CssUtils.h"
#include "PrintOptions.h"
#include "PreviewPagination.h"
#include "PreferencesDialog.h"
#include "FindDialog.h"
#include "ExportPdfDialog.h"
#include "ExportHtmlDialog.h"
#include "ExportDocxDialog.h"
#include "DocxExporter.h"
#include "Preferences.h"
#include "StaticHelpers.h"
#include "Readability.h"
#include "JsSnippets.h"
#include "JsRenderEngine.h"
#include "TableDialog.h"
#include "ChartDialog.h"
#include "StockChartDialog.h"
#include "AdvancedChartDialog.h"
#include "EmojiDialog.h"
#include "AboutDialog.h"
#include "LogWindow.h"
#include "MermaidDialog.h"
#include "SpellCheckDialog.h"
#include "SpellChecker.h"
#include "StoppardEngine.h"
#include "KatexHelperDialog.h"
#include "MchemHelperDialog.h"
#include "ChartSource.h"
#include "HtmlToMarkdown.h"
#include "DocxImporter.h"
#include "PdfImporter.h"
#include "ValidationReportDialog.h"

#include <QAtomicInteger>
#include <QWebChannel>

static constexpr const char *kMdFilter = "Markdown Files (*.md);;All Files (*)";
static constexpr const char *kOpenMdFilter = "Markdown Files (*.md *.markdown *.txt);;All Files (*)";
static constexpr int kMsPerMinute = 60000;

namespace {

// Runs the whole-document grammar pass for the Validation Report on a
// background thread. GrammarChecker (StoppardEngine) is stateless and
// thread-safe, so an instance created here is safe to call from run(). Plain
// QThread subclass (no new signals), so no moc is needed.
class ValidationReportThread : public QThread
{
public:
    ValidationReportThread(GrammarChecker *checker)
        : m_checker(checker)
    {
    }

    ~ValidationReportThread() override { delete m_checker; }

    // Set before start(); read after finished().
    QVector<ValidationReport::DocumentSource> sources;
    QVector<QList<GrammarChecker::Issue>> results;

protected:
    void run() override
    {
        results.reserve(sources.size());
        for (const auto &source : sources) {
            results.append(m_checker ? m_checker->check(source.text)
                                     : QList<GrammarChecker::Issue>());
        }
    }

private:
    GrammarChecker *m_checker = nullptr;
};

} // namespace



#include <QVBoxLayout>
#include <algorithm>
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
#include <QClipboard>
#include <QMimeData>
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
#include <QThread>
#include <QDateTime>
#include <memory>

bool MainWindow::s_notifyStaleCss = false;

MainWindow::MainWindow(QWidget *parent, bool skipCorpusRestore)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser())
    , m_cssConfig(new CssConfig())
    , m_cssLoader(new CssLoader(m_cssConfig))
    , m_cssWatcher(new QFileSystemWatcher(this))
{
    m_previewState = QSettings().value(Preferences::PreviewState, 1).toInt();
    if (m_previewState < 0 || m_previewState > 3) m_previewState = 1;
    m_printLayoutMode = QSettings().value(Preferences::PreviewShowPageBreaks, false).toBool();

    setupUi();
    setupMenuBar();

    m_corpusWatcher = new CorpusWatcher(this);
    connect(m_corpusWatcher, &CorpusWatcher::edited, this,
            &MainWindow::handleExternalEdit);
    connect(m_corpusWatcher, &CorpusWatcher::renamed, this,
            &MainWindow::handleExternalRename);
    connect(m_corpusWatcher, &CorpusWatcher::deleted, this,
            &MainWindow::handleExternalDelete);

    QFontDatabase::addApplicationFont(":/fonts/Symbola.ttf");

    connect(m_preview->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
            m_previewInitialized = true;
            syncPreviewScroll();
        } else {
            m_preview->showRenderError(QStringLiteral("Preview failed to load."));
        }
    });

    // The preview page routes link clicks to C++ via a QWebChannel bridge
    // (see PreviewBridge) instead of encoding URLs in the page fragment and
    // watching urlChanged. The old approach was racy: the fragment-clearing
    // replaceState could be lost against the deferred setHtml load, leaving a
    // stale #scriba-open: fragment so the next identical click became a no-op
    // replaceState and was silently dropped.
    m_webChannel = new QWebChannel(this);
    m_previewBridge = new PreviewBridge(this);
    m_webChannel->registerObject(QStringLiteral("scriba"), m_previewBridge);
    m_preview->page()->setWebChannel(m_webChannel);

    connect(m_previewBridge, &PreviewBridge::linkRequested, this,
        [this](const QString &href) {
            QUrl target(href);
            const QString anchor = target.fragment(QUrl::FullyDecoded);
            if (target.isLocalFile()) {
                QString localPath = target.toLocalFile();
                QFileInfo fi(localPath);
                if (fi.suffix().compare("md", Qt::CaseInsensitive) == 0) {
                    loadFile(localPath);
                    // `other.md#section`: jump to the heading once its ids
                    // exist (a fresh page load replaces the document, so the
                    // retry must live in C++, not in a JS closure).
                    if (!anchor.isEmpty())
                        scrollPreviewToAnchor(anchor);
                } else if (isSafePreviewImage(localPath)) {
                    // Local image link: show it in a preview overlay instead
                    // of handing it to the system viewer.
                    m_preview->page()->runJavaScript(
                        QStringLiteral("scribaShowImage('%1')").arg(escapeJsString(target.toString())));
                } else {
                    QDesktopServices::openUrl(target);
                }
            } else {
                QDesktopServices::openUrl(target);
            }
        });
    connect(m_previewBridge, &PreviewBridge::chartEditRequested, this,
        [this](const QString &kind, int line, int index, const QString &tex) {
            handleChartEdit(kind, line, index, tex);
        });

    refreshPreviewCss();

    setWindowTitle("Scriba");

    m_updateTimer = new DebounceTimer(QSettings().value(Preferences::PreviewUpdateDelay,
        Preferences::DefaultPreviewUpdateDelay).toInt(), this);
    connect(m_updateTimer, &QTimer::timeout, this,
        static_cast<void (MainWindow::*)()>(&MainWindow::updatePreview));

    m_anchorTimer = new QTimer(this);
    m_anchorTimer->setSingleShot(true);
    m_anchorTimer->setInterval(Debounce::AnchorScroll);
    connect(m_anchorTimer, &QTimer::timeout, this, &MainWindow::tryScrollPreviewToAnchor);

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
        applyCodeLangSetting();
    } else if (m_cssConfig->activeStylesheet().isEmpty()) {
        m_cssConfig->setActiveStylesheet(":/themes/github-light.css");
        m_cssLoader->invalidateCache();
        refreshPreviewCss();
    }

    // Warm up both base stylesheets so any stale config-dir copies are
    // superseded in one pass, then (in the real app) inform the user once.
    m_cssLoader->previewBaseCss();
    m_cssLoader->printBaseCss();
    if (s_notifyStaleCss && !m_cssLoader->staleBaseCssFiles().isEmpty())
        QTimer::singleShot(0, this, &MainWindow::notifyStaleBaseCss);

    QSettings settings;
    int asInterval = settings.value(Preferences::AutoSaveInterval, 0).toInt();
    if (asInterval > 0)
        m_autoSaveTimer->start(asInterval * kMsPerMinute);

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0 && index < m_editorStack->count())
            m_editorStack->setCurrentIndex(index);
        onTabChanged(index);
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);

    addTab();

    applyStyleSheetToAllEditors();
    QSettings s;
    applyEditorLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt());

    if (!skipCorpusRestore) {
        bool reopen = settings.value(Preferences::ReopenLastCorpus, true).toBool();
        if (reopen) {
            const QString last = settings.value(Preferences::LastCorpusPath).toString();
            if (!last.isEmpty() && QFileInfo::exists(last)) {
                openCorpusFile(last, /*skipPrompt=*/true);
            } else {
                QString raw = settings.value(Preferences::OnExitCorpusData).toString();
                if (!raw.isEmpty()) {
                    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
                    if (doc.isObject())
                        restoreCorpus(doc.object());
                }
            }
        }
    }

    if (m_tabs.isEmpty())
        addTab();

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

void MainWindow::notifyStaleBaseCss()
{
    QStringList stale = m_cssLoader->staleBaseCssFiles();
    if (stale.isEmpty())
        return;

    QStringList backups;
    for (const QString &path : stale)
        backups << path + ".bak";

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle("Stylesheets Updated");
    msgBox.setText("Outdated custom stylesheets were found in your configuration "
                   "and have been replaced with the built-in versions.");
    msgBox.setInformativeText("Your old files were kept as backups:\n" + backups.join('\n')
        + "\n\nIf you customised the preview or print styles in an older version "
          "of Scriba, you can re-apply them from these backups. Note that older "
          "stylesheets may not be fully compatible with the current rendering engine.");
    msgBox.setStandardButtons(QMessageBox::Ok);
    for (auto *btn : msgBox.buttons())
        btn->setIcon(QIcon());
    msgBox.exec();

    m_cssLoader->clearStaleBaseCssFlags();
}

void MainWindow::setupUi()
{
    m_tabBar = new QTabBar();
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setDrawBase(true);
    m_tabBar->setElideMode(Qt::ElideRight);

    m_editorStack = new QStackedWidget();

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);

    m_preview = new Preview(this);

    if (m_previewState == 0) {
        m_splitter->addWidget(m_editorStack);
        m_preview->setVisible(false);
    } else if (m_previewState == 3) {
        m_splitter->addWidget(m_preview);
        m_editorStack->setVisible(false);
    } else if (m_previewState == 1) {
        m_splitter->addWidget(m_editorStack);
        m_splitter->addWidget(m_preview);
    } else {
        m_splitter->addWidget(m_preview);
        m_splitter->addWidget(m_editorStack);
    }

    m_splitter->setSizes({600, 600});
    m_splitter->setStretchFactor(0, 1);
    if (m_previewState != 0 && m_previewState != 3) {
        m_splitter->setStretchFactor(1, 1);
        m_splitter->handle(1)->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_tabBar);
    centralLayout->addWidget(m_splitter, 1);
    setCentralWidget(central);

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
        m_tabBar->setCurrentIndex(existing);
        return existing;
    }

    auto *editor = new Editor();
    editor->setInsertActions(m_insertActions);
    editor->setMermaidAction(m_mermaidAction);
    editor->setCurrentFile(filePath);

    QString label = filePath.isEmpty() ? QStringLiteral("Untitled")
                                       : QFileInfo(filePath).fileName();

    // Populate m_tabs BEFORE adding the tab so currentChanged handlers
    // (which fire synchronously for the first tab) can find the TabInfo.
    int idx = m_tabs.size();
    m_tabs.append({editor, filePath, false});
    m_editorStack->addWidget(editor);
    m_tabBar->addTab(label);
    m_tabBar->setCurrentIndex(idx);
    m_tabBar->setTabToolTip(idx, filePath.isEmpty() ? QString() : filePath);
    // Stable per-tab identity so onTabMoved() can rebuild the parallel
    // containers (m_tabs, m_editorStack, m_reportTitles) to match the tab
    // bar's order after the user drags a tab. The Editor is deleted with its
    // tab (removeTab), so we never look this pointer up after removal.
    m_tabBar->setTabData(idx, QVariant::fromValue(reinterpret_cast<qulonglong>(editor)));

    connect(editor->document(), &QTextDocument::contentsChange, this,
        [this, editor](int, int charsRemoved, int charsAdded) {
            if (charsRemoved == 0 && charsAdded == 0)
                return; // format-only change (e.g. spell/syntax highlighting)
            for (int i = 0; i < m_tabs.size(); ++i) {
                if (m_tabs[i].editor == editor && !m_tabs[i].dirty) {
                    setTabDirty(i, true);
                    break;
                }
            }
        });

    // The gutter pencil opens the chart's helper dialog directly (the editor
    // knows the exact fence block, so no preview body-matching is needed).
    connect(editor, &Editor::chartEditRequested, this,
        [this, editor](int blockNumber) { editChartBlock(editor, blockNumber); });

    if (!m_cachedFullCss.isEmpty()) {
        editor->setStyleSheet(m_cachedFullCss + applyEditorSettings());
        editor->update();
    }
    editor->setCursorWidth(QSettings().value(Preferences::EditorCaretWidth,
                                              Preferences::DefaultEditorCaretWidth).toInt());
    {
        QSettings s;
        QTextBlockFormat fmt;
        fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                          QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(editor->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(fmt);
    }
    m_tabs[idx].dirty = false;
    updateTabLabel(idx);

    editor->updateGutterSettings();

    updateTabBarVisibility();
    editor->setFocus();
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
    m_editorStack->removeWidget(editor);
    m_tabBar->removeTab(index);

    // The spelling panel must never point at a deleted editor. Removing the
    // current tab re-targets it via currentChanged → connectTabEditor; any
    // other removal that deletes the panel's editor needs an explicit rebind.
    // This runs after removeTab(), so currentEditor() is valid.
    if (m_spellCheckDlg && m_spellCheckDlg->targetEditor() == editor) {
        if (Editor *next = currentEditor())
            m_spellCheckDlg->retarget(next);
    }
    delete editor;

    // Removing a tab renumbers every following tab: shift report-tab titles
    // down so updateTabLabel() still finds them.
    m_reportTitles.remove(index);
    QHash<int, QString> shifted;
    for (auto it = m_reportTitles.constBegin(); it != m_reportTitles.constEnd(); ++it) {
        const int key = it.key();
        shifted.insert(key > index ? key - 1 : key, it.value());
    }
    m_reportTitles = shifted;
    m_tocTabs.remove(index);
    QHash<int, QString> tocShifted;
    for (auto it = m_tocTabs.constBegin(); it != m_tocTabs.constEnd(); ++it) {
        const int key = it.key();
        tocShifted.insert(key > index ? key - 1 : key, it.value());
    }
    m_tocTabs = tocShifted;

    updateTabBarVisibility();
}

void MainWindow::onTabMoved(int from, int to)
{
    Q_UNUSED(from);
    Q_UNUSED(to);
    if (m_tabs.isEmpty())
        return;

    // The tab bar reorders itself when the user drags a tab, but the parallel
    // containers (m_tabs, m_editorStack, m_reportTitles) stay in their old
    // order. Rebuild them all from the tab bar's authoritative order using the
    // Editor* identity stamped in each tab's tabData. Without this, index-keyed
    // lookups (activeTabInfo()/currentEditor() -> m_tabs[tabBar->currentIndex()])
    // return the wrong tab after a drag, so the preview would show another
    // file's cached render. Called on every tabMoved during a drag; idempotent
    // because identity is the stable Editor*.

    QHash<Editor *, QString> oldReportTitles;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_reportTitles.contains(i))
            oldReportTitles.insert(m_tabs[i].editor, m_reportTitles.value(i));
    }
    QHash<Editor *, QString> oldTocTitles;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tocTabs.contains(i))
            oldTocTitles.insert(m_tabs[i].editor, m_tocTabs.value(i));
    }

    QVector<TabInfo> reordered;
    reordered.reserve(m_tabs.size());
    QVector<QWidget *> stackOrder;
    stackOrder.reserve(m_tabs.size());
    for (int i = 0; i < m_tabBar->count(); ++i) {
        auto *ed = reinterpret_cast<Editor *>(
            m_tabBar->tabData(i).toULongLong());
        if (!ed)
            continue;
        for (int j = 0; j < m_tabs.size(); ++j) {
            if (m_tabs[j].editor == ed) {
                reordered.append(m_tabs[j]);
                stackOrder.append(ed);
                break;
            }
        }
    }
    if (reordered.size() != m_tabs.size())
        return;

    m_tabs = reordered;

    const int active = m_tabBar->currentIndex();
    while (m_editorStack->count() > 0)
        m_editorStack->removeWidget(m_editorStack->widget(0));
    for (QWidget *w : stackOrder)
        m_editorStack->addWidget(w);
    if (active >= 0 && active < m_editorStack->count())
        m_editorStack->setCurrentIndex(active);

    m_reportTitles.clear();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (auto it = oldReportTitles.constFind(m_tabs[i].editor);
            it != oldReportTitles.constEnd()) {
            m_reportTitles.insert(i, it.value());
        }
    }

    m_tocTabs.clear();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (auto it = oldTocTitles.constFind(m_tabs[i].editor);
            it != oldTocTitles.constEnd()) {
            m_tocTabs.insert(i, it.value());
        }
    }

    m_connectedTabIndex = -1;
    connectActiveEditor();
    for (int i = 0; i < m_tabs.size(); ++i)
        updateTabLabel(i);
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

    if (m_updateTimer) {
        connect(editor, &QTextEdit::textChanged, m_updateTimer, qOverload<>(&QTimer::start));
        connect(editor, &QTextEdit::textChanged, this, [this, index]() {
            // The cached md->html render is stale as soon as the editor changes
            if (index >= 0 && index < m_tabs.size())
                m_tabs[index].previewHtmlValid = false;
        });
    }

    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::onEditorScroll);

    // The modeless spelling panel follows the active tab.
    if (m_spellCheckDlg && m_spellCheckDlg->isVisible())
        m_spellCheckDlg->retarget(editor);
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
    int idx = m_tabBar->currentIndex();
    connectTabEditor(idx);
}

Editor *MainWindow::currentEditor() const
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size())
        return nullptr;
    return m_tabs[idx].editor;
}

TabInfo *MainWindow::activeTabInfo()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size())
        return nullptr;
    return &m_tabs[idx];
}

void MainWindow::updateTabLabel(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    const TabInfo &info = m_tabs[index];
    QString name;
    if (m_reportTitles.contains(index))
        name = m_reportTitles.value(index);
    else if (m_tocTabs.contains(index))
        name = m_tocTabs.value(index);
    else
        name = info.filePath.isEmpty() ? QStringLiteral("Untitled")
                                       : QFileInfo(info.filePath).fileName();
    if (info.dirty)
        name += QStringLiteral(" *");
    if (index == m_tabBar->currentIndex())
        updateWindowTitle();
    m_tabBar->setTabText(index, name);
}

void MainWindow::updateWindowTitle()
{
    if (!m_corpus.filePath.isEmpty()) {
        setWindowTitle(QFileInfo(m_corpus.filePath).completeBaseName()
                       + QStringLiteral(" — Scriba"));
        return;
    }
    TabInfo *info = activeTabInfo();
    if (!info) {
        setWindowTitle(QStringLiteral("Scriba"));
        return;
    }
    QString title = info->filePath.isEmpty()
        ? QStringLiteral("Scriba - Untitled")
        : QStringLiteral("Scriba - ") + info->filePath;
    if (info->dirty)
        title += QStringLiteral(" *");
    setWindowTitle(title);
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
    if (auto *ed = currentEditor())
        ed->setFocus();

    TabInfo *info = activeTabInfo();
    if (info) {
        updateWindowTitle();
        m_preview->setDocumentPath(info->filePath);
        if (m_previewInitialized) {
            // The preview page stays alive across tab switches: push the new
            // tab's cached render through the incremental scribaUpdate() path
            // instead of reloading the whole page. Pre-scroll to the new
            // editor's position so scribaUpdate captures the right percentage
            // (it restores that after the heavy render pass).
            QSettings settings;
            if (settings.value(Preferences::SyncScroll, true).toBool()) {
                if (auto *ed = currentEditor()) {
                    auto *sb = ed->verticalScrollBar();
                    double range = sb->maximum() - sb->minimum();
                    double pct = range > 0
                        ? static_cast<double>(sb->value() - sb->minimum()) / range : 0.0;
                    m_preview->scrollToPercent(pct);
                }
            }
            updatePreview(true);
        } else {
            updatePreview();
        }

        applyEditorContentWidth(info->editor);
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    if (m_tabs.size() <= 1) {
        showSaveDiscardDialog(index);
        if (m_tabs.size() == 1 && !m_tabs[0].dirty) {
            m_tabs[0].filePath.clear();
            m_tabs[0].editor->clear();
            m_tabs[0].previewHtmlValid = false;
            m_tabs[0].dirty = false;
            updateTabLabel(0);
            updateWindowTitle();
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
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
    auto *discardBtn = msgBox.addButton(tr("&Discard"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);

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
    } else if (msgBox.clickedButton() == discardBtn) {
        removeTab(index);
    }
}

void MainWindow::closeCurrentTab()
{
    int idx = m_tabBar->currentIndex();
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

    QMenu *corpusMenu = fileMenu->addMenu(tr("C&orpus"));
    QAction *saveCorpusAct = corpusMenu->addAction(tr("&Save Corpus"));
    connect(saveCorpusAct, &QAction::triggered, this, &MainWindow::saveCorpusAction);
    QAction *saveCorpusAsAct = corpusMenu->addAction(tr("Save Corpus &As…"));
    connect(saveCorpusAsAct, &QAction::triggered, this, &MainWindow::saveCorpusAsAction);
    QAction *openCorpusAct = corpusMenu->addAction(tr("&Open Corpus…"));
    connect(openCorpusAct, &QAction::triggered, this, &MainWindow::openCorpusAction);
    QAction *tocAction = corpusMenu->addAction(tr("&View Table of Contents"));
    tocAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(tocAction, &QAction::triggered, this, &MainWindow::viewTableOfContents);
    corpusMenu->addSeparator();
    m_recentCorpusMenu = corpusMenu->addMenu(tr("Recent C&orpus"));
    updateRecentCorporaMenu();

    fileMenu->addSeparator();

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
            loadFile(info->filePath, true);
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

    QAction *renameAction = fileMenu->addAction("Re&name...");
    connect(renameAction, &QAction::triggered, this, &MainWindow::renameCurrentFile);

    fileMenu->addSeparator();

    QAction *closeTabAction = fileMenu->addAction("&Close Tab");
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    fileMenu->addSeparator();

    QMenu *importMenu = fileMenu->addMenu("&Import");

    QAction *importPdfAction = importMenu->addAction("Import &PDF...");
    importPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    connect(importPdfAction, &QAction::triggered, this, &MainWindow::importPdfFromFile);

    QAction *importDocxAction = importMenu->addAction("Import &Word (DOCX)...");
    importDocxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(importDocxAction, &QAction::triggered, this, &MainWindow::importDocxFromFile);

    QAction *importHtmlAction = importMenu->addAction("Import &HTML...");
    importHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(importHtmlAction, &QAction::triggered, this, &MainWindow::importHtmlFromFile);

    QMenu *exportMenu = fileMenu->addMenu("&Export");

    QAction *exportPdfAction = exportMenu->addAction("&Print / Export PDF...");
    exportPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::exportPdf);

    QAction *exportDocxAction = exportMenu->addAction("Export as &Word (DOCX)...");
    exportDocxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(exportDocxAction, &QAction::triggered, this, &MainWindow::exportDocx);

    QAction *exportHtmlAction = exportMenu->addAction("Export as &HTML...");
    exportHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    connect(exportHtmlAction, &QAction::triggered, this, &MainWindow::exportHtml);

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

    editMenu->addSeparator();

    QAction *pasteMarkdownAction = editMenu->addAction("Paste as &Markdown");
    pasteMarkdownAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(pasteMarkdownAction, &QAction::triggered, this, &MainWindow::pasteAsMarkdown);

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
        int count = m_tabBar->count();
        if (count > 1)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % count);
    });

    QAction *prevTabAction = viewMenu->addAction("&Previous Tab");
    prevTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTabAction, &QAction::triggered, this, [this]() {
        int count = m_tabBar->count();
        if (count > 1)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() - 1 + count) % count);
    });

    viewMenu->addSeparator();

    QAction *lineNumbersAction = viewMenu->addAction("Show &Line Numbers");
    lineNumbersAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    lineNumbersAction->setCheckable(true);
    lineNumbersAction->setChecked(QSettings().value(Preferences::ShowLineNumbers, true).toBool());
    connect(lineNumbersAction, &QAction::toggled, this, [this](bool checked) {
        QSettings s;
        s.setValue(Preferences::ShowLineNumbers, checked);
        s.sync();
        for (const auto &tab : m_tabs)
            if (tab.editor)
                tab.editor->updateGutterSettings();
    });

    m_wrapTextAction = viewMenu->addAction("Wrap &Text");
    m_wrapTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W));
    m_wrapTextAction->setCheckable(true);
    m_wrapTextAction->setChecked(QSettings().value(Preferences::EditorWrapEnabled, true).toBool());
    connect(m_wrapTextAction, &QAction::toggled, this, [this](bool checked) {
        QSettings s;
        s.setValue(Preferences::EditorWrapEnabled, checked);
        s.sync();
        for (const auto &tab : m_tabs)
            if (tab.editor)
                tab.editor->applyLineWrap();
    });

    viewMenu->addSeparator();

    m_layoutActions = new QActionGroup(this);
    m_layoutActions->setExclusive(true);

    const auto addLayoutAction = [this, viewMenu](const QString &text, int state) {
        QAction *a = viewMenu->addAction(text);
        a->setCheckable(true);
        a->setData(state);
        m_layoutActions->addAction(a);
        connect(a, &QAction::triggered, this, [this, state]() {
            setPreviewState(state);
        });
    };

    addLayoutAction("Editor | Pre&view", 1);
    addLayoutAction("Preview | Ed&itor", 2);
    addLayoutAction("&Editor", 0);
    addLayoutAction("&Preview", 3);
    syncPreviewLayout();

    viewMenu->addSeparator();

    QAction *fullScreenAction = viewMenu->addAction("&Full Screen");
    fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, this, [this](bool checked) {
        if (checked) {
            showFullScreen();
        } else {
            showNormal();
        }
    });

    m_showPageBreaksAction = viewMenu->addAction("Show Page &Breaks");
    m_showPageBreaksAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    m_showPageBreaksAction->setCheckable(true);
    m_showPageBreaksAction->setChecked(m_printLayoutMode);
    connect(m_showPageBreaksAction, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(Preferences::PreviewShowPageBreaks, on);
        m_printLayoutMode = on;
        if (!on)
            m_printLayoutFp.clear();
        // Force the full rebuild path so the page picks up the print/classic
        // base CSS, theme, page box and paginator (or the absence of them).
        m_previewInitialized = false;
        updatePreview();
    });

    for (int i = 1; i <= 10; ++i) {
        Qt::Key key = (i == 10) ? Qt::Key_0
                                : static_cast<Qt::Key>(Qt::Key_0 + i);
        QAction *tabAction = new QAction(QString("Switch to Tab %1").arg(i), this);
        tabAction->setShortcut(QKeySequence(Qt::ALT | key));
        connect(tabAction, &QAction::triggered, this, [this, i]() {
            if (i - 1 < m_tabBar->count())
                m_tabBar->setCurrentIndex(i - 1);
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

    toolsMenu->addSeparator();

    QAction *katexAction = toolsMenu->addAction("&KaTeX Equation...");
    katexAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(katexAction, &QAction::triggered, this, &MainWindow::showKatexHelper);

    QAction *mchemAction = toolsMenu->addAction("Chemistry &Notation...");
    mchemAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    connect(mchemAction, &QAction::triggered, this, &MainWindow::showMchemHelper);

    toolsMenu->addSeparator();

    QAction *chartAction = toolsMenu->addAction("&Chart Builder...");
    chartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(chartAction, &QAction::triggered, this, &MainWindow::showChartBuilder);

    QAction *stockAction = toolsMenu->addAction("&Stock Chart...");
    stockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(stockAction, &QAction::triggered, this, &MainWindow::showStockChartBuilder);

    QAction *advancedAction = toolsMenu->addAction("&Advanced Charts...");
    connect(advancedAction, &QAction::triggered, this, &MainWindow::showAdvancedChartBuilder);

    QAction *mermaidAction = toolsMenu->addAction("&Mermaid Diagrams...");
    mermaidAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(mermaidAction, &QAction::triggered, this, [this]() {
        MermaidDialog dlg(m_cssLoader->themeCss(), this);
        if (dlg.exec() == QDialog::Accepted) {
            QString block = dlg.mermaidBlock();
            Editor *ed = editor();
            if (!block.isEmpty() && ed)
                ed->insertPlainText(block);
        }
    });

    toolsMenu->addSeparator();

    QAction *spellCheckAction = toolsMenu->addAction("&Check Spelling...");
    spellCheckAction->setShortcut(QKeySequence(Qt::Key_F7));
    connect(spellCheckAction, &QAction::triggered, this, &MainWindow::showSpellCheckDialog);

    QAction *reportAction = toolsMenu->addAction("&Validation Report...");
    reportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F7));
    connect(reportAction, &QAction::triggered, this, &MainWindow::generateValidationReport);

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

    m_insertActions = {tableAction, emojiAction, katexAction, mchemAction, chartAction};
    m_mermaidAction = mermaidAction;

    for (TabInfo &info : m_tabs) {
        if (info.editor) {
            info.editor->setInsertActions(m_insertActions);
            info.editor->setMermaidAction(m_mermaidAction);
        }
    }
}

void MainWindow::refreshPreviewCss()
{
    QSettings settings;
    QString rawThemeCss = m_cssLoader->themeCss();
    if (m_printLayoutMode) {
        // Print layout strips the theme and swaps in the print base CSS; the
        // page is rebuilt from scratch when it is toggled off, so the classic
        // CSS never needs live-patching here.
        m_preview->setThemeBackgroundColor(CssUtils::themeColors(rawThemeCss).background);
        return;
    }
    int uiFontSize = settings.value(Preferences::UiFontSize, Preferences::DefaultUiFontSize).toInt();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss, uiFontSize);
    QString previewCss = chromeCss + rawThemeCss;
    QString previewBaseCss = m_cssLoader->previewBaseCss();

    bool needPreviewUpdate = (previewCss != m_cachedPreviewCss);
    bool needChromeUpdate = (chromeCss != m_cachedFullCss);
    bool needBaseUpdate = (previewBaseCss != m_cachedPreviewBaseCss);
    QString overlayCss = CssUtils::renderOverlayCss(rawThemeCss);
    bool needOverlayUpdate = (overlayCss != m_cachedOverlayCss);

    m_preview->setThemeBackgroundColor(CssUtils::themeColors(rawThemeCss).background);

    if (!needPreviewUpdate && !needChromeUpdate && !needBaseUpdate && !needOverlayUpdate)
        return;

    if (needPreviewUpdate) {
        m_cachedPreviewCss = previewCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('theme-css').textContent = '%1';")
                .arg(escapeJsString(previewCss));
            m_preview->page()->runJavaScript(js);
        }
    }

    if (needOverlayUpdate) {
        m_cachedOverlayCss = overlayCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('render-css').textContent = '%1';")
                .arg(escapeJsString(overlayCss));
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
        applyStyleSheetToAllEditors();
        QSettings s;
        applyEditorLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt());
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
    int size = settings.value(Preferences::EditorFontSize, Preferences::DefaultEditorFontSize).toInt();
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
    QTextBlockFormat fmt;
    fmt.setLineHeight(lineHeight, QTextBlockFormat::ProportionalHeight);
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!m_tabs[i].editor) continue;
        QSignalBlocker blocker(m_tabs[i].editor->document());
        QTextCursor cursor(m_tabs[i].editor->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(fmt);
        m_tabs[i].editor->refreshGutter();
    }
}

void MainWindow::applyEditorCaretWidth(int caretWidth)
{
    for (const auto &tab : m_tabs) {
        if (tab.editor)
            tab.editor->setCursorWidth(caretWidth);
    }
}

void MainWindow::applyStyleSheetToAllEditors()
{
    QString css = m_cachedFullCss + applyEditorSettings();
    for (const auto &tab : m_tabs) {
        if (tab.editor) {
            tab.editor->setStyleSheet(css);
            tab.editor->update();
        }
    }
}

void MainWindow::applyStyleSheetToAllEditors(const QString &fontFamily, int fontSize, int padding)
{
    QString css = m_cachedFullCss + applyEditorSettings(fontFamily, fontSize, padding);
    for (const auto &tab : m_tabs) {
        if (tab.editor) {
            tab.editor->setStyleSheet(css);
            tab.editor->update();
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

void MainWindow::applyCodeLangSetting()
{
    if (!m_previewInitialized)
        return;
    QSettings settings;
    bool enabled = settings.value(Preferences::ShowCodeLangPreview, true).toBool();
    QString css = enabled ? QString()
        : QLatin1String(Preferences::HideCodeLangCss);
    QString js = QString(
        "var e=document.getElementById('code-lang-css');"
        "if(e)e.textContent='%1';"
    ).arg(escapeJsString(css));
    m_preview->page()->runJavaScript(js);
}

void MainWindow::applyEditorContentWidth(Editor *editor)
{
    if (!editor)
        return;
    editor->applyLineWrap();
    QSettings settings;
    if (m_previewState == 0) {
        bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
        int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();
        editor->setCenterContent(centre, centreWidth);
    } else if (m_previewState == 1 || m_previewState == 2) {
        int splitWidth = settings.value(Preferences::SplitViewEditorMaxWidth, 0).toInt();
        editor->setCenterContent(splitWidth > 0, splitWidth);
    } else {
        editor->setCenterContent(false, 0);
    }
}

void MainWindow::applyPreviewSplitWidth()
{
    if (m_printLayoutMode) {
        // In print layout the page geometry (center-css) owns the preview
        // width; a split max-width would fight the page box.
        return;
    }
    if (!m_previewInitialized)
        return;
    QString css;
    if (m_previewState == 1 || m_previewState == 2) {
        QSettings settings;
        int splitWidth = settings.value(Preferences::SplitViewPreviewMaxWidth, 0).toInt();
        css = CssUtils::splitViewMaxWidthCss(splitWidth);
    }
    QString js = QString(
        "var e=document.getElementById('split-css');"
        "if(e)e.textContent='%1';"
    ).arg(escapeJsString(css));
    m_preview->page()->runJavaScript(js);
}

void MainWindow::updatePreview()
{
    updatePreview(false);
}

void MainWindow::updatePreview(bool tabSwitch)
{
    Editor *ed = currentEditor();
    if (!ed) return;

    QSettings prefs;
    updateStats();
    bool blockRawHtml = prefs.value(Preferences::BlockRawHtmlPreview, true).toBool();
    bool stripScripts = prefs.value(Preferences::StripPreviewScripts, true).toBool();
    TabInfo *info = activeTabInfo();
    QString html;
    if (info && info->previewHtmlValid
        && info->previewBlockRaw == blockRawHtml
        && info->previewStripScripts == stripScripts) {
        html = info->previewHtml;
    } else {
        html = m_parser->toHtml(ed->toPlainText(), blockRawHtml);
        if (stripScripts)
            html = JsRenderEngine::stripScriptTags(html);
        if (info) {
            info->previewHtml = html;
            info->previewBlockRaw = blockRawHtml;
            info->previewStripScripts = stripScripts;
            info->previewHtmlValid = true;
        }
    }

    QString rawThemeCss = m_cssLoader->themeCss();
    QString baseCss = m_cssLoader->previewBaseCss();
    int uiFontSize = prefs.value(Preferences::UiFontSize, Preferences::DefaultUiFontSize).toInt();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss, uiFontSize);
    QString previewCss = chromeCss + rawThemeCss;
    QString mermaidTheme = CssUtils::isDarkTheme(rawThemeCss)
        ? QStringLiteral("dark") : QStringLiteral("default");

    // Print layout: rebuild the preview as an exact page-by-page pagination of
    // the PDF export. Geometry comes from the same merged CSS + print options
    // the export path uses, so the on-screen page boxes match the printout.
    const PrintOptions::Options printOpts = m_printLayoutMode
        ? PrintOptions::fromSettings() : PrintOptions::Options();
    QString printLayoutCss;
    int printContentHpx = 0;
    if (m_printLayoutMode) {
        QString merged = m_cssLoader->printCss()
            + QStringLiteral("\n") + PrintOptions::buildCss(printOpts)
            + QStringLiteral("\n") + PrintOptions::buildPageOverrideCss(printOpts);
        const QSizeF pagePt = PrintOptions::parsePageSize(merged);
        const QMarginsF marginsPt = PrintOptions::parsePageMargins(merged);
        const double px = 96.0 / 72.0;
        const int contentW = qMax(160, qRound((pagePt.width() - marginsPt.left() - marginsPt.right()) * px));
        printContentHpx = qMax(160, qRound((pagePt.height() - marginsPt.top() - marginsPt.bottom()) * px));
        printLayoutCss = PreviewPagination::layoutCss(
            contentW, printContentHpx,
            qRound(marginsPt.top() * px), qRound(marginsPt.left() * px), qRound(marginsPt.bottom() * px));
        if (merged != m_printLayoutFp) {
            // Page geometry/options changed: force a full rebuild so the new
            // page box and paginator take effect.
            m_printLayoutFp = merged;
            m_previewInitialized = false;
        }
        baseCss = m_cssLoader->printCss();
        previewCss = QString();
        mermaidTheme = QStringLiteral("default");
    }

    bool cssChanged = (previewCss != m_cachedPreviewCss);
    if (cssChanged) {
        m_cachedPreviewCss = previewCss;
    }

    QUrl baseUrl;
    QString docPath = info ? info->filePath : QString();
    if (!docPath.isEmpty()) {
        baseUrl = QUrl::fromLocalFile(QFileInfo(docPath).absolutePath() + "/");
    }
    // A Table-of-Contents tab is unbacked, but its relative links resolve
    // against the corpus root; mirror the per-file base for it.
    const int curIdx = m_tabBar->currentIndex();
    if (m_tocTabs.contains(curIdx))
        baseUrl = QUrl::fromLocalFile(m_corpus.rootDir() + "/");

    QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();
    bool cspEnabled = prefs.value(Preferences::EnableCspPreview, true).toBool();
    if (!m_previewInitialized) {
        m_cachedPreviewBaseCss = baseCss;
        int heavyRenderDelay = prefs.value(Preferences::HeavyRenderDelay,
            Preferences::DefaultHeavyRenderDelay).toInt();
        QString renderCss = CssUtils::renderOverlayCss(rawThemeCss);
        m_cachedOverlayCss = renderCss;
        bool striping = prefs.value(Preferences::TableStriping, true).toBool();
        QString stripeInit = striping ? QString()
            : QLatin1String(Preferences::TableStripeCss);
        bool showCodeLang = prefs.value(Preferences::ShowCodeLangPreview, true).toBool();
        QString codeLangInit = showCodeLang ? QString()
            : QLatin1String(Preferences::HideCodeLangCss);
        QString centerCss = printLayoutCss;
        if (!m_printLayoutMode && m_previewState == 3) {
            bool centre = prefs.value(Preferences::CentreSingleViewContent, true).toBool();
            int centreWidth = prefs.value(Preferences::CentreSingleViewWidth, 800).toInt();
            if (centre)
                centerCss = QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth);
        }
        QString splitCss;
        if (!m_printLayoutMode && (m_previewState == 1 || m_previewState == 2)) {
            int splitWidth = prefs.value(Preferences::SplitViewPreviewMaxWidth, 0).toInt();
            splitCss = CssUtils::splitViewMaxWidthCss(splitWidth);
        }
        QString fullHtml = QString(
            "<!DOCTYPE html><html><head>"
            "<meta charset=\"utf-8\">"
            "<style id=\"base-css\">%1</style>"
            "<style id=\"theme-css\">%2</style>"
            "<style id=\"stripe-css\">%4</style>"
            "<style id=\"center-css\">%5</style>"
            "<style id=\"split-css\">%6</style>"
            "<style id=\"code-lang-css\">%7</style>"
            "<style id=\"render-css\">%8</style>"
            "<style id=\"image-overlay-css\">#scriba-image-overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.75);z-index:9999;display:none;align-items:center;justify-content:center}#scriba-image-overlay .scriba-image-box{position:relative;max-width:92%;max-height:92%;display:flex;flex-direction:column;align-items:center}#scriba-image-overlay .scriba-image-view{max-width:92vw;max-height:85vh;object-fit:contain;border-radius:4px;box-shadow:0 4px 24px rgba(0,0,0,.5)}#scriba-image-overlay .scriba-image-caption{color:#eee;font-size:13px;margin-top:8px;max-width:92vw;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}#scriba-image-overlay .scriba-image-close{position:absolute;top:-14px;right:-14px;width:28px;height:28px;border-radius:50%;border:none;background:#333;color:#fff;font-size:16px;line-height:1;cursor:pointer}#scriba-image-overlay .scriba-image-close:hover{background:#555}</style>"
            "<style id=\"edit-btn-css\">.scriba-chart-wrap{position:relative;display:block}.scriba-mathbox{position:relative;white-space:normal}.scriba-math{white-space:normal}.scriba-edit-btn{position:absolute;top:4px;right:4px;z-index:6;display:inline-block;padding:2px 7px;font-size:12px;line-height:1.3;color:#fff;background:rgba(60,60,60,.75);border:1px solid rgba(255,255,255,.35);border-radius:4px;text-decoration:none;cursor:pointer;opacity:0;transition:opacity .15s ease}.scriba-chart-wrap:hover .scriba-edit-btn,.scriba-mathbox:hover .scriba-edit-btn{opacity:1}@media print{.scriba-edit-btn{display:none!important}}</style>"
            "<style>" DEFAULT_EMOJI_FONT "#preview .emoji-char{font-family:'Symbola',monospace}.emoji{height:1em;width:1em;vertical-align:-0.1em;display:inline-block}</style>"
            "<script src=\"qrc:///highlight.min.js\"></script>"
            "<script src=\"qrc:///mermaid.min.js\"></script>"
            "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
            "<script src=\"qrc:///katex.min.js\"></script>"
            "<script src=\"qrc:///contrib/mhchem.min.js\"></script>"
            "<script src=\"qrc:///echarts.min.js\"></script>"
            "<script src=\"qrc:///twemoji.min.js\"></script>"
            "<script src=\"qrc:///emoji.js\"></script>"
            "<script src=\"qrc:///qtwebchannel/qwebchannel.js\"></script>"
            "<script>window._scribaHeavyDelay=" + QString::number(heavyRenderDelay) + ";" + mermaidInitJs + headingIdJs + anchorNavJs + katexInitJs + echartsInitJs + setImgTitlesJs + setFootnoteTitlesJs + imageOverlayJs + chartEditJs + "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}function scribaUpdate(html,themeCss,mermaidTheme,emojiMode,delay,baseUrl){if(!document.body)return false;try{window._scribaGen=(window._scribaGen||0)+1;var gen=window._scribaGen;var sy=window.scrollY;var sh=document.body.scrollHeight;var ih=window.innerHeight;var pct=sh>ih?sy/(sh-ih):0;if(themeCss){var tc=document.getElementById('theme-css');if(tc)tc.textContent=themeCss;}if(baseUrl){var b=document.getElementById('scriba-base');if(!b){b=document.createElement('base');b.id='scriba-base';var hd=document.head;hd.insertBefore(b,hd.firstChild);}b.href=baseUrl;}else{var b2=document.getElementById('scriba-base');if(b2)b2.remove();}window._scribaBasePath=baseUrl?new URL(baseUrl).pathname:location.pathname;var sc=document.getElementById('scriba-content');if(sc)sc.innerHTML=html;else return false;clearTimeout(window._scribaHeavyTimer);window._scribaHeavyTimer=setTimeout(function(){if(gen!==window._scribaGen)return;mermaid.initialize({startOnLoad:false,theme:mermaidTheme});var mp=initMermaid();initKaTeX();var vp=initECharts();hljs.highlightAll();generateHeadingIds();setImgTitles();setFootnoteTitles();replaceEmoji(document.body);twemojiParse(emojiMode);function restoreScroll(){if(Math.abs(window.scrollY-sy)<2){var ih2=window.innerHeight;window.scrollTo(0,pct*Math.max(1,document.body.scrollHeight-ih2));}}var p=[];if(typeof mp!=='undefined')p.push(mp);if(typeof vp!=='undefined')p.push(vp);var imgs=document.querySelectorAll('img:not(.emoji)');if(imgs.length>0){p.push(new Promise(function(r){var n=0,t=imgs.length;function c(){n++;if(n>=t)r();}for(var i=0;i<imgs.length;i++){if(imgs[i].complete)c();else{imgs[i].onload=c;imgs[i].onerror=c;}}}));}if(p.length)Promise.all(p).then(restoreScroll);else restoreScroll();},(typeof delay==='number'&&delay>=0)?delay:window._scribaHeavyDelay);return true;}catch(e){scribaShowRenderError(e&&e.message?e.message:e);scribaEndRender();return false;}}function scribaBeginRender(){var c=document.getElementById('scriba-content');if(c)c.innerHTML='';var o=document.getElementById('scriba-rendering-overlay');if(!o&&document.body){o=document.createElement('div');o.id='scriba-rendering-overlay';o.textContent='Rendering…';document.body.insertBefore(o,document.body.firstChild);}if(o)o.style.display='flex';}function scribaEndRender(){var o=document.getElementById('scriba-rendering-overlay');if(o)o.style.display='none';}function scribaShowRenderError(m){var c=document.getElementById('scriba-content');if(!c&&document.body){c=document.createElement('div');c.id='scriba-content';document.body.appendChild(c);}if(!c)return;m=String(m==null?'Unknown render error':m);var t=document.createElement('div');t.style.cssText='margin:2rem auto;max-width:720px;padding:1.2rem 1.4rem;border:1px solid #d33;border-radius:6px;background:#fdf0f0;color:#8b0000;font-family:system-ui,sans-serif;';t.innerHTML='<strong>Preview error</strong><pre style=\"white-space:pre-wrap;word-break:break-word;font-family:monospace;margin:0.5rem 0 0;color:#6b0000;\">'+String(m).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')+'</pre>';c.innerHTML='';c.appendChild(t);}document.addEventListener('DOMContentLoaded',function(){window._scribaBasePath=location.pathname;mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});hljs.registerAliases('ec',{languageName:'json'});hljs.highlightAll();generateHeadingIds();initKaTeX();setImgTitles();setFootnoteTitles();replaceEmoji(document.body);twemojiParse('" + emojiMode + "');var p=[];var mp=window.mermaidReady=initMermaid();var vp=window.echartsReady=initECharts();if(typeof mp!=='undefined')p.push(mp);if(typeof vp!=='undefined')p.push(vp);var imgs=document.querySelectorAll('img:not(.emoji)');if(imgs.length>0){p.push(new Promise(function(r){var n=0,t=imgs.length;function c(){n++;if(n>=t)r();}for(var i=0;i<imgs.length;i++){if(imgs[i].complete)c();else{imgs[i].onload=c;imgs[i].onerror=c;}}}));}var scribaHideOverlay=function(){scribaEndRender();};if(p.length)Promise.all(p).then(scribaHideOverlay,scribaHideOverlay);else scribaHideOverlay();setTimeout(scribaHideOverlay,10000);});</script>"
            "</head><body id=\"preview\">"
            "<div id=\"scriba-rendering-overlay\">Rendering…</div>"
            "<div id=\"scriba-content\">%3</div>"
            "<script>function scribaBridgeOpen(href){if(window.scribaBridge)window.scribaBridge.openLink(href);else window.scribaPendingOpen=href;}"
            "function scribaBridgeEdit(kind,line,idx,tex){if(window.scribaBridge)window.scribaBridge.editChart(kind,line,idx,tex);else window.scribaPendingEdit={kind:kind,line:line,idx:idx,tex:tex};}"
            "function scribaBridgeFlush(){if(!window.scribaBridge)return;if(window.scribaPendingOpen!==undefined){window.scribaBridge.openLink(window.scribaPendingOpen);window.scribaPendingOpen=undefined;}if(window.scribaPendingEdit){window.scribaBridge.editChart(window.scribaPendingEdit.kind,window.scribaPendingEdit.line,window.scribaPendingEdit.idx,window.scribaPendingEdit.tex);window.scribaPendingEdit=null;}}"
            "function scribaBridgeInit(){"
            "if(window.scribaBridge)return;"
            "if(typeof QWebChannel==='undefined'||!window.qt||!window.qt.webChannelTransport){setTimeout(scribaBridgeInit,50);return;}"
            "new QWebChannel(qt.webChannelTransport,function(ch){window.scribaBridge=ch.objects.scriba;scribaBridgeFlush();});"
            "}"
            "scribaBridgeInit();"
            "document.addEventListener('click',function(e){"
            "var l=e.target.closest('a');if(!l)return;"
            "if(l.hash&&l.hash.indexOf('#scriba-edit:')===0){"
            "e.preventDefault();"
            "var p=l.hash.slice(13).split(':');if(p.length<3)return;"
            "var tex=p.length>3?p.slice(3).join(':'):'';"
            "try{tex=decodeURIComponent(tex);}catch(err){}"
            "scribaBridgeEdit(p[0],parseInt(p[1],10),parseInt(p[2],10),tex);"
            "return;"
            "}"
            "if(l.hash&&l.hash.length>1&&l.pathname===window._scribaBasePath){"
            "e.preventDefault();"
            "scribaScrollToSlugRetry(l.hash);"
            "return;"
            "}"
            "e.preventDefault();"
            "scribaBridgeOpen(l.href)"
            "})</script>"
            "</body></html>"
        ).arg(baseCss, previewCss, html, stripeInit, centerCss, splitCss, codeLangInit, renderCss);
        if (m_printLayoutMode) {
            // Re-paginate after each render pass (see PreviewPagination) and
            // embed the print option overrides + paginator for this geometry.
            fullHtml = PreviewPagination::patchIncrementalPaginate(fullHtml);
            const QString printOptionsCss = PrintOptions::buildCss(printOpts);
            int headEnd = fullHtml.indexOf("</head>");
            if (headEnd >= 0)
                fullHtml.insert(headEnd,
                    QStringLiteral("<style id=\"print-options-css\">%1</style>").arg(printOptionsCss));
            int bodyEnd = fullHtml.indexOf("</body>");
            if (bodyEnd >= 0)
                fullHtml.insert(bodyEnd,
                    PreviewPagination::paginatorScript(printOpts, printContentHpx));
        }
        if (cspEnabled) {
            int headEnd = fullHtml.indexOf("</head>");
            if (headEnd >= 0)
                fullHtml.insert(headEnd, QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">").arg(Security::CspHeader));
        }
        m_preview->setHtmlWithOverlay(fullHtml, baseUrl);
    } else {
        QString escapedHtml = escapeJsString(html);
        QString escapedCss = cssChanged ? escapeJsString(previewCss) : QString();
        int delay = -1;
        if (tabSwitch) {
            int heavyDelay = prefs.value(Preferences::HeavyRenderDelay,
                Preferences::DefaultHeavyRenderDelay).toInt();
            delay = std::min(heavyDelay, Debounce::TabSwitchRender);
        }
        // Re-assert the active document's base URL on EVERY render, not just
        // tab switches. Relative image paths resolve against the shared page's
        // <base> element; with an empty baseUrl scribaUpdate removes it, making
        // images fall back to the stale page URL from the last full load.
        QString escapedBaseUrl;
        if (!baseUrl.isEmpty())
            escapedBaseUrl = escapeJsString(baseUrl.toString());
        QString js = QString("scribaUpdate('%1','%2','%3','%4',%5,'%6')")
            .arg(escapedHtml, escapedCss, mermaidTheme, emojiMode,
                 QString::number(delay), escapedBaseUrl);
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

void MainWindow::scrollPreviewToAnchor(const QString &anchor)
{
    m_pendingAnchor = anchor;
    m_anchorTries = 0;
    m_anchorTimer->start();
}

void MainWindow::tryScrollPreviewToAnchor()
{
    if (m_pendingAnchor.isEmpty())
        return;
    const QString js = QStringLiteral("scribaScrollToSlug('%1')")
        .arg(escapeJsString(m_pendingAnchor));
    m_preview->page()->runJavaScript(js, [this](const QVariant &result) {
        // Give up after ~6s; the ids appear after the heavy render pass, so
        // retries normally succeed on the second or third tick.
        if (result.toBool() || ++m_anchorTries > 20) {
            m_anchorTimer->stop();
            m_pendingAnchor.clear();
        } else {
            m_anchorTimer->start();
        }
    });
}

void MainWindow::showPreferences()
{
    QString oldStylesheet = m_cssConfig->activeStylesheet();
    CssUtils::ThemeColors tc = CssUtils::themeColors(m_cssLoader->themeCss());
    PreferencesDialog dlg(m_cssConfig, m_cssLoader, this,
        tc.background.name(), tc.text.name(), &m_corpus);
    auto updateAll = [this]() {
        syncCssWatcher();
        refreshPreviewCss();
        applyStripeSetting();
        applyCodeLangSetting();
    };
    connect(&dlg, &PreferencesDialog::stylesheetChanged, this, updateAll);
    connect(&dlg, &PreferencesDialog::editorSettingsChanged, this,
        [this](const QString &f, int s, int lh, int p, int cw) {
            applyStyleSheetToAllEditors(f, s, p);
            applyEditorLineHeight(lh);
            applyEditorCaretWidth(cw);
        });
    connect(&dlg, &PreferencesDialog::uiFontSizeChanged, this,
        [this](int) { refreshPreviewCss(); });
    connect(&dlg, &PreferencesDialog::underlineColorsChanged, this,
        [this]() {
            for (const auto &tab : m_tabs)
                if (tab.editor)
                    tab.editor->refreshUnderlines();
        });
    QSettings s;
    if (dlg.exec() == QDialog::Accepted) {
        applyStyleSheetToAllEditors();
        applyEditorLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt());
        applyEditorCaretWidth(s.value(Preferences::EditorCaretWidth, Preferences::DefaultEditorCaretWidth).toInt());
        updateAll();
        updateStats();
        for (const auto &tab : m_tabs) {
            if (tab.editor) {
                tab.editor->invalidateEmojiIconCache();
                tab.editor->updateGutterSettings();
                tab.editor->recheckSpelling();
                applyEditorContentWidth(tab.editor);
            }
        }
        if (m_wrapTextAction)
            m_wrapTextAction->setChecked(s.value(Preferences::EditorWrapEnabled, true).toBool());
        applyPreviewSplitWidth();
        if (m_showPageBreaksAction)
            m_showPageBreaksAction->setChecked(s.value(Preferences::PreviewShowPageBreaks, false).toBool());
        updatePreview();

        int interval = s.value(Preferences::AutoSaveInterval, 0).toInt();
        if (interval > 0)
            m_autoSaveTimer->start(interval * kMsPerMinute);
        else
            m_autoSaveTimer->stop();

        int heavyDelay = s.value(Preferences::HeavyRenderDelay,
            Preferences::DefaultHeavyRenderDelay).toInt();
        if (m_previewInitialized)
            m_preview->page()->runJavaScript(QString("window._scribaHeavyDelay=%1").arg(heavyDelay));

        m_updateTimer->setInterval(s.value(Preferences::PreviewUpdateDelay,
            Preferences::DefaultPreviewUpdateDelay).toInt());

        // Corpus page may have changed the recent list, the dictionary
        // merge/override mode, or the active corpus's monitor flag.
        updateRecentCorporaMenu();
        applyCorpusDictionary();
        if (!m_corpus.filePath.isEmpty()) {
            m_corpus.save();
            if (m_corpus.monitor)
                startCorpusWatcher();
            else
                stopCorpusWatcher();
        }
    } else {
        m_cssConfig->setActiveStylesheet(oldStylesheet);
        m_cssLoader->invalidateCache();
        applyStyleSheetToAllEditors();
        applyEditorLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt());
        applyEditorCaretWidth(s.value(Preferences::EditorCaretWidth, Preferences::DefaultEditorCaretWidth).toInt());
    }
}

void MainWindow::showChartBuilder()
{
    ChartDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString spec = dlg.generatedSpec();
        Editor *ed = currentEditor();
        if (!spec.isEmpty() && ed)
            ed->insertPlainText(spec);
    }
}

void MainWindow::showStockChartBuilder()
{
    StockChartDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString spec = dlg.generatedSpec();
        Editor *ed = currentEditor();
        if (!spec.isEmpty() && ed)
            ed->insertPlainText(spec);
    }
}

void MainWindow::showAdvancedChartBuilder()
{
    AdvancedChartDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString spec = dlg.generatedSpec();
        Editor *ed = currentEditor();
        if (!spec.isEmpty() && ed)
            ed->insertPlainText(spec);
    }
}

void MainWindow::showKatexHelper()
{
    KatexHelperDialog dlg(m_cssLoader->themeCss(), QString(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString latex = dlg.generatedLatex();
        Editor *ed = currentEditor();
        if (!latex.isEmpty() && ed)
            ed->insertPlainText(latex);
    }
}

void MainWindow::showMchemHelper()
{
    MchemHelperDialog dlg(m_cssLoader->themeCss(), QString(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString notation = dlg.generatedNotation();
        Editor *ed = currentEditor();
        if (!notation.isEmpty() && ed)
            ed->insertPlainText(notation);
    }
}

void MainWindow::editChartBlock(Editor *ed, int blockNumber)
{
    if (!ed || blockNumber < 0)
        return;
    const QPair<int, int> range = ed->fencedCodeBlockRange(blockNumber);
    if (range.first < 0)
        return;
    const QString body = ed->fenceBody(blockNumber);
    const QString lang = ed->fenceLanguage(blockNumber);

    if (lang == QLatin1String("mermaid")) {
        MermaidDialog dlg(body, m_cssLoader->themeCss(), this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString replacement = dlg.mermaidBlock().trimmed();
        if (!replacement.isEmpty())
            ed->replaceBlockRange(range.first, range.second, replacement);
        return;
    }
    if (lang == QLatin1String("ec")) {
        const QByteArray json = body.toUtf8();
        switch (ChartSource::detectEcType(json)) {
        case ChartSource::EcType::Stock: {
            StockChartDialog dlg(body, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QString replacement = dlg.generatedSpec().trimmed();
            if (!replacement.isEmpty())
                ed->replaceBlockRange(range.first, range.second, replacement);
            break;
        }
        case ChartSource::EcType::Chart: {
            ChartDialog dlg(body, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QString replacement = dlg.generatedSpec().trimmed();
            if (!replacement.isEmpty())
                ed->replaceBlockRange(range.first, range.second, replacement);
            break;
        }
        case ChartSource::EcType::Sankey:
        case ChartSource::EcType::Boxplot:
        case ChartSource::EcType::Parallel:
        case ChartSource::EcType::ThemeRiver:
        case ChartSource::EcType::Graph:
        case ChartSource::EcType::Treemap:
        case ChartSource::EcType::Sunburst: {
            AdvancedChartDialog dlg(body, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QString replacement = dlg.generatedSpec().trimmed();
            if (!replacement.isEmpty())
                ed->replaceBlockRange(range.first, range.second, replacement);
            break;
        }
        default:
            QMessageBox::warning(this, tr("Chart"),
                tr("This chart was not created by Scriba's chart helpers and "
                   "cannot be reopened for editing."));
        }
        return;
    }

    QMessageBox::warning(this, tr("Chart"),
        tr("This chart was not created by Scriba's chart helpers and "
           "cannot be reopened for editing."));
}

void MainWindow::handleChartEdit(const QString &kind, int line, int index, const QString &tex)
{
    Editor *ed = currentEditor();
    if (!ed)
        return;

    if (kind == QLatin1String("mermaid") || kind == QLatin1String("ec")) {
        // The preview's line numbers are only approximate; the anchor carries
        // the fenced block's body, so match on that (the line only breaks
        // ties between identical charts).
        int blockNumber = ed->findFenceByBody(tex, line);
        if (blockNumber < 0) {
            QMessageBox::warning(this, tr("Edit"),
                tr("Could not locate this diagram in the document."));
            return;
        }
        editChartBlock(ed, blockNumber);
        return;
    }

    // Math: katex or mchem. The preview's line/index are only approximate, so
    // resolve the equation by its inner text (line/index break ties).
    bool mchem = tex.contains(QLatin1String("\\ce{"));
    int matchIndex = -1;
    int blockNumber = ed->findMathByContent(tex, line, index, &matchIndex);
    if (blockNumber < 0) {
        QMessageBox::warning(this, tr("Edit"),
            tr("Could not locate this equation in the document."));
        return;
    }
    if (mchem) {
        MchemHelperDialog dlg(m_cssLoader->themeCss(), tex, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString replacement = dlg.generatedNotation().trimmed();
        if (!replacement.isEmpty())
            ed->replaceInlineMath(blockNumber, matchIndex, replacement);
    } else {
        KatexHelperDialog dlg(m_cssLoader->themeCss(), tex, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString replacement = dlg.generatedLatex().trimmed();
        if (!replacement.isEmpty())
            ed->replaceInlineMath(blockNumber, matchIndex, replacement);
    }
}

void MainWindow::showSpellCheckDialog()
{
    Editor *ed = currentEditor();
    if (!ed || !ed->spellChecker() || !ed->spellChecker()->isLoaded()) {
        showCenteredWarning(
            tr("Spell checking is disabled"),
            tr("The spelling checker is not loaded, so a document-wide check "
               "cannot run."),
            tr("Enable \u201cCheck spelling as you type\u201d on the Spelling "
               "page of Preferences."));
        return;
    }
    if (!m_spellCheckDlg) {
        m_spellCheckDlg = new SpellCheckDialog(ed, this);
        m_spellCheckDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    // A modeless panel that follows the active tab: re-point at the current
    // editor each time it is summoned (or after a tab switch) and bring it
    // to the front.
    m_spellCheckDlg->retarget(ed);
    m_spellCheckDlg->show();
    m_spellCheckDlg->raise();
    m_spellCheckDlg->activateWindow();
}

void MainWindow::generateValidationReport()
{
    if (m_reportInFlight)
        return;

    QVector<TabEntry> tabs;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_reportTitles.contains(i))
            continue; // never re-scan an earlier report tab
        const TabInfo &info = m_tabs[i];
        if (!info.editor)
            continue;
        QString name = info.filePath.isEmpty()
            ? tr("Untitled") : QFileInfo(info.filePath).fileName();
        if (info.dirty)
            name += QStringLiteral(" *");
        tabs.append({i, name});
    }

    ValidationReportDialog dlg(tabs, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_reportOptions = dlg.options();
    m_reportInFlight = true;

    const QSet<int> selected = dlg.selectedTabIndices();
    m_reportSources.clear();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!selected.contains(i))
            continue;
        Editor *ed = m_tabs[i].editor;
        if (!ed)
            continue;
        m_reportSources.append({m_tabs[i].filePath, ed->toPlainText()});
    }

    QSettings settings;
    // An active corpus's dictionary overrides the global dialect/language for
    // the report; empty corpus fields fall back to the per-user preferences.
    const CorpusDictionary &dict = m_corpus.dictionary;
    const bool corpusActive = !m_corpus.filePath.isEmpty();
    const QString dialect = corpusActive && !dict.dialect.isEmpty()
        ? dict.dialect
        : settings
              .value(Preferences::GrammarDialect, QStringLiteral("American")).toString();

    // Spelling: a fresh checker honors the configured dictionary and dialect
    // regardless of which tab is active. Used only on the UI thread, and only
    // when the user asked for the spelling category.
    std::unique_ptr<SpellChecker> spellChecker;
    if (m_reportOptions.categories.contains(ValidationReport::Category::Spelling)) {
        spellChecker = std::make_unique<SpellChecker>();
        spellChecker->setDialect(dialect);
        const QString language = corpusActive && !dict.language.isEmpty()
            ? dict.language
            : settings.value(Preferences::DictionaryLanguage).toString();
        const QString resolved = language.isEmpty()
            ? SpellChecker::defaultLanguageForDialect(dialect) : language;
        if (!spellChecker->loadLanguage(resolved)) {
            for (const QString &lang : SpellChecker::availableLanguages()) {
                if (spellChecker->loadLanguage(lang))
                    break;
            }
        }
    }

    ValidationReport report;
    m_reportDocs = report.scan(m_reportSources,
                               spellChecker ? spellChecker.get() : nullptr,
                               m_reportOptions);

    if (m_reportOptions.categories.contains(ValidationReport::Category::Grammar)) {
        // Grammar pass on a background thread (whole-document and expensive);
        // the results are merged into m_reportDocs when the thread finishes.
        auto *worker = new ValidationReportThread(new StoppardEngine(dialect));
        worker->sources = m_reportSources;
        m_reportThread = worker;
        connect(worker, &QThread::finished, this, [this, worker]() {
            if (m_reportThread == worker)
                m_reportThread = nullptr;
            onValidationReportReady(worker->results);
            worker->deleteLater();
        });
        worker->start();
        statusBar()->showMessage(tr("Generating validation report..."));
    } else {
        openValidationReport(); // no grammar selected: assemble synchronously
    }
}

void MainWindow::onValidationReportReady(
    const QVector<QList<GrammarChecker::Issue>> &grammarIssues)
{
    const int count = qMin(m_reportDocs.size(), grammarIssues.size());
    for (int i = 0; i < count; ++i) {
        m_reportDocs[i].issues[ValidationReport::Category::Grammar] =
            ValidationReport::grammarIssuesToLineIssues(m_reportSources[i].text,
                                                        grammarIssues[i]);
    }
    openValidationReport();
}

void MainWindow::openValidationReport()
{
    m_reportInFlight = false;
    statusBar()->clearMessage();

    const QDateTime now = QDateTime::currentDateTime();
    const QString md = ValidationReport::renderMarkdown(
        m_reportDocs, now.toString(Qt::ISODate), m_reportOptions.categories);

    int idx = addTab(QString());
    if (idx < 0 || idx >= m_tabs.size())
        return;
    Editor *ed = m_tabs[idx].editor;
    ed->setPlainText(md);
    ed->document()->setModified(false);
    m_tabs[idx].dirty = false;
    m_reportTitles.insert(idx,
        tr("Validation Report - ") + now.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    updateTabLabel(idx);
    m_tabBar->setTabToolTip(idx, tr("Validation report (regenerate with Ctrl+Shift+F7)"));
}

void MainWindow::viewTableOfContents()
{
    if (m_corpus.documents.isEmpty()) {
        showCenteredWarning(tr("No Corpus"), tr("No corpus is open."), QString());
        return;
    }
    const QString md = renderTocMarkdown();

    // Refresh an existing TOC tab in place and bring it to the front.
    if (!m_tocTabs.isEmpty()) {
        refreshOpenToc();
        for (auto it = m_tocTabs.begin(); it != m_tocTabs.end(); ++it) {
            if (it.key() < m_tabs.size() && m_tabs[it.key()].editor) {
                m_tabBar->setCurrentIndex(it.key());
                break;
            }
        }
        return;
    }

    const int idx = addTab(QString());
    if (idx < 0 || idx >= m_tabs.size())
        return;
    Editor *ed = m_tabs[idx].editor;
    ed->setReadOnly(true);
    ed->setPlainText(md);
    ed->document()->setModified(false);
    m_tabs[idx].dirty = false;
    m_tocTabs.insert(idx, QStringLiteral("📑 Table of Contents"));
    updateTabLabel(idx);
    m_tabBar->setCurrentIndex(idx);
    refreshPreviewForTocTab(idx, m_corpus.rootDir());
}

QString MainWindow::renderTocMarkdown() const
{
    QHash<QString, QString> links;
    for (const CorpusDocument &d : m_corpus.documents) {
        if (d.path.isEmpty())
            continue;
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(d.path).isAbsolute())
            links.insert(abs, QUrl::fromLocalFile(abs).toString());   // out-of-root: absolute URL
        else
            links.insert(abs, d.path);                                 // in-root: relative to the corpus root
    }
    return CorpusIndex::renderToc(m_corpus, links);
}

void MainWindow::refreshOpenToc()
{
    const QString md = renderTocMarkdown();
    for (auto it = m_tocTabs.begin(); it != m_tocTabs.end(); ++it) {
        if (it.key() >= m_tabs.size() || m_tabs[it.key()].editor == nullptr)
            continue;
        m_tabs[it.key()].editor->setPlainText(md);
        m_tabs[it.key()].editor->document()->setModified(false);
        m_tabs[it.key()].dirty = false;
        if (m_tabBar->currentIndex() == it.key())
            refreshPreviewForTocTab(it.key(), m_corpus.rootDir());
    }
}

void MainWindow::refreshPreviewForTocTab(int index, const QString &rootDir)
{
    Q_UNUSED(rootDir);
    if (index < 0 || index >= m_tabs.size())
        return;
    // Force a re-parse so the (unchanged) cached previewHtml can't serve
    // stale link resolution; updatePreview derives the <base> from m_tocTabs.
    m_tabs[index].previewHtmlValid = false;
    if (m_tabBar->currentIndex() == index)
        updatePreview(true);
}

void MainWindow::stopValidationReport()
{
    if (!m_reportThread)
        return;
    QThread *thread = m_reportThread;
    m_reportThread = nullptr;
    disconnect(thread, &QThread::finished, this, nullptr);
    thread->quit();
    thread->wait();
    delete thread; // ValidationReportThread dtor frees the grammar checker
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
        int offset = dlg.isHtml() ? 16 : 2;
        cursor.setPosition(insertPos + offset, QTextCursor::MoveAnchor);
        ed->setTextCursor(cursor);
        if (!dlg.isHtml())
            ed->formatTableAt(insertPos);
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

    QScrollBar *scrollBar = ed->verticalScrollBar();
    const int scrollBefore = scrollBar->value();
    const QTextCursor caretBefore = ed->textCursor();

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
        else {
            ed->setTextCursor(caretBefore);
            scrollBar->setValue(scrollBefore);
        }
    }

    if (!found)
        statusBar()->showMessage("No matches found", 3000);
    else if (scrollBar->value() != scrollBefore)
        ed->centerCursor();

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

void MainWindow::setPreviewState(int state)
{
    if (state < 0 || state > 3)
        return;
    m_previewState = state;
    QSettings().setValue(Preferences::PreviewState, state);
    syncPreviewLayout();

    QSettings settings;
    bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
    int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();

    Editor *ed = currentEditor();

    if (m_previewState == 0) {
        m_preview->setVisible(false);
        m_editorStack->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
    } else if (m_previewState == 3) {
        m_editorStack->setVisible(false);
        m_preview->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
        if (m_previewInitialized && !m_printLayoutMode) {
            QString css = centre
                ? QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth)
                : QString();
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent='%1'").arg(css));
        }
    } else if (m_previewState == 1) {
        m_splitter->insertWidget(0, m_editorStack);
        m_splitter->insertWidget(1, m_preview);
        m_editorStack->setVisible(true);
        m_preview->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
        if (m_previewInitialized && !m_printLayoutMode)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
    } else {
        m_splitter->insertWidget(0, m_preview);
        m_splitter->insertWidget(1, m_editorStack);
        m_preview->setVisible(true);
        m_editorStack->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
        if (m_previewInitialized && !m_printLayoutMode)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
    }

    int w = m_splitter->width();
    if (w <= 0) w = 1200;
    m_splitter->setSizes({w / 2, w / 2});
}

void MainWindow::togglePreview()
{
    setPreviewState((m_previewState + 1) % 4);
}

void MainWindow::syncPreviewLayout()
{
    if (!m_layoutActions)
        return;
    for (QAction *a : m_layoutActions->actions())
        a->setChecked(a->data().toInt() == m_previewState);
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
    int charsNoSpace = countCharactersWithoutSpaces(text);
    int charsWithSpace = countCharactersWithSpaces(text);
    int paragraphs = countParagraphs(text);
    int complexWords = countComplexWords(words);
    double lexDensity = lexicalDensity(words);
    double readingEase = fleschReadingEase(wordCount, sentences, totalSyllables);

    QSettings s;
    QStringList enabled = s.value(Preferences::StatusBarMetrics).toStringList();
    double wps = s.value(Preferences::WordsPerSecond, 3.33).toDouble();
    int spWpm = s.value(Preferences::SpeakingWpm, 150).toInt();

    auto formula = Preferences::formulaFromString(
        s.value(Preferences::ReadabilityFormula,
            Preferences::formulaToString(Preferences::Formula::FleschKincaid)).toString());
    double grade = 0.0;
    switch (formula) {
    case Preferences::Formula::ColemanLiau:
        grade = colemanLiauGrade(wordCount, sentences, charsNoSpace);
        break;
    case Preferences::Formula::GunningFog:
        grade = gunningFogGrade(wordCount, sentences, complexWords);
        break;
    case Preferences::Formula::Smog: {
        int polysyllables = 0;
        for (const QString &w : words) {
            if (estimateSyllables(w) >= 3)
                ++polysyllables;
        }
        grade = smogGrade(sentences, polysyllables);
        break;
    }
    case Preferences::Formula::ARI:
        grade = ariGrade(wordCount, sentences, charsNoSpace);
        break;
    default:
        grade = fleschKincaidGrade(wordCount, sentences, totalSyllables);
        break;
    }
    int age = qMax(static_cast<int>(grade) + 5, 5);

    struct Metric {
        const char *key;
        QString value;
    };
    QVector<Metric> allMetrics;
    allMetrics.push_back({"words", QStringLiteral("%1 word%2").arg(wordCount).arg(wordCount == 1 ? "" : "s")});
    allMetrics.push_back({"sentences", QStringLiteral("%1 sentence%2").arg(sentences).arg(sentences == 1 ? "" : "s")});
    allMetrics.push_back({"paragraphs", QStringLiteral("%1 paragraph%2").arg(paragraphs).arg(paragraphs == 1 ? "" : "s")});
    allMetrics.push_back({"char-nospace", QStringLiteral("%1 char").arg(charsNoSpace)});
    allMetrics.push_back({"char-withspace", QStringLiteral("%1 char").arg(charsWithSpace)});
    allMetrics.push_back({"reading-time", QStringLiteral("~%1 min read").arg(static_cast<int>(wordCount / (wps * 60)) + 1)});
    allMetrics.push_back({"speaking-time", QStringLiteral("~%1 min speak").arg(wordCount / spWpm + 1)});
    allMetrics.push_back({"reading-age", QStringLiteral("%1: Age %2+").arg(QLatin1String(Preferences::formulaLabel(formula))).arg(age)});
    allMetrics.push_back({"flesch-ease", QStringLiteral("RE: %1").arg(static_cast<int>(readingEase))});
    allMetrics.push_back({"syllables", QStringLiteral("%1 syllable%2").arg(totalSyllables).arg(totalSyllables == 1 ? "" : "s")});
    allMetrics.push_back({"complex-words", QStringLiteral("%1 complex").arg(complexWords)});
    allMetrics.push_back({"lexical-density", QStringLiteral("%1% unique").arg(static_cast<int>(lexDensity))});
    allMetrics.push_back({"avg-wps", QStringLiteral("%1 w/s").arg(static_cast<double>(wordCount) / qMax(sentences, 1), 0, 'f', 1)});
    allMetrics.push_back({"avg-spw", QStringLiteral("%1 syl/w").arg(static_cast<double>(totalSyllables) / qMax(wordCount, 1), 0, 'f', 2)});

    QStringList parts;
    int count = 0;
    for (const auto &m : allMetrics) {
        if (enabled.contains(QLatin1String(m.key))) {
            parts << m.value;
            if (++count >= 5) break;
        }
    }

    m_statsLabel->setText(parts.isEmpty() ? QString() : parts.join(" · "));
}

void MainWindow::loadFile(const QString &filePath, bool forceReload)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filePath);
        return;
    }

    int existing = findTabByPath(filePath);
    if (!forceReload && existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
        file.close();
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    TabInfo *info = nullptr;
    int idx;
    bool replacedUntitled = false;

    if (forceReload && existing >= 0) {
        idx = existing;
        m_tabBar->setCurrentIndex(idx);
        m_tabs[idx].previewHtmlValid = false;
        QSignalBlocker blocker(m_tabs[idx].editor);
        m_tabs[idx].editor->setPlainText(content);
        {
            QSettings s;
            QTextBlockFormat fmt;
            fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                              QTextBlockFormat::ProportionalHeight);
            QTextCursor cursor(m_tabs[idx].editor->document());
            cursor.select(QTextCursor::Document);
            cursor.mergeBlockFormat(fmt);
        }
        m_tabs[idx].dirty = false;
        updateTabLabel(idx);
        info = &m_tabs[idx];
    } else {
        idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size() && m_tabs[idx].filePath.isEmpty() && !m_tabs[idx].dirty && m_tabs[idx].editor->toPlainText().isEmpty()) {
            m_tabs[idx].filePath = filePath;
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(content);
            m_tabs[idx].previewHtmlValid = false;
            {
                QSettings s;
                QTextBlockFormat fmt;
                fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                                  QTextBlockFormat::ProportionalHeight);
                QTextCursor cursor(m_tabs[idx].editor->document());
                cursor.select(QTextCursor::Document);
                cursor.mergeBlockFormat(fmt);
            }
            m_tabs[idx].editor->setCurrentFile(filePath);
            m_tabs[idx].dirty = false;
            info = &m_tabs[idx];
            updateTabLabel(idx);
            m_tabBar->setTabToolTip(idx, filePath);
            replacedUntitled = true;
        } else {
            idx = addTab(filePath);
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(content);
            m_tabs[idx].previewHtmlValid = false;
            {
                QSettings s;
                QTextBlockFormat fmt;
                fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                                  QTextBlockFormat::ProportionalHeight);
                QTextCursor cursor(m_tabs[idx].editor->document());
                cursor.select(QTextCursor::Document);
                cursor.mergeBlockFormat(fmt);
            }
            m_tabs[idx].dirty = false;
            updateTabLabel(idx);
            info = &m_tabs[idx];
        }
    }

    updateWindowTitle();
    m_preview->setDocumentPath(filePath);
    m_previewInitialized = false;
    updatePreview();
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
    // A new directory changes how relative image paths resolve (they hang off
    // the shared page's <base> element), so re-render to move the base over.
    QString oldDir = QFileInfo(info->filePath).absolutePath();
    info->filePath = filePath;
    ed->setCurrentFile(filePath);
    info->dirty = false;

    int idx = m_tabBar->currentIndex();
    updateTabLabel(idx);
    m_tabBar->setTabToolTip(idx, filePath);

    updateWindowTitle();
    m_preview->setDocumentPath(filePath);
    statusBar()->showMessage("Saved", 2000);

    if (pathChanged) {
        if (QFileInfo(filePath).absolutePath() != oldDir && m_previewInitialized)
            updatePreview();
    }
}

void MainWindow::renameCurrentFile()
{
    TabInfo *info = activeTabInfo();
    if (!info)
        return;

    if (info->filePath.isEmpty()) {
        QMessageBox::information(this, "Rename",
            "Save the file first; unsaved tabs cannot be renamed.");
        return;
    }

    const QFileInfo finfo(info->filePath);
    // Use a file dialog so the user can pick a new name/location like a save.
    // DontConfirmOverwrite defers the existing-file check to our own warning
    // (a rename must never silently replace another file).
    QString newPath = QFileDialog::getSaveFileName(
        this, "Rename File", info->filePath, QString::fromLatin1(kMdFilter),
        nullptr, QFileDialog::DontConfirmOverwrite);
    if (newPath.isEmpty())
        return;

    if (newPath == info->filePath)
        return;

    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, "Rename",
            "Cannot rename: a file with that name already exists.");
        return;
    }

    if (!QFile::rename(info->filePath, newPath)) {
        QMessageBox::warning(this, "Rename", "Could not rename file: " + info->filePath);
        return;
    }

    Editor *ed = currentEditor();
    if (!ed)
        return;
    QString oldDir = QFileInfo(info->filePath).absolutePath();
    const QString oldAbs = QFileInfo(info->filePath).absoluteFilePath();
    info->filePath = newPath;
    ed->setCurrentFile(newPath);

    int idx = m_tabBar->currentIndex();
    updateTabLabel(idx);
    m_tabBar->setTabToolTip(idx, newPath);

    updateWindowTitle();
    m_preview->setDocumentPath(newPath);
    statusBar()->showMessage("Renamed to " + newPath, 2000);

    // The base directory changed: re-render so relative images (which resolve
    // against the shared page's <base> element) follow the new location.
    if (QFileInfo(newPath).absolutePath() != oldDir && m_previewInitialized)
        updatePreview();

    // Update any corpus links that pointed at the old path.
    rewriteLinksForFile(oldAbs, newPath);
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

void MainWindow::importHtmlFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import HTML File", QString(), "HTML Files (*.html *.htm);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import HTML", "Could not open file: " + path);
        return;
    }

    const QString markdown = HtmlToMarkdown::convert(
        QString::fromUtf8(file.readAll()), QUrl::fromLocalFile(path));
    file.close();

    if (markdown.isEmpty()) {
        QMessageBox::warning(this, "Import HTML",
            "No convertible content was found in the file.");
        return;
    }

    int idx = addTab();
    QSignalBlocker blocker(m_tabs[idx].editor);
    m_tabs[idx].editor->setPlainText(markdown);
    m_tabs[idx].previewHtmlValid = false;
    {
        QSettings s;
        QTextBlockFormat fmt;
        fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                          QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(m_tabs[idx].editor->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(fmt);
    }
    m_tabs[idx].dirty = true;
    updateTabLabel(idx);
    m_previewInitialized = false;
    updatePreview();
    statusBar()->showMessage("Imported " + QFileInfo(path).fileName(), 3000);
}

// Opens a Word (.docx) package, converts the body OOXML to Markdown and loads
// it into a new tab. Embedded images are written according to the Import
// preference (next to the document, a configured folder, system temp, or ask).
void MainWindow::importDocxFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Word Document", QString(), "Word Documents (*.docx);;All Files (*)");
    if (path.isEmpty())
        return;

    QSettings s;
    DocxImportOptions opts;
    const QString loc = s.value(Preferences::ImportImageLocation,
        QStringLiteral("currentDir")).toString();
    if (loc == QLatin1String("customDir"))
        opts.imageLocation = DocxImportOptions::ImageLocation::CustomDir;
    else if (loc == QLatin1String("tempDir"))
        opts.imageLocation = DocxImportOptions::ImageLocation::TempDir;
    else if (loc == QLatin1String("ask"))
        opts.imageLocation = DocxImportOptions::ImageLocation::Ask;
    else
        opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.customImageDir = s.value(Preferences::ImportImageDir).toString();
    opts.documentDir = QFileInfo(path).absolutePath();

    DocxImportResult result = DocxImporter::import(path, opts);
    if (!result.ok) {
        QMessageBox::warning(this, "Import Word Document", result.error);
        return;
    }

    int idx = addTab();
    QSignalBlocker blocker(m_tabs[idx].editor);
    m_tabs[idx].editor->setPlainText(result.markdown);
    m_tabs[idx].previewHtmlValid = false;
    {
        QTextBlockFormat fmt;
        fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                          QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(m_tabs[idx].editor->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(fmt);
    }
    m_tabs[idx].dirty = true;
    updateTabLabel(idx);
    m_previewInitialized = false;
    updatePreview();

    if (!result.warnings.isEmpty()) {
        statusBar()->showMessage("Imported " + QFileInfo(path).fileName()
                                 + " (" + QString::number(result.warnings.size())
                                 + " warnings)", 5000);
        QMessageBox::information(this, "Import Word Document",
                                 result.warnings.join('\n'));
    } else {
        statusBar()->showMessage("Imported " + QFileInfo(path).fileName(), 3000);
    }
}

// Opens a PDF file, extracts its text via pdf.js and heuristically converts
// it to Markdown in a new tab. Ghost images (pure scans) yield a warning.
void MainWindow::importPdfFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import PDF Document", QString(), "PDF Files (*.pdf);;All Files (*)");
    if (path.isEmpty())
        return;

    PdfImportResult result = PdfImporter::convert(path);
    if (!result.ok) {
        QMessageBox::warning(this, "Import PDF Document", result.error);
        return;
    }

    QSettings s;
    int idx = addTab();
    QSignalBlocker blocker(m_tabs[idx].editor);
    m_tabs[idx].editor->setPlainText(result.markdown);
    m_tabs[idx].previewHtmlValid = false;
    {
        QTextBlockFormat fmt;
        fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                          QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(m_tabs[idx].editor->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(fmt);
    }
    m_tabs[idx].dirty = true;
    updateTabLabel(idx);
    m_previewInitialized = false;
    updatePreview();

    statusBar()->showMessage("Imported " + QFileInfo(path).fileName()
                             + " (" + QString::number(result.pages) + " pages)", 5000);
}

void MainWindow::pasteAsMarkdown()
{
    Editor *ed = currentEditor();
    if (!ed) return;

    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    QString text;
    if (mime && mime->hasHtml()) {
        text = HtmlToMarkdown::convert(mime->html());
        if (text.isEmpty())
            return;
    } else if (mime && mime->hasText()) {
        text = mime->text();
    } else {
        return;
    }

    ed->textCursor().insertText(text);
}

void MainWindow::exportPdf()
{
    Editor *ed = currentEditor();
    if (!ed) return;
    TabInfo *info = activeTabInfo();
    if (!info) return;

    QSettings prefs;
    QString markdown = ed->toPlainText();
    QString html = m_parser->toHtml(markdown, prefs.value(Preferences::BlockRawHtmlExport, true).toBool());

    if (prefs.value(Preferences::StripExportScripts, true).toBool())
        html = JsRenderEngine::stripScriptTags(html);

    ExportPdfDialog dlg(html, info->filePath, m_cssLoader, this);
    dlg.exec();
}

void MainWindow::exportDocx()
{
    Editor *ed = currentEditor();
    if (!ed) return;
    TabInfo *info = activeTabInfo();
    if (!info) return;

    // Show export dialog to get math mode preference
    ExportDocxDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    DocxMathMode mathMode = dlg.selectedMathMode();
    DocxExportOptions opts;
    opts.mathMode = mathMode;
    opts.landscape = dlg.isLandscape();
    opts.marginTopCm = dlg.marginTop();
    opts.marginBottomCm = dlg.marginBottom();
    opts.marginLeftCm = dlg.marginLeft();
    opts.marginRightCm = dlg.marginRight();
    opts.pageNumbers = dlg.hasPageNumbers();

    QSettings prefs;
    QString markdown = ed->toPlainText();
    QString html = m_parser->toHtml(markdown, prefs.value(Preferences::BlockRawHtmlExport, true).toBool());
    if (prefs.value(Preferences::StripExportScripts, true).toBool())
        html = JsRenderEngine::stripScriptTags(html);
    QString css = m_cssLoader->previewBaseCss() + "\n" + m_cssLoader->themeCss();
    if (!prefs.value(Preferences::ShowCodeLangExport, true).toBool())
        css += QStringLiteral("\n") + QLatin1String(Preferences::HideCodeLangCss);

    QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    QString mermaidTheme = CssUtils::isDarkTheme(m_cssLoader->themeCss())
        ? QStringLiteral("dark") : QStringLiteral("default");

    QUrl baseUrl;
    if (!info->filePath.isEmpty())
        baseUrl = QUrl::fromLocalFile(QFileInfo(info->filePath).absolutePath() + "/");

    // Use image mode: render KaTeX as PNG images in WebEngine
    // Use OMML mode: keep KaTeX HTML for conversion in HtmlToOoxml
    QString fullHtml;
    if (mathMode == DocxMathMode::Images) {
        fullHtml = JsRenderEngine::buildFullHtmlForDocx(html, css, emojiMode, mermaidTheme);
    } else {
        fullHtml = JsRenderEngine::buildFullHtmlForDocxOmml(html, css, emojiMode, mermaidTheme);
    }
    if (prefs.value(Preferences::EnableCspExport, true).toBool()) {
        int headEnd = fullHtml.indexOf("</head>");
        if (headEnd >= 0)
            fullHtml.insert(headEnd, QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">").arg(Security::CspHeader));
    }
    QString renderedHtml = JsRenderEngine::renderSync(fullHtml, baseUrl.toString());

    if (renderedHtml.isEmpty()) {
        showCenteredWarning("Export Failed",
            "Could not render the document for DOCX export.",
            "The JavaScript rendering step timed out or failed.");
        return;
    }

    renderedHtml = JsRenderEngine::replaceQrcUrls(renderedHtml);
    renderedHtml = JsRenderEngine::embedImages(renderedHtml, baseUrl);
    renderedHtml = JsRenderEngine::embedResources(renderedHtml, ScriptHandling::Strip);

    // Include KaTeX CSS so HtmlToOoxml can resolve font metrics for math spans
    QString katexCss = JsRenderEngine::katexCss();
    QString docxCss = css;
    if (!katexCss.isEmpty())
        docxCss += QStringLiteral("\n") + katexCss;

    QString defaultName = info->filePath.isEmpty()
        ? "document.docx"
        : QFileInfo(info->filePath).completeBaseName() + ".docx";

    QString path = QFileDialog::getSaveFileName(
        this, "Export as Word (DOCX)", defaultName, "Word Documents (*.docx)");
    if (path.isEmpty()) return;

    if (!DocxExporter::exportToDocx(renderedHtml, path, docxCss, opts)) {
        showCenteredWarning("Export Failed",
            "Could not export the document as DOCX.",
            "Check that the file is not open in another application and that the path is writable.");
    }
}

void MainWindow::exportHtml()
{
    Editor *ed = currentEditor();
    if (!ed) return;
    TabInfo *info = activeTabInfo();
    if (!info) return;

    ExportHtmlDialog dlg(m_cssConfig, m_cssLoader, info->filePath, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString themePath = dlg.selectedThemePath();
    if (themePath.isEmpty())
        return;

    // Load the selected theme CSS
    QFile themeFile(themePath);
    QString themeCss;
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text))
        themeCss = QString::fromUtf8(themeFile.readAll());

    QString baseCss = m_cssLoader->previewBaseCss();
    QString combinedCss = baseCss + "\n" + themeCss;

    QSettings prefs;
    QString markdown = ed->toPlainText();
    QString bodyHtml = m_parser->toHtml(markdown, prefs.value(Preferences::BlockRawHtmlExport, true).toBool());

    QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    QString mermaidTheme = CssUtils::isDarkTheme(themeCss)
        ? QStringLiteral("dark") : QStringLiteral("default");

    QUrl baseUrl;
    if (!info->filePath.isEmpty())
        baseUrl = QUrl::fromLocalFile(QFileInfo(info->filePath).absolutePath() + "/");

    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, combinedCss, emojiMode, mermaidTheme);
    QString renderedBody = JsRenderEngine::renderSync(fullHtml, baseUrl.toString());

    if (renderedBody.isEmpty()) {
        showCenteredWarning("Export Failed",
            "Could not render the document for HTML export.",
            "The JavaScript rendering step timed out or failed.");
        return;
    }

    renderedBody = JsRenderEngine::replaceQrcUrls(renderedBody);
    renderedBody = JsRenderEngine::embedImages(renderedBody, baseUrl);
    renderedBody = JsRenderEngine::embedResources(renderedBody, dlg.selectedScriptHandling());

    // Strip #editor rule from theme CSS for the export (editor-only styling)
    QString exportCss = themeCss;
    exportCss.remove(QRegularExpression(R"(#editor\s*\{[^}]*\})"));
    exportCss = baseCss + "\n" + exportCss;

    // Responsive SVG rule for ECharts charts (baked-in SVG width needs to scale)
    exportCss += QStringLiteral(
        "\n.echarts-chart svg{max-width:100%;height:auto;width:auto!important}");

    if (!prefs.value(Preferences::ShowCodeLangExport, true).toBool())
        exportCss += QStringLiteral("\n") + QLatin1String(Preferences::HideCodeLangCss);

    // Include KaTeX CSS so math renders correctly without external dependencies
    QString katexCss = JsRenderEngine::katexCss();

    QString cspMeta;
    if (prefs.value(Preferences::EnableCspExport, true).toBool())
        cspMeta = QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">\n").arg(Security::CspHeader);

    QString output = QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "%4"
        "<style>%1</style>\n"
        "<style>%3</style>\n"
        "</head>\n"
        "<body>%2</body>\n"
        "</html>\n"
    ).arg(exportCss, renderedBody, katexCss, cspMeta);

    QString defaultName = info->filePath.isEmpty()
        ? "document.html"
        : QFileInfo(info->filePath).completeBaseName() + ".html";

    QString path = QFileDialog::getSaveFileName(
        this, "Export as HTML", defaultName, "HTML Files (*.html);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showCenteredWarning("Export Failed",
            "Could not write the HTML file.",
            "Check that the path is writable.");
        return;
    }

    f.write(output.toUtf8());
    f.close();
    statusBar()->showMessage("Exported as HTML", 2000);
}

void MainWindow::refreshCorpusFromTabs()
{
    m_corpus.documents.clear();
    const int rawActive = m_tabBar->currentIndex();
    int fileActive = 0;
    int activeIndex = -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        const TabInfo &info = m_tabs[i];
        if (m_reportTitles.contains(i) || m_tocTabs.contains(i))
            continue;   // generated report/TOC tabs are not part of the corpus

        CorpusDocument d;
        if (!info.filePath.isEmpty())
            d.path = Corpus::storedPath(m_corpus.rootDir(), info.filePath);
        else {
            d.content = info.editor->toPlainText();
            d.name = tabTitleForEmbedded(i);
        }
        d.cursorBlock = info.editor->textCursor().blockNumber();
        d.cursorCol = info.editor->textCursor().positionInBlock();
        d.scroll = info.editor->verticalScrollBar()->value();
        d.folds = info.editor->foldedBlockNumbers();
        m_corpus.documents.append(d);

        if (i == rawActive)
            activeIndex = fileActive;
        ++fileActive;
    }
    m_corpus.active = activeIndex;
}

QJsonObject MainWindow::serializeCorpus()
{
    refreshCorpusFromTabs();
    return m_corpus.toJson();
}

QString MainWindow::tabTitleForEmbedded(int index) const
{
    if (index < 0 || index >= m_tabs.size())
        return QString();
    if (m_reportTitles.contains(index))
        return m_reportTitles.value(index);
    if (m_tocTabs.contains(index))
        return m_tocTabs.value(index);
    return m_tabs[index].filePath.isEmpty() ? QStringLiteral("Untitled")
                                            : QFileInfo(m_tabs[index].filePath).fileName();
}

void MainWindow::restoreTabState(int idx, const CorpusDocument &d)
{
    if (idx < 0 || idx >= m_tabs.size())
        return;
    Editor *ed = m_tabs[idx].editor;
    if (!ed)
        return;

    if (!d.folds.isEmpty())
        ed->restoreFolds(d.folds);
    QTextCursor tc = restoreCursorPosition(ed->document(), d.cursorBlock, d.cursorCol);
    ed->setTextCursor(tc);
    const int scroll = d.scroll;
    QTimer::singleShot(0, [this, idx, scroll]() {
        if (idx < m_tabs.size() && m_tabs[idx].editor)
            m_tabs[idx].editor->verticalScrollBar()->setValue(scroll);
    });
}

void MainWindow::restoreCorpus(const QJsonObject &corpus)
{
    if (corpus["version"].toInt() != 1) return;
    QJsonArray docs = corpus["documents"].toArray();
    if (docs.isEmpty()) return;

    QVector<CorpusDocument> parsed;
    for (const QJsonValue &v : docs) {
        const QJsonObject o = v.toObject();
        CorpusDocument d;
        d.path = o["path"].toString();
        d.content = o["content"].toString();
        d.name = o["name"].toString();
        const QJsonObject c = o["cursor"].toObject();
        d.cursorBlock = c["block"].toInt();
        d.cursorCol = c["col"].toInt();
        d.scroll = o["scroll"].toInt();
        for (const QJsonValue &f : o["folds"].toArray())
            d.folds.append(f.toInt());
        parsed.append(d);
    }

    bool firstTabIsEmpty = (m_tabs.size() == 1 && m_tabs[0].filePath.isEmpty()
                            && !m_tabs[0].dirty && m_tabs[0].editor->toPlainText().isEmpty());

    if (firstTabIsEmpty) {
        int idx = m_tabBar->currentIndex();
        disconnectTabEditor(idx);
        Editor *ed = m_tabs[idx].editor;
        m_tabs.removeAt(idx);
        m_editorStack->removeWidget(ed);
        m_tabBar->removeTab(idx);
        delete ed;
    }

    for (const CorpusDocument &d : parsed) {
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        QString content;
        if (!d.path.isEmpty()) {
            QFile f(abs);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;   // missing document: don't resurrect a blank tab
            content = QString::fromUtf8(f.readAll());
            f.close();
        } else {
            content = d.content;
        }

        int idx = addTab(abs);
        QSignalBlocker blocker(m_tabs[idx].editor);
        m_tabs[idx].editor->setPlainText(content);
        m_tabs[idx].previewHtmlValid = false;
        {
            QSettings s;
            QTextBlockFormat fmt;
            fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                              QTextBlockFormat::ProportionalHeight);
            QTextCursor cursor(m_tabs[idx].editor->document());
            cursor.select(QTextCursor::Document);
            cursor.mergeBlockFormat(fmt);
        }
        m_tabs[idx].dirty = false;
        updateTabLabel(idx);
        restoreTabState(idx, d);
    }

    if (corpus["active"].toInt(0) >= 0 && corpus["active"].toInt(0) < m_tabBar->count())
        m_tabBar->setCurrentIndex(corpus["active"].toInt(0));

    updateTabBarVisibility();
    if (auto *ed = currentEditor())
        ed->setFocus();
}

void MainWindow::saveCorpusAction()
{
    if (m_corpus.filePath.isEmpty()) {
        saveCorpusAsAction();
        return;
    }
    refreshCorpusFromTabs();
    QString error;
    if (!m_corpus.save(&error)) {
        showCenteredWarning(tr("Save Corpus Failed"),
            tr("Could not save the corpus file."),
            tr("Check that the file is not open in another application and that the path is writable."));
        return;
    }
    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Corpus saved: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 2000);
}

void MainWindow::saveCorpusAsAction()
{
    QString startDir;
    if (TabInfo *info = activeTabInfo(); info && !info->filePath.isEmpty())
        startDir = QFileInfo(info->filePath).absolutePath();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Corpus As"), startDir, tr("Scriba Corpus (*.scriba)"));
    if (path.isEmpty())
        return;
    m_corpus.filePath = QFileInfo(path).absoluteFilePath();
    refreshCorpusFromTabs();
    QString error;
    if (!m_corpus.save(&error)) {
        showCenteredWarning(tr("Save Corpus Failed"),
            tr("Could not save the corpus file."),
            tr("Check that the file is not open in another application and that the path is writable."));
        return;
    }
    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Corpus saved: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 2000);
}

void MainWindow::openCorpusAction()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Corpus"), QString(), tr("Scriba Corpus (*.scriba)"));
    if (path.isEmpty())
        return;
    openCorpusFile(path, /*skipPrompt=*/false);
}

void MainWindow::openCorpusFile(const QString &path, bool skipPrompt)
{
    Corpus loaded;
    QString error;
    if (!Corpus::loadFile(path, &loaded, &error)) {
        showCenteredWarning(tr("Open Corpus Failed"),
            tr("Could not open the corpus file."), error);
        return;
    }
    if (!skipPrompt && !maybeDiscardCurrentTabs())
        return;

    closeAllTabs();

    m_corpus = loaded;

    int missing = 0;
    for (int i = 0; i < m_corpus.documents.size(); ++i) {
        const CorpusDocument &d = m_corpus.documents.at(i);
        if (!d.path.isEmpty()) {
            const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
            QFile f(abs);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { ++missing; continue; }
            const QString content = QString::fromUtf8(f.readAll());
            f.close();
            const int idx = addTab(abs);
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(content);
            m_tabs[idx].dirty = false;
            m_tabs[idx].previewHtmlValid = false;
            updateTabLabel(idx);
            restoreTabState(idx, d);
        } else {
            const int idx = addTab(QString());
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(d.content);
            m_tabs[idx].dirty = false;
            updateTabLabel(idx);
            restoreTabState(idx, d);
        }
    }
    if (m_corpus.active >= 0 && m_corpus.active < m_tabBar->count())
        m_tabBar->setCurrentIndex(m_corpus.active);

    applyCorpusDictionary();
    startCorpusWatcher();

    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    updateTabBarVisibility();
    updateRecentCorporaMenu();
    if (missing > 0)
        statusBar()->showMessage(tr("Corpus opened; %1 document(s) missing").arg(missing), 4000);
    else
        statusBar()->showMessage(tr("Corpus opened: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 2000);
}

bool MainWindow::maybeDiscardCurrentTabs()
{
    bool anyDirty = false;
    bool hasUntitledDirty = false;
    for (const TabInfo &info : m_tabs) {
        if (info.dirty) {
            anyDirty = true;
            if (info.filePath.isEmpty())
                hasUntitledDirty = true;
        }
    }
    if (!anyDirty)
        return true;
    const ClosePromptResult ret = promptUnsavedChanges(hasUntitledDirty);
    if (ret == ClosePromptResult::Cancel)
        return false;
    if (ret == ClosePromptResult::Save)
        return saveAllDirtyTabs();
    return true;   // Discard
}

bool MainWindow::saveAllDirtyTabs()
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        TabInfo &info = m_tabs[i];
        if (!info.dirty)
            continue;
        if (info.filePath.isEmpty()) {
            const QString file = saveAsDialogPath();
            if (file.isEmpty())
                return false;
            info.filePath = file;
        }
        QFile file(info.filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showCenteredWarning(tr("Save Failed"),
                tr("Could not save \"%1\".\n%2").arg(info.filePath, file.errorString()),
                QString());
            return false;
        }
        file.write(info.editor->toPlainText().toUtf8());
        info.dirty = false;
        updateTabLabel(i);
    }
    return true;
}

void MainWindow::closeAllTabs()
{
    while (m_tabs.size() > 1) {
        int idx = m_tabBar->currentIndex();
        removeTab(idx);
    }
    if (m_tabs.size() == 1) {
        int idx = 0;
        disconnectTabEditor(idx);
        m_tabs[0].editor->clear();
        m_tabs[0].previewHtmlValid = false;
        m_tabs[0].filePath.clear();
        m_tabs[0].dirty = false;
        // A virtual TOC/report tab left as the sole tab is blanked into an
        // Untitled placeholder; drop its mapping so it can't haunt the next corpus.
        m_tocTabs.remove(idx);
        m_reportTitles.remove(idx);
    }
}

void MainWindow::addRecentCorpus(const QString &path)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    QSettings s;
    QStringList recents = s.value(Preferences::RecentCorpora).toStringList();
    recents.removeAll(abs);
    recents.prepend(abs);
    while (recents.size() > Preferences::MaxRecentCorpora)
        recents.removeLast();
    s.setValue(Preferences::RecentCorpora, recents);
    updateRecentCorporaMenu();
}

void MainWindow::updateRecentCorporaMenu()
{
    if (!m_recentCorpusMenu)
        return;
    QSettings s;
    QStringList recents = s.value(Preferences::RecentCorpora).toStringList();
    recents.erase(std::remove_if(recents.begin(), recents.end(),
        [](const QString &p) { return !QFileInfo::exists(p); }), recents.end());
    s.setValue(Preferences::RecentCorpora, recents);

    m_recentCorpusMenu->clear();
    if (recents.isEmpty()) {
        QAction *emptyAction = m_recentCorpusMenu->addAction(tr("(empty)"));
        emptyAction->setEnabled(false);
        return;
    }
    for (const QString &p : recents) {
        QAction *a = m_recentCorpusMenu->addAction(QFileInfo(p).fileName());
        a->setToolTip(p);
        connect(a, &QAction::triggered, this, [this, p]() { openCorpusFile(p); });
    }
}

void MainWindow::applyCorpusDictionary()
{
    if (m_corpus.filePath.isEmpty())
        return;
    const QString mode = QSettings().value(
        Preferences::CorpusDictionaryMode, QStringLiteral("override")).toString();
    const bool merge = (mode == QLatin1String("merge"));
    for (const TabInfo &tab : m_tabs) {
        if (tab.editor)
            tab.editor->applyCorpusDictionary(m_corpus.dictionary, merge);
    }
}

void MainWindow::startCorpusWatcher()
{
    stopCorpusWatcher();
    if (!m_corpus.monitor || m_corpus.filePath.isEmpty())
        return;
    QStringList files;
    for (const CorpusDocument &d : m_corpus.documents) {
        if (!d.path.isEmpty()) {
            const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
            if (QFileInfo::exists(abs))
                files.append(abs);
        }
    }
    m_corpusWatcher->setMonitoredFiles(files);
}

void MainWindow::stopCorpusWatcher()
{
    m_corpusWatcher->clear();
}

void MainWindow::handleExternalEdit(const QString &path)
{
    const int idx = findTabByPath(path);
    if (idx < 0)
        return;
    const QString policy = QSettings().value(
        Preferences::CorpusExternalEditPolicy, QStringLiteral("autoReload")).toString();
    if (policy == QLatin1String("prompt")) {
        if (QMessageBox::question(this, tr("File changed on disk"),
                 tr("%1 was modified outside Scriba. Reload it?")
                     .arg(QFileInfo(path).fileName())) != QMessageBox::Yes)
            return;
        loadFile(path, /*forceReload=*/true);
    } else if (policy == QLatin1String("autoReload")) {
        if (m_tabs[idx].dirty) {
            if (QMessageBox::question(this, tr("File changed on disk"),
                     tr("%1 was modified outside Scriba. Reload it and discard your changes?")
                         .arg(QFileInfo(path).fileName())) != QMessageBox::Yes)
                return;
        }
        loadFile(path, /*forceReload=*/true);
    } else {                                     // autoReloadDirty
        loadFile(path, /*forceReload=*/true);
    }
}

void MainWindow::handleExternalRename(const QString &from, const QString &to)
{
    const int idx = findTabByPath(from);
    if (idx >= 0) {
        m_tabs[idx].filePath = to;
        m_tabs[idx].editor->setCurrentFile(to);
        updateTabLabel(idx);
        m_tabBar->setTabToolTip(idx, to);
        m_preview->setDocumentPath(to);
    }
    for (CorpusDocument &d : m_corpus.documents) {
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(abs).absoluteFilePath() == QFileInfo(from).absoluteFilePath()) {
            d.path = Corpus::storedPath(m_corpus.rootDir(), to);
            break;
        }
    }
    m_corpus.save();
    statusBar()->showMessage(tr("Corpus updated: %1 → %2")
                                 .arg(QFileInfo(from).fileName(), QFileInfo(to).fileName()),
                             4000);
    rewriteLinksForFile(from, to);
    if (!m_tocTabs.isEmpty())
        refreshOpenToc();
}

void MainWindow::handleExternalDelete(const QString &path)
{
    const int idx = findTabByPath(path);
    if (idx < 0)
        return;
    statusBar()->showMessage(tr("%1 was deleted on disk").arg(QFileInfo(path).fileName()), 4000);
    if (!m_tocTabs.isEmpty())
        refreshOpenToc();
}

void MainWindow::rewriteLinksForFile(const QString &oldAbs, const QString &newAbs)
{
    const QSettings prefs;
    const QString policy = prefs.value(Preferences::CorpusLinkRewritePolicy,
                                       QStringLiteral("prompt")).toString();
    if (policy == QLatin1String("ignore"))
        return;

    struct TabEdit { Editor *editor = nullptr; int tabIndex = -1; QString replaced; };
    QStringList affectedDocs;
    QList<TabEdit> tabEdits;
    const bool scopeAll = prefs.value(Preferences::CorpusLinkRewriteScope,
                                      QStringLiteral("open")).toString() == QLatin1String("all");

    auto consider = [&](const QString &absPath, Editor *ed, int tabIndex, bool onDisk) {
        const QString source = ed ? ed->toPlainText() : [&]{
            QFile f(absPath);
            if (!f.open(QIODevice::ReadOnly))
                return QString();
            const QString s = QString::fromUtf8(f.readAll());
            f.close();
            return s;
        }();
        const QString docDir = QFileInfo(absPath).absolutePath();
        const QString rewritten = LinkFixer::rewrite(source, docDir, oldAbs, newAbs);
        if (rewritten == source)
            return;
        affectedDocs.append(absPath);
        if (ed)
            tabEdits.append({ed, tabIndex, rewritten});
        else if (onDisk) {
            QFile f(absPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(rewritten.toUtf8());
                f.close();
            }
        }
    };

    for (const CorpusDocument &d : m_corpus.documents) {
        if (d.path.isEmpty())
            continue;
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(abs).absoluteFilePath() == QFileInfo(oldAbs).absoluteFilePath())
            continue;                                  // the renamed doc itself
        const int tabIdx = findTabByPath(abs);
        if (tabIdx >= 0)
            consider(abs, m_tabs[tabIdx].editor, tabIdx, false);
        else if (scopeAll)
            consider(abs, nullptr, -1, true);
    }

    if (affectedDocs.isEmpty())
        return;
    if (policy == QLatin1String("prompt")) {
        if (QMessageBox::question(this, tr("Update links"),
                 tr("Update links to %1 in %2 document(s)?")
                     .arg(QFileInfo(oldAbs).fileName()).arg(affectedDocs.size())) != QMessageBox::Yes)
            return;
    }
    for (const TabEdit &te : tabEdits) {
        te.editor->setPlainText(te.replaced);
        if (te.tabIndex >= 0) {
            setTabDirty(te.tabIndex, true);
            updateTabLabel(te.tabIndex);
        }
    }
    statusBar()->showMessage(tr("Updated links to %1 in %2 document(s)")
                                 .arg(QFileInfo(oldAbs).fileName()).arg(affectedDocs.size()), 4000);
}

void MainWindow::updateTabBarVisibility()
{
    m_tabBar->setVisible(m_tabs.size() > 1);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // A still-running validation grammar worker would keep a QThread alive
    // past the MainWindow it signals back to: stop and reap it first.
    stopValidationReport();

    QSettings s;

    bool autoSave = s.value(Preferences::AutoSaveOnExit, false).toBool();
    bool anyDirty = false;
    bool hasUntitledDirty = false;

    for (const TabInfo &info : m_tabs) {
        if (info.dirty) {
            anyDirty = true;
            if (info.filePath.isEmpty()) {
                hasUntitledDirty = true;
            }
        }
    }

    auto saveAllDirtyTabs = [this](QCloseEvent *event) {
        for (TabInfo &info : m_tabs) {
            if (!info.dirty) continue;
            if (info.filePath.isEmpty()) {
                QString file = saveAsDialogPath();
                if (file.isEmpty()) {
                    event->ignore();
                    return false;
                }
                info.filePath = file;
            }
            QFile file(info.filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox msgBox(this);
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.setWindowTitle("Save Failed");
                msgBox.setText(QString("Could not save \"%1\".\n%2")
                    .arg(info.filePath, file.errorString()));
                msgBox.setStandardButtons(QMessageBox::Ok);
                for (auto *btn : msgBox.buttons())
                    btn->setIcon(QIcon());
                msgBox.exec();
                event->ignore();
                return false;
            }
            file.write(info.editor->toPlainText().toUtf8());
            info.dirty = false;
        }
        return true;
    };

    if (anyDirty && (!autoSave || hasUntitledDirty)) {
        ClosePromptResult ret = promptUnsavedChanges(hasUntitledDirty);
        if (ret == ClosePromptResult::Cancel) {
            event->ignore();
            return;
        }
        if (ret == ClosePromptResult::Save && !saveAllDirtyTabs(event)) {
            return;
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

    QJsonObject corpus = serializeCorpus();

    if (corpus["files"].toArray().isEmpty()) {
        s.remove(Preferences::OnExitCorpusData);
    } else {
        s.setValue(Preferences::OnExitCorpusData, QString::fromUtf8(QJsonDocument(corpus).toJson(QJsonDocument::Compact)));
    }

    QMainWindow::closeEvent(event);
}

QString MainWindow::saveAsDialogPath()
{
    return QFileDialog::getSaveFileName(this, "Save File", QString(), kMdFilter);
}

MainWindow::ClosePromptResult MainWindow::promptUnsavedChanges(bool hasUntitledDirty)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setWindowTitle("Unsaved Changes");
    if (hasUntitledDirty) {
        msgBox.setText("There are unsaved changes in untitled tabs.\n"
            "Save all before closing?");
    } else {
        msgBox.setText("There are unsaved changes.\n"
            "Save all before closing?");
    }
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
    auto *discardBtn = msgBox.addButton(tr("&Discard"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);
    auto ret = msgBox.exec();

    if (ret == QMessageBox::Cancel)
        return ClosePromptResult::Cancel;
    if (ret == QMessageBox::Save)
        return ClosePromptResult::Save;
    return ClosePromptResult::Discard;
}

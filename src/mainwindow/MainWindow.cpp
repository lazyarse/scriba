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
#include "editor/Editor.h"
#include "preview/Preview.h"
#include "preview/PreviewBridge.h"
#include "corpus/Corpus.h"
#include "corpus/CorpusWatcher.h"
#include "css/CssConfig.h"
#include "css/CssLoader.h"
#include "css/CssUtils.h"
#include "preview/MarkdownParser.h"
#include "prefs/Preferences.h"
#include "prefs/PreferencesDialog.h"
#include "preview/PrintOptions.h"
#include "preview/Readability.h"
#include "StaticHelpers.h"
#include "spell/SpellChecker.h"
#include "spell/StoppardEngine.h"
#include "validation/ValidationReport.h"
#include <QAtomicInteger>
#include <QWebChannel>

static constexpr int kMsPerMinute = 60000;



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
            m_lastSyncLine = -1.0;
            syncPreviewScroll();
            // The anchor index is only ready after the DOMContentLoaded heavy
            // pass settles; re-assert a couple of times so a freshly loaded
            // page converges to the editor's position.
            QTimer::singleShot(300, this, &MainWindow::syncPreviewScroll);
            QTimer::singleShot(1500, this, &MainWindow::syncPreviewScroll);
            // A cross-document anchor jump (other.md#section) starts its retry
            // timer at click time, while the page is still being replaced, so
            // the fresh page must get a full retry budget once it has loaded.
            if (!m_pendingAnchor.isEmpty())
                scrollPreviewToAnchor(m_pendingAnchor);
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

MainWindow::~MainWindow()
{
    // Reap the background preview-render worker if a large document was ever
    // rendered (closeEvent() already does this on the normal exit path, but
    // tests and other destruction paths skip it).
    stopPreviewRenderWorker();
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
        stripButtonIcon(btn);
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
        // Render-affecting preferences (typography, hard breaks, ordered-list
        // numbering) changed on this page: drop the cached HTML so the next
        // updatePreview() re-renders instead of serving a stale render.
        for (auto &tab : m_tabs)
            tab.previewHtmlValid = false;
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
        updateTabBarVisibility();
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

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
        showMaximized();
    else
        showFullScreen();
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

void MainWindow::updateTabBarVisibility()
{
    const bool alwaysShow = QSettings().value(Preferences::TabBarAlwaysShow, false).toBool();
    m_tabBar->setVisible(alwaysShow || m_tabs.size() > 1);
}

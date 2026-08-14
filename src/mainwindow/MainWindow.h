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

#include <QMainWindow>
#include <QSplitter>
#include <QFileSystemWatcher>
#include <QToolButton>
#include <QLabel>
#include <QTimer>
#include <QTextDocument>
#include <QTabBar>
#include <QStackedWidget>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QJsonObject>
#include <QActionGroup>
#include <QUrl>
#include <QThread>

#include <functional>

#include "validation/ValidationReport.h"
#include "corpus/Corpus.h"
#include "preview/PrintOptions.h"

class Editor;
class Preview;
class PreviewBridge;
class MarkdownParser;
class CssConfig;
class CssLoader;
class ChartDialog;
class StockChartDialog;
class TableDialog;
class LogWindow;
class FindDialog;
class MermaidDialog;
class KatexHelperDialog;
class MchemHelperDialog;
class SpellCheckDialog;
class CorpusWatcher;
class CorpusFilesPanel;
class QDockWidget;
class PreviewRenderWorker;
class QWebChannel;
class QMenu;
class QMenuBar;

struct TabInfo {
    Editor *editor = nullptr;
    QString filePath;
    bool dirty = false;
    // Content hash of the last saved/loaded state: dirty is decided by
    // comparing the live document text against this, not by
    // QTextDocument::isModified() (whose undo-stack anchor desyncs when
    // a document signal-blocked format op, e.g. applyEditorLineHeight,
    // appends an undo command). See addTab()'s contentsChange handler.
    QByteArray savedHash;
    int lastUndoSteps = 0;          // undo/redo-stack baselines for detecting
    int lastRedoSteps = 0;          //   undo/redo in the contentsChange handler
    QString previewHtml;            // cached md->html render (see previewBlockRaw/StripScripts)
    bool previewHtmlValid = false;
    bool previewBlockRaw = false;
    bool previewStripScripts = false;
};

// Shared preamble of the three exporters: resolved editor/tab plus the
// markdown rendered to its export HTML body (BlockRawHtmlExport applied).
struct ExportPreamble {
    Editor *ed = nullptr;
    TabInfo *info = nullptr;
    QString html;
    bool ok = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, bool skipCorpusRestore = false);
    // Stops the background preview-render worker if it was started. Done here
    // as well as in closeEvent() so any destruction path (including tests)
    // reaps the worker thread and its orphaned worker object.
    ~MainWindow() override;
    void loadFile(const QString &filePath, bool forceReload = false);
    // Exposed for tests; the UI path is openCorpusAction().
    void openCorpusFile(const QString &path, bool skipPrompt = false);
    Editor *editor() const { return currentEditor(); }
    Preview *preview() const { return m_preview; }
    // Exposed for tests (print-layout scroll sync); UI-internal otherwise.
    QAction *showPageBreaksAction() const { return m_showPageBreaksAction; }

    // Enables the one-time "outdated custom stylesheets superseded" dialog.
    // Set from main(); off in unit tests so no modal dialog blocks them.
    static void setNotifyStaleCss(bool enabled) { s_notifyStaleCss = enabled; }

    // Appends a default suffix to a save-dialog result if the chosen path has
    // no extension at all; any existing suffix (e.g. ".txt") is respected as
    // typed. Exposed for tests.
    static QString ensureDefaultSuffix(const QString &path, const char *suffix);

    enum class ClosePromptResult { Save, Cancel, Discard };

    // Exposed for tests (recent-corpora menu); UI-internal otherwise.
    void addRecentCorpus(const QString &path);
    void updateRecentCorporaMenu();

private slots:
    void updatePreview();
    void onPreviewRenderReady(quint64 generation, const QString &html);
    void showPreferences();
    void onCssFileChanged();
    void onEditorScroll();
    void toggleFullscreen();
    void togglePreview();
    void onCorpusFileActivated(const QString &absPath);
    void insertCorpusResourceLink(const QString &absPath);
    void updateStats();
    void showChartBuilder();
    void showStockChartBuilder();
    void showAdvancedChartBuilder();
    void showLogWindow();
    void showKatexHelper();
    void showMchemHelper();
    void showSpellCheckDialog();
    void generateValidationReport();
    void handleChartEdit(const QString &kind, int line, int index, const QString &tex);
    void editChartBlock(Editor *ed, int blockNumber);

    void onFindNext();
    void onFindPrev();
    void onReplace(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);
    void onReplaceAll(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);
    void onTabChanged(int index);
    void onTabMoved(int from, int to);
    void onTabCloseRequested(int index);
    void closeCurrentTab();

public:
    void toggleFindDialog();
    void autoSave();
    void showTableInsert();

private:
    void setupUi();
    // Runs a dialog (returning the text to insert) and inserts it at the
    // cursor if non-empty. Shared by the chart/KaTeX/mhchem builders.
    void insertFromDialog(const std::function<QString()> &runDialog);
    void setupMenuBar();
    void buildFileMenu(QMenuBar *bar);
    void buildEditMenu(QMenuBar *bar);
    void buildViewMenu(QMenuBar *bar);
    void buildToolsMenu(QMenuBar *bar);
    void buildHelpMenu(QMenuBar *bar);
    void updatePreview(bool tabSwitch);
    // Decomposed from updatePreview: per-render CSS/environment computation,
    // base-URL resolution, and the two JS/DOM assembly helpers. The document
    // shell and its script payload live in resources/preview-shell.html and
    // resources/preview-script.js.
    struct PreviewEnviron {
        QString rawThemeCss;
        QString baseCss;
        QString previewCss;
        QString mermaidTheme;
        PrintOptions::Options printOpts;
        QString printLayoutCss;
        int printContentHpx = 0;
        bool cssChanged = false;
    };
    PreviewEnviron computePreviewCssAndEnviron();
    QUrl computePreviewBaseUrl(const TabInfo *info) const;
    QString buildPreviewShellHtml(int heavyRenderDelay, const QString &mermaidTheme,
                                  const QString &emojiMode, const QString &baseCss,
                                  const QString &previewCss, const QString &html,
                                  const QString &stripeInit, const QString &centerCss,
                                  const QString &splitCss, const QString &codeLangInit,
                                  const QString &renderCss) const;
    // Large-document preview rendering: the md→HTML pass runs on a shared
    // background worker (PreviewRenderWorker) so opening a big file never
    // blocks the UI. requestPreviewRender() snapshots the text, bumps
    // m_renderGeneration and dispatches the worker; onPreviewRenderReady()
    // validates the generation (dropping results superseded by a newer edit or
    // a tab switch), caches the HTML on the requesting tab and commits it to
    // the page. Small documents keep the synchronous inline path.
    struct PendingPreviewRender {
        quint64 gen = 0;
        Editor *editor = nullptr;
        bool blockRawHtml = true;
        bool stripScripts = true;
        bool tabSwitch = false;
    };
    void requestPreviewRender(Editor *ed, bool tabSwitch);
    void commitPreviewHtml(const QString &html, bool tabSwitch, const TabInfo *info);
    void stopPreviewRenderWorker();
    QString buildUpdateCallJavascript(const QString &html, bool cssChanged,
                                      const QString &previewCss, const QString &mermaidTheme,
                                      const QString &emojiMode, const QUrl &baseUrl,
                                      bool tabSwitch) const;
    void saveFile(const QString &filePath);
    void renameCurrentFile();
    void importHtmlFromFile();
    void importDocxFromFile();
    void importPdfFromFile();
    // Creates a new tab holding imported markdown and refreshes the preview.
    // Shared tail of the HTML/DOCX/PDF importers.
    void openImportedTab(const QString &markdown);
    void pasteAsMarkdown();
    void exportPdf();
    void exportDocx();
    void exportHtml();
    // Resolves the active editor/tab and renders its markdown to the export
    // HTML body. Shared preamble of exportPdf/exportDocx/exportHtml.
    ExportPreamble currentExportHtml();
    void syncPreviewScroll();
    double currentEditorTopSourceLine();
    void scrollPreviewToAnchor(const QString &anchor);
    void tryScrollPreviewToAnchor();
    void syncCssWatcher();
    void refreshPreviewCss();
    QString applyEditorSettings();
    QString applyEditorSettings(const QString &fontFamily, int fontSize, int padding);
    void applyEditorLineHeight(int lineHeight);
    void applyEditorCaretWidth(int caretWidth);
    void applyStyleSheetToAllEditors();
    void applyStyleSheetToAllEditors(const QString &fontFamily, int fontSize, int padding);
    void applyStripeSetting();
    void applyCodeLangSetting();
    void applyEditorContentWidth(Editor *editor);
    void applyPreviewSplitWidth();
    void setPreviewState(int state);
    void syncPreviewLayout();
    void showCenteredWarning(const QString &title, const QString &text, const QString &informative);
    void notifyStaleBaseCss();
    static bool s_notifyStaleCss;
    bool findText(const QString &text, bool backward, bool useRegex, bool caseSensitive);
    int countMatches(const QString &text, bool useRegex, bool caseSensitive) const;

    int addTab(const QString &filePath = QString());
    void removeTab(int index);
    bool removeEmptyUntitledTab();
    int findTabByPath(const QString &filePath) const;
    void connectTabEditor(int index);
    void disconnectTabEditor(int index);
    void updateTabLabel(int index);
    void updateWindowTitle();
    void setTabDirty(int index, bool dirty);
    // Re-baselines a tab against the on-disk content: clears the dirty marker
    // AND re-anchors the document's modified state (undo-stack position) so a
    // later undo back to this point clears the asterisk again.
    void setTabSaved(int index);
    void connectActiveEditor();
    void disconnectActiveEditor();
    Editor *currentEditor() const;
    TabInfo *activeTabInfo();
    void showSaveDiscardDialog(int index);

    QJsonObject serializeCorpus();
    QString tabTitleForEmbedded(int index) const;
    void refreshCorpusFromTabs();
    void restoreTabState(int idx, const CorpusDocument &d);
    void restoreCorpus(const QJsonObject &corpus);
    void saveCorpusAction();
    void saveCorpusAsAction();
    void newCorpusAction();
    void openCorpusAction();
    bool maybeDiscardCurrentTabs();
    bool saveAllDirtyTabs();
    // In "prompt" mode, walks the untitled corpus tabs and saves each one to a
    // real file via the Save-As dialog. Returns false if the user cancels so
    // the caller aborts the corpus save (or the window close). No-op (returns
    // true) in the default "embed" mode.
    bool promptSaveUnsavedCorpusDocs();
    void closeAllTabs();
    void applyCorpusDictionary();
    void startCorpusWatcher();
    void stopCorpusWatcher();
    // Shows/hides and re-roots the Corpus Files sidecar panel to match the
    // current corpus lifecycle. Idempotent; called from the watcher helpers.
    void updateCorpusFilesPanel();
    void handleExternalEdit(const QString &path);
    void handleExternalRename(const QString &from, const QString &to);
    void handleExternalDelete(const QString &path);
    void rewriteLinksForFile(const QString &oldAbs, const QString &newAbs);
    void openCorpusToc();
    void refreshCorpusToc();
    // Absolute path of the configured corpus TOC file, or "" if the corpus is
    // unsaved. Used for sidecar exclusion (never a corpus document).
    QString corpusTocPath() const;
    bool isCorpusTocPath(const QString &absPath) const;
    QHash<QString, QString> tocLinks() const;
    void exportCorpus();
    void renderDocumentHtml(const QString &markdown, const QString &baseDir,
                            bool omml, QString *body);
    // Untitled tabs in an open corpus resolve relative links against the
    // corpus root (matching the preview); clear on corpus close / new corpus.
    void applyUntitledLinkBaseDir();

    // Validation Report (Tools → Validation Report…): snapshots the open
    // documents, scans spelling/links/markdown synchronously, runs the
    // expensive whole-document grammar pass on a background thread, then
    // opens the assembled markdown in a new tab.
    void onValidationReportReady(const QVector<QList<GrammarChecker::Issue>> &grammarIssues);
    void openValidationReport();
    void stopValidationReport();

    QSplitter *m_splitter;
    Preview *m_preview;
    QWebChannel *m_webChannel = nullptr;
    PreviewBridge *m_previewBridge = nullptr;
    double m_lastSyncLine = -1.0;
    MarkdownParser *m_parser;
    CssConfig *m_cssConfig;
    CssLoader *m_cssLoader;
    QFileSystemWatcher *m_cssWatcher;
    CorpusWatcher *m_corpusWatcher = nullptr;
    CorpusFilesPanel *m_corpusFilesPanel = nullptr;
    QDockWidget *m_corpusFilesDock = nullptr;
    QTabBar *m_tabBar;
    QStackedWidget *m_editorStack;
    QVector<TabInfo> m_tabs;
    int m_connectedTabIndex = -1;
    bool m_previewInitialized = false;
    bool m_printLayoutMode = false;
    QString m_printLayoutFp;    // merged print CSS+options fingerprint (page geometry)
    // Background preview-render worker (large documents only). See
    // requestPreviewRender()/onPreviewRenderReady().
    QThread *m_renderThread = nullptr;
    PreviewRenderWorker *m_renderWorker = nullptr;
    quint64 m_renderGeneration = 0;
    PendingPreviewRender m_pendingRender;
    QString m_cachedPreviewCss;
    QString m_cachedFullCss;
    QString m_cachedPreviewBaseCss;
    QString m_cachedOverlayCss;
    bool m_chromeUpdateScheduled = false;
    QToolButton *m_fullscreenBtn = nullptr;
    QToolButton *m_previewBtn = nullptr;
    QLabel *m_statsLabel = nullptr;
    LogWindow *m_logWindow = nullptr;
    QTimer *m_autoSaveTimer = nullptr;
    int m_previewState = 1;
    QActionGroup *m_layoutActions = nullptr;
    FindDialog *m_findDialog = nullptr;
    SpellCheckDialog *m_spellCheckDlg = nullptr;
    QList<QAction *> m_insertActions;
    QAction *m_mermaidAction;
    QAction *m_wrapTextAction = nullptr;
    QAction *m_showPageBreaksAction = nullptr;
    QTimer *m_updateTimer = nullptr;
    QTimer *m_anchorTimer = nullptr;
    QString m_pendingAnchor;
    int m_anchorTries = 0;
    Corpus m_corpus;
    QMenu *m_recentCorpusMenu = nullptr;

    // Validation Report state. m_reportSources/m_reportDocs are snapshots made
    // on the UI thread before the grammar worker starts; the worker's results
    // are merged into m_reportDocs in onValidationReportReady(). m_reportTitles
    // keys report tab indices to their date-stamped title so updateTabLabel()
    // keeps them after edits.
    bool m_reportInFlight = false;
    QThread *m_reportThread = nullptr;
    QHash<int, QString> m_reportTitles;
    QVector<ValidationReport::DocumentSource> m_reportSources;
    QVector<ValidationReport::DocumentReport> m_reportDocs;
    ValidationReport::ValidationOptions m_reportOptions;

protected:
    void updateTabBarVisibility();
    void closeEvent(QCloseEvent *event) override;

    // Seam for tests: shows the "Unsaved Changes" prompt and returns the
    // chosen action. Overridable so tests can observe that the prompt call
    // was made without driving the real modal dialog.
    virtual ClosePromptResult promptUnsavedChanges(bool hasUntitledDirty);

    // Seam for tests: prompts for a save path for an untitled tab and returns
    // the chosen path, or an empty string if the user cancelled. Overridable
    // so tests can simulate cancel/success without the real file dialog.
    virtual QString saveAsDialogPath();
};


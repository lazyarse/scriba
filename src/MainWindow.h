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

#include "ValidationReport.h"
#include "Corpus.h"

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
class QWebChannel;
class QMenu;

struct TabInfo {
    Editor *editor = nullptr;
    QString filePath;
    bool dirty = false;
    QString previewHtml;            // cached md->html render (see previewBlockRaw/StripScripts)
    bool previewHtmlValid = false;
    bool previewBlockRaw = false;
    bool previewStripScripts = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, bool skipCorpusRestore = false);
    ~MainWindow() = default;
    void loadFile(const QString &filePath, bool forceReload = false);
    // Exposed for tests; the UI path is openCorpusAction().
    void openCorpusFile(const QString &path, bool skipPrompt = false);
    Editor *editor() const { return currentEditor(); }
    Preview *preview() const { return m_preview; }

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
    void showPreferences();
    void onCssFileChanged();
    void onEditorScroll();
    void toggleFullscreen();
    void togglePreview();
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
    void setupMenuBar();
    void updatePreview(bool tabSwitch);
    void saveFile(const QString &filePath);
    void renameCurrentFile();
    void importHtmlFromFile();
    void importDocxFromFile();
    void importPdfFromFile();
    void pasteAsMarkdown();
    void exportPdf();
    void exportDocx();
    void exportHtml();
    void syncPreviewScroll();
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
    int findTabByPath(const QString &filePath) const;
    void connectTabEditor(int index);
    void disconnectTabEditor(int index);
    void updateTabLabel(int index);
    void updateWindowTitle();
    void setTabDirty(int index, bool dirty);
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
    void openCorpusAction();
    bool maybeDiscardCurrentTabs();
    bool saveAllDirtyTabs();
    void closeAllTabs();
    void applyCorpusDictionary();
    void startCorpusWatcher();
    void stopCorpusWatcher();
    void handleExternalEdit(const QString &path);
    void handleExternalRename(const QString &from, const QString &to);
    void handleExternalDelete(const QString &path);
    void rewriteLinksForFile(const QString &oldAbs, const QString &newAbs);
    void viewTableOfContents();
    QString renderTocMarkdown() const;
    void exportCorpus();
    void renderDocumentHtml(const QString &markdown, const QString &baseDir,
                            bool omml, QString *body);
    void refreshOpenToc();
    void refreshPreviewForTocTab(int index, const QString &rootDir);

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
    MarkdownParser *m_parser;
    CssConfig *m_cssConfig;
    CssLoader *m_cssLoader;
    QFileSystemWatcher *m_cssWatcher;
    CorpusWatcher *m_corpusWatcher = nullptr;
    QTabBar *m_tabBar;
    QStackedWidget *m_editorStack;
    QVector<TabInfo> m_tabs;
    int m_connectedTabIndex = -1;
    bool m_previewInitialized = false;
    bool m_printLayoutMode = false;
    QString m_printLayoutFp;    // merged print CSS+options fingerprint (page geometry)
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
    // Table-of-Contents tabs: index -> fixed tab title. Like m_reportTitles,
    // these are generated, unbacked virtual documents excluded from the corpus.
    QHash<int, QString> m_tocTabs;
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


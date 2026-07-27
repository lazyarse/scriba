#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QFileSystemWatcher>
#include <QToolButton>
#include <QLabel>
#include <QTimer>
#include <QTextDocument>
#include <QTabWidget>
#include <QVector>
#include <QJsonObject>

class Editor;
class Preview;
class MarkdownParser;
class CssConfig;
class CssLoader;
class VegaLiteDialog;
class TableDialog;
class LogWindow;
class FindDialog;
class MermaidPieDialog;
class MermaidFlowchartDialog;
class MermaidSequenceDialog;
class MermaidGanttDialog;
class MermaidClassDialog;
class MermaidErDialog;
class MermaidStateDialog;
class MermaidMindmapDialog;
class MermaidTimelineDialog;
class MermaidJourneyDialog;
class MermaidQuadrantDialog;
class MermaidSankeyDialog;
class KatexHelperDialog;

struct TabInfo {
    Editor *editor = nullptr;
    QString filePath;
    bool dirty = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, bool skipSessionRestore = false);
    ~MainWindow() = default;
    void loadFile(const QString &filePath);
    Editor *editor() const { return currentEditor(); }
    Preview *preview() const { return m_preview; }

private slots:
    void updatePreview();
    void showPreferences();
    void onCssFileChanged();
    void onEditorScroll();
    void toggleFullscreen();
    void togglePreview();
    void updateStats();
    void showChartBuilder();
    void showLogWindow();
    void showKatexHelper();

    void onFindNext();
    void onFindPrev();
    void onReplace(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);
    void onReplaceAll(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void closeCurrentTab();

public:
    void toggleFindDialog();
    void autoSave();
    void showTableInsert();

private:
    void setupUi();
    void setupMenuBar();
    void saveFile(const QString &filePath);
    void exportPdf();
    void exportDocx();
    void syncPreviewScroll();
    void syncCssWatcher();
    void refreshPreviewCss();
    QString applyEditorSettings();
    QString applyEditorSettings(const QString &fontFamily, int fontSize, int padding);
    void applyEditorLineHeight(int lineHeight);
    void applyStripeSetting();
    void showCenteredWarning(const QString &title, const QString &text, const QString &informative);
    bool findText(const QString &text, bool backward, bool useRegex, bool caseSensitive);
    int countMatches(const QString &text, bool useRegex, bool caseSensitive) const;

    int addTab(const QString &filePath = QString());
    void removeTab(int index);
    int findTabByPath(const QString &filePath) const;
    void connectTabEditor(int index);
    void disconnectTabEditor(int index);
    void updateTabLabel(int index);
    void setTabDirty(int index, bool dirty);
    void connectActiveEditor();
    void disconnectActiveEditor();
    Editor *currentEditor() const;
    TabInfo *activeTabInfo();
    void showSaveDiscardDialog(int index);

    QJsonObject serializeSession() const;
    void restoreSession(const QJsonObject &session);
    void saveSessionAction();
    void saveSessionAsAction();
    void loadSessionAction();

    QSplitter *m_splitter;
    Preview *m_preview;
    MarkdownParser *m_parser;
    CssConfig *m_cssConfig;
    CssLoader *m_cssLoader;
    QFileSystemWatcher *m_cssWatcher;
    QTabWidget *m_tabWidget;
    QVector<TabInfo> m_tabs;
    int m_connectedTabIndex = -1;
    bool m_previewInitialized = false;
    QString m_cachedPreviewCss;
    QString m_cachedFullCss;
    QString m_cachedPreviewBaseCss;
    bool m_chromeUpdateScheduled = false;
    QToolButton *m_fullscreenBtn = nullptr;
    QToolButton *m_previewBtn = nullptr;
    QLabel *m_statsLabel = nullptr;
    LogWindow *m_logWindow = nullptr;
    QTimer *m_autoSaveTimer = nullptr;
    int m_previewState = 1;
    FindDialog *m_findDialog = nullptr;
    QList<QAction *> m_insertActions;
    QList<QAction *> m_mermaidActions;
    QTimer *m_updateTimer = nullptr;

protected:
    void updateTabBarVisibility();
    void closeEvent(QCloseEvent *event) override;
};


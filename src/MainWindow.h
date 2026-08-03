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
class MermaidDialog;
class KatexHelperDialog;
class MchemHelperDialog;

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
    void loadFile(const QString &filePath, bool forceReload = false);
    Editor *editor() const { return currentEditor(); }
    Preview *preview() const { return m_preview; }

    // Enables the one-time "outdated custom stylesheets superseded" dialog.
    // Set from main(); off in unit tests so no modal dialog blocks them.
    static void setNotifyStaleCss(bool enabled) { s_notifyStaleCss = enabled; }

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
    void showMchemHelper();

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
    void exportHtml();
    void syncPreviewScroll();
    void scrollPreviewToAnchor(const QString &anchor);
    void tryScrollPreviewToAnchor();
    void syncCssWatcher();
    void refreshPreviewCss();
    QString applyEditorSettings();
    QString applyEditorSettings(const QString &fontFamily, int fontSize, int padding);
    void applyEditorLineHeight(int lineHeight);
    void applyStyleSheetToAllEditors();
    void applyStyleSheetToAllEditors(const QString &fontFamily, int fontSize, int padding);
    void applyStripeSetting();
    void applyCodeLangSetting();
    void applyEditorContentWidth(Editor *editor);
    void applyPreviewSplitWidth();
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
    QTabBar *m_tabBar;
    QStackedWidget *m_editorStack;
    QVector<TabInfo> m_tabs;
    int m_connectedTabIndex = -1;
    bool m_previewInitialized = false;
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
    FindDialog *m_findDialog = nullptr;
    QList<QAction *> m_insertActions;
    QAction *m_mermaidAction;
    QTimer *m_updateTimer = nullptr;
    QTimer *m_anchorTimer = nullptr;
    QString m_pendingAnchor;
    int m_anchorTries = 0;

protected:
    void updateTabBarVisibility();
    void closeEvent(QCloseEvent *event) override;
};


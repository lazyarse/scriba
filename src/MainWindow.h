#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QFileSystemWatcher>
#include <QToolButton>
#include <QLabel>

class Editor;
class Preview;
class MarkdownParser;
class CssConfig;
class CssLoader;
class VegaLiteDialog;
class TableDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;
    void loadFile(const QString &filePath);
    Editor *editor() const { return m_editor; }
    Preview *preview() const { return m_preview; }

private slots:
    void updatePreview();
    void showPreferences();
    void showFindDialog();
    void onCssFileChanged();
    void onEditorScroll();
    void toggleFullscreen();
    void togglePreview();
    void updateStats();
    void showChartBuilder();
    void showTableInsert();

private:
    void setupUi();
    void setupMenuBar();
    void saveFile(const QString &filePath);
    void exportPdf();
    void syncPreviewScroll();
    void syncCssWatcher();
    void refreshPreviewCss();
    void applyStripeSetting();
    void loadSample();
    void showCenteredWarning(const QString &title, const QString &text, const QString &informative);
    QSplitter *m_splitter;
    Editor *m_editor;
    Preview *m_preview;
    MarkdownParser *m_parser;
    CssConfig *m_cssConfig;
    CssLoader *m_cssLoader;
    QFileSystemWatcher *m_cssWatcher;
    QString m_currentFile;
    bool m_previewInitialized = false;
    QString m_cachedPreviewCss;
    QString m_cachedFullCss;
    QString m_cachedPreviewBaseCss;
    bool m_chromeUpdateScheduled = false;
    QToolButton *m_fullscreenBtn = nullptr;
    QToolButton *m_previewBtn = nullptr;
    QLabel *m_statsLabel = nullptr;
    int m_previewState = 1;

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif

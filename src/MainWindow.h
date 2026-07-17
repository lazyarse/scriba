#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QFileSystemWatcher>

class Editor;
class Preview;
class MarkdownParser;
class CssManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;
    void loadFile(const QString &filePath);

private slots:
    void updatePreview();
    void showPreferences();
    void showFindDialog();
    void onCssFileChanged();
    void onEditorScroll();

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
    CssManager *m_cssManager;
    QFileSystemWatcher *m_cssWatcher;
    QString m_currentFile;
    bool m_previewInitialized = false;
    QString m_cachedPreviewCss;
    QString m_cachedFullCss;
    QString m_cachedPreviewBaseCss;
    bool m_chromeUpdateScheduled = false;
};

#endif

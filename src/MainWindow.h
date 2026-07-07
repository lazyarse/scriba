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
    void syncPreviewScroll();
    void syncCssWatcher();

    QSplitter *m_splitter;
    Editor *m_editor;
    Preview *m_preview;
    MarkdownParser *m_parser;
    CssManager *m_cssManager;
    QFileSystemWatcher *m_cssWatcher;
    QString m_currentFile;
    bool m_previewInitialized = false;
};

#endif

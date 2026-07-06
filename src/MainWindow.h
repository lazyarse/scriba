#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>

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

private slots:
    void updatePreview();
    void showPreferences();

private:
    void setupUi();
    void setupMenuBar();
    void loadFile(const QString &filePath);
    void saveFile(const QString &filePath);

    QSplitter *m_splitter;
    Editor *m_editor;
    Preview *m_preview;
    MarkdownParser *m_parser;
    CssManager *m_cssManager;
    QString m_currentFile;
};

#endif

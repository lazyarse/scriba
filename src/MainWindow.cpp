#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "MarkdownParser.h"
#include "CssManager.h"
#include "PreferencesDialog.h"
#include "Preferences.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QStatusBar>
#include <QSettings>
#include <QColor>
#include <QDir>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser())
    , m_cssManager(new CssManager())
    , m_cssWatcher(new QFileSystemWatcher(this))
{
    setupUi();
    setupMenuBar();
    setWindowTitle("Scriba");
    showFullScreen();

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(80);
    connect(timer, &QTimer::timeout, this, &MainWindow::updatePreview);
    connect(m_editor, &QPlainTextEdit::textChanged, timer, qOverload<>(&QTimer::start));

    connect(m_cssWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onCssFileChanged);
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::onEditorScroll);
    syncCssWatcher();

    /* Re-open last file if enabled */
    QSettings settings;
    if (settings.value(Preferences::ReopenLastFile, true).toBool()) {
        QString lastFile = settings.value(Preferences::LastOpenedFile).toString();
        if (!lastFile.isEmpty()) {
            loadFile(lastFile);
        }
    }
}

void MainWindow::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    m_editor = new Editor();
    m_preview = new Preview();

    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setSizes({600, 600});
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *newAction = fileMenu->addAction("&New");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        m_editor->clear();
        m_currentFile.clear();
        setWindowTitle("Scriba - Untitled");
    });

    QAction *openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Open Markdown File", QString(), "Markdown Files (*.md *.markdown *.txt);;All Files (*)");
        if (!file.isEmpty()) loadFile(file);
    });

    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        if (m_currentFile.isEmpty()) {
            QString file = QFileDialog::getSaveFileName(this, "Save Markdown File", QString(), "Markdown Files (*.md);;All Files (*)");
            if (!file.isEmpty()) saveFile(file);
        } else {
            saveFile(m_currentFile);
        }
    });

    QAction *saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "Save Markdown File As", QString(), "Markdown Files (*.md);;All Files (*)");
        if (!file.isEmpty()) saveFile(file);
    });

    fileMenu->addSeparator();

    QAction *prefsAction = fileMenu->addAction("&Preferences...");
    connect(prefsAction, &QAction::triggered, this, &MainWindow::showPreferences);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::updatePreview()
{
    QString markdown = m_editor->toPlainText();
    QString html = m_parser->toHtml(markdown);
    QString css = m_cssManager->combinedCss();

    QString baseTag;
    QString docPath = m_preview->documentPath();
    if (!docPath.isEmpty()) {
        QUrl baseUrl = QUrl::fromLocalFile(QFileInfo(docPath).absolutePath() + "/");
        baseTag = QString("<base href=\"%1\">").arg(baseUrl.toString());
    }

    if (!m_previewInitialized) {
        QString fullHtml = QString(
            "<html><head>%1<style id=\"user-css\">%2</style></head><body>%3</body></html>"
        ).arg(baseTag, css, html);
        m_preview->setHtml(fullHtml);
        m_previewInitialized = true;
    } else {
        QString escapedCss = css;
        escapedCss.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n");
        QString escapedHtml = html;
        escapedHtml.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n");

        QString js = QString(
            "var _sy = window.scrollY;"
            "document.getElementById('user-css').textContent = '%1';"
            "document.body.innerHTML = '%2';"
            "window.scrollTo(0, _sy);"
        ).arg(escapedCss, escapedHtml);
        m_preview->page()->runJavaScript(js);
    }
}

void MainWindow::showPreferences()
{
    PreferencesDialog dlg(m_cssManager, m_editor, this);
    if (dlg.exec() == QDialog::Accepted) {
        QSettings settings;
        QString family = settings.value(Preferences::EditorFont, "Monospace").toString();
        int size = settings.value(Preferences::EditorFontSize, 14).toInt();
        QColor color(settings.value(Preferences::EditorFontColor, "#333333").toString());
        m_editor->applyFontSettings(family, size, color);
        m_cssManager->invalidateCache();
        syncCssWatcher();
        updatePreview();
    }
}

void MainWindow::syncCssWatcher()
{
    QStringList dirs = m_cssWatcher->directories();
    if (!dirs.isEmpty()) {
        m_cssWatcher->removePaths(dirs);
    }

    QStringList files = m_cssWatcher->files();
    if (!files.isEmpty()) {
        m_cssWatcher->removePaths(files);
    }

    QString cssDir = m_cssManager->cssDirectory();
    if (!cssDir.isEmpty()) {
        m_cssWatcher->addPath(cssDir);
    }

    for (const QString &file : m_cssManager->enabledFiles()) {
        QString path = QDir(cssDir).filePath(file);
        if (QFile::exists(path)) {
            m_cssWatcher->addPath(path);
        }
    }
}

void MainWindow::onCssFileChanged()
{
    m_cssManager->invalidateCache();
    updatePreview();
    syncCssWatcher();
}

void MainWindow::onEditorScroll()
{
    if (m_syncingScroll || !m_previewInitialized) return;

    QScrollBar *sb = m_editor->verticalScrollBar();
    double pct = (sb->maximum() > 0) ? sb->value() / double(sb->maximum()) : 0.0;

    QString js = QString(
        "var h = document.documentElement.scrollHeight - window.innerHeight;"
        "window.scrollTo(0, %1 * h);"
    ).arg(pct, 0, 'f', 4);

    m_syncingScroll = true;
    m_preview->page()->runJavaScript(js, [this](const QVariant &) { m_syncingScroll = false; });
}

void MainWindow::loadFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filePath);
        return;
    }

    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    m_currentFile = filePath;
    setWindowTitle("Scriba - " + QFileInfo(filePath).fileName());
    m_preview->setDocumentPath(filePath);
    m_previewInitialized = false;

    QSettings settings;
    settings.setValue(Preferences::LastOpenedFile, filePath);
}

void MainWindow::saveFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file: " + filePath);
        return;
    }

    file.write(m_editor->toPlainText().toUtf8());
    m_currentFile = filePath;
    setWindowTitle("Scriba - " + QFileInfo(filePath).fileName());
    m_preview->setDocumentPath(filePath);
    statusBar()->showMessage("Saved", 2000);

    QSettings settings;
    settings.setValue(Preferences::LastOpenedFile, filePath);
}

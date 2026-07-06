# Scriba — Markdown Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a split-screen Markdown editor with live preview, CSS styling, and file management.

**Architecture:** Single-window Qt6 app with QSplitter layout. Left: QPlainTextEdit editor. Right: QTextBrowser preview. Markdown parsed via md4c (vendored C library). CSS managed via Qt resources + user files. QSettings for persistence.

**Tech Stack:** C++17, Qt6 (Core, Gui, Widgets), CMake 3.16+, md4c (vendored)

---

## File Structure

```
scriba/
├── CMakeLists.txt                    # Build configuration
├── src/
│   ├── main.cpp                      # App entry point
│   ├── MainWindow.h                  # Main window header
│   ├── MainWindow.cpp                # Main window implementation
│   ├── Editor.h                      # Editor widget header
│   ├── Editor.cpp                    # Editor widget implementation
│   ├── Preview.h                     # Preview widget header
│   ├── Preview.cpp                   # Preview widget implementation
│   ├── MarkdownParser.h              # Markdown parser header
│   ├── MarkdownParser.cpp            # Markdown parser implementation
│   ├── CssManager.h                  # CSS manager header
│   ├── CssManager.cpp                # CSS manager implementation
│   ├── PreferencesDialog.h           # Preferences dialog header
│   └── PreferencesDialog.cpp         # Preferences dialog implementation
├── resources/
│   ├── default.css                   # Default stylesheet
│   └── scriba.qrc                    # Qt resource file
└── vendor/
    └── md4c/
        ├── md4c.h                    # md4c header
        └── md4c.c                    # md4c source
```

---

### Task 1: Project Scaffolding

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `vendor/md4c/md4c.h`
- Create: `vendor/md4c/md4c.c`
- Create: `resources/scriba.qrc`
- Create: `resources/default.css`

- [ ] **Step 1: Create CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(scriba VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(scriba
    src/main.cpp
    src/MainWindow.cpp
    src/Editor.cpp
    src/Preview.cpp
    src/MarkdownParser.cpp
    src/CssManager.cpp
    src/PreferencesDialog.cpp
    vendor/md4c/md4c.c
    resources/scriba.qrc
)

target_include_directories(scriba PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/md4c
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scriba PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
```

- [ ] **Step 2: Download md4c**

Run: `cd vendor/md4c && curl -L -o md4c.h https://raw.githubusercontent.com/mity/md4c/master/src/md4c.h && curl -L -o md4c.c https://raw.githubusercontent.com/mity/md4c/master/src/md4c.c`

If network unavailable, create stub files (see Step 2b).

- [ ] **Step 2b: Create md4c stubs (if needed)**

Create minimal `vendor/md4c/md4c.h`:

```c
#ifndef MD4C_H
#define MD4C_H

#include <stddef.h>

typedef struct {
    unsigned parser_version;
    unsigned long flags;
} MD_PARSER;

#define MD_FLAG_TABLES (1 << 0)
#define MD_FLAG_STRIKETHROUGH (1 << 1)
#define MD_FLAG_TASKLISTS (1 << 2)
#define MD_FLAG_PERMISSIVEAUTOLINKS (1 << 3)
#define MD_FLAG_PERMISSIVEURLAUTOLINKS (1 << 4)

#define MD_HTML_FLAG XHTML (1 << 0)
#define MD_HTML_FLAG_SKIPHTML (1 << 1)

int md_parse(const char* text, size_t size, const MD_PARSER* parser, void* userdata);

int md_markdown_to_html(const char* text, size_t size, unsigned long parser_flags, unsigned long render_flags, void (*process_output)(const char*, size_t, void*), void* userdata);

#endif
```

Create minimal `vendor/md4c/md4c.c`:

```c
#include "md4c.h"
#include <string.h>

/* Minimal stub - replace with real md4c for production */
int md_markdown_to_html(const char* text, size_t size, unsigned long parser_flags, unsigned long render_flags, void (*process_output)(const char*, size_t, void*), void* userdata) {
    /* Output as-is wrapped in basic HTML */
    process_output("<p>", 3, userdata);
    process_output(text, size, userdata);
    process_output("</p>", 4, userdata);
    return 0;
}
```

- [ ] **Step 3: Create Qt resource file**

Create `resources/scriba.qrc`:

```xml
<RCC>
    <qresource prefix="/">
        <file>default.css</file>
    </qresource>
</RCC>
```

- [ ] **Step 4: Create default.css**

Create `resources/default.css`:

```css
body {
    font-family: Georgia, 'Times New Roman', serif;
    font-size: 16px;
    line-height: 1.6;
    max-width: 800px;
    margin: 0 auto;
    padding: 20px;
    color: #333;
}

h1, h2, h3, h4, h5, h6 {
    margin-top: 1.5em;
    margin-bottom: 0.5em;
    font-weight: bold;
}

h1 { font-size: 2em; border-bottom: 1px solid #eee; padding-bottom: 0.3em; }
h2 { font-size: 1.5em; border-bottom: 1px solid #eee; padding-bottom: 0.3em; }
h3 { font-size: 1.25em; }

p {
    margin: 1em 0;
}

code {
    background-color: #f4f4f4;
    padding: 0.2em 0.4em;
    border-radius: 3px;
    font-family: 'Consolas', 'Monaco', monospace;
    font-size: 0.9em;
}

pre {
    background-color: #f4f4f4;
    padding: 16px;
    border-radius: 6px;
    overflow-x: auto;
    line-height: 1.45;
}

pre code {
    background: none;
    padding: 0;
}

blockquote {
    margin: 1em 0;
    padding: 0.5em 1em;
    border-left: 4px solid #ddd;
    color: #666;
    background-color: #f9f9f9;
}

table {
    border-collapse: collapse;
    width: 100%;
    margin: 1em 0;
}

th, td {
    border: 1px solid #ddd;
    padding: 8px 12px;
    text-align: left;
}

th {
    background-color: #f4f4f4;
    font-weight: bold;
}

tr:nth-child(even) {
    background-color: #f9f9f9;
}

img {
    max-width: 100%;
    height: auto;
}

a {
    color: #0366d6;
    text-decoration: none;
}

a:hover {
    text-decoration: underline;
}

hr {
    border: none;
    border-top: 1px solid #eee;
    margin: 2em 0;
}

ul, ol {
    padding-left: 2em;
}

li {
    margin: 0.5em 0;
}

input[type="checkbox"] {
    margin-right: 0.5em;
}
```

- [ ] **Step 5: Create main.cpp**

Create `src/main.cpp`:

```cpp
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Scriba");
    app.setOrganizationName("Scriba");
    app.setApplicationVersion("1.0.0");

    MainWindow window;
    window.show();

    return app.exec();
}
```

- [ ] **Step 6: Create stub headers and source files**

Create minimal header/source pairs for each component (empty classes):

`src/MainWindow.h`:
```cpp
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() = default;

private:
    void setupUi();
    void setupMenuBar();
};

#endif
```

`src/MainWindow.cpp`:
```cpp
#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenuBar();
    setWindowTitle("Scriba");
    resize(1200, 800);
}

void MainWindow::setupUi() {}

void MainWindow::setupMenuBar() {}
```

`src/Editor.h`:
```cpp
#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
};

#endif
```

`src/Editor.cpp`:
```cpp
#include "Editor.h"

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setPlaceholderText("Start writing markdown...");
}
```

`src/Preview.h`:
```cpp
#ifndef PREVIEW_H
#define PREVIEW_H

#include <QTextBrowser>

class Preview : public QTextBrowser
{
    Q_OBJECT

public:
    explicit Preview(QWidget *parent = nullptr);
    void setHtmlContent(const QString &html);
};

#endif
```

`src/Preview.cpp`:
```cpp
#include "Preview.h"

Preview::Preview(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
}

void Preview::setHtmlContent(const QString &html)
{
    setHtml(html);
}
```

`src/MarkdownParser.h`:
```cpp
#ifndef MARKDOWNPARSER_H
#define MARKDOWNPARSER_H

#include <QString>

class MarkdownParser
{
public:
    static QString toHtml(const QString &markdown);
};

#endif
```

`src/MarkdownParser.cpp`:
```cpp
#include "MarkdownParser.h"
#include <md4c.h>
#include <string.h>

struct OutputBuffer {
    QString result;
};

static void processOutput(const char *data, size_t size, void *userdata)
{
    OutputBuffer *buffer = static_cast<OutputBuffer*>(userdata);
    buffer->result.append(QString::fromUtf8(data, static_cast<int>(size)));
}

QString MarkdownParser::toHtml(const QString &markdown)
{
    QByteArray utf8 = markdown.toUtf8();
    OutputBuffer buffer;

    unsigned long parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEURLAUTOLINKS;
    unsigned long renderFlags = 0;

    md_markdown_to_html(utf8.constData(), utf8.size(), parserFlags, renderFlags, processOutput, &buffer);

    return buffer.result;
}
```

`src/CssManager.h`:
```cpp
#ifndef CSSMANAGER_H
#define CSSMANAGER_H

#include <QString>
#include <QStringList>

class CssManager
{
public:
    CssManager();

    QString combinedCss() const;
    void setCssDirectory(const QString &directory);
    QString cssDirectory() const;
    void setEnabledFiles(const QStringList &files);
    QStringList enabledFiles() const;
    QStringList availableFiles() const;

private:
    QString loadCssFile(const QString &filePath) const;
    QString m_cssDirectory;
    QStringList m_enabledFiles;
};

#endif
```

`src/CssManager.cpp`:
```cpp
#include "CssManager.h"
#include <QFile>
#include <QDir>
#include <QSettings>

CssManager::CssManager()
{
    QSettings settings;
    m_cssDirectory = settings.value("cssDirectory", "").toString();
    m_enabledFiles = settings.value("enabledCssFiles", QStringList{"default.css"}).toStringList();
}

QString CssManager::combinedCss() const
{
    QString css;

    /* Always load default from resources */
    QFile defaultFile(":/default.css");
    if (defaultFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        css += QString::fromUtf8(defaultFile.readAll());
        css += "\n";
    }

    /* Load user CSS files */
    QDir dir(m_cssDirectory);
    for (const QString &file : m_enabledFiles) {
        if (file == "default.css") continue;
        QString path = dir.filePath(file);
        css += loadCssFile(path);
        css += "\n";
    }

    return css;
}

void CssManager::setCssDirectory(const QString &directory)
{
    m_cssDirectory = directory;
    QSettings settings;
    settings.setValue("cssDirectory", directory);
}

QString CssManager::cssDirectory() const
{
    return m_cssDirectory;
}

void CssManager::setEnabledFiles(const QStringList &files)
{
    m_enabledFiles = files;
    QSettings settings;
    settings.setValue("enabledCssFiles", files);
}

QStringList CssManager::enabledFiles() const
{
    return m_enabledFiles;
}

QStringList CssManager::availableFiles() const
{
    QDir dir(m_cssDirectory);
    return dir.entryList(QStringList() << "*.css", QDir::Files, QDir::Name);
}

QString CssManager::loadCssFile(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.readAll());
    }
    return "";
}
```

`src/PreferencesDialog.h`:
```cpp
#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include "CssManager.h"

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(CssManager *cssManager, QWidget *parent = nullptr);

private slots:
    void selectDirectory();
    void addCssFile();
    void removeCssFile();
    void updatePreview();

private:
    void setupUi();

    CssManager *m_cssManager;
    QListWidget *m_listWidget;
    QLabel *m_previewLabel;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
};

#endif
```

`src/PreferencesDialog.cpp`:
```cpp
#include "PreferencesDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

PreferencesDialog::PreferencesDialog(CssManager *cssManager, QWidget *parent)
    : QDialog(parent)
    , m_cssManager(cssManager)
{
    setupUi();
    setWindowTitle("Preferences");
    resize(500, 400);
}

void PreferencesDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    /* Directory selection */
    QHBoxLayout *dirLayout = new QHBoxLayout();
    QLabel *dirLabel = new QLabel("CSS Directory:");
    QLineEdit *dirEdit = new QLineEdit(m_cssManager->cssDirectory());
    QPushButton *browseBtn = new QPushButton("Browse...");
    dirLayout->addWidget(dirLabel);
    dirLayout->addWidget(dirEdit);
    dirLayout->addWidget(browseBtn);
    mainLayout->addLayout(dirLayout);

    connect(browseBtn, &QPushButton::clicked, this, &PreferencesDialog::selectDirectory);

    /* File list */
    QHBoxLayout *listLayout = new QHBoxLayout();
    m_listWidget = new QListWidget();
    m_addButton = new QPushButton("Add");
    m_removeButton = new QPushButton("Remove");

    QVBoxLayout *btnLayout = new QVBoxLayout();
    btnLayout->addWidget(m_addButton);
    btnLayout->addWidget(m_removeButton);
    btnLayout->addStretch();

    listLayout->addWidget(m_listWidget);
    listLayout->addLayout(btnLayout);
    mainLayout->addLayout(listLayout);

    /* Preview */
    m_previewLabel = new QLabel("CSS Preview:");
    mainLayout->addWidget(m_previewLabel);

    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addCssFile);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeCssFile);

    /* Buttons */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    /* Load current files */
    for (const QString &file : m_cssManager->enabledFiles()) {
        m_listWidget->addItem(file);
    }
}

void PreferencesDialog::selectDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select CSS Directory", m_cssManager->cssDirectory());
    if (!dir.isEmpty()) {
        m_cssManager->setCssDirectory(dir);
        /* Update available files */
        m_listWidget->clear();
        for (const QString &file : m_cssManager->enabledFiles()) {
            m_listWidget->addItem(file);
        }
    }
}

void PreferencesDialog::addCssFile()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select CSS Files", m_cssManager->cssDirectory(), "CSS Files (*.css)");
    QStringList enabled = m_cssManager->enabledFiles();
    for (const QString &file : files) {
        QFileInfo info(file);
        if (!enabled.contains(info.fileName())) {
            enabled.append(info.fileName());
        }
    }
    m_cssManager->setEnabledFiles(enabled);
    m_listWidget->clear();
    for (const QString &f : enabled) {
        m_listWidget->addItem(f);
    }
}

void PreferencesDialog::removeCssFile()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QStringList enabled = m_cssManager->enabledFiles();
    enabled.removeAll(item->text());
    m_cssManager->setEnabledFiles(enabled);
    delete m_listWidget->takeRow(m_listWidget->row(item));
}

void PreferencesDialog::updatePreview()
{
    m_previewLabel->setText(m_cssManager->combinedCss().left(500));
}
```

- [ ] **Step 7: Build and verify**

Run: `cd /home/tpa/code/scriba && mkdir -p build && cd build && cmake .. && make`

Expected: Build succeeds (may have warnings, but no errors).

---

### Task 2: MainWindow with Splitter Layout

**Files:**
- Modify: `src/MainWindow.h`
- Modify: `src/MainWindow.cpp`

- [ ] **Step 1: Update MainWindow.h**

```cpp
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
```

- [ ] **Step 2: Update MainWindow.cpp**

```cpp
#include "MainWindow.h"
#include "Editor.h"
#include "Preview.h"
#include "MarkdownParser.h"
#include "CssManager.h"
#include "PreferencesDialog.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser(this))
    , m_cssManager(new CssManager())
{
    setupUi();
    setupMenuBar();
    setWindowTitle("Scriba");
    resize(1200, 800);

    /* Debounce preview updates */
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(150);
    connect(timer, &QTimer::timeout, this, &MainWindow::updatePreview);
    connect(m_editor, &QPlainTextEdit::textChanged, timer, qOverload<>(&QTimer::start));
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

    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu("&Edit");
    QAction *prefsAction = editMenu->addAction("&Preferences...");
    connect(prefsAction, &QAction::triggered, this, &MainWindow::showPreferences);
}

void MainWindow::updatePreview()
{
    QString markdown = m_editor->toPlainText();
    QString html = m_parser->toHtml(markdown);
    QString css = m_cssManager->combinedCss();

    QString fullHtml = QString(
        "<html><head><style>%1</style></head><body>%2</body></html>"
    ).arg(css, html);

    m_preview->setHtmlContent(fullHtml);
}

void MainWindow::showPreferences()
{
    PreferencesDialog dlg(m_cssManager, this);
    if (dlg.exec() == QDialog::Accepted) {
        updatePreview();
    }
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
    statusBar()->showMessage("Saved", 2000);
}
```

- [ ] **Step 3: Fix MainWindow constructor**

Update the constructor to not pass `this` to MarkdownParser (it's not a QObject):

```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new MarkdownParser())
    , m_cssManager(new CssManager())
```

And update the destructor or member to handle cleanup:

```cpp
private:
    // ...
    MarkdownParser *m_parser;  // Plain pointer, not owned
```

- [ ] **Step 4: Build and verify**

Run: `cd /home/tpa/code/scriba/build && cmake .. && make`

Expected: Build succeeds.

- [ ] **Step 5: Test manually**

Run: `./scriba`

Expected: Window opens with split layout, text on left, preview on right. Typing markdown updates preview.

---

### Task 3: Editor Improvements

**Files:**
- Modify: `src/Editor.h`
- Modify: `src/Editor.cpp`

- [ ] **Step 1: Add line number gutter (optional)**

Update `Editor.h`:

```cpp
#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    int lineNumberAreaWidth();
};

#endif
```

Update `Editor.cpp`:

```cpp
#include "Editor.h"
#include <QKeyEvent>

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    /* Handle tab for indentation */
    if (event->key() == Qt::Key_Tab) {
        insertPlainText("    ");
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void Editor::updateLineNumberArea(const QRect &rect, int dy)
{
    Q_UNUSED(rect)
    Q_UNUSED(dy)
}

int Editor::lineNumberAreaWidth()
{
    return 0; /* Placeholder for future line numbers */
}
```

- [ ] **Step 2: Build and verify**

Run: `cd /home/tpa/code/scriba/build && cmake .. && make`

Expected: Build succeeds.

---

### Task 4: Preview Image Resolution

**Files:**
- Modify: `src/Preview.h`
- Modify: `src/Preview.cpp`

- [ ] **Step 1: Add search path support**

Update `Preview.h`:

```cpp
#ifndef PREVIEW_H
#define PREVIEW_H

#include <QTextBrowser>

class Preview : public QTextBrowser
{
    Q_OBJECT

public:
    explicit Preview(QWidget *parent = nullptr);
    void setHtmlContent(const QString &html);
    void setDocumentPath(const QString &path);
};

#endif
```

Update `Preview.cpp`:

```cpp
#include "Preview.h"
#include <QFileInfo>

Preview::Preview(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
}

void Preview::setHtmlContent(const QString &html)
{
    setHtml(html);
}

void Preview::setDocumentPath(const QString &path)
{
    if (!path.isEmpty()) {
        setSearchPaths(QStringList() << QFileInfo(path).absolutePath());
    }
}
```

- [ ] **Step 2: Update MainWindow to set document path**

In `MainWindow::loadFile()`, add after loading file:

```cpp
m_preview->setDocumentPath(filePath);
```

In `MainWindow::saveFile()`, add after saving:

```cpp
m_preview->setDocumentPath(filePath);
```

- [ ] **Step 3: Build and verify**

Run: `cd /home/tpa/code/scriba/build && cmake .. && make`

Expected: Build succeeds. Test with markdown containing `![](image.png)`.

---

### Task 5: Final Integration

**Files:**
- Verify all files are complete

- [ ] **Step 1: Run full build**

Run: `cd /home/tpa/code/scriba && rm -rf build && mkdir build && cd build && cmake .. && make`

Expected: Clean build, no errors.

- [ ] **Step 2: Test all features**

1. Open app → Window shows split layout
2. Type markdown → Preview updates live
3. Type table → Table renders correctly
4. Type `![alt](image.png)` → Image shows if file exists
5. File > Open → Load .md file
6. File > Save → Save .md file
7. Edit > Preferences → Dialog opens
8. Preferences > Browse → Select CSS directory
9. Preferences > Add → Add CSS file
10. Preferences > Remove → Remove CSS file

- [ ] **Step 3: Create README.md**

Create project README with build instructions and usage.

---

## Notes

- The md4c stub in Task 1 is a minimal placeholder. For production, replace with the real md4c library from https://github.com/mity/md4c
- QTextBrowser has limitations for complex HTML/CSS. For full CSS support, consider migrating Preview to QWebEngineView in a future iteration
- Line numbers and syntax highlighting are marked as future enhancements

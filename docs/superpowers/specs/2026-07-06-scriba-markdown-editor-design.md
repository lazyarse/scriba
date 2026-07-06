# Scriba — Markdown Editor Design

## Overview

A simple split-screen Markdown editor built with C++ and Qt6. Left pane: plain text editor. Right pane: live-rendered preview styled via CSS.

## Features

- Split-screen layout: editor (left) | preview (right)
- Live preview on text change
- Full CommonMark + GFM support (tables, strikethrough, task lists) via md4c
- Image rendering: local file paths and URLs in `![](path)` syntax
- CSS-based preview styling (default + user custom CSS)
- File menu: New, Open, Save, Save As, Quit
- Preferences: manage CSS directory and custom CSS files

## Architecture

### Components

#### MainWindow (QMainWindow)
- QSplitter containing Editor (left) and Preview (right)
- Menu bar: File, Edit
- File menu actions: New, Open, Save, Save As, Quit
- Edit menu actions: Preferences
- Connects Editor::textChanged → updatePreview()
- updatePreview() calls MarkdownParser::toHtml(), gets CSS from CssManager, injects into Preview

#### Editor (QPlainTextEdit)
- Plain text editing
- Emits textChanged signal on content change
- Optional: line numbers, syntax highlighting (future)

#### Preview (QTextBrowser)
- Renders HTML content
- Resolves local images via setSearchPaths() to the document's directory
- Opens external URL links via setOpenExternalLinks(true)
- Accepts HTML with embedded <style> block

#### MarkdownParser
- Wraps md4c C library
- Single method: QString toHtml(const QString& markdown)
- Handles GFM extensions: tables, strikethrough, task lists, autolinks

#### CssManager
- Loads default.css from Qt resources
- Loads user-selected CSS files from user's CSS directory
- Combines all CSS into single stylesheet string
- Persistence via QSettings:
  - cssDirectory (QString)
  - enabledCssFiles (QStringList)

#### PreferencesDialog (QDialog)
- CSS directory selector (QFileDialog)
- List widget showing available CSS files in directory
- Add/Remove buttons for CSS files
- Checkbox to enable/disable each CSS file
- Preview of combined CSS output

### Data Flow

```
User types in Editor
  → Editor emits textChanged
  → MainWindow::updatePreview() called (debounced ~150ms)
  → MarkdownParser::toHtml(text) → HTML string
  → CssManager::combinedCss() → CSS string
  → Preview receives: "<html><head><style>CSS</style></head><body>HTML</body></html>"
  → Preview renders
```

### File I/O

- Open: QFile → readAll → Editor::setPlainText
- Save: Editor::toPlainText → QFile::write
- Format: plain UTF-8 text (.md)
- Unsaved changes tracked via document()->isModified()

### Configuration (QSettings)

```ini
[General]
cssDirectory=/home/user/.config/scriba/styles
enabledCssFiles=default.css;custom.css
windowGeometry=@ByteArray(...)
splitterState=@ByteArray(...)
```

## Build System

- CMake 3.16+
- Qt6 (Core, Gui, Widgets)
- md4c: vendored in vendor/md4c/ (single md4c.h + md4c.c, MIT license)
- Qt resources: default.css bundled via .qrc

## Project Layout

```
scriba/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── MainWindow.h
│   ├── MainWindow.cpp
│   ├── Editor.h
│   ├── Editor.cpp
│   ├── Preview.h
│   ├── Preview.cpp
│   ├── MarkdownParser.h
│   ├── MarkdownParser.cpp
│   ├── CssManager.h
│   ├── CssManager.cpp
│   ├── PreferencesDialog.h
│   └── PreferencesDialog.cpp
├── resources/
│   ├── default.css
│   └── scriba.qrc
└── vendor/
    └── md4c/
        ├── md4c.h
        └── md4c.c
```

## Default CSS (default.css)

- Serif font for body, monospace for code
- Table borders and cell padding
- Code block background and border radius
- Image max-width: 100%
- Heading hierarchy with spacing
- Blockquote left border and italic
- Task list checkbox styling

## Error Handling

- File open/save errors: QMessageBox warning
- Invalid markdown: render as-is (md4c is fault-tolerant)
- Missing images: broken image icon in preview
- No CSS directory set: use default.css only

## Future Considerations

- Line numbers in editor
- Syntax highlighting
- Export to HTML/PDF
- Find and replace
- Multiple tabs
- Print support

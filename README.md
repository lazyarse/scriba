# Scriba

<div style="">
    <img src="resources/icons/scriba.svg" width="140" style="float:left; margin-right:20px;" />
A configurable, no-nonsense, split-screen Markdown editor for coders and other autistics with a pretentious Latin name. Built from the ground-up because all the others are well, terrible.

Originally envisioned to be simple: a text-editor on the left and a simple HTML preview on the right but, my bad. 

Built with C++ and Qt6.
</div>

![](docs/images/screenshot.png)

## Features

Designed with a restricted feature-set in mind; useful, not bloated.

### The Basics

- WOW! It's 2004 again with full CommonMark + GFM support (tables, strikethrough, task lists), image rendering from local files and URLs, and admonitions (note, tip, important, warning, caution) with custom titles and icons
- Editor and preview pane with configurable split-screen layout: have the preview on the right, the left, or hidden, and basic search
- CSS-based theming: editor, preview, and chrome all styled from one file. Also included: sample themes for you to moan about. Fair warning: Qt can't style application or dialogs titlebars
- PDF export with print-specific CSS stylesheets

### Standing on the shoulders of giants:

- Themable syntax highlighting for fenced code blocks (auto-detects language via highlight.js)
- [LaTeX math rendering](https://katex.org/) (KaTeX, inline `$...$` and display `$$...$$`)
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie)
- [Vega-Lite data visualization](https://vega.github.io/vega-lite/) (bar, scatter, line, layered, interactive)
- [md4c](https://github.com/mity/md4c) — CommonMark-compliant Markdown parser, modified to accept image sizes (`#WIDTHxHEIGHT` suffix)

### Candy:

- Full-screen mode (F11 or menu bar icon)
- Sentence, word count, estimated reading time, and age level so you can suitably patronise
- Ordered and Unordered list item autocompletion on a new line after a previous list item and `Tab` to indent, `Shift+Tab` to outdent list items
- Optional alternating table row striping because let's be honest, it's annoying as hell
- A [sample.md](sample.md) file in resources with a live examples of the features including admonitions, math, diagrams, charts, and images

## Prerequisites

- CMake 3.16+
- Qt6 development libraries
- GCC/Clang with C++17 support

### Installing Qt6 on Debian/Ubuntu

```bash
sudo apt install qt6-base-dev qt6-webengine-dev
```

## Building

```bash
mkdir -p build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

The post-build step automatically removes cached base stylesheets (`~/.config/Scriba/Scriba/*.css`), so no manual cleanup needed on rebuild.

## Running

```bash
./scriba              # empty editor
./scriba file.md      # open file
./scriba --debug      # enable JS console logging via Qt logging system
```
JS console messages are routed through Qt's `QLoggingCategory` system. By default they're silent; pass `--debug` or set `QT_LOGGING_RULES="scriba.preview.debug=true"` to see them.


## We All Love Shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+N | New |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+R | Reload |
| Ctrl+P | Export PDF |
| Ctrl+F | Find |
| Ctrl+G | Vega-Lite Chart Builder |
| Ctrl+B | Toggle Preview Pane |
| Ctrl+Alt+P | Preferences |
| F11 | Toggle Fullscreen |
| Ctrl+Q | Quit |

## Custom CSS / Themes

See [themes.md](themes.md) for how to write themes, customize admonition icons, and understand the selector structure.

## Project Structure

```
scriba/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── MainWindow.cpp          — Main window, file I/O, CSS management, scroll sync
│   ├── Editor.cpp              — Text editor widget (tab/enter key handling)
│   ├── Preview.cpp             — HTML preview widget (QWebEngineView)
│   ├── MarkdownParser.cpp      — md4c-based markdown parser
│   ├── MdRenderer.cpp          — Custom renderer with data-line attributes
│   ├── CssConfig.cpp           — CSS config persistence
│   ├── CssLoader.cpp           — Loads user/system CSS
│   ├── CssUtils.cpp            — CSS derivation (chrome, themes)
│   ├── CssEditorDialog.cpp     — Dialog for editing custom CSS
│   ├── PreferencesDialog.cpp   — Preferences UI
│   ├── FindDialog.cpp          — Find text dialog
│   ├── ExportPdfDialog.cpp     — PDF export dialog
│   ├── StaticHelpers.cpp       — List continuation, indent/outdent helpers
│   └── Preferences.h           — Preferences struct (header-only)
├── resources/
│   ├── scriba.qrc              — Qt resource file
│   ├── sample.md               — Sample document
│   ├── default.css             — Default theme
│   ├── editor-base.css         — Editor base styles
│   ├── preview-base.css        — Preview base styles
│   ├── print-base.css          — Print styles
│   ├── highlight.min.js        — Syntax highlighting
│   ├── mermaid.min.js          — Mermaid diagrams
│   ├── katex.min.js            — LaTeX math
│   ├── katex.min.css           — KaTeX styles
│   ├── vega.min.js             — Vega (visualization)
│   ├── vega-lite.min.js        — Vega-Lite (charts)
│   ├── vega-embed.min.js       — Vega-Embed (renderer)
│   ├── themes/                 — Built-in CSS themes (15 themes)
│   ├── fonts/                  — KaTeX web fonts
│   ├── icons/                  — Editor icons
│   └── contrib/                — Third-party JS (auto-render)
├── tests/
│   ├── test_markdown_parser.cpp
│   ├── test_md_renderer_helpers.cpp
│   ├── test_css_utils.cpp
│   ├── test_static_helpers.cpp
│   ├── test_editor_list_continuation.cpp
│   ├── test_cursor_restore.cpp
│   └── test_katex_integration.cpp
└── vendor/
    └── md4c/                   — Markdown parser library (MIT, gitignored)
```

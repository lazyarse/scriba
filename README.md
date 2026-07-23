# Scriba Markdown Editor

<div>
    <img src="resources/icons/scriba.svg" width="140" style="float:left; margin-right:20px;" />

**A configurable, no-nonsense, split-screen, off-line Markdown editor with a *pretentious* Latin name**. With a restricted feature-set: useful; no bloat, it's designed to *actually* do what it should without plugins.

<br />
<div align="center">
  :cucumber:
  <img src="docs/images/badge-opencode.svg" />
  :cucumber:
  <br />

  <img src="docs/images/badge-cpp17.svg" alt="C++17">
  <img src="docs/images/badge-qt6.svg" alt="Qt 6">
  <img src="docs/images/badge-tests.svg" alt="tests">
  <img src="docs/images/badge-worthit.svg" alt="Worth it?">
</span>
</div>

<div align="center">

![](docs/images/screenshot.png#700x)

![](docs/images/autocomplete-demo.gif#700x)
<br/><em>I want it known that this took way longer than expected to script.</em>

</div>

## Features

### The Basics

- WOW! It's 2004 again with full CommonMark + GFM support (tables, strikethrough, task lists), image rendering from local files and URLs, and admonitions with custom titles and icons (note, tip, important, warning, caution) 
- Editor and preview pane with configurable split-screen layout: have the preview on the right, the left, or hidden
- CSS-based theming: editor, preview, and chrome all styled from one file. Also included: sample themes for you to moan about. (Fair warning: Qt can't style application or dialog titlebars different to the system theme)
- PDF export with print-specific CSS stylesheets
- Find with regex search
- A shocking 80ms debounce timer that causes a little delay in the rendered preview to bother fast typers like you

### Standing on the shoulders of giants:

- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [LaTeX math rendering](https://katex.org/) (KaTeX, inline `$...$` and display `$$...$$`)
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie)
- [Vega-Lite data visualization](https://vega.github.io/vega-lite/) (bar, scatter, line, layered, interactive) and a chart helper because writing JSON is hard.
- [md4c](https://github.com/mity/md4c) — CommonMark-compliant Markdown parser

### Candy:

- Full-screen mode (<key>F11</key> or menu bar icon)
- Cursor restore on application start to pick up where you left off
- Sentence, word count, estimated reading time, and age-level to suitably patronise
- That annoying `<kbd>` css styling to look just like a key. I like it. Feel free to remove it in the `preview-base.css` and `print-base.css`.
- Table generator - Create either a table with a header row in markdown format, or an HTML generated table when no header row is needed. 
- Table auto-completion: <kbd>Enter</kbd> auto creates a new row either in-cell or at end of the row, <kbd>Enter</kbd> on blank row ends autocomplete, <kbd>Tab</kbd> to next cell, <kbd>Shift</kbd>+<kbd>Tab</kbd> to previous cell
- Ordered and Unordered list item autocompletion after a previous list item with <kbd>Enter</kbd>. <kbd>Tab</kbd> to indent, <kbd>Shift</kbd>+<kbd>Tab</kbd> to outdent. <kbd>Enter</kbd> again to stop autocomplete
- Optional alternating table row striping
- Emojis + search + autocomplete because all technical documentation now requires them
- File / directory autocomplete when typing locations for local images / files in links e.g. `![](foo/bar` could autocomplete to `![](foo/bar.png)`
- A debug log window output to see rendering errors
- A [sample.md](sample.md) file in resources with live examples of the features including admonitions, math, diagrams, charts, and images
- Selected text + <kbd>Tab</kbd> will indent, <kbd><Shift></kbd>+<kbd>Tab</kbd> will dedent.
- <kbd>Down</kbd> on the last line will place the cursor at the end of the line

## Shortcuts for All Features

| Shortcut | Action |
|---|---|---|
| <kbd>Ctrl</kbd>+<kbd>N</kbd> | New |
| <kbd>Ctrl</kbd>+<kbd>O</kbd> | Open |
| <kbd>Ctrl</kbd>+<kbd>S</kbd> | Save |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>S</kbd> | Save As |
| <kbd>Ctrl</kbd>+<kbd>R</kbd> | Reload |
| <kbd>Ctrl</kbd>+<kbd>P</kbd> | Export PDF |
| <kbd>Ctrl</kbd>+<kbd>F</kbd> | Find |
| <kbd>Ctrl</kbd>+<kbd>G</kbd> | Vega-Lite Chart Builder |
| <kbd>Ctrl</kbd>+<kbd>B</kbd> | Toggle Preview Pane |
| <kbd>Ctrl</kbd>+<kbd>E</kbd> | Emoji Search :eyes: :mag: |
| <kbd>Ctrl</kbd>+<kbd>T</kbd> | Table Generator |
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>P</kbd> | Preferences |
| <kbd>F11</kbd> | Toggle Fullscreen |
| <kbd>Ctrl</kbd>+<kbd>Q</kbd> | Quit |

## Custom CSS / Themes

See [themes.md](themes.md) for how to write themes, customize admonition icons, and understand the selector structure.

## Security

Due to Chromium's sandbox issues, it's inheritently unsafe to run this as `root` incase you inadvertedly run crafted HTML code.


## Prerequisites

- CMake 3.16+
- Qt6 development libraries
  ```bash
  sudo apt install qt6-base-dev qt6-webengine-dev
  ```
- GCC/Clang with C++17 support
- **Chromium ≥ 140** (for PDF export). Install on Debian/Ubuntu:
  ```bash
  sudo apt install chromium
  ```
  If Chromium is not found, PDF export falls back to Qt's built-in renderer
  (cannot suppress default page headers/footers).


## Building

```bash
mkdir -p build
cmake --build build --target clean 2>/dev/null   # remove stale _autogen dirs after branch switches
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

The post-build step automatically removes cached base stylesheets (`~/.config/Scriba/Scriba/*.css`), so no manual cleanup needed on rebuild.

### Build .deb package

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cpack --config build/CPackConfig.cmake -G DEB
```

Output: `build/scriba-1.0.0-Linux.deb`

### Build with tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON && cmake --build build -j$(nproc)
```

### Run tests

```bash
# With a display server (X11/Wayland):
cd build && ctest --output-on-failure

# Headless/CI (requires xvfb):
cd build && xvfb-run ctest --output-on-failure
```

Qt WebEngine requires a GPU context for rendering; tests that exercise the preview (`test_scroll_sync`) need `xvfb-run` in headless environments.

## Running

```bash
./scriba              # empty editor
./scriba file.md      # open file
./scriba --debug      # enable JS console logging via Qt logging system
```
JS console messages are routed through Qt's `QLoggingCategory` system. By default they're silent; pass `--debug` or set `QT_LOGGING_RULES="scriba.preview.debug=true"` to see them.
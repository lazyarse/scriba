# Scriba Markdown Editor

<div>
    <img src="resources/icons/scriba.svg" width="140" style="float:left; margin-right:20px;" />

**A no-nonsense, configurable, split-screen, off-line Markdown editor with a *pretentious* Latin name**. An opinionated restricted feature-set: useful; no bloat, and designed to *actually* do what it should without plugin hell.

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

![](docs/images/screenshot.png)

![](docs/images/autocomplete-demo.gif)
<br/><em>I want it known this took way longer than expected to script.</em>

</div>

## Features

### The Basics

- WOW! It's 2004 again with full CommonMark + GFM support (tables, strikethrough, task lists), image rendering from local files and URLs, and admonitions with custom titles and icons (note, tip, important, warning, caution) 
- Multiple document editor and preview pane with configurable split-screen layout: have the preview on the right, the left, on its own, or hidden
- CSS-based theming: editor, preview, and chrome all styled from one file with sample themes for you to moan about. The editor's colours stay in sync with the theme, preferences has separate styling options, Fair warning: Qt can't style application or dialog titlebars different to the system theme. 
- PDF export with print-specific CSS stylesheets
- Find and replace with highlighting, regex search and replace with back-references 
- Up to ten tabs for working in a `session` with load / save session handling for different projects
- The usual <kbd>Ctrl</kbd>+<kbd>X</kbd>, <kbd>Ctrl</kbd>+<kbd>C</kbd>, <kbd>Ctrl</kbd>+<kbd>V</kbd>, <kbd>Ctrl</kbd>+<kbd>Z</kbd>, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd>, <kbd>Ctrl</kbd>+<kbd>A</kbd> do what you'd expect.
- A shocking 80ms debounce timer that causes a little delay in the rendered preview to bother fast typers like you

### Standing on the shoulders of giants:

- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [LaTeX math rendering](https://katex.org/) (KaTeX, inline `$...$` and display `$$...$$`) and helper
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie, etc.) and a chart helper
- [Vega-Lite data visualization](https://vega.github.io/vega-lite/) (bar, scatter, line, layered, interactive) and a chart helper because writing JSON is hard.
- [md4c](https://github.com/mity/md4c) — CommonMark-compliant Markdown parser

### Candy:

- Full-screen mode
- Cursor and view-port restore on re-start to pick up where you left off
- Sentence, word count, estimated reading time and reading age to suitably patronise. (Oh look, this `README` is aimed at high-school-educated people)
- That annoying `<kbd>` css styling to look just like a key. I like it. Feel free to remove it in the `preview-base.css` and `print-base.css`.
- Table generator - Create either a table with a header row in markdown format, or an HTML generated table when no header row is needed. 
- Table auto-completion: <kbd>Enter</kbd> auto creates a new row either in-cell or at end of the row, <kbd>Enter</kbd> on blank row ends autocomplete, <kbd>Tab</kbd> to next cell, <kbd>Shift</kbd>+<kbd>Tab</kbd> to previous cell
- Ordered and Unordered list item autocompletion after a previous list item with <kbd>Enter</kbd>. <kbd>Tab</kbd> to indent, <kbd>Shift</kbd>+<kbd>Tab</kbd> to outdent. <kbd>Enter</kbd> again to stop autocomplete
- Optional alternating table row striping
- Emojis (b+w / colour) + picker + autocomplete because all technical documentation now requires them...and I'm powerless to stop the trend. :chart_with_upwards_trend:
- File / directory autocomplete when typing locations for local images / files in links e.g. `![](foo/bar` autocompletes to `![](foo/bar.png)`
- A debug log window output to see your rendering errors
- A [docs/sample.md](docs/sample.md) file in resources with live examples of the features including admonitions, math, diagrams, charts, and images
- Selected text + <kbd>Tab</kbd> will indent, <kbd>Shift</kbd>+<kbd>Tab</kbd> will dedent.
- <kbd>Down</kbd> on the last line will place the cursor at the end of the line
- hyperlink destination shown when hovered in preview, markdown-syntax and html `img` title shown as tooltip falling back to `alt` text.
- Duplicate a line with <kbd>Ctrl</kbd>+<kbd>D</kbd> because <kbd>Home</kbd><kbd>Shift</kbd>+<kbd>Down</kbd><kbd>Left</kbd> are too many steps.

## Non-Features

- No plugins: You will not spend three hours evaluating whether `scriba-vim-mode` is worth it.
- No AI features: (*I know. Don't even say it.*)
- No real-time collaboration: Your Google Doc trauma is safe here.
- No subscription: It was hard enough convincing you to install Qt6.
- No Electron. Your RAM has better things to do.

## Shortcuts

### Scriba

| Shortcut | Action |
|---|---|
| <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>P</kbd> | Scriba Preferences |
| <kbd>Ctrl</kbd>+<kbd>Q</kbd> | Quit |
| <kbd>Ctrl</kbd>+<kbd>B</kbd> | Toggle Preview Pane Layout |
| <kbd>F11</kbd> | Toggle Fullscreen |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd> | Show Debug Log |
| <kbd>Alt</kbd>+<kbd>1-0</kbd> | Switch between open tabs (0 = tab 10) |

### Files

| Shortcut | Action |
|---|---|
| <kbd>Ctrl</kbd>+<kbd>N</kbd> | New |
| <kbd>Ctrl</kbd>+<kbd>O</kbd> | Open |
| <kbd>Ctrl</kbd>+<kbd>S</kbd> | Save |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>S</kbd> | Save As |
| <kbd>Ctrl</kbd>+<kbd>R</kbd> | Reload `.md` |
| <kbd>Ctrl</kbd>+<kbd>P</kbd> | Print / Export PDF |

### Editor

| Shortcut | Action |
|---|---|
| <kbd>Ctrl</kbd>+<kbd>F</kbd> | Find / Replace |
| <kbd>F3</kbd> | Next Search Result |
| <kbd>Shift</kbd>+<kbd>F3</kbd> | Previous Search Result|
| <kbd>Ctrl</kbd>+<kbd>G</kbd> | Vega-Lite Chart Builder |
| <kbd>Ctrl</kbd>+<kbd>D</kbd> | Duplicate line |

### Helpers

| Shortcut | Action |
|---|---|
| <kbd>Ctrl</kbd>+<kbd>E</kbd> | Emoji Search :eyes: :mag: |
| <kbd>Ctrl</kbd>+<kbd>T</kbd> | Tables |
| <kbd>Ctrl</kbd>+<kbd>K</kbd> | Katex |
| <kbd>Ctrl</kbd>+<kbd>G</kbd> | Vega-Lite |

## Custom CSS / Themes

See [docs/themes.md](docs/themes.md) for how to write themes, customize admonition icons, and understand the selector structure.

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
- Approximately 10 minutes of your time that you will never get back.


## Building

```bash
mkdir -p build
cmake --build build --target clean 2>/dev/null   # remove stale _autogen dirs after branch switches
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

The post-build step automatically removes cached base stylesheets (`~/.config/scriba/*.css`), so no manual cleanup needed on rebuild.

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
cd build && ctest --output-on-failure -j1
```

Tests auto-wrap in `xvfb-run` when available (CMake detects it) so they don't crash on your headless CI.

## Running

```bash
./scriba                    # empty editor
./scriba file.md            # open file
./scriba file1.md file2.md  # open files
```
JS console messages are captured via the Debug Log window (Tools → Debug Log).



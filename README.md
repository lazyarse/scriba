# Scriba Markdown Editor
<div>
    <img src="resources/icons/scriba.svg" width="140" style="float:left; margin-right:20px;" />

**A privacy-first, no-nonsense, configurable, split-screen Markdown editor with a *pretentious* Latin name**. Designed to *actually* do what it should with an opinionated restricted feature-set: useful; no bloat without plugin hell. No node, React, Angular. Just a right, proper, local C++ application.

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

- ---

## Features

### Standing on the shoulders of giants:

- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [LaTeX math rendering](https://katex.org/) (KaTeX, inline `$...$` and display `$$...$$`) with [MChem macro](https://mhchem.github.io/MathJax-mhchem/) and helpers
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie, etc.) and helper
- [Vega-Lite data visualization](https://vega.github.io/vega-lite/) (bar, scatter, line, layered, interactive) and helper
- [md4c](https://github.com/mity/md4c) — we patched this CommonMark-compliant Markdown parser + GFM support

### The Basics

- Handle docsets with a tabbed display across different projects as a `session` with load / save
- Print and PDF export with print-specific CSS stylesheets because we know you want prints to look different
- Everything you'd expect a Markdown editor to do, including admonitions with custom titles + icons, Find and Replace with regex back-references, full-screen mode, 
- Easy accessibility support with CSS-based theming to help you use Scriba: the editor, preview, and chrome all styled from one file with sample themes for you to moan about. The editor's colours stay in sync with the theme but you can override them. Fair warning: Qt can't style application or dialog titlebars different to the system theme. 
- A shocking 80ms debounce timer that causes delay in the rendered preview to bother fast typers like you

### Candy:

- Cursor and view-port restore on re-start to pick up where you left off
- Sentence, word count, estimated reading time and reading age to suitably patronise. (Oh look, this `README` is aimed at high-school-educated people)
- That annoying `<kbd>` css styling to look just like a key. I like it. Feel free to remove it in the `preview-base.css` and `print-base.css`
- Table generator - Create either a table with a header row in markdown format, or an HTML generated table when no header row is needed
- Table auto-completion: <kbd>Enter</kbd> auto creates a new row either in-cell or at end of the row, <kbd>Enter</kbd> on blank row ends autocomplete, <kbd>Tab</kbd> to next cell, <kbd>Shift</kbd>+<kbd>Tab</kbd> to previous cell
- Ordered and Unordered list item autocompletion after a previous list item with <kbd>Enter</kbd>. <kbd>Tab</kbd> to indent, <kbd>Shift</kbd>+<kbd>Tab</kbd> to outdent. <kbd>Enter</kbd> again to stop autocomplete
- Optional alternating table row striping
- Emojis (b+w / colour) + picker + autocomplete because all technical documentation now requires them...and I'm powerless to stop the trend :chart_with_upwards_trend:
- File / directory autocomplete when typing locations for local images / files in links e.g. `![](foo/bar` autocompletes to `![](foo/bar.png)`
- A debug log window output to see your rendering errors
- A [docs/sample.md](docs/sample.md) file in resources with live examples of the features including admonitions, math, diagrams, charts, and images
- hyperlink destination shown when hovered in preview, markdown-syntax and html `img` title shown as tooltip falling back to `alt` text

## Non-Features

- No plugins: You will not spend three hours evaluating whether `scriba-vim-mode` is worth it.
- No AI features: (*I know. Don't even say it.*)
- No real-time collaboration: Your Google Doc trauma is safe here.
- No subscription: It was hard enough convincing you to install Qt6.
- No Electron / node. Your RAM has better things to do.

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
| <kbd>Ctrl</kbd>+<kbd>D</kbd> | Duplicate line |
| <kbd>Ctrl</kbd>+<kbd>Z</kbd> | Undo |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | Redo |
| <kbd>Ctrl</kbd>+<kbd>X</kbd> | Cut |
| <kbd>Ctrl</kbd>+<kbd>C</kbd> | Copy |
| <kbd>Ctrl</kbd>+<kbd>V</kbd> | Paste |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> | Select All |
| <kbd>Down</kbd> | On the last line will place the cursor at the end of the line |
| <kbd>Tab</kbd> | Indent selected text |
| <kbd>Shift</kbd>+<kbd>Tab</kbd> | Dedent selected text |

### Helpers

| Shortcut | Action |
|---|---|
| <kbd>Ctrl</kbd>+<kbd>E</kbd> | Emoji Search :eyes: :mag: |
| <kbd>Ctrl</kbd>+<kbd>T</kbd> | Tables |
| <kbd>Ctrl</kbd>+<kbd>K</kbd> | Katex |
| <kbd>Ctrl</kbd>+<kbd>G</kbd> | Vega-Lite |
| <kbd>Alt</kbd>+<kbd>T</kbd> <kbd>M</kbd> | Mermaid Diagrams |

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
- GCC/Clang with C++17 support (Linux) or Visual Studio 2022 with "Desktop development with C++" workload (Windows)
- On Windows: Qt 6.8+ (MSVC 2022 64-bit) from qt.io, CMake, Git for Windows
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

### Build on Windows

In an **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64"
cmake --build build -j4 --config Release
```

Binary: `build\Release\scriba.exe`

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

On Windows, add Qt's bin directory to PATH first:

```cmd
set PATH=C:\Qt\6.10.3\msvc2022_64\bin;%PATH%
build\Release\scriba.exe
build\Release\scriba.exe file.md
```

For a permanent fix, add `C:\Qt\6.10.3\msvc2022_64\bin` to your system PATH.

JS console messages are captured via the Debug Log window (Tools → Debug Log).

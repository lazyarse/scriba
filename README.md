# Scriba Markdown Editor
<div>
    <img src="resources/icons/scriba.svg" width="140" style="float:left; margin-right:20px;" />

**A privacy-first, no-nonsense, configurable Markdown editor for technical people.** Designed to *actually* do what you want without plugin hell. No node / react / angular / bloat, just a binary that sits on your computer and doesn't call outside. Get ready to leave your Google Docs trauma behind.

<div align="center">
  :cucumber:
  <img src="docs/images/badge-opencode.svg" />
  :cucumber:
  <br />

  <img src="docs/images/badge-cpp17.svg" alt="C++17">
  <img src="docs/images/badge-qt6.svg" alt="Qt 6">
  <img src="docs/images/badge-tests.svg" alt="tests">
</span>
</div>

- ---

## Features

- Load / Save multiple documents in a `session` with a tabbed interface and pick up exactly where you left off with cursor and view-port restore in the last-edited document
- Print and PDF export using print-specific CSS stylesheets
- Readability metrics to keep your writing audience-centred: sentence, word, character, paragraph, syllable counts; reading and speaking times and more. All configurable in Preferences → Writing.
- Find and Replace with regex search and replacement back-references
- PDF, DOCX, and HTML export
- Useful in-editor autocomplete to assist creating tables, local filenames, and emojis
- Numerous [chart helpers](docs/chart-helpers.md) for vega-lite and [mermaid](docs/mermaid.md) with manual data-entry and CSV pasting because writing JSON and fenced blocks at 2am is difficult
- [keyboard shortcuts](docs/shortcuts.md) for everything<sup>*</sup>
-  First-class accessibility support to help you use Scriba the way you want, with CSS-based GUI theming: preview and chrome all styled from one file whilst the editor's colours stay in sync. Override the editor's font family and size, line-height and more. Fair warning: Qt can't style application or dialog titlebars different to the system theme. 

<sup>*</sup> <em>Maybe</em>.

### Standing on the shoulders of giants:
- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [LaTeX math rendering](https://katex.org/) (KaTeX, inline `$...$` and display `$$...$$`) with the [MChem macro](https://mhchem.github.io/MathJax-mhchem/) and helpers
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie, etc.) and helper
- [Vega-Lite data visualization](https://vega.github.io/vega-lite/) (bar, scatter, line, layered, interactive) and helper
- [md4c](https://github.com/mity/md4c) — we patched this CommonMark-compliant Markdown parser + GFM support

### And a few more things...

- That annoying `<kbd>` css styling to look just like a key. If you're a monster, remove it in the `preview-base.css` and `print-base.css`
- Table auto-completion: <kbd>Enter</kbd> auto creates a new row either in-cell or at end of the row, <kbd>Enter</kbd> on blank row ends autocomplete, <kbd>Tab</kbd> to next cell, <kbd>Shift</kbd>+<kbd>Tab</kbd> to previous cell
- Ordered and Unordered list item autocompletion after a previous list item with <kbd>Enter</kbd>. <kbd>Tab</kbd> to indent, <kbd>Shift</kbd>+<kbd>Tab</kbd> to outdent. <kbd>Enter</kbd> again to stop autocomplete
- Optional alternating table row striping
- Emojis (b+w / colour) + picker + autocomplete because all technical documentation now requires them...and I'm powerless to stop the trend :chart_with_upwards_trend:
- File / directory autocomplete when typing locations for local images / files in links e.g. `![](foo/bar` autocompletes to `![](foo/bar.png)`
- A debug log window output to see your rendering errors
- A [docs/sample.md](docs/sample.md) file in resources with live examples of the features including admonitions, math, diagrams, charts, and images
- hyperlink destination shown when hovered in preview, markdown-syntax and html `img` title shown as tooltip falling back to `alt` text for your safety
- You may not need roads where you're going, but you do need security and it isn't an afterthough, see the [security docs](docs/security.md)


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

### Build on Linux

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

## Technical Docs

See:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [PDF Export](docs/pdf-export.md)
- [Table Handlng](docs/table_handling.md)
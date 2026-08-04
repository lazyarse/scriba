![Scriba Icon](resources/icons/scriba.svg)

# Scriba Markdown Editor

**A privacy-first, no-nonsense, configurable Markdown editor for technical people.** Designed to *actually* do what it should without plugin hell. No node, react, angular, or bloat; just a binary which sits on your computer and doesn't phone home. Get ready to leave your Google Docs trauma behind.

:cucumber:
![Opencode Badge + Big Pickles!](docs/images/badge-opencode.svg)
:cucumber:

![C++23 Badge](docs/images/badge-cpp23.svg)
![Qt6 Badge](docs/images/badge-qt6.svg)
![Tests Badge](docs/images/badge-tests.svg)

---

![Screenshot](docs/images/screenshot.png)

See the [Gallery](docs/gallery.md) for more screenshots.

---

## Features

- Load / Save multiple documents in a `session` with a tabbed interface
- Pick up exactly where you left off with cursor and view-port restore in documents
- Privacy-first, in-editor spell and grammar check with various dialects. Import audience-specific word sets with ease: legal, medical, technical, etc. — all options naturally configurable
- Print and PDF export using print-specific CSS style sheets and many type-setting and orphan prevention features 
- Readability metrics to keep your writing audience-focused: sentence / word / character / paragraph / syllable counts; reading and speaking times and more. Only view what metrics are important to you
- Find and Replace with optional regex search and replacement back-references
- Export: PDF, DOCX, and HTML; Import: HTML
- Useful in-editor auto complete to assist in creating tables, lists, links to local filenames, and yes, even emojis
- Numerous [helpers](docs/chart-helpers.md) for ECharts charts (including stock/candlestick) and [mermaid](docs/mermaid.md) diagrams with manual data-entry and CSV import, and helpers for LaTeX, MChem, and tables 
- [keyboard shortcuts](docs/shortcuts.md) for everything (or maybe, nearly everything) and a [kitchensink.md](docs/kitchensink.md) with full feature examples
- Accessibility support to help you use Scriba the way you need, with CSS-based GUI themes: preview and chrome all styled from one file whilst the editor's colours stay in sync. Override the editor's font family and size, line-height and more. Fair warning: Qt can't style application or dialog title bars differently to the system theme. 
- [Application security](docs/security.md) is not an afterthought and both the rendered preview and exports are secured against XSS attacks and various injection vectors for you and your audience

### Standing on the Shoulders of Giants:

- Our [Stoppard](https://www.github.com/lazyarse/stoppard) engine for fast, privacy-first spell and grammar checking, 2x faster than Hunspell, and 7x faster than Harper
- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [LaTeX mathematics rendering](https://katex.org/) with the [MChem macro](https://mhchem.github.io/MathJax-mhchem/) and helpers
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie, etc.) and helper
- [Apache ECharts](https://echarts.apache.org/) data visualisation (bar, scatter, line, pie, candlestick/stock with volume, zoom and moving averages) and helper
- [md4c](https://github.com/mity/md4c) — a patched version of the CommonMark-compliant Markdown parser with GitHub Flavour Markdown support
- [turndown.js](https://github.com/mixmark-io/turndown) — the counterpart to md4c: HTML-to-Markdown conversion (with the [GFM plugin](https://github.com/mixmark-io/turndown-plugin-gfm)) for importing HTML files and pasting content copied from the web

## Custom CSS / Themes

See [themes.md](docs/themes.md) for how to write themes, customise admonition icons, and understand the selector structure.

## Security

Due to Chromium sandbox issues, it's inherently unsafe to run this as `root` in case you inadvertently run crafted HTML code.

## Prerequisites

- CMake 3.16+
- Qt6 development libraries
  ```bash
  sudo apt install qt6-base-dev qt6-webengine-dev
  ```
- GCC/Clang with C++23 support (Linux) or Visual Studio 2022 with "Desktop development with C++" workload (Windows)
- On Windows: Qt 6.8+ (MSVC 2022 64-bit) from qt.io, CMake, Git for Windows
- **Chromium ≥ 140** (for PDF export). Install on Debian/Ubuntu:
  ```bash
  sudo apt install chromium
  ```
  If Chromium is not found, PDF export falls back to Qt's built-in renderer
  (cannot suppress default page headers/footers).
- Approximately 10 minutes of your time that you will never get back.

---

## Building

### Build on Linux

```bash
mkdir -p build
cmake --build build --target clean 2>/dev/null   # remove stale _autogen dirs after branch switches
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

The post-build step automatically removes cached base style sheets (`~/.config/scriba/*.css`), so no manual cleanup needed on rebuild.

### Build on Windows

In an **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64"
cmake --build build -j4 --config Release
```

Binary: `build\Release\scriba.exe`

Copy from QEMU to host (host needs an ssh-server running): `scp file.txt user@10.0.2.2:~/`

### Build Windows installer

Requires [NSIS](https://nsis.sourceforge.io/) installed (`choco install nsis`). In an **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64"
cmake --build build -j4 --config Release
cpack --config build/CPackConfig.cmake -G NSIS
```

Output: `scriba-<version>-win64.exe` (version derived from `git describe --tags --always --dirty`; includes a `-dirty` suffix if the working tree has uncommitted changes)

The NSIS installer adds Scriba to the Start Menu and registers `.md` files to open with Scriba.

> **Windows filename note:** colons (`:`) are not allowed in filenames on Windows. If your git tag
> contains a colon, `cpack` will fail. Stick to semver tags like `v1.2.3` to avoid this.

### Build .deb package

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cpack --config build/CPackConfig.cmake -G DEB
```

Copy the `.deb` to `/tmp/` before installing if your home directory blocks `_apt`:
```bash
cp scriba-*-Linux.deb /tmp/ && sudo apt install /tmp/scriba-*-Linux.deb
```

Output: `scriba-1.0.0-Linux.deb` (may include a `-dirty` suffix if the working tree has uncommitted changes)

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
```
On Windows, add it via:
1. Open System Properties → Advanced → Environment Variables
2. Under System variables, find Path → Edit → New → paste C:\Qt\6.10.3\msvc2022_64\bin
3. OK out, restart terminal/cmd
Or via PowerShell (admin):
[Environment]::SetEnvironmentVariable("Path", [Environment]::GetEnvironmentVariable("Path", "Machine") + ";C:\Qt\6.10.3\msvc2022_64\bin", "Machine")
```

JS console messages are captured via the Debug Log window (Tools → Debug Log).

## Technical Docs

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [PDF Export](docs/pdf-export.md)
- [Table Handling](docs/table_handling.md)

---

## License

Scriba is free software released under the [GNU General Public License v3.0](LICENSE) (or, at your option, any later version). It comes with ABSOLUTELY NO WARRANTY.

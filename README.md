![Scriba Icon](resources/icons/scriba.svg#120x)

![C++23 Badge](docs/images/badge-cpp23.svg)
![Qt6 Badge](docs/images/badge-qt6.svg)
![Tests Badge](docs/images/badge-tests.svg)

# Scriba Markdown Editor

**A privacy-first, no-nonsense, full-featured, and configurable Markdown editor for people who write.** 

Designed to *actually* do what you need without a plugin ecosystem. No node, react, angular, or bloat; instead, a single binary on your computer that respects your privacy and that of your clients. Get ready to leave your Google Docs trauma behind.

---

## Features

- **Load / Save multiple documents** in a `session` with a tabbed interface
- Pick up exactly where you left-off with **cursor and view-port restore** on restart
- **Offline, in-editor spell and grammar checks** with various dialects. **Import custom `session`-specific word sets** with ease: legal, medical, technical, etc.
- Print and PDF export use **print-specific CSS stylesheets** and adhere to common typesetting principles including **orphan, split quote / codeblock, and hanging-line prevention**
- **Readability metrics in realtime** to keep your writing audience-focused including sentence / word / character / paragraph / syllable counts, estimated reading and speaking times, and more --- as you type. Choose what metrics are important to you
- **In-editor underlining** for: typos, not-good grammar, markdown lint issues, malformed urls, and broken links to local files and navigation header sections
- **A validation report** to validate all documents in a `session` according to your needs

- **Create _and_ edit charts and diagrams with ease** using two-way [chart assistants](docs/chart-assistants.md) for ECharts and [Mermaid](docs/mermaid.md) diagrams, manual or CSV-file (+ field mappings) data-entry; extra assistants for LaTeX, MChem, and creating tables
- **"Find and Replace" with regex search _and_ replacement back-references**
- **Export**: PDF, DOCX, and HTML; **Import**: HTML
- **Painless tables** --- after you create a header-row, Scriba will: create a separator-row, blank rows for data, and cell-padding to keep your source looking like an aligned table instead of mangled text and pipes
- **Fuzzy auto-complete suggestions** for links to local filenames, and even emojis; 
- **Fold headers, fenced-code blocks, tables, and lists** to reduce vertical space
- **Auto-correct for commonly mis-spelt words that you define** like "_hte_" / "_the_", "_nad_" / "_and_" to keep you focused on content meaning
- **Auto-save**  
- **Keyboard shortcuts to make you more productive**: [keyboard shortcuts](docs/shortcuts.md) for amost everything; a [kitchensink.md](docs/kitchensink.md) with full feature examples that isn't a Markdown 101; an internal cache of page renders to prevent preview regeneration when switching tabs; and, jump up and down the document from header to header with a simple keyboard shortcut
- **Accessibility support** to help you use Scriba the way you need with [CSS-based GUI themes](docs/themes.md): preview and chrome all styled from one file whilst the editor's colours stay in sync. **Override the editor's font family, size, and line-height, change the caret width to improve visibility, and more**. Fair warning: Qt can't style application or dialog title bars differently to the system theme. 
- [Application security](docs/security.md) is not an afterthought and both the rendered preview and exports are secured against XSS attacks and other injection vectors to protect you and your audience.

### Standing on the Shoulders of Giants:

- Our [Stoppard](https://www.github.com/lazyarse/stoppard) engine for fast, privacy-first spell and grammar checking, 2x faster than Hunspell, and 7x faster than Harper
- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [KaTeX mathematics rendering](https://katex.org/) with the [MChem macro](https://mhchem.github.io/MathJax-mhchem/)
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie, etc.)
- [Apache ECharts](https://echarts.apache.org/) data visualisation (bar, scatter, line, pie, candlestick/stock with volume, zoom and moving averages)
- [md4c](https://github.com/mity/md4c) — a patched version of the CommonMark-compliant Markdown parser with GitHub Flavour Markdown support
- [turndown.js](https://github.com/mixmark-io/turndown) — the counterpart to md4c: HTML-to-Markdown conversion (with the [GFM plugin](https://github.com/mixmark-io/turndown-plugin-gfm)) for importing HTML files and pasting content copied from the web

---

## Custom CSS / Themes

Make Scriba pretty! See [themes.md](docs/themes.md) for how to write themes, customise admonition icons, and understand the selector structure.

## Security

Due to Chromium sandbox issues, it's inherently unsafe to run this as `root` in case you inadvertently run crafted HTML code.

---

_See the [Gallery](docs/gallery.md) for more screenshots..._

![Screenshot](docs/images/screenshot.png)

---

## Prerequisites

- CMake 3.16+
- Qt6 development libraries
  ```bash
  sudo apt install qt6-base-dev qt6-webengine-dev qt6-webchannel-dev
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

The post-build step automatically removes cached base stylesheets (`~/.config/scriba/*.css`), so no manual cleanup needed on rebuild.

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

Copy the `.deb` to `/tmp/` before installing — your home directory likely blocks `_apt`:
```bash
cp scriba-*-Linux.deb /tmp/ && sudo apt install /tmp/scriba-*-Linux.deb
```

If you install straight from `./scriba-...deb` and your home directory is mode `700` (the default for `~/`) you may see:

```
'...scriba-...-Linux.deb' couldn't be accessed by user '_apt'. - pkgAcquire::Run (13: Permission denied)
```

and the same message for every parent directory of the `.deb`.

**This is an apt behaviour, not a Scriba bug.** To isolate package acquisition, `apt` reads the file as the unprivileged `_apt` user, which must be able to traverse *every* directory above the `.deb` — not just read the file itself. `apt` can reach `/var/cache/apt/archives/` (where it downloads normally-installed packages itself), but `_apt` cannot step into a `700` home directory, so it falls back to running as root. The install still succeeds; the message is non-fatal.

It surfaces for `./scriba` because it's a *locally-built* `.deb` inside a restrictive home directory — any hand-built package there triggers it, while repos-installed software never does. Fix by copying the `.deb` to a world-traversable location like `/tmp/` (above), or install without apt's sandbox:

```bash
sudo dpkg -i scriba-*-Linux.deb
```

Output: `scriba-1.0.0-Linux.deb` (may include a `-dirty` suffix if the working tree has uncommitted changes)

### Build with tests

During development, build the test suite in a separate Debug build dir (`build-dbg/`) — it compiles faster than a Release test build and keeps `build/` for the Release binary:

```bash
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && cmake --build build-dbg -j$(nproc)
```

The `-D` flags are only needed the first time a build dir is configured; CMake caches them in `<dir>/CMakeCache.txt`, so later rebuilds are just `cmake --build build-dbg -j$(nproc)`.

### Run tests

```bash
cd build-dbg && ctest --output-on-failure -j4
```

Tests auto-wrap in `xvfb-run` when available (CMake detects it) so they don't crash on your headless CI. Parallel runs are safe: WebEngine suites serialize via `RESOURCE_LOCK webkit`, and each test's config is isolated to a per-process temp dir.

## Running

```bash
./scriba                    # empty editor
./scriba file.md            # open file
./scriba file1.md file2.md  # open files
```

On Windows, add Qt's bin directory to PATH first:

```plaintext
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
- [Gotchas](docs/gotchas.md)

---

## License

Scriba is free software released under the [GNU General Public License v3.0](LICENSE) (or, at your option, any later version). It comes with ABSOLUTELY NO WARRANTY.



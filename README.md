![Scriba Icon](resources/icons/scriba.svg#120x)

![C++23 Badge](docs/images/badge-cpp23.svg)
![Qt6 Badge](docs/images/badge-qt6.svg)

# Scriba Markdown Editor

**A privacy-first, no-nonsense, full-featured, and configurable Markdown editor for people who write.**

Designed to *actually* do what you need quickly and without a plugin ecosystem. No node, react, angular, or bloat; instead, a single binary on your computer that respects your privacy and that of your clients. Get ready to leave your Google Docs trauma behind.

---

## Features

- **Load / Save multiple documents** in a `corpus` with a tabbed interface. A corpus is a saved group of documents (`.scriba` file) with a **Recent Corpus** menu, **live directory monitoring** for external changes (auto-reload / prompt policies), **automatic link rewriting** when a document is renamed or moved, and a **corpus-specific spell-check dictionary** (override or merge with your global one).
- Pick up exactly where you left-off with per-tab **cursor and view-port restore** on restart
- **Offline, in-editor spell and grammar checks** with various dialects. **Import custom `corpus`-specific word sets** with ease: legal, medical, technical, etc.
- Print and PDF export use **print-specific CSS stylesheets** and adhere to common typesetting principles including **orphan, split quote / codeblock, and hanging-line prevention**
- **Readability metrics in real-time** to keep your writing audience-focused including sentence / word / character / paragraph / syllable counts, estimated reading and speaking times, and more --- as you type. Choose what metrics are important to you
- **In-editor underlining** for: typos, not-good grammar, markdown lint issues, malformed urls, and broken links to local files and navigation header sections
- **A validation report** to validate all documents in a `corpus` according to your needs
- **Create _and_ edit charts and diagrams with ease** using two-way [chart assistants](docs/chart-assistants.md) for ECharts and [Mermaid](docs/mermaid.md) diagrams, manual or CSV-file (+ field mappings) data-entry; extra assistants for LaTeX, MChem, and creating tables
- **"Find and Replace" with regex search _and_ replacement back-references**
- **Export**: PDF, DOCX, and HTML; **Import**: HTML and DOCX
- **Painless tables** --- after you create a header-row, Scriba will: create a separator-row, blank rows for data, and cell-padding to keep your source looking like an aligned table instead of mangled text and pipes
- **Fuzzy auto-complete suggestions** for links to local filenames, and even emojis; 
- **Fold headers, fenced-code blocks, tables, and lists** to reduce vertical space
- **Source auto-correct for commonly mis-spelt words** like "_hte_" -> "_the_", "_nad_" -> "_and_" and add your own
- **Auto-save**, - **[Themes](docs/themes.md)** (Fair warning: Qt can't style application or dialog title bars differently to the system theme), and crash recovery of the last corpus.
- **Keyboard shortcuts to make you more productive**: [keyboard shortcuts](docs/shortcuts.md) for amost everything; a [kitchensink.md](docs/kitchensink.md) with full feature examples that isn't a Markdown 101; an internal cache of page renders to prevent preview regeneration when switching tabs; and, jump up and down the document from header to header with a simple keyboard shortcut
- **Accessibility support** to help you use Scriba the way you need with [CSS-based GUI themes](docs/themes.md): preview and chrome all styled from one file whilst the editor's colours stay in sync.
- **Override the editor's font family, size, and line-height, change the caret width to improve visibility, and more**
- [Application security](docs/security.md) is not an afterthought and both the rendered preview and exports are secured against XSS attacks and other injection vectors to protect you and your audience.
- **Searchable settings** to help you modify Scriba quickly.

### Standing on the Shoulders of Giants:

- Our [Stoppard](https://www.github.com/lazyarse/stoppard) engine for fast, privacy-first spell and grammar checking
- [highlight.js](https://highlightjs.org/) for themable syntax highlighting in fenced code blocks which auto-detects language
- [KaTeX mathematics rendering](https://katex.org/) with the [MChem macro](https://mhchem.github.io/MathJax-mhchem/)
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie, etc.)
- [Apache ECharts](https://echarts.apache.org/) data visualisation (bar, scatter, line, pie, candlestick/stock with volume, zoom and moving averages)
- [md4c](https://github.com/mity/md4c) — a patched version of the CommonMark-compliant Markdown parser with GitHub Flavour Markdown support
- [turndown.js](https://github.com/mixmark-io/turndown) — the counterpart to md4c: HTML-to-Markdown conversion (with the [GFM plugin](https://github.com/mixmark-io/turndown-plugin-gfm)) for importing HTML files and pasting content copied from the web

---

## Security

Due to Chromium sandbox issues, it's inherently unsafe to run this as `root` in case you inadvertently run crafted HTML code.

---

_See the [Gallery](docs/gallery.md) for more screenshots..._

![Screenshot](docs/images/screenshot.png)

---

## Running

```bash
./scriba                    # empty editor
./scriba file.md            # open file
./scriba file1.md file2.md  # open files
```

---

## Building

Full build instructions for Linux, Windows, and the installers — plus how to build and run the test suite — are in [docs/build.md](docs/build.md).

Quick start on Linux:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
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

# Scriba Markdown Editor

<div>
    <img src="resources/icons/scriba.svg" width="140" style="float:left; margin-right:20px;" />
A configurable, no-nonsense, split-screen Markdown editor for coders and other autistics with a pretentious Latin name. Built from the ground-up because all the others are not fit for purpose. Originally envisioned to be simple text-editor on the left and a HTML preview on the right application, but, my bad. 

<p align="center">
:cucumber:
<img src="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMjAuMTA1MDM5OTc4MDI3MzQiIGhlaWdodD0iMjQuNSIgdmlld0JveD0iMCAwIDE3MS41Nzg2Mjg1NDAwMzkwNiAzNSIgY2xhc3M9ImJhZGdlLXN2ZyIgZGF0YS12LTg2NTgzYWZkPSIiPjxkZWZzIGRhdGEtdi04NjU4M2FmZD0iIj48IS0tLS0+PCEtLS0tPjwhLS0tLT48L2RlZnM+PHBhdGggZGF0YS12LTg2NTgzYWZkPSIiIGQ9Ik0gOSAwIEwgNjkuNDU1MDg1NzU0Mzk0NTMgMCBMIDY5LjQ1NTA4NTc1NDM5NDUzIDM1IEwgOSAzNSBRIDAgMzUgMCAyNiBMIDAgOSBRIDAgMCA5IDAgWiIgZmlsbD0iIzMxQzRGMyIvPjwhLS1bLS0+PHBhdGggZGF0YS12LTg2NTgzYWZkPSIiIGQ9Ik0gNjkuNDU1MDg1NzU0Mzk0NTMgMCBMIDE2Mi41Nzg2Mjg1NDAwMzkwNiAwIFEgMTcxLjU3ODYyODU0MDAzOTA2IDAgMTcxLjU3ODYyODU0MDAzOTA2IDkgTCAxNzEuNTc4NjI4NTQwMDM5MDYgMjYgUSAxNzEuNTc4NjI4NTQwMDM5MDYgMzUgMTYyLjU3ODYyODU0MDAzOTA2IDM1IEwgNjkuNDU1MDg1NzU0Mzk0NTMgMzUgWiIgZmlsbD0iIzM4OUFENSIvPjwhLS1dLS0+PCEtLS0tPjx0ZXh0IHg9IjM0LjcyNzU0Mjg3NzE5NzI2NiIgeT0iMTcuNSIgZHk9IjAuMzVlbSIgZm9udC1zaXplPSIxMiIgZm9udC1mYW1pbHk9IlJvYm90bywgc2Fucy1zZXJpZiIgZmlsbD0iI0ZGRkZGRiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgbGV0dGVyLXNwYWNpbmc9IjIiIGZvbnQtd2VpZ2h0PSI2MDAiIGZvbnQtc3R5bGU9Im5vcm1hbCIgdGV4dC1kZWNvcmF0aW9uPSJub25lIiBmaWxsLW9wYWNpdHk9IjEiIGZvbnQtdmFyaWFudD0ibm9ybWFsIiBzdHlsZT0idGV4dC10cmFuc2Zvcm06dXBwZXJjYXNlOyIgZGF0YS12LTg2NTgzYWZkPSIiPkFHRU5UPC90ZXh0PjwhLS0tLT48dGV4dCB4PSIxMjAuNTE2ODU3MTQ3MjE2OCIgeT0iMTcuNSIgZHk9IjAuMzVlbSIgZm9udC1zaXplPSIxMiIgZm9udC1mYW1pbHk9Ik1vbnRzZXJyYXQsIHNhbnMtc2VyaWYiIGZpbGw9IiNGRkZGRkYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZvbnQtd2VpZ2h0PSI5MDAiIGxldHRlci1zcGFjaW5nPSIyIiBmb250LXN0eWxlPSJub3JtYWwiIHRleHQtZGVjb3JhdGlvbj0ibm9uZSIgZmlsbC1vcGFjaXR5PSIxIiBmb250LXZhcmlhbnQ9Im5vcm1hbCIgc3R5bGU9InRleHQtdHJhbnNmb3JtOnVwcGVyY2FzZTsiIGRhdGEtdi04NjU4M2FmZD0iIj5PUEVOQ09ERTwvdGV4dD48IS0tLS0+PC9zdmc+" /> 
:cucumber:
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue" alt="C++17">
:cucumber:
  <img src="https://img.shields.io/badge/Qt-6-green" alt="Qt 6">
:cucumber:
  <img src="https://img.shields.io/badge/tests-passing-brightgreen" alt="tests">
:cucumber:
</p>
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

### Eye-Candy:

- Full-screen mode (F11 or menu bar icon)
- Sentence, word count, estimated reading time, and age level to suitably patronise
- Ordered and Unordered list item autocompletion on a new line after a previous list item and `Tab` to indent, `Shift+Tab` to outdent list items
- Optional alternating table row striping because let's be honest, it's annoying
- A [sample.md](sample.md) file in resources with a live examples of the features including admonitions, math, diagrams, charts, and images
- File / directory autocomplete when typing locations for local images / files in links e.g. `![](foo/bar<tab>` could autocomplete to `![](foo/bar.png)`

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

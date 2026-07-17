# Scriba

A configurable, no-nonsense split-screen Markdown editor for coders and other autistics. Built with C++ and Qt6.

## Features

A restricted feature-set; useful, not bloated.

### The Basics

- WOW! It's 2004 again with full CommonMark + GFM support (tables, strikethrough, task lists), image rendering from local files and URLs, and admonitions (note, tip, important, warning, caution) with custom titles
- Editor and preview pane with configurable split-screen layout: have the preview on the right, left, or hidden
- CSS-based theming: editor, preview, and chrome all styled from one file, and included sample themes for you to moan about. Fair warning: Qt can't style titlebars (both application and dialogs)
- PDF export with print-specific CSS
- Sample `md` file in resources with a quick overview of the features.

### Candy:

- Full-screen mode (F11 or menu bar icon)
- Word count and estimated reading time
- Ordered and Unordered list item autocompletion and `Tab` to indent, `Shift+Tab` to outdent list items
- Optional alternating table row striping because let's be honest, it's annoying as hell

### Standing on the shoulders of giants:

- Syntax highlighting for fenced code blocks (auto-detects language via highlight.js)
- [LaTeX math rendering](https://katex.org/) (KaTeX, inline `$...$` and display `$$...$$`)
- [Mermaid diagram rendering](https://mermaid.js.org/intro/) (flowcharts, sequence, state, pie)
- [Vega-Lite data visualization](https://vega.github.io/vega-lite/) (bar, scatter, line, layered, interactive)

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
```

## Usage

1. Type Markdown in the editor pane
2. See live rendered preview in the adjacent pane
3. ???
4. PROFIT!!1

## Admonitions

Use `> [!type]` syntax inside a blockquote:

```markdown
> [!note]
> This is a note.

> [!tip]
> This is a tip.

> [!warning]
> This is a warning.

> [!warning HIGH VOLTAGE!]
> Shocking! This is a warning with a custom title.

```


### Adding icons to admonition titles

Use CSS `::before` to prepend an icon to `.admonition-title`:

```css
.admonition.note .admonition-title::before       { content: "\2139\00a0"; }
.admonition.tip .admonition-title::before        { content: "\2714\00a0"; }
.admonition.important .admonition-title::before  { content: "\26A0\00a0"; }
.admonition.warning .admonition-title::before    { content: "\26A0\00a0"; }
.admonition.caution .admonition-title::before    { content: "\2716\00a0"; }
```

These use Unicode characters (info, checkmark, warning, cross) and are purely CSS-based — no image files needed.

## Mermaid Diagrams

Render diagrams inside fenced code blocks with the `mermaid` language tag:

```markdown
\`\`\`mermaid
flowchart LR
  A[Write] --> B{Preview?}
  B -->|Yes| C[Live render]
  B -->|No| D[Keep typing]
\`\`\`
```

Supports flowcharts, sequence diagrams, state diagrams, and pie charts. Diagrams render live as you type.

See the [Mermaid docs](https://mermaid.js.org/intro/) for the full syntax reference.

## Vega-Lite Charts

Render interactive data visualizations inside fenced code blocks with the `vl` language tag:

```markdown
\`\`\`vl
{
  "$schema": "https://vega.github.io/schema/vega-lite/v6.json",
  "width": "container",
  "data": {"values": [{"x":"A","y":28}, {"x":"B","y":55}]},
  "mark": "bar",
  "encoding": {
    "x": {"field":"x", "type":"nominal"},
    "y": {"field":"y", "type":"quantitative"}
  }
}
\`\`\`
```

Supports any Vega-Lite spec: bar, scatter, line, area, layered, faceted, and interactive charts. Charts render live as you type and fill the available preview width.

See the [Vega-Lite docs](https://vega.github.io/vega-lite/) for the full spec reference and examples.

## LaTeX Math

Render inline and display math using KaTeX. Wrap inline math in single dollar signs and display math in double dollar signs:

The quadratic formula is $x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}$.

$$
\int_0^\infty e^{-x^2} \, dx = \frac{\sqrt{\pi}}{2}
$$

```markdown
The quadratic formula is $x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}$.

$$
\int_0^\infty e^{-x^2} \, dx = \frac{\sqrt{\pi}}{2}
$$
```


Both inline and display math render live as you type.

See the [KaTeX docs](https://katex.org/) for supported functions and syntax.

## Custom CSS / Themes

Scriba ships with 15 built-in themes (Catppuccin, Dracula, Nord, Tokyo Night, etc.) in `resources/themes/`. Each theme controls the editor background, preview typography, app chrome (menus, scrollbar, splitter), and syntax highlighting colors.

### Writing a theme

A theme CSS file targets three parts of the app:

| Selector | Target | Purpose |
|---|---|---|
| `#editor` | Text editor widget | `background-color`, `color` |
| `body`, `h1`, `code`, etc. | Preview HTML | Rendered Markdown |
| `.hljs-*` | Code blocks | Syntax highlighting colors |

App chrome (menus, scrollbars, splitter) is auto-derived from the `#editor` colors — themes only need to set `#editor { background-color; color; }`.

Example:

```css
#editor {
    background-color: #1e1e2e;
    color: #cdd6f4;
}

body {
    font-family: Georgia, serif;
    max-width: 800px;
    margin: 0 auto;
    padding: 20px;
    color: #cdd6f4;
    background-color: #1e1e2e;
}

.hljs { color: #cdd6f4; }
.hljs-keyword { color: #cba6f7; }
.hljs-string { color: #a6e3a1; }
```

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

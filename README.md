# Scriba

A simple split-screen Markdown editor built with C++ and Qt6.

## Features

- Configurable split-screen layout: preview on right, left, or hidden (toggle via menu bar icon)
- Full-screen mode (F11 or menu bar icon)
- Full CommonMark + GFM support (tables, strikethrough, task lists)
- Syntax highlighting for fenced code blocks (auto-detects language via highlight.js)
- Word count and estimated reading time in the status bar
- List item autocompletion (press Enter to continue a list)
- Tab to indent, Shift+Tab to outdent list items
- Admonitions (note, tip, important, warning, caution)
- Mermaid diagram rendering (flowcharts, sequence, state, pie)
- LaTeX math rendering (KaTeX, inline `$...$` and display `$$...$$`)
- Image rendering from local files and URLs
- CSS-based theming: editor, preview, and chrome all styled from one file
- PDF export with print-specific CSS
- Alternating table row striping (toggleable)
- File menu: New, Open, Save, Save As
- Preferences: manage CSS directory and custom stylesheets

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
./scriba
```

## Usage

1. Type Markdown in the editor pane
2. See live rendered preview in the adjacent pane
3. Click the **layout icon** in the menu bar to cycle: preview right → preview left → preview hidden
4. Press **F11** or click the **full-screen icon** to toggle full-screen mode
5. Use **File > Open** to load `.md` files
6. Use **File > Save** to save your work
7. Go to **File > Preferences** to configure CSS styling

## Admonitions

Use `> [!type]` syntax inside a blockquote:

```markdown
> [!note]
> This is a note.

> [!tip]
> This is a tip.

> [!warning]
> This is a warning.
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

## Custom CSS / Themes

Scriba ships with 15 built-in themes (Catppuccin, Dracula, Nord, Tokyo Night, etc.) in `resources/themes/`. Each theme controls the editor background, preview typography, app chrome (menus, scrollbar, splitter), and syntax highlighting colors.

### Activating a theme

1. Open **File > Preferences**
2. Select a stylesheet from the list and click the checkbox to activate it
3. The editor, preview, and chrome all update immediately

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
│   ├── MainWindow.cpp    — Main window with splitter layout
│   ├── Editor.cpp        — Text editor widget
│   ├── Preview.cpp       — HTML preview widget
│   ├── MarkdownParser.cpp — md4c-based markdown parser
│   ├── MdRenderer.cpp    — Custom renderer with data-line attributes
│   ├── CssManager.cpp    — CSS file management
│   └── PreferencesDialog.cpp — Preferences UI
├── resources/
│   ├── themes/           — Built-in CSS themes (Catppuccin, Dracula, Nord, etc.)
│   └── scriba.qrc        — Qt resource file
└── vendor/
    └── md4c/             — Markdown parser library (MIT)
```

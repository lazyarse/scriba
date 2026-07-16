# Scriba

A simple split-screen Markdown editor built with C++ and Qt6.

## Features

- Split-screen layout: editor on left, live preview on right
- Full CommonMark + GFM support (tables, strikethrough, task lists)
- Admonitions (note, tip, important, warning, caution)
- Image rendering from local files and URLs
- CSS-based theming: editor, preview, and chrome all styled from one file
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
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Running

```bash
./scriba
```

## Usage

1. Type Markdown in the left pane
2. See live rendered preview on the right
3. Use **File > Open** to load `.md` files
4. Use **File > Save** to save your work
5. Go to **File > Preferences** to configure CSS styling

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

## Custom CSS / Themes

Scriba uses a single CSS file to style both the **editor** (`#editor`) and the **preview** (`#preview`). Each pane picks up the rules that apply to it and ignores the rest.

### Adding a theme

1. Open **File > Preferences**
2. Click **Add** to select `.css` files
3. Click the checkbox next to a stylesheet to activate it
4. Click **Remove** to remove it from the list

### Writing a theme

A theme CSS file controls the entire application appearance with three kinds of rules:

| Selector | Target | Purpose |
|---|---|---|
| `#editor { ... }` | Text editor widget | `background-color`, `color` |
| `QSplitter::handle`, `QMenuBar`, etc. | App chrome | Menus, scrollbars, splitter |
| `body`, `h1`, `code`, etc. | Preview HTML | Rendered Markdown |

Example:

```css
#editor {
    background-color: #ffffff;
    color: #333333;
}

QSplitter::handle {
    background-color: #ccc;
}
QMenuBar {
    background-color: #f0f0f0;
    color: #333;
}

body {
    font-family: Georgia, serif;
    max-width: 800px;
    margin: 0 auto;
    padding: 20px;
    color: #333;
}
```

The active stylesheet is applied to the whole app — menus, scrollbars, and the splitter automatically match the theme.

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
│   ├── default.css       — Default preview stylesheet
│   └── scriba.qrc        — Qt resource file
└── vendor/
    └── md4c/             — Markdown parser library (MIT)
```

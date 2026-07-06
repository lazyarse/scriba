# Scriba

A simple split-screen Markdown editor built with C++ and Qt6.

## Features

- Split-screen layout: editor on left, live preview on right
- Full CommonMark + GFM support (tables, strikethrough, task lists)
- Admonitions (note, tip, important, warning, caution)
- Image rendering from local files and URLs
- CSS-based preview styling
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

## Custom CSS

1. Open **File > Preferences**
2. Click **Browse** to select a directory containing `.css` files
3. Click **Add** to enable custom stylesheets
4. Click **Remove** to disable them

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

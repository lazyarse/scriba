# Themes & Admonition Icons

Scriba ships with 15 built-in themes (Catppuccin, Dracula, Nord, Tokyo Night, etc.) in `resources/themes/`. Each theme controls the editor background, preview typography, app chrome (menus, scrollbar, splitter), and syntax highlighting colors.

## Writing a theme

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

## Admonition icons

Use CSS `::before` to prepend an icon to `.admonition-title`:

```css
.admonition.note .admonition-title::before       { content: "\2139\00a0"; }
.admonition.tip .admonition-title::before        { content: "\2714\00a0"; }
.admonition.important .admonition-title::before  { content: "\26A0\00a0"; }
.admonition.warning .admonition-title::before    { content: "\26A0\00a0"; }
.admonition.caution .admonition-title::before    { content: "\2716\00a0"; }
```

These use Unicode characters (info, checkmark, warning, cross) and are purely CSS-based — no image files needed.

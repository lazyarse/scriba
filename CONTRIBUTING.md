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
│   ├── test_katex_integration.cpp
│   ├── test_readability.cpp
│   ├── test_vegalite_dialog.cpp
│   ├── test_editor_autocomplete.cpp
│   ├── test_editor_completion_ui.cpp
│   └── test_scroll_sync.cpp
└── vendor/
    └── md4c/                   — Markdown parser library (MIT, gitignored)
```

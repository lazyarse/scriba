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
├── docs/
│   ├── sample.md               — Sample document (canonical copy)
├── resources/
│   ├── scriba.qrc              — Qt resource file
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

## PDF Printing: Qt Binary Patch Required

### The Problem

`PreferCSSMarginsForPrinting = true` is supposed to make Chromium honor CSS `@page { margin, size }` rules. But Qt maps it to Chromium's `kDefaultMargins` (0) instead of `kNoMargins` (1). The result: Chromium ignores the CSS and outputs zero/page-default margins regardless of `@page` rules. This affects all tested Qt versions (Debian 6.10.2, official Qt 6.10.3, 6.11.1, 6.12.0-beta1).

The fix requires a one-byte binary patch to `libQt6WebEngineCore.so`:

```
File: /usr/lib/x86_64-linux-gnu/libQt6WebEngineCore.so.6.x.y
Offset: 0xe01ca9 (may vary per build)
Change: 0x03 → 0x01
```

To find the correct offset for your specific Qt build, search for the `"marginsType"` string in the `.rodata` section, then find the `lea rdi/rX` instruction that references it. The second reference (the else-branch of `PrintViewManagerQt::DidPrintPage`) will have a `mov ecx, 3` (`b9 03 00 00 00`) nearby — the `03` byte is what you change to `01`. Here's a recipe:

```bash
# 1. Find "marginsType" string offset
python3 -c "
with open('libQt6WebEngineCore.so.6.x.y', 'rb') as f:
    d = f.read()
    idx = d.index(b'marginsType')
    print(f'marginsType at: 0x{idx:x}')
"

# 2. Find LEA instructions referencing that offset (code references)
#    Look for the second pair (the else-branch). Near it you'll find:
#    b9 03 00 00 00  — that's the byte to patch (03→01)
```

The offset `0xe01ca9` in this guide is for **Debian Qt 6.10.2**. Other builds (official Qt, different versions) will have different offsets but the pattern is the same: in the else-branch of the `marginsType` Set call, change `mov ecx, kPrintableAreaMargins(3)` → `mov ecx, kNoMargins(1)`.

This changes the else-branch at `PrintViewManagerQt::DidPrintPage` from `kPrintableAreaMargins` (3) → `kNoMargins` (1). With `PreferCSSMarginsForPrinting = true`, Chromium now receives `kNoMargins` and defers entirely to CSS `@page` for margins, size, margin boxes, `:first`/`:left`/`:right`, and page counters.

Without this patch, `ExportPdfDialog::onPageLoaded()` previously ran a `parsePageMargins` workaround that parsed `@page { margin }` via regex and piped values through `QPageLayout`. That approach could not handle `@page { size }`, margin boxes, named pages, or page selectors. The workaround was removed when the binary patch was applied.

The full patching plan (Option A: binary patch, Option B: LD_PRELOAD, Option C: full Qt source rebuild) is documented in `qt-knomargins-patch.md`.

## Testing

Test suites set `QCoreApplication::setOrganizationName("ScribaTest")` / `setApplicationName("ScribaTest")` so their QSettings data (and default CSS files written by `CssLoader`) land in `~/.config/ScribaTest/` instead of the real app's `~/.config/Scriba/`. This keeps test config isolated from the user's config.

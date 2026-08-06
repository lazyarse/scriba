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
│   ├── SpellChecker.cpp        — Spell-checking wrapper (stoppard-backed)
│   ├── SpellHighlighter.cpp    — Markdown-aware squiggle highlighter
│   ├── GrammarChecker.h        — Grammar-check interface
│   ├── StoppardEngine.cpp      — Grammar checker backed by the vendored stoppard engine
│   └── Preferences.h           — Preferences struct (header-only)
├── docs/
│   ├── kitchensink.md          — Sample document (canonical copy)
├── resources/
│   ├── scriba.qrc              — Qt resource file
│   ├── default.css             — Default theme
│   ├── preview-base.css        — Preview base styles
│   ├── print-base.css          — Print styles
│   ├── highlight.min.js        — Syntax highlighting
│   ├── mermaid.min.js          — Mermaid diagrams
│   ├── katex.min.js            — LaTeX math
│   ├── katex.min.css           — KaTeX styles
│   ├── echarts.min.js          — Apache ECharts (charts)
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
│   ├── test_scroll_sync.cpp
│   ├── test_spell_checker.cpp
│   ├── test_spell_highlighter.cpp
│   └── test_grammar_checker.cpp
└── vendor/
    ├── md4c/                   — Markdown parser library (MIT, tracked in repo, src/ has local patches in patches/)
    └── stoppard/               — Grammar engine (GPL-3.0, vendored working copy)
```

## Documentation Assets

### Screenshot

After adding a new menu item or any significant UI change, update `docs/images/screenshot.png`:

```bash
bash scripts/update-screenshot.sh
```

This launches `build/scriba` under `xvfb-run`, loads `docs/kitchensink.md`, waits 3s, then captures the window with `import` (ImageMagick).

New dialog or preferences-page captures must also be added to `docs/gallery.md`, which lists every screenshot except the main `screenshot.png` (that one stays embedded in the README).

### Autocomplete Demo GIF

After changing autocomplete behavior, update `docs/images/autocomplete-demo.gif`:

```bash
python3 scripts/create-autocomplete-demo.py
```

The script auto-wraps in `xvfb-run`. It simulates keystrokes via `xdotool`, captures frames with `mss` (Python), and assembles the GIF.

**Dependencies:**
```
sudo apt install xvfb xdotool
pip install -r scripts/requirements.txt
```

Both scripts expect a pre-built binary at `build/scriba`. The main window opens at Qt's default 640×480 (it doesn't set its own size), so the shared `demo_helper.py` sizes it manually with `xdotool windowsize` (1000×400, `windowmove`d to 0,0) before capturing; `update-screenshot.sh:22` sizes its window to 1280×800.

### Table Demo GIF

After changing table formatting or alignment behavior, update `docs/images/table-demo.gif`:

```bash
python3 scripts/create-table-demo.py
```

Like the autocomplete demo, this auto-wraps in `xvfb-run`, simulates keystrokes with `xdotool`, captures frames with `mss`, and assembles the GIF. It needs a pre-built binary at `build/scriba` and the same `xvfb`/`xdotool`/Pillow/mss dependencies as above. The script writes its config to `/tmp/scriba-demo-config` (`dictionaryLanguage=en_GB`, `grammarDialect=British`) so the British spelling "Centre" in the header column isn't flagged by the spell checker, and both scripts share the key-simulation, capture, and GIF-assembly helpers in `scripts/demo_helper.py`.

### Keyboard Shortcuts

After adding a new keyboard shortcut, update `resources/shortcuts.html` to document it. The file is loaded via the Help → Keyboard Shortcuts menu item and displayed in a `QTextBrowser`.

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

## Themes

### Theme file structure

Each CSS file in `resources/themes/` has four sections in order:

1. **Editor styles** (`#editor`) — background and text color for the source editor pane
2. **CSS custom properties** (`:root`) — checkbox styling variables
3. **Preview pane styles** (`body` and descendants) — full HTML preview styling including headings, code blocks, tables, blockquotes, admonitions, etc.
4. **highlight.js block** — syntax highlighting CSS for fenced code blocks, preceded by a `/* highlight.js */` comment

New themes must include all four sections. The preview styles follow a consistent template across all themes — copy an existing theme and replace the color values.

### highlight.js integration

The highlight.js blocks at the end of each theme were sourced from the [highlight.js theme repository](https://github.com/highlightjs/highlight.js/tree/main/src/styles) and color-mapped to match each theme's palette. Two hljs style variants are used:

- **base16 style** (most themes) — includes `::selection` styling, `operator` opacity, and language-specific overrides
- **GitHub/Hirse style** (github-dark, github-light, catppuccin-latte, tokyo-night-dark, tokyo-night-light) — simpler structure based on GitHub's syntax highlighting

When creating a new theme, find the closest matching highlight.js theme from the repository, adjust its colors to fit your palette, and append it after the `/* highlight.js */` comment. Ensure the hljs background matches the theme's `pre` background color and the default text matches `pre code` color.

### Adding a new theme

1. Copy an existing theme file in `resources/themes/`
2. Replace the color values throughout (editor bg/text, body bg/text, headings, code blocks, links, admonition accents, blockquote styling)
3. Source or write a matching highlight.js block and append it at the end
4. Register the new file in `resources/scriba.qrc` under `<file>themes/your-theme.css</file>`
5. Rebuild — themes are compiled into the binary via Qt resources

## Upgrading md4c

The vendored markdown parser at `vendor/md4c/` has local patches in `vendor/md4c/patches/`. To upgrade:

1. Copy new upstream `src/*` into `vendor/md4c/src/`
2. From the repo root: `git apply vendor/md4c/patches/*.patch`
3. If patches fail, rework and update the .patch files
4. Commit updated files and patches

### Qt Widget Chrome

The chrome CSS for Qt widgets (dialogs, buttons, inputs, spinboxes) is generated programmatically in `src/CssUtils.cpp:deriveChromeCss()` by extracting theme background/text colors. Styling `QSpinBox` requires explicit sub-control rules for `up-button`, `down-button`, `up-arrow`, and `down-arrow` — if you style the widget without them, Fusion stops rendering the arrow glyphs and they become invisible. Arrow images (`resources/arrow-up*.svg` / `arrow-down*.svg`) are manually created in light/dark color variants and conditionally loaded via per-theme-brightness variables (same pattern as `checkbox-checked*.svg`). Always mirror the existing spinbox rule when modifying chrome CSS.

## Preview Link Handling

Clicking a hyperlink in the preview opens local `.md` files in a new editor tab and everything else (http, https, mailto, etc.) in the system browser.

### Why `about:blank#scriba-open:`?

The preview page is loaded via `QWebEnginePage::setHtml()`, which creates an internal `data:` URL. Chromium silently blocks navigation from `data:` to `file:` URLs before Qt ever sees the request in `acceptNavigationRequest()`. However, navigation from `data:` to `http:` and `about:` is allowed.

To work around this, a delegated JavaScript click handler is injected into the preview HTML (`MainWindow.cpp:951-956`) that intercepts every link click, prevents the default navigation, and instead redirects to `about:blank#scriba-open:<encoded-url>`. The `about:` scheme navigation reaches `PreviewPage::acceptNavigationRequest()` (in `Preview.cpp:48-55`), which detects the `scriba-open:` fragment header, extracts the real URL, and emits `openLinkRequested(url)`. No actual navigation to `about:blank` occurs — `acceptNavigationRequest` returns `false`, cancelling it.

### Signal flow

```
User clicks link in preview
  → JS handler: e.preventDefault()
  → JS: window.location.href = 'about:blank#scriba-open:' + encodeURIComponent(link.href)
  → PreviewPage::acceptNavigationRequest(url, NavigationTypeOther)
  → detects scriba-open fragment, emits openLinkRequested(realUrl)
  → MainWindow handler:
      • realUrl.isLocalFile() && suffix == "md"  →  loadFile(localPath)  [new tab]
      • otherwise                                →  QDesktopServices::openUrl(realUrl)
```

### Key files

- `src/Preview.cpp` — `PreviewPage::acceptNavigationRequest` intercepts the fake navigation
- `src/MainWindow.cpp` — JS injection in the HTML template; signal handler that opens tabs or browser
- `src/Preview.h` — `openLinkRequested` signal declaration

## Preview Rendering: Dual Debounce

Every keystroke in the editor triggers `updatePreview()` in `MainWindow.cpp`, which calls `scribaUpdate()` in the preview page's JS context. To keep typing responsive while still rendering heavy visualizations, there are two debounce delays:

- **80ms** — `MainWindow` debounces `updatePreview()` calls so fast typing doesn't flood the WebEngine. On each call, `scribaUpdate()` sets `document.body.innerHTML` immediately for a snappy text preview.
- **1500ms** — After the HTML update, `scribaUpdate()` sets a `setTimeout` that runs Mermaid, KaTeX, Vega-Lite, highlight.js, and twemoji. If the user types again within 1500ms, the timer is reset and the previous render is discarded (via a generation counter `window._scribaGen`).

This split avoids re-running expensive JS libraries on every character while keeping text updates instant. Scroll position is restored using a percentage (`pct = scrollY / scrollHeight`) saved before innerHTML, so diagram height changes don't drift the viewport.

## Development workflow

Two build directories, each serving a different job:

- `build-dbg/` — **Debug** build for the dev/test loop (`-DBUILD_TESTS=ON`). Fastest to compile; assertions on.
- `build/` — **Release** build for the final binary only (tests OFF). Build it after the test suite passes.

Full builds are heavy (WebEngine resources, JS bundles, many test targets), so give the build command a generous timeout — 10m is the baseline — rather than the default 120 s.

### Dev loop (tests) — `build-dbg`

```bash
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && cmake --build build-dbg -j$(nproc)
cd build-dbg && ctest --output-on-failure -j1
```

Use true `Debug`, not `RelWithDebInfo` — RelWithDebInfo still compiles at `-O2`/`-O3`, so it builds at Release speed with none of the dev-loop benefit. Debug compiles far faster (the ~10 test targets each recompile `MainWindow.cpp` plus ~35 other sources), at the cost of slower test runtimes — which barely matters since most tests are bound by WebEngine startup, not CPU.

### Release binary — `build`

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF && cmake --build build -j$(nproc)
```

Build only after the test suite passes; tests are OFF here (they live in `build-dbg`).

After building, smoke-test the freshly linked binary:

```bash
timeout 3 build/scriba || true
```

This must run AFTER `cmake --build build` — running it right after the build-dbg dev loop smoke-tests a stale `build/scriba` from the previous Release build.

The `-D` flags above are only needed the first time a build dir is configured (or to change a cached value). CMake stores them in `<dir>/CMakeCache.txt`, so later rebuilds are just `cmake --build build-dbg -j$(nproc)` / `cmake --build build -j$(nproc)`.

## Testing

Every test binary starts from `tests/TestConfig.h` (`setupTestConfig()`), which redirects **all** config access to `~/.config/scribaTest/` and wipes that directory so each run starts clean:

- QSettings data lands in `~/.config/scribaTest/scribaTest.conf` (org/app = `scribaTest`), not the real app's `~/.config/scriba/scriba.conf`
- File-based config (base CSS, themes, spell-check dictionaries) is redirected via the `SCRIBA_TEST_CONFIG_DIR` env var, honoured by `CssUtils::scribaConfigDir()` (used by `CssLoader::configDir()` and `SpellChecker::configDictDir()`)

Suites with their own `main()` call `setupTestConfig()` right after creating the `QApplication`; the 3 config-focused suites (`test_css_loader`, `test_settings_migration`, `test_css_config`) link the shared `scriba_test_main` static library instead of `GTest::gtest_main`. **Never write outside `~/.config/scribaTest/` from a test** — the real user config in `~/.config/scriba/` must stay untouched by test runs.

## Base CSS Versioning

`preview-base.css` and `print-base.css` are both bundled in qrc and copyable to the config dir via the CSS editor dialog (`CssLoader::setPreviewBaseCss`/`setPrintBaseCss`). Because a stale saved copy would shadow the bundled stylesheet globally (breaking rendering after app updates), each saved copy carries a version marker:

```css
/* scriba-base-css-version: <sha256-of-current-bundled-css> */
```

On load, `CssLoader::loadBaseCss()` compares the marker against a SHA-256 of the bundled qrc resource:

- marker matches → the user's edited copy is honoured
- marker missing or outdated → the file is renamed to `.bak`, the bundled CSS is used, and the file is recorded via `staleBaseCssFiles()`

On the first run of a session, `MainWindow` shows a one-time dialog listing superseded `.bak` backups (disabled in unit tests via `MainWindow::setNotifyStaleCss(false)`). When changing the bundled base CSS, no marker bookkeeping is needed — the hash comparison handles it automatically.

## Settings Schema Versioning

`Preferences.h` maintains a `ConfigVersion` key and a `CurrentConfigVersion` counter. `Preferences::migrateSettings(QSettings&)` is called from `main.cpp` at startup and upgrades old configs in place:

- renames renamed keys (e.g. `reopenLastFile` → `reopenLastSession`)
- removes keys for options that no longer exist (`darkMode`, `editorOnLeft`, `showFoldIcons`, `firstRun`, `printCssFiles`, `activePrintCssFile`, `cssDirectory`, `enabledCssFiles`, `EditorFont`/`editorFont`)
- stamps `ConfigVersion`; already-current configs and unknown keys are left untouched

When removing or renaming a setting, extend `migrateSettings` instead of just deleting the read — otherwise stale configs silently fall back to defaults with no migration path. `test_settings_migration` covers each rule.

## CSS Theme Validation

`CssConfig` validates the stored active stylesheet at load time: qrc paths must exist among `bundledThemes()`, file paths must exist on disk. An invalid value is cleared and persisted, falling back to the default theme. This prevents a deleted user theme file from bricking the editor UI.

### Editor typing integration tests

`test_editor_typing` drives the real `Editor` widget with `QTest` key events (`keyClicks`/`keyClick`). There is no WebEngine involved and the input is fully deterministic, so a failure is a genuine behaviour regression — **do not rerun to "confirm" it was flaky, investigate it**. This applies especially to tests asserting exact sentinel semantics from `StaticHelpers`:

- the empty list marker (`- ` / `1. ` / `- [ ] ` + Enter) clears to a bare newline
- a blank table row (`|  |  |` + Enter) exits the table
- a blank HTML table row exits table autocomplete

If you change `handleListReturn`, `handleTableReturn`, `indentListLine`, `outdentListLine`, or the table navigation helpers, update both the pure helper tests (`test_editor_list_continuation.cpp`) and the key-event integration tests (`test_editor_typing.cpp`).

New typing-centred suites should derive from the shared fixture `tests/EditorTestHarness.h` (`EditorTestHarness`, in the `test_editor_helpers` static library) rather than re-implementing key simulation. It provides `typeText`, `press`, `typeLine`, `run(...)`, `placeCursor`, `placeCursorAtEnd`, `selectLines`, `waitForFolds`, `text`, `cursorBlock`, `cursorColumn`, and `assertCursor`. The constructor takes a `CompletionPrefs` struct (`{file, emoji, language}` bools, default all off) that `SetUp` writes to QSettings before constructing the `Editor` — so autocomplete popups stay off for plain typing suites and are enabled (e.g. `CompletionPrefs::all()`) for completion suites. Suites needing extra per-test state (a temp doc directory, a current file) override `SetUp` and call the base one first.

The completion popup logic lives in pure `StaticHelpers` functions — `extractLinkPath`, `linkPathReplaceStart`, `matchFileEntries`, `extractHtmlPath`, `htmlPathReplaceStart`, `extractEmojiCode` — so the pure unit tests in `test_editor_autocomplete.cpp` test the real code rather than a copy. If you change the completion matching/replacement semantics, update those pure tests and the key-event integration tests in `test_editor_completion_ui.cpp` (which drives the popup end-to-end via the harness).

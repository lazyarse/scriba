# AGENTS.md

## What

C++23 desktop Markdown editor using Qt6 (Widgets + WebEngine). Vendored markdown parser (md4c). GoogleTest (GTest) for tests.

## Build

Two build dirs, each serving a different job:

- `build-dbg/` — **Debug** build for the dev/test loop (`-DBUILD_TESTS=ON`). Fastest to compile; assertions on. Use this while developing and running the test suite.
- `build/` — **Release** build for the final binary only (tests OFF). Build it after the test suite passes; this is the binary you ship and package.

Full builds are heavy (WebEngine resources, JS bundles, many test targets). Use `timeout 12m` with bash or pass a large timeout value — the default 120s may not be enough.

### Dev loop (tests) — `build-dbg`

```bash
cmake --preset debug && cmake --build build-dbg -j4
cd build-dbg && ctest --output-on-failure -j4
```

Use true `Debug`, NOT `RelWithDebInfo`: RelWithDebInfo still compiles at `-O2`/`-O3`, so it builds at Release speed with none of the dev-loop benefit. Debug compiles far faster — the app sources compile once into `scriba_app`, and the ~18 full-app test targets just link it — at the cost of slower test runtimes, which barely matters since most tests are bound by WebEngine startup, not CPU.

### Release binary — `build`

```bash
# build only after tests pass; tests are OFF here (they live in build-dbg)
# clean only needed after branch switches (stale _autogen dirs);
# normal incremental rebuilds: skip clean, just configure + build
cmake --preset release && cmake --build build -j4
timeout 10 xvfb-run -a build/scriba || true   # smoke-test the freshly linked binary — AFTER this build, under Xvfb so it never opens on the main display
```

Binary: `build/scriba`

Both dirs are configured via `CMakePresets.json` (`cmake --preset debug` / `cmake --preset release`), so a deleted build dir can be recreated from scratch with the correct flags (`-DCMAKE_BUILD_TYPE` + `-DBUILD_TESTS`) — the preset is the canonical path. A bare `cmake --build build-dbg` on a missing dir fails, and a bare `cmake -B build-dbg` (no preset/flags) would default to `BUILD_TESTS=OFF` and an empty build type. Once configured, later rebuilds are just `cmake --build build-dbg -j4` / `cmake --build build -j4` (the flags live in each dir's `CMakeCache.txt`).

### Verification sequence (order matters)

Full check after any code/resource change:

1. `cmake --build build-dbg -j4` — dev/test loop build
2. `cd build-dbg && ctest --output-on-failure -j4` — tests pass
3. `cmake --build build -j4` — rebuild the Release binary
4. `timeout 10 xvfb-run -a build/scriba || true` — smoke-test the **freshly** built Release binary (under Xvfb so it never shows on the main display)
5. (UI changes only) `scripts/update-screenshot.sh <affected targets>` — regenerate the screenshot gallery from the **freshly** built Release binary

Steps 4 and 5 must come AFTER step 3. Running them right after step 1/2 smoke-tests / captures screenshots from a stale `build/scriba` from the previous Release build. The screenshot script expects a pre-built binary at `build/scriba` (it only builds one if none exists, which would then be a Release build anyway — but if a stale one is present it will happily capture that instead, so always rebuild first).

## Package (Linux)

```bash
cpack --config build/CPackConfig.cmake -G DEB
# copy to /tmp/ before installing if apt's _apt user can't traverse your home dir:
# cp scriba-*-Linux.deb /tmp/ && sudo apt install /tmp/scriba-*-Linux.deb
```

Output: `scriba-<version>-Linux.deb`

## Package (Windows — NSIS installer)

Requires [NSIS](https://nsis.sourceforge.io/) installed. Run in **x64 Native Tools Command Prompt for VS 2022**:

```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\msvc2022_64"
cmake --build build -j4 --config Release
cpack --config build/CPackConfig.cmake -G NSIS
```

Output: `scriba-<version>-win64.exe`

The installer adds Scriba to the Start Menu and registers `.md` files to open with Scriba.

## Run

```bash
build/scriba              # empty editor
build/scriba file.md      # open file
build/scriba corpus.scriba   # open a corpus (all its documents, watcher, active tab)
```

On Windows (x64 Native Tools Command Prompt for VS 2022):
```cmd
set PATH=C:\Qt\6.10.3\msvc2022_64\bin;%PATH%
build\Release\scriba.exe
build\Release\scriba.exe file.md
```

## Dependencies

Qt6: Core, Gui, Widgets, Network, WebEngineWidgets, WebChannel. Install on Debian/Ubuntu:
```bash
sudo apt install qt6-base-dev qt6-webengine-dev qt6-webchannel-dev
```

## Structure

- `src/` — all application code (headers + cpp), namespaced into per-concern subdirectories: `src/mainwindow/` (MainWindow + its split TUs), `src/editor/` (Editor + split TUs, Gutter, MdTable), `src/preview/` (Preview, PreviewBridge, Pagination, MarkdownParser/MdRenderer, JsRenderEngine, Typography, PrintOptions), `src/io/` (DOCX/HTML/PDF import+export, Ooxml converters, Zip/Csv readers), `src/css/` (theme/CSS machinery), `src/dialogs/` (smaller standalone dialogs), `src/charts/` (chart dialogs + ECharts/Mermaid parsers), `src/mermaid/` (MermaidDialog + GitGraphBuilder), `src/corpus/` (Corpus, CorpusIndex, CorpusWatcher, LinkFixer), `src/spell/` (SpellChecker/Highlighter, GrammarChecker, StoppardEngine), `src/validation/` (LinkValidator, MdLintConfig/MdLintEngine/MdLintRules), `src/prefs/` (Preferences + pages). The big classes are split into per-concern translation units (single class, one file per concern — see Key files for what lives where). Cross-cutting files stay at `src/` root: `main.cpp` and `StaticHelpers`
- **Include convention:** cross-directory includes are written with an explicit `dir/` prefix (`#include "editor/Editor.h"`); same-directory self-includes stay bare (`"Editor.h"`). `${CMAKE_SOURCE_DIR}/src` stays on the include path, so `tests/` and root files use bare includes. The `dir/` prefix makes the dependency greppable and collision-proof by construction — don't rely on bare includes resolving via a multi-directory `-I` path
- `vendor/md4c/` — vendored markdown parser (tracked in repo, with patches in `vendor/md4c/patches/`)
- `vendor/mathml2omml/` — vendored MathML↔OMML library (v3.0.0, working copy for local changes; scriba's `HtmlToOoxml.cpp` still needs adapting to the 3.0.0 API)
- `vendor/ta-lib/` — vendored TA-Lib C library (v0.7.1, BSD-3-Clause) powering the stock-chart indicator math in `src/charts/Indicators.cpp` (see Gotchas for the upgrade procedure)
- `resources/` — Qt QRC resources: CSS themes, JS libraries (mermaid, KaTeX, highlight.js, echarts, klinecharts, lightweight-charts, turndown, pdf.js), print styles, the shared preview shell (`preview-shell.html` + `preview-script.js`)
- `resources/themes/` — bundled CSS themes (15 themes: Catppuccin, Dracula, Nord, etc.)
- `tests/` — GoogleTest (GTest)-based test suites

## Key files

- `src/mainwindow/MainWindow.cpp` — app entry, ctor/`setupUi`, `updateStats`, showPreferences, apply-settings helpers. Split per concern:
  - `src/mainwindow/MainWindow_Tabs.cpp` — tab/add/remove/close/dirty bookkeeping (`addTab`, `loadFile`'s tab helpers, `showSaveDiscardDialog`, `promptUnsavedChanges`)
  - `src/mainwindow/MainWindow_File.cpp` — `loadFile`, `saveFile`, `renameCurrentFile`, `autoSave`, the three importers, `pasteAsMarkdown`, `exportPdf`/`exportDocx`/`exportHtml`, `closeEvent`
  - `src/mainwindow/MainWindow_Preview.cpp` — `updatePreview`, scroll sync, CSS watching, `buildPreviewShellHtml`
  - `src/mainwindow/MainWindow_Corpus.cpp` — corpus serialize/restore/save/open, corpus dictionary, watcher, link rewriting, recent-corpora menu, Table of Contents helpers
  - `src/mainwindow/MainWindow_Menu.cpp` — `setupMenuBar` + all per-menu builders, chart/insert-from-dialog slots, find/replace
- `src/editor/Editor.cpp` — core editor widget (ctor/dtor, wrapping/margins, spell painting, emoji cache, `keyPressEvent` dispatch shell, paste/mime, hover/tooltips, `applySpellSettings`, `applyCorpusDictionary`), split per concern:
  - `src/editor/EditorTyping.cpp` — per-key handlers, `insertParagraphWithLineHeight`, `applyAutoCorrect`, `toggleCheckbox`, `deleteLine`, `changeCodeLanguage`
  - `src/editor/EditorTable.cpp` — table formatting/branching (`formatMdTableBlock`, `onCursorPositionChanged`, insert/delete row/col)
  - `src/editor/EditorFolding.cpp` — fold scan/apply machinery, gutter (test_folding/gutter suites cover it)
  - `src/editor/EditorCompletions.cpp` — QCompleter machinery (file/emoji/language completions)
  - `src/editor/EditorMenu.cpp` — `contextMenuEvent` + misspelling/grammar submenus
  - `src/editor/EditorScrollBar.cpp` — scrollbar bookmarks/synchronization
  - `src/editor/IssueSummaryPane.cpp` — issue summary (spell/grammar/lint) panel
- `src/prefs/PreferencesDialog.cpp` — dialog shell (ctor, `setupUi` dispatch, search/filter) + six page units: `PreferencesPages.cpp`, `PreferencesPagesTheme.cpp`, `PreferencesPagesContent.cpp`, `PreferencesPagesSystem.cpp`, `PreferencesPagesAppearance.cpp`, `PreferencesPagesLint.cpp` (page builders must preserve widget object names — `test_preferences_search` keys on them)
- `src/mermaid/MermaidDialog.cpp` — dialog shell + git-graph/panels, split into `MermaidDialog_Flowchart.cpp`, `_Gantt.cpp`, `_Class.cpp`, `_State.cpp`, `_Others.cpp` (one per chart family: `build*`/`refresh*`/`saveCurrent`/`load…Data`)
- `src/io/OoxmlToHtml.cpp` — thin public entry; the `ooxmlconv::Converter` implementation lives in `src/io/OoxmlConverter.cpp`/`.h`
- `src/preview/MarkdownParser.cpp` — wraps md4c, emits HTML with `data-line` attributes
- `src/css/CssLoader.cpp` — loads user/system CSS, writes base stylesheets to `~/.config/scriba/`. Base CSS copies (preview/print) carry a `/* scriba-base-css-version: <sha256> */` marker; a copy whose marker doesn't match the bundled qrc hash is superseded (renamed `.bak`) and the bundled CSS used, with a one-time dialog on the next app start (see `MainWindow::setNotifyStaleCss`/`notifyStaleBaseCss`)
- `resources/scriba.qrc` — Qt resource bundle (must list any new resource files). CMakeLists.txt defines the source-list variables `SCRIBA_EDITOR_SOURCES`, `SCRIBA_MAINWINDOW_SOURCES`, `SCRIBA_APP_WEBENGINE_SOURCES` (and per-dialog lists) so adding a file to a split class is a one-line edit per target. The qrc compiles once into the `scriba_resources` object library (twemoji: `scriba_twemoji`); the 55 app sources in `SCRIBA_APP_WEBENGINE_SOURCES` compile once into the `scriba_app` object library, linked by `scriba` and all 18 full-app test targets. The Editor sources compile once into `scriba_editor` and the Preferences sources into `scriba_prefs` (both pulled out of `SCRIBA_APP_WEBENGINE_SOURCES`); the standalone editor/prefs test targets (`test_folding`, `test_gutter_*`, `test_editor_*`, ...) link `scriba_editor`/`scriba_prefs` instead of recompiling them

## Conventions

- Every Bash invocation must start with `date -R` — prefix commands as `date -R && <cmd>` (use `;` instead of `&&` when a later step must run even if an earlier one fails) so each tool execution and each command in the persistent shell session is time-stamped.
- Do not git commit, revert, add, delete, or otherwise mutate repo history without explicit permission
- Two build dirs: `build-dbg/` for the Debug dev/test loop, `build/` for the Release binary (see Build above)
- C++23, Qt coding style, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC` enabled
- No header-only files — every `.h` has a `.cpp`
- CSS theming: editor uses `#editor` selector, preview uses standard HTML selectors
- New source files must be added to both the appropriate `src/<dir>/` and a `SCRIBA_*_SOURCES` variable in `CMakeLists.txt`: app-class files (used by full-app test targets) go in `SCRIBA_APP_WEBENGINE_SOURCES` (picked up via `scriba_app`), everything else in `add_executable(scriba ...)`. Consumers of `scriba_app` must also link `scriba_resources` explicitly — object-library files do not propagate transitively. Note `SCRIBA_EDITOR_SOURCES`/`SCRIBA_PREFS_SOURCES` live in their own object libs (`scriba_editor`/`scriba_prefs`), so a consumer of the app sources that needs Editor/Preferences symbols must link those too
- All new `.cpp`/`.h` files (src, tests, vendored libs) must start with the GPL-3.0 license header: `// Copyright (C) 2026 LazyArse` followed by the standard "This program is free software..." notice (see LICENSE)
- New resource files must be added to both `resources/` and `resources/scriba.qrc`
- Post-build step deletes `~/.config/scriba/{editor,preview,print}-base.css` — don't rely on those persisting across builds. If a stale saved copy slips through anyway, the version-marker logic supersedes it to `.bak` and shows a one-time dialog
- Tests must only ever touch `~/.config/scribaTest/` (see `tests/TestConfig.h` / `setupTestConfig()`); never write to the real `~/.config/scriba/` from a test. Suites with a custom `main()` must call `setupTestConfig()`; config-focused suites link the `scriba_test_main` static library
- Dialog buttons (QDialogButtonBox and standalone QPushButton) must have icons stripped: `for (auto *btn : buttonBox->buttons()) btn->setIcon(QIcon());`. Add `&` keyboard shortcuts to all dialog buttons where possible (unique per dialog).
- Always rebuild after making changes — CSS, resource, or source files all require a rebuild to take effect
- After building the Release binary, run it briefly to check for segfaults: `timeout 10 xvfb-run -a build/scriba || true` (Xvfb so it never opens on the main display). This must run AFTER `cmake --build build -j4` — running it right after the build-dbg dev loop smoke-tests a stale `build/scriba` from the previous Release build (see the ordered Verification sequence under Build)
- Only rebuild the .deb package when explicitly asked to — do not rebuild it automatically after changes
- After adding a new keyboard shortcut, update `resources/shortcuts.html` to document it
- When you discover a markdown/rendering constraint, an edge case, or behavior that *looks* like a bug but is spec-correct (e.g. tight-vs-loose lists, or CommonMark's content-column indent rule for nested lists), document it in `docs/gotchas.md` rather than just a code comment — give future debugging a canonical place to look
- After adding a new menu item or any significant UI change, update `docs/images/screenshot.png` by running `scripts/update-screenshot.sh screenshot` (or the bare `scripts/update-screenshot.sh` for the full suite)
- `scripts/update-screenshot.sh` accepts targets to capture only specific dialogs/preferences pages instead of the whole slow suite (the full run takes minutes — WebEngine startup plus per-dialog waits). After a UI change, run it with just the affected target(s), e.g. `scripts/update-screenshot.sh preferences-spelling` or `scripts/update-screenshot.sh table-dialog katex-dialog`. Preference pages are named `preferences-<page>` (or the bare page name); run `scripts/update-screenshot.sh --help` for the full list
- When adding a new dialog — or a new page to an existing dialog (e.g. a new Preferences page) — add a capture of it to `scripts/update-screenshot.sh` (write a `shot_<name>()` function and add one `"<name>=<fn>|<help text>"` line to the `TARGETS` table at the top of the script — `--help`, argument validation, the full suite and the dispatch all derive from that table. For a new Preferences page, also add the page name to the `PREF_PAGES` array and the `idx` mapping in `shot_preference_page`), then re-run the script with just that target to regenerate the gallery in `docs/images/`, and link the new image in `docs/gallery.md` (the README embeds only the main `screenshot.png`; all other captures live in the gallery)
- `scripts/update-screenshot.sh` captures from the **Release** binary at `build/scriba`, so it must be run AFTER `cmake --build build -j4` — never right after the build-dbg dev loop, or it captures a stale `build/scriba` from the previous Release build. (See the Ordered Verification sequence under Build.) Always rebuild Release first.
- `scripts/update-screenshot.sh` and `scripts/create-autocomplete-demo.py` must send keys with XTEST (`xdotool windowfocus $WID` then `xdotool key ...` without `--window`); `xdotool key --window` (XSendEvent) does not trigger shortcuts in this Qt app
- The main window doesn't set its own size (opens at Qt's default 640×480), so xvfb/xdotool scripts must size it manually via `xdotool windowsize $WID WxH` — see `scripts/update-screenshot.sh:22` (1280×800) and `scripts/demo_helper.py:92` (1000×400, also `windowmove`d to 0,0)
- After changing autocomplete behavior, update `docs/images/autocomplete-demo.gif` by running `scripts/create-autocomplete-demo.py`. Requires: xvfb-run, xdotool, and Python packages from `scripts/requirements.txt` (`pip install -r scripts/requirements.txt`).
- After changing table formatting/alignment behavior, update `docs/images/table-demo.gif` by running `scripts/create-table-demo.py`. Both demo scripts share the Xvfb/xdotool/mss/GIF helpers in `scripts/demo_helper.py`.
- Create tests for each new feature — use GoogleTest (GTest), add test files to `tests/` directory and register in `CMakeLists.txt`
- Any new test that spawns QtWebEngine — links `Qt6::WebEngineWidgets`, constructs a `QWebEngineView`/`QWebEnginePage`, or pulls in `src/preview/Preview.cpp`/`MainWindow.cpp`/a dialog `.cpp` that creates one — gets parallel safety from `setupTestConfig()`: it sets a unique per-process application name, so each suite's WebEngine data dirs (AppDataLocation/CacheLocation) are isolated and WebEngine suites may run concurrently under `ctest -jN`. Make sure the new test calls `setupTestConfig()` (link `scriba_test_main`, or call it from a custom `main()`); no `RESOURCE_LOCK` is needed.
- For typing-centred integration tests, reuse the shared key-simulation fixture in `tests/EditorTestHarness.h` (GTest base class `EditorTestHarness` with `typeText`, `press`, `typeLine`, `run`, `placeCursor`, `selectLines`, `assertCursor`, `waitForFolds`); new test targets link the `test_editor_helpers` static library
- Sub-agents run in the background by default: dispatch them with the `delegate(prompt, agent, timeout_minutes?, model?)` tool (from the global `@aeondave/opencode-background-agents` plugin) so the main session keeps working instead of blocking, then read the result with `delegation_read(id)` once the completion notification arrives (use `delegation_status`/`delegation_peek`/`delegation_steer` for live supervision). Do not spin synchronous, blocking sub-agent work with the native `task` tool when `delegate` is available. Fallback when the plugin's tools are absent: fire-and-forget `nohup opencode run` processes (see the `background-sessions` skill)
- When presenting a plan or fix proposal, state it as normal text — do not use a structured question widget asking "shall I proceed/continue/go". Let the user reply naturally.

## CRITICAL

- NEVER COMMIT, REVERT, ADD, DELETE, CHECKOUT, STAGE, OR OTHERWISE MUTATE REPO HISTORY WITHOUT EXPLICIT PERMISSION. NO EXCEPTIONS.
- NEVER RUN `git stash` WITHOUT ASKING FOR CONFIRMATION FIRST. Stashing captures the whole working tree, so it can clobber or swallow simultaneous edits (the user or another agent may be editing the same files). If a stash is needed for a baseline check, ask first, and prefer non-destructive alternatives where possible.

## Gotchas

- `vendor/md4c/` contains the vendored markdown parser with local patches in `vendor/md4c/patches/`. To upgrade md4c: copy new upstream `src/*` into `vendor/md4c/src/`, then `git apply vendor/md4c/patches/*.patch` from the repo root. If patches fail, rework and update the .patch files. See `docs/md4c-flags.md` for the flag set to keep in sync.
- `vendor/mathml2omml/` is the vendored MathML↔OMML library (v3.0.0, working copy — local changes go straight into it, no patch workflow). To upgrade: copy new upstream `mathml2omml.{cpp,h}` over the vendored ones. The v3.0.0 API uses `std::string_view` throughout (`XmlSink` methods, `convert()` params) and the string-returning `convert()` returns `std::expected<std::string, std::string>`; `src/io/HtmlToOoxml.cpp` (`QtOmmlSink`) has been adapted accordingly.
- `vendor/ta-lib/` is the vendored TA-Lib C library (v0.7.1, BSD-3-Clause) powering the stock-chart indicator math in `src/charts/Indicators.cpp` (sma/ema/macd/rsi/boll/kdj). To upgrade: re-copy `include/` + `src/{ta_common,ta_func,ta_abstract}` from the new release tarball (the three src dirs are picked up by `file(GLOB ... CONFIGURE_DEPENDS)` in `vendor/ta-lib/CMakeLists.txt`), refresh the static `include/ta_config.h` (upstream generates it via autotools; in v0.7.1 no source file includes it), and re-check the lookbacks/conventions `Indicators.cpp` and `tests/test_indicators.cpp` consume (lookbacks: SMA/EMA `period-1`, RSI `period`, MACD `(slow-1)+(signal-1)`, STOCH `(fastK-1)+(slowK-1)+(slowD-1)`; conventions: MACD hist `macd−signal`, flat RSI → 0.0, KDJ = TA_STOCH + local `J = 3K−2D`). No patch workflow. License-header exception: vendored ta-lib files keep their upstream BSD-3-Clause headers — do NOT add the GPL header (miniz precedent).
- `vendor/stoppard/` carries a local `foldWord` fork-fix (see `vendor/stoppard/AGENTS.md` → Vendored drift) — re-apply it after every stoppard sync until the fix lands upstream.
- Tests exist in `tests/` — run with `cd build-dbg && ctest --output-on-failure -j4` after building `build-dbg` with `-DBUILD_TESTS=ON`. Always run ctest from `build-dbg/` — running it from the repo root finds no tests and leaves a `Testing/Temporary/` bookkeeping dir in the source tree (gitignored via `/Testing/`, but still clutter). Tests auto-wrap in `xvfb-run` when available. When invoking a single test binary directly (e.g. `build-dbg/test_editor_typing --gtest_filter=...`), bypassing ctest skips the auto-wrap — run it on a virtual screen instead: `xvfb-run -a bash -c 'build-dbg/test_editor_typing ...'`. Otherwise it opens on the main display, where real keystrokes can land in the focused test window and masquerade as typing/tab flakiness (stray characters appearing mid-test). For repeated runs, keep one Xvfb across the whole loop so only one virtual server is spun up. Running in parallel is safe: `setupTestConfig()` (tests/TestConfig.h) isolates each test's config into a per-process temp dir (`QDir::tempPath()`, wiped on use) and gives each process a unique application name, so Qt WebEngine's disk-based data dirs (AppDataLocation/CacheLocation) are per-process too and even WebEngine-spawning suites may run concurrently under `ctest -jN`. `-j4` runs the whole suite in roughly half the wall time of `-j1` with no flakes.
- Editor typing integration tests (`test_editor_typing` and `test_editor_typing_2`, split to run in parallel) drive the real `Editor` widget with `QTest` key events — no WebEngine, deterministic input. Tests asserting exact sentinel semantics (empty list marker clears to a bare newline, blank table row exits the table) verify real `keyPressEvent` behaviour: if they fail it is a genuine behaviour regression, NOT flakiness. Do not rerun to "confirm" — investigate.
- `QTextDocument::contentsChange` vs `contentsChanged` (verified empirically against Qt 6.10): on a bare `QTextDocument`, `QTextCursor::insertText()` emits ONLY `contentsChanged` — no `contentsChange`. Typing into a real `QTextEdit` (or `QTextCursor` on its document) does emit `contentsChange` with real removed/added counts. Format-only work (`QSyntaxHighlighter::rehighlight()`, spell/syntax highlighting) emits `contentsChange` with `(0, 0)` in-app. So: tests asserting `contentsChange`-driven behavior (e.g. grammar-lint scheduling) must drive a real `QTextEdit` via `QTest::keyClicks`/`keyClicks`, and code that must react only to real edits should connect `contentsChange` and skip `removed == 0 && added == 0` (see `SpellHighlighter::scheduleGrammarLint` and `MainWindow.cpp:342`).
- `QTextDocument::isModified()` is an undo-stack-anchored flag (`modifiedState != undoState`, recomputed in `QTextDocumentPrivate::contentsChanged()`), NOT a content comparison — and `modificationChanged` fires only on transitions. Two ways it desyncs: (1) `setModified(false)` when the flag is already `false` is a no-op (early-returns in `QTextDocumentPrivate::setModified`), so it can't casually re-anchor; (2) a `QSignalBlocker` on the *document* (only `MainWindow::applyEditorLineHeight`) suppresses the `modificationChanged` emission but not the internal recompute, so a blocked format op leaves `isModified()==true` while the app's marker never updated — a fresh `MainWindow`'s first tab starts internally-modified, and the next edit is no *transition*, so the tab never shows as dirty. That's why tab dirty tracking does NOT use `isModified`/`modificationChanged` at all: it compares `Md5(toPlainText())` against a saved hash (`TabInfo::savedHash`, refreshed by `MainWindow::setTabSaved`), with `contentsChange` marking dirty immediately on real edits and undo/redo detected via `availableUndoSteps()/availableRedoSteps()` deltas (`lastUndoSteps`/`lastRedoSteps`) then re-checked against the hash (see the handler in `MainWindow::addTab`). `setModified(false)` is still called in `setTabSaved` for `isModified` consumers/tests, and `clearUndoRedoStacks()` must run BEFORE it in load paths so the anchor isn't left stale. If you ever add another document-level `QSignalBlocker` around a text-format op, the hash approach stays correct by construction.
- Tests must be cross-platform (Linux + Windows). Never hardcode `/tmp`, `file:///tmp/`, or any platform-specific paths. Use `QDir::tempPath()`, `QTemporaryDir`, `QTemporaryFile`, and `QUrl::fromLocalFile()` instead.
- `Qt6::WebEngineWidgets` is a heavy dependency; requires `xvfb` for headless/CI — install with `sudo apt install xvfb`.
- Admonition support is CSS-only (`::before` pseudo-elements), not markdown parser extensions.
- `docs/kitchensink.md` is the full-rendering example document (used by `scripts/update-screenshot.sh`): it exercises every renderer the preview supports — mermaid, KaTeX, ECharts, highlight.js, tables, admonitions, emoji. When changing rendering, open it in the app to visually check all features.
- `docs/typesetting-example.md` is the printable PDF-typesetting demo and tutorial (directives + every option); export it to PDF for the manual eyeball check of the print feature.
- The code-fence language autocomplete list is hardcoded in `src/editor/EditorCompletions.cpp` (`codeLanguages()`, at :330). When bumping `resources/highlight.min.js`, re-extract the bundled languages with `grep -oE 'grmr_[a-zA-Z0-9+-]+' resources/highlight.min.js` and keep that table (and its aliases) in sync. The table also includes app-rendered languages that hljs has no grammar for (e.g. `mermaid`, `ec` — rendered by mermaid.js/ECharts in the preview) — keep those too.
- This app must work fully offline. No CDN, no network-dependent features. All assets (JS, fonts, SVG) must be bundled via qrc.

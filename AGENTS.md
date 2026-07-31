# AGENTS.md

## What

C++17 desktop Markdown editor using Qt6 (Widgets + WebEngine). Vendored markdown parser (md4c). Qt Test framework for tests.

## Build

Full build is heavy (WebEngine resources, JS bundles, many test targets). Use `timeout 300000` with bash or pass a large timeout value — the default 120s may not be enough.

```bash
mkdir -p build
# clean only needed after branch switches (stale _autogen dirs);
# normal incremental rebuilds: skip clean, just configure + build
cmake --build build --target clean 2>/dev/null
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4
```

Binary: `build/scriba`

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
```

On Windows (x64 Native Tools Command Prompt for VS 2022):
```cmd
set PATH=C:\Qt\6.10.3\msvc2022_64\bin;%PATH%
build\Release\scriba.exe
build\Release\scriba.exe file.md
```

## Dependencies

Qt6: Core, Gui, Widgets, Network, WebEngineWidgets. Install on Debian/Ubuntu:
```bash
sudo apt install qt6-base-dev qt6-webengine-dev
```

## Structure

- `src/` — all application code (headers + cpp), flat layout
- `vendor/md4c/` — vendored markdown parser (tracked in repo, with patches in `vendor/md4c/patches/`)
- `resources/` — Qt QRC resources: CSS themes, JS libraries (mermaid, KaTeX, highlight.js, vega), print styles
- `resources/themes/` — bundled CSS themes (15 themes: Catppuccin, Dracula, Nord, etc.)
- `tests/` — Qt Test-based test suites

## Key files

- `src/MainWindow.cpp` — app entry, file I/O, CSS management, scroll sync
- `src/MarkdownParser.cpp` — wraps md4c, emits HTML with `data-line` attributes
- `src/CssLoader.cpp` — loads user/system CSS, writes base stylesheets to `~/.config/scriba/`
- `resources/scriba.qrc` — Qt resource bundle (must list any new resource files)

## Conventions

- Do not git commit, revert, add, delete, or otherwise mutate repo history without explicit permission
- Always build with `-DCMAKE_BUILD_TYPE=Release` unless debugging
- C++17, Qt coding style, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC` enabled
- No header-only files — every `.h` has a `.cpp`
- CSS theming: editor uses `#editor` selector, preview uses standard HTML selectors
- New source files must be added to both `src/` and the `add_executable(scriba ...)` list in `CMakeLists.txt`. If `MainWindow.cpp` uses the new class, also add it to the `test_scroll_sync` target (which compiles `MainWindow.cpp` directly)
- New resource files must be added to both `resources/` and `resources/scriba.qrc`
- Post-build step deletes `~/.config/scriba/{editor,preview}-base.css` — don't rely on those persisting across builds
- Dialog buttons (QDialogButtonBox and standalone QPushButton) must have icons stripped: `for (auto *btn : buttonBox->buttons()) btn->setIcon(QIcon());`. Add `&` keyboard shortcuts to all dialog buttons where possible (unique per dialog).
- Always rebuild after making changes — CSS, resource, or source files all require a rebuild to take effect
- After building, run the application briefly to check for segfaults: `timeout 3 build/scriba || true`
- Only rebuild the .deb package when explicitly asked to — do not rebuild it automatically after changes
- After adding a new keyboard shortcut, update `resources/shortcuts.html` to document it
- After adding a new menu item or any significant UI change, update `docs/images/screenshot.png` by running `scripts/update-screenshot.sh`
- After changing autocomplete behavior, update `docs/images/autocomplete-demo.gif` by running `scripts/create-autocomplete-demo.py`. Requires: xvfb-run, xdotool, and Python packages from `scripts/requirements.txt` (`pip install -r scripts/requirements.txt`).
- Create tests for each new feature — use Qt Test framework (QTest), add test files to `tests/` directory and register in `CMakeLists.txt`
- When presenting a plan or fix proposal, state it as normal text — do not use a structured question widget asking "shall I proceed/continue/go". Let the user reply naturally.

## CRITICAL

- NEVER COMMIT, REVERT, ADD, DELETE, CHECKOUT, STAGE, OR OTHERWISE MUTATE REPO HISTORY WITHOUT EXPLICIT PERMISSION. NO EXCEPTIONS.

## Gotchas

- `vendor/md4c/` contains the vendored markdown parser with local patches in `vendor/md4c/patches/`. To upgrade md4c: copy new upstream `src/*` into `vendor/md4c/src/`, then `git apply vendor/md4c/patches/*.patch` from the repo root. If patches fail, rework and update the .patch files.
- Tests exist in `tests/` — run with `cd build && ctest --output-on-failure -j1` after building with `-DBUILD_TESTS=ON`. Tests auto-wrap in `xvfb-run` when available. Run with `-j1` (not parallel) to avoid WebEngine fork contention causing flaky failures.
- Tests must be cross-platform (Linux + Windows). Never hardcode `/tmp`, `file:///tmp/`, or any platform-specific paths. Use `QDir::tempPath()`, `QTemporaryDir`, `QTemporaryFile`, and `QUrl::fromLocalFile()` instead.
- `Qt6::WebEngineWidgets` is a heavy dependency; requires `xvfb` for headless/CI — install with `sudo apt install xvfb`.
- Admonition support is CSS-only (`::before` pseudo-elements), not markdown parser extensions.
- The code-fence language autocomplete list is hardcoded in `src/Editor.cpp` (`codeLanguages()`). When bumping `resources/highlight.min.js`, re-extract the bundled languages with `grep -oE 'grmr_[a-zA-Z0-9+-]+' resources/highlight.min.js` and keep that table (and its aliases) in sync.
- This app must work fully offline. No CDN, no network-dependent features. All assets (JS, fonts, SVG) must be bundled via qrc.

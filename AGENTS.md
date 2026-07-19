# AGENTS.md

## What

C++17 desktop Markdown editor using Qt6 (Widgets + WebEngine). Vendored markdown parser (md4c). Qt Test framework for tests.

## Build

```bash
mkdir -p build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

Binary: `build/scriba`

## Package

```bash
cpack --config build/CPackConfig.cmake -G DEB
```

Output: `scriba-<version>-Linux.deb`

## Run

```bash
build/scriba              # empty editor
build/scriba file.md      # open file
```

## Dependencies

Qt6: Core, Gui, Widgets, Network, WebEngineWidgets. Install on Debian/Ubuntu:
```bash
sudo apt install qt6-base-dev qt6-webengine-dev
```

## Structure

- `src/` — all application code (headers + cpp), flat layout
- `vendor/md4c/` — vendored markdown parser (gitignored, must exist at build time)
- `resources/` — Qt QRC resources: CSS themes, JS libraries (mermaid, KaTeX, highlight.js, vega), sample.md, print styles
- `resources/themes/` — bundled CSS themes (15 themes: Catppuccin, Dracula, Nord, etc.)
- `tests/` — Qt Test-based test suites

## Key files

- `src/MainWindow.cpp` — app entry, file I/O, CSS management, scroll sync
- `src/MarkdownParser.cpp` — wraps md4c, emits HTML with `data-line` attributes
- `src/CssLoader.cpp` — loads user/system CSS, writes base stylesheets to `~/.config/Scriba/Scriba/`
- `resources/scriba.qrc` — Qt resource bundle (must list any new resource files)

## Conventions

- Do not git commit, revert, add, delete, or otherwise mutate repo history without explicit permission
- Always build with `-DCMAKE_BUILD_TYPE=Release` unless debugging
- C++17, Qt coding style, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC` enabled
- No header-only files — every `.h` has a `.cpp`
- CSS theming: editor uses `#editor` selector, preview uses standard HTML selectors
- New source files must be added to both `src/` and the `add_executable(scriba ...)` list in `CMakeLists.txt`. If `MainWindow.cpp` uses the new class, also add it to the `test_scroll_sync` target (which compiles `MainWindow.cpp` directly)
- New resource files must be added to both `resources/` and `resources/scriba.qrc`
- Post-build step deletes `~/.config/Scriba/Scriba/{editor,preview}-base.css` — don't rely on those persisting across builds
- QDialogButtonBox buttons must have icons stripped: `for (auto *btn : buttonBox->buttons()) btn->setIcon(QIcon());`
- Always rebuild after making changes — CSS, resource, or source files all require a rebuild to take effect
- After building, run the application briefly to check for segfaults: `timeout 3 build/scriba || true`
- Rebuild the .deb package after any source/resource change: `cpack --config build/CPackConfig.cmake -G DEB`
- After adding a new menu item or any significant UI change, update `docs/images/screenshot.png` by running `scripts/update-screenshot.sh`
- Create tests for each new feature — use Qt Test framework (QTest), add test files to `tests/` directory and register in `CMakeLists.txt`

## Gotchas

- `vendor/` is gitignored. Clone with submodules or ensure `vendor/md4c/` exists before building.
- Tests exist in `tests/` — run with `cd build && ctest --output-on-failure` after building with `-DBUILD_TESTS=ON`.
- `Qt6::WebEngineWidgets` is a heavy dependency; headless/CI environments may need `xvfb-run`.
- Admonition support is CSS-only (`::before` pseudo-elements), not markdown parser extensions.
- This app must work fully offline. No CDN, no network-dependent features. All assets (JS, fonts, SVG) must be bundled via qrc.

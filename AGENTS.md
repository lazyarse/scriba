# AGENTS.md

## What

C++17 desktop Markdown editor using Qt6 (Widgets + WebEngine). Vendored markdown parser (md4c). No tests, no CI, no linter.

## Build

```bash
mkdir -p build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

Binary: `build/scriba`

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
- `resources/` — Qt QRC resources: CSS themes, mermaid.js, sample.md, print styles
- `resources/themes/` — bundled CSS themes (Catppuccin, Dracula, Nord, etc.)

## Key files

- `src/MainWindow.cpp` — app entry, file I/O, CSS management, scroll sync
- `src/MarkdownParser.cpp` — wraps md4c, emits HTML with `data-line` attributes
- `src/CssManager.cpp` — loads user/system CSS, writes base stylesheets to `~/.config/Scriba/Scriba/`
- `resources/scriba.qrc` — Qt resource bundle (must list any new resource files)

## Conventions

- Do not git commit, revert, add, delete, or otherwise mutate repo history without explicit permission
- Always build with `-DCMAKE_BUILD_TYPE=Release` unless debugging
- C++17, Qt coding style, `CMAKE_AUTOMOC`/`CMAKE_AUTORCC` enabled
- No header-only files — every `.h` has a `.cpp`
- CSS theming: editor uses `#editor` selector, preview uses standard HTML selectors
- New source files must be added to both `src/` and the `add_executable()` list in `CMakeLists.txt`
- New resource files must be added to both `resources/` and `resources/scriba.qrc`
- Post-build step deletes `~/.config/Scriba/Scriba/{editor,preview}-base.css` — don't rely on those persisting across builds
- QDialogButtonBox buttons must have icons stripped: `for (auto *btn : buttonBox->buttons()) btn->setIcon(QIcon());`
- Always rebuild after making changes — CSS, resource, or source files all require a rebuild to take effect
- After building, run the application briefly to check for segfaults: `timeout 3 build/scriba || true`

## Gotchas

- `vendor/` is gitignored. Clone with submodules or ensure `vendor/md4c/` exists before building.
- No automated tests. Manual verification only — build and run the app.
- `Qt6::WebEngineWidgets` is a heavy dependency; headless/CI environments may need `xvfb-run`.
- Admonition support is CSS-only (`::before` pseudo-elements), not markdown parser extensions.

# Codebase Tidy-Up Plan

## Priority 1: Bugfixes (behavior-changing)
- **MermaidSequenceDialog** — missing `connect(delBtn, ...)` calls after rows are added; delete buttons don't work
- **MermaidFlowchartDialog** — add-row logic issue
- **ExportPdfDialog** `mermaidInitJs` — doesn't return the `mermaid.run()` promise, differing from MainWindow version
- **`escapeJsString()`** — doesn't escape `"` or backticks, fragile for double-quoted JS contexts

## Priority 2: Remove Stale Artifacts (~1.5 GB)
- Delete 17 accumulated `.deb` files from repo root (v1.4.1 → v1.9.0)
- Remove unused `resources/icons/scriba-inverted.svg` (not in QRC)
- Remove dead code: `extractContentWidth()`, `firstFontFamily()` in `StaticHelpers`

## Priority 3: Major Duplication Reduction
- **JS snippets** (`mermaidInitJs`, `headingIdJs`, `katexInitJs`, `vegaLiteInitJs`, `setImgTitlesJs`) — copy-pasted in both `MainWindow.cpp` and `ExportPdfDialog.cpp` with subtle differences. Extract to a shared source file (e.g. `JsSnippets.h/cpp`)
- **12 `showMermaid*` methods** in `MainWindow.cpp` — identical body, only dialog class changes. Replace with a template method or macro
- **Delete-button lambda** — duplicated ~20× across 12 Mermaid dialog files. Move into `MermaidDialogBase`
- **3 Mermaid dialogs bypassing `setupMainLayout()`** — `MermaidClassDialog`, `MermaidErDialog`, `MermaidMindmapDialog` manually duplicate the right-panel + button-bar code. Refactor to use base class

## Priority 4: Consistency & Modernization
- **Header guards**: Unify all 34 `.h` files to `#pragma once` (currently 31x `#ifndef`, 3x `pragma once`)
- **`CssLoader::printCss()`**: Remove `const_cast` by making cache members `mutable` or removing `const`
- **`clearSentinel` (0x2412)**: Currently defined as `static const QChar` in 4 places. Move to a shared location (`StaticHelpers`)
- **Make local `QRegularExpression` objects `static const`** where possible (Editor.cpp creates some on every keypress)

## Priority 5: CMakeLists.txt Cleanup
- Test target list is massively repetitive — define a helper function/loop for the Mermaid dialog test targets

## Priority 6: Minor Polish
- Deduplicate file dialog filter strings (`"Markdown Files (*.md);;All Files (*)"` appears in 3 places with a 4th variant)
- Extract magic numbers (splitter sizes, debounce intervals, font sizes, button sizes) into named constants
- Strip `QIcon()` from dialog buttons (existing pattern, verify all dialogs follow it)

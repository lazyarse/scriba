# Codebase Tidy-Up Plan

## Priority 1: Bugfixes (behavior-changing)
### 1.1 MermaidSequenceDialog — message table rows missing delete buttons
- Lines 70-78: `m_messageTable` created with 2 rows, `addMessageDeleteButton` lambda defined, but never called for the 2 initial rows
- **Fix**: Add `addMessageDeleteButton(0); addMessageDeleteButton(1);` after line 89

### 1.2 MermaidFlowchartDialog — missing `schedulePreviewUpdate()` in add-row lambdas
- Add-node lambda (line 176): missing `schedulePreviewUpdate()`
- Add-edge lambda (line 185): missing `schedulePreviewUpdate()`
- **Fix**: Add call at end of both lambdas

### 1.3 MermaidPieDialog — missing `schedulePreviewUpdate()` + column init in add-row
- Add-row lambda (lines 71-75): missing both the update call and `setItem()` for new row cells
- **Fix**: Add `schedulePreviewUpdate()` and initialize label/value columns with empty items

### 1.4 ExportPdfDialog — mermaidInitJs doesn't return promise (deferred)
- Needs investigation — deferred for now

### 1.5 `escapeJsString()` — missing `"` and `` ` `` escaping
- StaticHelpers.cpp:11-16: only escapes `\`, `'`, `\n`, `\r`
- **Fix**: Add `.replace("\"", "\\\"").replace("`", "\\`")`

## Priority 2: Remove Stale Artifacts (done)
- Delete 17 accumulated `.deb` files from repo root
- Remove unused `resources/icons/scriba-inverted.svg`
- Remove dead code: `extractContentWidth()`, `firstFontFamily()` in `StaticHelpers`

## Priority 3: Major Duplication Reduction
### 3.1 Extract 5 shared JS snippets into `JsSnippets.h/cpp`
- mermaidInitJs, headingIdJs, katexInitJs, vegaLiteInitJs, setImgTitlesJs
- currently copy-pasted in MainWindow.cpp and ExportPdfDialog.cpp with subtle differences
- Create shared source with `extern const QString` constants
- Register in CMakeLists.txt add_executable and scriba_core

### 3.2 Collapse 12 `showMermaid*()` methods into a template
- 12 methods in MainWindow.cpp with identical body, only dialog class changes
- Replace with `template<typename T> void showMermaidDialog()`
- Remove 12 individual slot declarations from MainWindow.h

### 3.3 Move delete-button lambda into `MermaidDialogBase`
- Add `void addDeleteButton(QTableWidget *table, int column, int row)`
- Update all 12 Mermaid dialogs
- Handle 3 dialogs with post-delete refresh (Flowchart, Sequence, State)

### 3.4 Refactor MermaidClass/Er/Mindmap to use `setupMainLayout()`
- 3 dialogs bypass setupMainLayout(), duplicating ~50-70 lines each
- Refactor constructor to call setupUi() + setupMainLayout() pattern

## Priority 4: Consistency & Modernization (done)
- Header guards: all 34 files now use `#pragma once`
- CssLoader::printCss(): removed const_cast
- clearSentinel: moved to shared location
- QRegularExpression objects made static const

## Priority 5: CMakeLists.txt Cleanup (done)
- add_mermaid_test() function replaces 12 repetitive test targets

## Priority 6: Minor Polish (done)
- Deduplicated file dialog filter strings
- Extracted auto-save magic number
- Button icon stripping verified across all dialogs

# Table Handling

Markdown table insertion and keyboard navigation.

## Inserting a Table

**Tools > Table Insert...** (Ctrl+T) opens a dialog to create a table with a chosen number of columns and an optional header row.

- **With header** — generates a markdown table with header, separator, and one data row
- **Without header** — generates an HTML `<table>` with one row

Cursor is placed in the first cell.

## Implementation

- `src/StaticHelpers.cpp` — `handleTableReturn()` and `tableNavCell()`
- `src/src/editor/Editor.cpp` — `keyPressEvent()` Enter and Tab handlers
- `src/src/dialogs/TableDialog.cpp` — table insertion dialog

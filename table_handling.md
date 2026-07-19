# Table Handling

Markdown table insertion and keyboard navigation.

## Inserting a Table

**Tools > Table Insert...** (Ctrl+T) opens a dialog to create a table with a chosen number of columns and an optional header row.

- **With header** — generates a markdown table with header, separator, and one data row
- **Without header** — generates an HTML `<table>` with one row

Cursor is placed in the first cell.

## Keyboard Navigation

### Tab

- Inside a table cell: moves cursor to the **next cell** in the same row
- At the last cell of a row: moves cursor to the **first cell of the next data row** (skips separator rows)
- If no next row exists: behaves as normal Tab

### Shift+Tab

- Inside a table cell: moves cursor to the **previous cell** in the same row
- At the first cell of a row: moves cursor to the **last cell of the previous data row** (skips separator rows)
- If no previous row exists: behaves as normal Shift+Tab

### Enter (Row Continuation)

Enter anywhere in a table row (start, middle, or end of any cell) creates a new row — no cell content spills over.

- On the **first row** of a new table (line above is not a table row): inserts a separator `|---|---|` and a data row `|  |  |`, cursor lands in the first cell of the data row
- On a **data row** (line above is also a table row): inserts a new data row below with the same column count, cursor lands in the first cell
- On a **separator row** (`|---|---|` or similar): behaves as normal Enter (inserts a blank line)
- On a **blank row** (all cells empty): clears the row and exits table auto-completion (cursor moves to a new blank line)

## Implementation

- `src/StaticHelpers.cpp` — `handleTableReturn()` and `tableNavCell()`
- `src/Editor.cpp` — `keyPressEvent()` Enter and Tab handlers
- `src/TableDialog.cpp` — table insertion dialog

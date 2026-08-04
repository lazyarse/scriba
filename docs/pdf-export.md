# PDF Export

## How it works

The export dialog (`ExportPdfDialog`) renders the markdown as HTML inside a hidden
`QWebEngineView`, waits for async JS (KaTeX, Mermaid, ECharts) to finish, then
generates a PDF using one of two paths:

1. **Headless Chromium** (preferred) — serializes the rendered DOM and pipes it
   through `chromium --headless=new --no-margins --no-pdf-header-footer
   --print-to-pdf=...`. Requires Chromium ≥ 140 (with `--headless=new` support).

2. **Qt `printToPdf` fallback** — used when no Chromium binary is found. Cannot
   suppress default PDF headers/footers.

## Page size parsing

The `@page { size: ... }` CSS rule is parsed to determine the viewport width for
JS rendering (so ECharts charts render at the correct dimensions).

### Supported named sizes

| Name     | Width (pt) | Height (pt) |
|----------|------------|-------------|
| A3       | 841.9      | 1190.6      |
| A4       | 595.3      | 841.9       |
| A5       | 419.5      | 595.3       |
| Letter   | 612        | 792         |
| Legal    | 612        | 1008        |
| Tabloid  | 792        | 1224        |

Orientation keywords `landscape` / `portrait` are recognised and swap dimensions
accordingly. Explicit dimensions (e.g. `size: 210mm 297mm`) are also supported.

### Limitations

- Only the **first** `@page` block is parsed. Multiple `@page` rules targeting
  different pages (`:first`, `:left`, `:right`, named pages) are ignored.
- Only `size` and `margin` are read from the `@page` block. Other properties
  (`marks`, `bleed`, etc.) are not inspected by the dialog — they pass through
  to the browser engine normally.
- If no `@page` block is found, defaults to A4 portrait with zero margins.

## Requirements

- **Chromium ≥ 140** (for `--headless=new`). Install via:
  `sudo apt install chromium` (Debian/Ubuntu) or your distribution's package
  manager. The binary is searched at `/usr/bin/chromium`,
  `/usr/bin/chromium-browser`, `/usr/bin/google-chrome`, and `$PATH`.

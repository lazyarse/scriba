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

**Platform note:** the Chromium binary search (`ExportPdfDialog::findChromiumBinary`)
probes only Linux paths (`/usr/bin/chromium`, `/usr/bin/chromium-browser`,
`/usr/bin/google-chrome`, `/usr/bin/google-chrome-stable`, and `chromium` on
`$PATH`). On Windows none of these resolve, so PDF export always uses the Qt
`printToPdf` fallback there — default header/footer suppression and the full
CSS Paged Media extras (margin boxes, `:first`/`:left`/`:right`, named pages)
are unavailable on Windows unless a `chromium.exe`/`chrome.exe` probe is added.

## Page size parsing

The `@page { size: ... }` CSS rule is parsed to determine the viewport width for
JS rendering (so ECharts charts render at the correct dimensions) and the page
geometry for the Qt `printToPdf` fallback. When more than one `@page` block is
present, the **last** one wins — the per-export typesetting override is appended
after `print-base.css`'s own `@page { margin: 15mm; }` precisely so it overrides
the base block.

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

- Only the **last** `@page` block is parsed. Multiple `@page` rules targeting
  different pages (`:first`, `:left`, `:right`, named pages) are ignored.
- Only `size` and `margin` are read from the `@page` block. Other properties
  (`marks`, `bleed`, etc.) are not inspected by the dialog — they pass through
  to the browser engine normally.
- If no `@page` block is found, defaults to A4 portrait with zero margins.

## Typesetting options

Typesetting for PDF export is controlled by defaults in **Preferences →
Printing**, overridable per export via the **Typesetting** group in the export
dialog (any change there applies only to that export; **Reset to saved
defaults** reverts to the saved settings).

| Option | Default | Effect |
|--------|---------|--------|
| Split code blocks | Never | Never: code blocks never split across pages. Over 50/100 lines: blocks taller than the page content box are allowed to split; everything else stays together. |
| Keep tables together | On | Tables stay on one page where possible. |
| Keep headings with following text | On | Headings avoid a page break directly after them. |
| Keep figures together | On | Mermaid, KaTeX, ECharts, admonitions and code blocks stay on one page where possible. |
| Avoid orphan/widow lines | On | `orphans`/`widows` set to 1 on paragraphs. **Caveat:** Chromium's support in print layout is partial/unreliable — best-effort only. |
| Margin / Page size | Base default | Free-form CSS values, e.g. `18mm` or `A4 landscape`. |

### In-source directives

Two HTML-comment directives control page breaks at specific points in the
document. They are invisible in the rendered preview and in the exported PDF,
and are a documented extension — see `docs/gotchas.md` for the exact contract
(flush-left, own line; it must form its own block):

```markdown
<!-- keep -->
```python
def hello():
    print("Hello, Scriba!")
```
```

```markdown
<!-- break -->

## Next Section
```

`<!-- keep -->` pins the next top-level block (code, table, figure, paragraph,
…) to one page. `<!-- break -->` forces a page break before it.

For a printable demonstration of every option and both directives, export
[`docs/typesetting-example.md`](typesetting-example.md) to PDF — the file is a
brief tutorial and a live example in one.

## Requirements

- **Chromium ≥ 140** (for `--headless=new`). Install via:
  `sudo apt install chromium` (Debian/Ubuntu) or your distribution's package
  manager. The binary is searched at `/usr/bin/chromium`,
  `/usr/bin/chromium-browser`, `/usr/bin/google-chrome`, and `$PATH`.

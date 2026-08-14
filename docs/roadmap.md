# Roadmap

General roadmap for planned Scriba changes, plus per-feature status notes.
Each entry keeps the status legend below.

## Status legend

- **Done** — shipped, tested, documented.
- **Partial** — works, but only covers a documented subset.
- **Not started** — planned, not yet built.

## Future changes

### Ordered-list numbering — DOCX `start` support

Preview, print/PDF, exported HTML and DOCX export now honour the
`OrderedListMarker` preference (decimal / lowercase letters / lowercase roman,
`.` or `)` suffix; see the plan in
`docs/superpowers/plans/2026-08-13-ordered-list-numbering.md`).

The renderer preserves the source start number (`2024.` → `<ol start="2024">`)
in the preview and HTML/PDF exports. **DOCX export does not yet map `start`**:
Word needs a separate `w:num`/`w:abstractNum` instance per distinct start value,
with each list paragraph's `w:numPr` pointing at the right one (`handleList` in
`src/src/io/HtmlToOoxml.cpp` would allocate `w:numId`s). Status: **Partial** — next
phase.

### Ordered-list numbering — uppercase numerals

Uppercase variants (`A.` / `I.`) are not offered in the preference yet. Trivial
to add: two `@counter-style`/`list-style-type` rules in the base CSS, two combo
items, and a few renderer enum values. Status: **Not started**.

---

## ECharts helper roadmap

The Chart Builder, Stock Chart Builder and Advanced Charts dialogs reverse-parse
an existing ` ```ec ` block back into their table rows (see
`src/src/charts/EChartsParser.cpp`). The doc-driven corpus in `docs/echarts.md` (21 blocks)
is exercised by `tests/test_chartsource_docs.cpp` and must round-trip.

| Series type | Dialog | Status |
|---|---|---|
| bar / line / area / scatter / pie / funnel / gauge | Chart Builder | Done |
| radar | Chart Builder | Done |
| heatmap (matrix + calendar) | Chart Builder | Done |
| effectScatter / pictorialBar | Chart Builder (with ripple / repeat toggles) | Done |
| candlestick | Stock Chart | Done |
| sankey | Advanced Charts | Done |
| boxplot | Advanced Charts | Done |
| parallel | Advanced Charts | Done |
| themeRiver | Advanced Charts | Done |
| graph | Advanced Charts | Done |
| treemap / sunburst | Advanced Charts (shared tree editor) | Done |
| custom / map | raw-source fallback | Not started |

## Known round-trip limitations

All the "reverse into the table" parsers **preserve the data, not the
decoration**: layout options, tooltips, visual maps, `force`/`roam`, axis
types, colours and legends are dropped on re-insertion. See
`docs/gotchas.md` → "Advanced Charts pencil-edit preserves data, not chart
styling".

- **Internal node values in treemap/sunburst** — the flattened leaf view loses
  the value of an internal node that also has children. A better rebuild would
  infer total values from leaf sums, but that is not implemented.
- **Multi-series specs** — only `series[0]` is recovered (the styled example
  of `docs/echarts.md`). Multi-series editing deserves its own data model
  (a "series" column or group prefix) — parked.

## Ideas not yet scoped

- A "Paste from ECharts website" importer that accepts the copied spec from
  https://echarts.apache.org examples into whichever dialog matches.
- Sparkline / mini-upsell in the preview — let ECharts drive an animated
  preview in the dialog (already does) and add a "insert as image" path.
- Generic two-table "sankey builder" already covered by the Advanced Charts
  panel; no separate Sankey dialog planned.
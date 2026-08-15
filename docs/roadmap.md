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

### Per-sentence sentence-length heatmap (writing analysis)

A visual scan of sentence lengths in the Writing Analysis dock: each sentence of
the current document becomes a colored cell whose colour encodes word count
(green → short, through amber/orange to red → long), so overlong sentences are
visible at a glance. Follows the Writing Analysis panel
(`docs/superpowers/plans/2026-08-15-scriba-tone-readability-panel.md`), which is
its host. Status: **Not started** — next after that plan lands.

**Data pipeline (all reuse, no new threading):**
- **Sentence spans.** `Readability::countSentences` (`src/preview/Readability.cpp:20`)
  uses `QTextBoundaryFinder::Sentence` but returns only a count — no spans.
  Extend `Readability` with a span-returning splitter (`QVector<QPair<int,int>>`
  sentenceSpans built from the same boundary finder). Do **not** reach into
  `vendor/stoppard/src/tokenizer.h`'s `splitSentences` — it is internal to the
  vendored lib, not public API.
- **Word count per sentence.** Reuse the `[^A-Za-z0-9']+` split already used by
  `WritingAnalysisWorker::analyze` (`src/writing/WritingAnalysisWorker.cpp`),
  intersected with each sentence span.
- **Markdown awareness.** Sentences inside fenced code, inline code, URLs, math
  and HTML tags are not prose and must be excised *before* splitting — reuse
  `SpellHighlighter::protectedRanges(line)` (`src/spell/SpellHighlighter.cpp`),
  the same scanner the spell/grammar passes use.
- **Threading.** Extend `WritingAnalysisResult` with
  `QVector<SentenceStat>{ int start; int length; int wordCount; }`, filled in
  `WritingAnalysisWorker::analyze` and delivered through the existing
  generation-tagged `onResult` — no new worker.

**Thresholds / bucket table (decision anchor for the plan):** map `wordCount` →
colour using the genre profile's `maxSentenceWords` where available (read
`Preferences::GrammarGenre`; the table lives in
`vendor/stoppard/src/rules_style.cpp`'s `genreProfileFor`). Recommended buckets:
green ≤ 0.75×max, amber ≤ max, orange ≤ 1.25×max, red > 1.25×max — so Business's
24-word cap is stricter than General's 32. Pin the exact table in the plan.

**Rendering:** a plain `QPainter` strip widget (`src/writing/SentenceHeatmap.{h,cpp}`)
— a row of cells sized to fit the dock, no WebEngine. Derive cell colours from
the palette (not hardcoded) so dark themes stay legible.

**Interaction:** hover → `QToolTip` with the sentence text (truncated) + word
count; click → map the span to an editor cursor and scroll to it, reusing the
same span→cursor mapping the validation report / scroll-to-issue logic uses.

**Open questions the plan must decide up front:**
1. Strip-of-cells in the dock (recommended v1 — non-invasive, no editor painting
   changes) vs inline underlines via the existing Editor overlay (stretch goal).
2. Show every sentence coloured by bucket (recommended — shows the length
   distribution) vs only over-threshold sentences.
3. Cell layout at narrow dock widths: fixed-width cells with horizontal scroll
   vs shrink-to-fit.
4. Genre-aware scale (recommended, ties this to the genre-profiles plan) vs a
   static scale.

**Files (expected):** `src/writing/SentenceHeatmap.{h,cpp}` (new, added to
`SCRIBA_APP_WEBENGINE_SOURCES`), extend `src/writing/WritingAnalysisWorker.{h,cpp}`
(result struct + `analyze`), extend `src/writing/WritingAnalysisDock.cpp`,
extend `src/preview/Readability.{h,cpp}` (`sentenceSpans`), extend
`tests/test_writing_analysis.cpp`. Keep the bucket mapping a pure function so it
is unit-testable without a widget.

**Gotchas to carry into the plan:** the boundary finder splits oddly around
abbreviations/parentheticals (document any surprising splits); excise protected
ranges *first*; guard empty / single-sentence / very long documents (cap the
strip or let it scroll); the dock's existing debounce already covers re-analysis
after edits. After the UI change, regenerate the affected screenshot gallery
targets per `AGENTS.md`.

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
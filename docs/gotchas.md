# Gotchas

## Tight vs Loose Lists (blank lines between items)

Blank lines between list items are not an error and do **not** split a list into
two. CommonMark (and therefore the md4c parser) absorbs them into the *same*
list and marks it **loose**: every item is wrapped in a paragraph, i.e.
`<li><p>…</p></li>`. Removing the blank lines makes the list **tight**:
`<li>…</li>` with no paragraph wrapper.

```markdown
- a
- b


- c
```

renders as a single loose 3-item list, exactly like the tight 3-item list
produced by deleting the blank lines — only the `<p>` wrappers differ.

This is **not a bug**. The visible spacing difference in the preview comes from
the base CSS: `resources/preview-base.css` gives `li` `margin: 0.5em 0` and `p`
`margin: 1em 0`, so loose-list items (whose content sits in a `<p>`) stack up
with much more vertical room than tight ones.

If the extra spacing is unwanted, fix it in CSS (e.g. `li > p { margin: 0.25em 0; }`
in `resources/preview-base.css`) — not in the parser. Don't "fix" this by
splitting lists on blank lines in `src/Editor.cpp`'s fold logic or
`src/MarkdownParser.cpp`; that would diverge from CommonMark.

## Content column: when a "nested" item is actually a sibling

A nested list item must be indented at least to the **content column** of its
parent — the column right after the parent's marker. The marker width is 2 for
bullets (`- `, `+ `, `* `) but *varies* for ordered markers: `digits + delimiter
+ space` (3 for `1. `, 4 for `10. `, 3 for `1) `). So:

- `- a` → its content column is column 2 → `  - sub` (2 spaces) **does** nest.
- `1. a` → its content column is column 3 → `  1. sub` (2 spaces) does **not**
  nest; only 3+ spaces do.

Items indented short of the content column are still legal top-level markers
(CommonMark allows up to 3 leading spaces), so they don't error — they *flatten*.
With the same marker type they merge into the parent list (a `1./2./3.` "list"
with 2-space sublines is really one flat list); with a different type (`-` under
`1.`) they split off into a sibling list. Verify with cmark if in doubt.

The Tab key in Scriba's editor is content-column aware: it indents bullets by 2
and ordered items by `digits + delimiter + 1` spaces, so Tab on `1. item`
produces `   1. item` (3 spaces) which nests correctly. Manually-typed 2-space
indents under ordered items will silently render flat — that is correct
CommonMark behavior, not an editor bug. (`outdentListLine`/`indentListLine` in
`src/StaticHelpers.cpp` implement this; tests in
`tests/test_editor_list_continuation.cpp` and `tests/test_editor_typing.cpp`
pin the widths.)

Nesting depth with Tab therefore steps by the marker width per level:
bullets 2 → 4 → 6, ordered single-digit 3 → 6 → 9.

## Nested ordered lists must start at 1

Indentation alone isn't enough for a nested ordered list to render. CommonMark
only opens an indented ordered sub-list when its **first marker is numbered `1`**;
an indented `2.`, `3.`, ... under an item is *lazy continuation text* and renders
as plain paragraph lines inside the parent item (`<li>asdf\n3. adsf\n4. asdf</li>`),
not as a nested `<ol>`. Verified empirically with cmark: `   1.` nests, `   2.`/
`   3.`/`   4.` do not — under both ordered and bullet parents.

So Scriba renumbers on Tab: when you indent an ordered item,
`Editor::renumberNestedOrderedList` (Editor.cpp) rewrites the contiguous
same-indent ordered run that includes it to 1, 2, 3... — that's what turns a
Tab on the `3.` line into a real, renderable sublist. Top-level lists (indent 0)
are never renumbered, so intentional start numbers like `2024.` survive; manual
`   2.`+ indents in existing files still render flat (paragraph text) and must
be typed as `1.`+ to nest.

Shift+Tab mirrors this: `Editor::renumberOutdentedOrderedList` renumbers the
outdented item (and its contiguous same-indent ordered run) to continue the
enclosing list, so a nested `9.` outdented after `5. e` becomes `6.`, and
detaching a whole sublist re-cascades the following items
(`4. foo/5. bar` → `6. foo/7. bar`). When no same-level ordered item precedes
the outdented line, its own number is kept (so `2024.`-style lists survive).
Outdenting an already-top-level line never touches numbers.

## Typography arrows are render-time only

The Smart Typography "Arrows" option converts `-> <- <-> => <= >= != +-` to
`→ ← ↔ ⇒ ≤ ≥ ≠ ±` **in the preview and exports only** — the Markdown source in
the editor keeps the ASCII form (see `Typography::apply` in `src/Typography.cpp`).
Putting this in Typography (rather than the Replacements autocorrect list) is
deliberate: pairs like `<=`/`>=`/`=>`/`!=` contain `=`, which the replacements
table can't store (`typo=replacement` splits on `=`, and the save path rejects
typos containing it), and the renderer sees `<->` as a single token, so the
`<-`-prefix collision that would break keystroke-driven expansion doesn't apply.

Behaviour to rely on when debugging:

- The arrow rules run **before** the Dashes rules, so `a->b` becomes `a→b` even
  when "Dashes" is also enabled, while `a--b` still becomes `a–b`. Order matters
  only for the `-` trigger: keep the Arrows block ahead of the Dashes block in
  `apply()`.
- Math (`$...$`/`$$...$$`), code spans and fenced code never reach `apply()`:
  `\ce{CH4 + 2O2 -> CO2}` inside `$...$` keeps `->` for KaTeX/mchem.
- Like the other Typography toggles, `Arrows` defaults **off** and is opt-in.

## ECharts pencil-edit degrades rich hand-written charts to the builder's canonical form

Clicking the pencil on a rendered ` ```ec ` chart re-opens it in the Chart
Builder (`ChartDialog`) / Stock Chart dialog (see `MainWindow::editChartBlock`
and the reverse parsers in `src/EChartsParser.cpp`). Only charts the helpers
themselves can produce round-trip faithfully. Hand-written variants that use
features the builder doesn't expose *still parse* (the data is recovered into
the table) but re-inserting them **rebuilds the chart in the dialog's canonical
single-series form**, discarding the extra styling:

- Pie vs **donut** — a `radius: ["40%","70%"]` pie re-opens as a plain pie.
- **Multi-series radar** — only `series[0]` is recovered (its `value` array is
  mapped against `radar.indicator`); sibling radar series are dropped.
- **Gauge** min/max/unit (`detail.formatter`, `min: 0, max: 220`) — re-derived
  automatically from the data; custom ranges/units are not restored.
- **Calendar/heatmap** layout (`cellSize`, `top`/`left`, custom `visualMap`
  bounds) — recomputed from the data.
- Funnel `left/top/bottom/width` layout, tooltip formatters, legends, etc. are
  generally not preserved; only the tablular data and title/animate/tooltip
  toggles are.

Chart types the builder has no model for at all (effectScatter, box, sankey,
treemap, sunburst, graph, pictorialBar, themeRiver, parallel) don't even open
the dialog — the pencil shows the "cannot be reopened" warning and you edit the
raw JSON. The doc-driven round-trip corpus in `docs/echarts.md` pins exactly
which types parse-and-rebuild (`tests/test_chartsource_docs.cpp`).

## Advanced Charts pencil-edit recovers data, not chart styling

The Advanced Charts dialog (sankey, boxplot, parallel, themeRiver, graph,
treemap, sunburst) reverses an existing ` ```ec ` block back into its table
rows (see `ChartSource::parse{...}Spec` in `src/EChartsParser.cpp`). As with
the Chart Builder, only the tabular data plus title/animate are recovered;
everything else about a hand-written chart is dropped when the block is
re-inserted through `generatedSpec()`:

- **Sankey** — an internal node's own `value` (when it has children) is not
  recoverable: only the link table is restored, and node names are re-derived
  from the links.
- **Treemap / Sunburst** — the table is a flattened leaf view (`Level 1 /
  Level 2 / ... / Value`). An internal node that also carries a `value` loses
  that value on rebuild (the parser only keeps the *leaf* path + value; see
  `TreeSpecData`). Deep nesting beyond the dialog's current column count
  recovers as far as the columns allow.
- **Graph** — `nodeValues` default to `0` when absent, and links without a
  `value` come back as `value: 0`.
- **Parallel** — dimension headers are read from `parallelAxis` in `dim`
  order (a sparse/out-of-order `dim` array is re-sorted by `dim`).
- **Boxplot** — requires one five-number row per x-axis category; a mismatch
  makes the block "cannot be reopened".
- Everything else (layout options, `force`/`roam`, `symbolSize`,
  `rippleEffect` scale, `dataZoom`, `tooltip`, grid padding) is discarded; only
  the data and the type survive the round-trip. The 21-block corpus in
  `docs/echarts.md` and its serializers in `tests/test_chartsource_docs.cpp`
  pin the round-trip behaviour.

## Chart Builder ripple / repeat toggles round-trip through the spec

`effectScatter` and `pictorialBar` re-obey toggles from the Chart Builder.
`ChartSpecData::rippleEffect` and `ChartSpecData::repeatSymbol` are the
*presence* of the `rippleEffect` / `symbolRepeat` keys in the original spec, so
disabling the toggle in the dialog and re-inserting writes a spec without those
keys — which round-trips back as "off", not as a guessed default, on the next
pencil-edit.

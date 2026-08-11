# Gotchas

## "Corpus" disambiguation: ECharts test suite vs. the Corpus feature

The word "corpus" has two unrelated meanings in this repo. The **ECharts test
suite** (the 21-block sample document in `docs/echarts.md`, serializers in
`tests/test_chartsource_docs.cpp`) calls its collection of chart blocks a
"corpus" for round-trip testing. That predates and is unrelated to the
**session→corpus feature**: a saved group of documents (`.scriba` file, Corpus
menu, directory monitoring, corpus dictionary). When searching for "corpus",
don't conflate the two — the ECharts corpus is purely a markdown data fixture.

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

## QTabBar QSS: widget borders paint under the tabs

A QSS `border-bottom` declared on the `QTabBar` selector itself is painted as
part of the widget's box, **before** the tabs are drawn
(`QTabBar::paintEvent` in qtbase: widget box → base frame → tabs → selected tab
last). So a bar-level border is hidden underneath opaque tabs — the line would
only peek out in the gaps between tabs. To get a single continuous divider
under the tab strip (with the active tab breaking it), each tab carries its own
`border-bottom: 1px solid %4` (adjacent tabs join into one line) and the
selected tab keeps `border: none` so the line stops there and it merges into
the editor below. `setDrawBase(true)` is fine to leave on: the base frame is
drawn before the tabs too, so the tabs cover it.

The active tab's rect spans to the tabbar's bottom edge, so its opaque
background (`%7`, the editor background) overpaints the divider — that is what
visually "cuts" the line under it. Keep the selected-tab rule borderless;
adding a matching-color bottom border is not needed and risks a 1px seam.

## Empty borderless table rows have a lone pipe

An empty row of a borderless table (`foo | bar`, no edge pipes) is written with
leading and trailing spaces: `   |    `. To a row-parser that trims leading
whitespace this looks like a fully-bordered row (`|    |`), which made Enter
land the cursor in cell 2 and made Tab from cell 1 create a new row. The
disambiguation lives in `mdRowStyle`: a leading/trailing pipe only counts as a
*border* when the row holds at least two pipes (`pipes.size() >= 2`), because a
bordered row always pairs its edge pipe with a separator pipe after the first
cell. A lone pipe is always a cell separator, never a border. Keep this rule
when reworking the table helpers — and note `formatMdTable` derives the border
style from the separator row, so it is immune to the ambiguity.

## md4c ends a table at any row with four or more leading spaces

md4c's indented-code check runs *before* its table-continuation check, so a
table row that starts with 4+ spaces is reclassified as an indented code block
and the table silently ends: an empty row typed with padding ≥ 2 (or a wide
right/center-aligned first column) rendered as its own block. GFM (pandoc) keeps
such rows in the table, so this is an md4c divergence. Borderless rows are the
target (they have no leading pipe to anchor column zero); bordered rows start
with `|` and are immune.

The fix: `capTableRowIndent` in `src/MdTable.cpp` caps the leading whitespace of
every no-leading-pipe row (`makeEmptyTableRow` and `formatMdTable`) at three
spaces. The cost: a capped row's first pipe may sit one column left of its
aligned neighbours (e.g. `   |    ` under `foo | bar`). Render-validity wins;
if you ever align borderless rows by hand, keep them under four leading spaces.

### Git Graph topology

The Mermaid Git Graph panel (`src/GitGraphBuilder.cpp`, bundled libgit2) assigns
each local branch the commits reachable from its tip, walking **first parents**:
a commit `c` belongs to branch `b` iff `c` is on `b`'s first-parent chain.
Consequences that look like bugs but are intended:

- A merge commit only appears on the branch it was created *on* (its first
  parent). The merged-in branch shows the pre-merge commits instead — e.g. for
  `main: c1 c2 c3`, `feature` branched from c2, then `main` merges `feature`
  with `--no-ff`:
  - `feature` walk = `c5 c4 c2 c1` (4 commits, the merge `c6` is *not* included)
  - `main` walk = `c6 c3 c2 c1` (the merge commit + 3 first-parent commits)
- A commit reachable from two branches is assigned to the branch whose
  first-parent chain contains it first (main processed before feature), so
  shared ancestors like `c1`/`c2` show under `main`.
- "Limit" and "date range" filters apply to the *walk*, so a merge commit
  filtered out by date also drops its `merge <branch>` line from the output.
- The graph is emitted with commits in reverse-chronological order (newest
  first) so the `gitGraph` reads top-to-bottom, matching Mermaid's defaults;
  branches appear in the order they are first used.

## DOCX import (OoxmlToHtml -> DocxImporter)

`DocxImporter` (ZipReader -> OoxmlToHtml -> HtmlToMarkdown/turndown) converts
`.docx` to Markdown. Quirks worth knowing:

- **Tables**: header rows come out as `<th>` inside `<thead>` only when the
  source marks them with `w:trPr/w:tblHeader`. turndown's GFM plugin turns
  `<thead>`+`<tbody>` tables into pipe tables; without a marked header row the
  table survives as raw HTML. Scriba's own `DocxExporter` does not emit
  `w:tblHeader`, so export->import round-trips lose the GFM table (raw HTML
  remains, content intact).
- **Cell merge spans**: `w:gridSpan` is emitted as a `colspan`; `w:vMerge`
  continuation cells render **empty** (their text is lost; the restart cell —
  "first cell wins" — keeps its content). This is deliberately lossy since
  Markdown has no vertical-merge representation; the merged cell's text in
  continuation rows does not import.
- **EMF/WMF images**: an attempt is made to rasterize them via `QImageReader`
  into PNG (plugin/platform dependent — often failing on Linux, sometimes
  working on Windows); on failure they are skipped with a warning.
  PNG/JPEG/GIF/SVG/WebP/BMP/TIFF are extracted directly.
- **Image alt text**: stored in `wp:docPr descr`; not all producers set it, and
  Scriba's own exporter doesn't write it, so `![...]` alt is often empty after
  an export->import round-trip.
- **Hard breaks**: `w:br` becomes `<br>` and turndown keeps a single trailing
  newline, so `<br>` in a paragraph yields a hard line break in Markdown. This
  is the predictable round-trip behaviour regardless of the `HardSoftBreaks`
  preference (that pref only affects the editor's *writing* behaviour).
- **Image placement** is a user preference (`Preferences::ImportImageLocation`):
  next to the document (`media/`), a configured folder, the system temp dir
  (until the doc is saved), or ask each time. An unsaved document with
  "next to the document" escalates to asking; if no folder is chosen the
  `<img>` tags are stripped rather than leaking `docximg://` placeholders.
- **Loose vs tight lists**: loose-ness is inferred from **inline** `w:spacing`
  in each item's `w:pPr` (emits `<li><p>…</p></li>`, which turndown turns into
  a loose Markdown list). Paragraph-mark defaults (`w:docDefaults`/styles-level
  spacing) are **not** considered, so Word lists whose spacing is inherited from
  a style may still import as tight.

## Empty mermaid diagrams in the chart-helper dialog

Feeding mermaid an **empty** `.mermaid` div makes `mermaid.run()` reject, and the
error-handler in `MermaidDialog::mermaidPreviewHtml` stringifies the rejection
value — an object — as `[object Object]`. So "Git Graph selected but no repo
loaded" (or any helper with no data yet) shows a red `[object Object]` box in
the live preview if the empty string is passed straight to mermaid.

Scriba guards this in `MermaidDialog::updatePreview`: when `buildDiagram()` is
empty/whitespace it renders `MermaidDialog::emptyPreviewHtml()` (a blank pane)
instead of invoking mermaid at all. Do not "fix" the red box by tweaking the JS
error handler alone — the real problem is calling `mermaid.run` on empty input;
keep the empty-path check.

Related: `MainWindow.cpp` opens the dialog via the theme-only constructor
(`MermaidDialog(themeCss, parent)`). Passing an *empty* `existingDiagram` into
the prefill constructor is **not** the same thing — it routes through
`prefillFromSource("")`, which fails to parse and forces the raw-source
fallback panel (combo index 13, "Diagram Source") instead of the default first
helper (Pie). Use the theme-only ctor for a fresh dialog and the prefill ctor
only when editing an existing diagram.

## QtWebEngine `runJavaScript` does not await returned promises

`QWebEnginePage::runJavaScript` (Qt 6) fires its result callback as soon as the
script *returns* — it does **not** wait for a script that returns a Promise to
resolve, and it delivers the resolved value as the callback argument only when
the script itself is synchronous. Scripts that `await` something therefore
resolve asynchronously inside the page, and the C++ callback fires immediately
with an empty/pre-resolution value.

So a converter like `src/PdfImporter.cpp` that runs a long async pipeline
(`pdfjsLib.getDocument(...).promise` → `getPage` → text extraction) must not
rely on the second `runJavaScript` round-trip reading the result back — it will
race and observe an empty global. The pattern that works:

1. The async script stashes its JSON result in a page global
   (`window.__scribaPdfResult`), never returning a promise to C++.
2. C++ polls that global with a `QTimer` (every ~100 ms), calling
   `page->runJavaScript("window.__scribaPdfResult || ''", cb)` on each tick,
   quitting its `QEventLoop` as soon as the global is non-empty (or the
   deadline hits).

Also: **QtWebEngine refuses `new Worker(blob, {type: 'module'})`** for blob
URLs ("Refused to cross-origin redirects of the top-level worker script"),
while dynamic `import(blobUrl)` succeeds. pdf.js's fake-worker fallback
(`PDFWorker._setupFakeWorkerGlobal` uses `import(workerSrc)`) happily drives
the bundled worker via `import()`, so pin the pdf.js worker through the built-in
fake-worker path by pre-registering `globalThis.pdfjsWorker = await import(...)`
(PDFWorker initializes interestingly after that). See the `getDocument` setup in
`src/PdfImporter.cpp` for the exact incantation.

## In-source print directives (`<!-- keep -->` / `<!-- page-break -->`)

The two directives that pin a block to one page or force a page break are parsed
by `MarkdownParser::toHtml` (scanner in `src/MarkdownParser.cpp`, class
attachment in `src/MdRenderer.cpp`) and must satisfy a strict contract:

- **Flush-left and on their own line.** A directive indented inside a list item,
  or trailing after paragraph text, is not recognized (it stays an ordinary
  HTML comment — which md4c strips in the preview/export anyway).
- **It must form its own block.** The directive attaches to the *next
  top-level block*. A directive on the line immediately after paragraph text is
  merged into that paragraph and silently ignored — keep a blank line *before*
  the directive when it follows paragraph text. When the target is a paragraph,
  separate it from the directive with a blank line; targets that interrupt a
  paragraph themselves (ATX headings, fenced code blocks) need no blank line
  after the directive (verified: `<!-- page-break -->\n## Heading` and
  `<!-- keep -->\n```cpp …` both work). A blank line after the directive is
  always safe, so the example doc (`docs/typesetting-example.md`) uses it
  everywhere.
- **They must stay comments.** The scanner matches `^<!--\s*(keep|...)\s*-->$`
  exactly. A directive written as plain text (without `<!-- -->`) is just prose
  and has no effect. Because it *is* a comment, it disappears from both the
  preview and the exported PDF — that invisibility is the acceptance test.

**Why tokens instead of line numbers?** `MdRenderer::m_currentLine` is advanced
only by `\n` in `MD_TEXT` callbacks; md4c does not emit a trailing softbreak for
a block's final line, so the counter lags by the number of blank lines between
blocks — mapping directives to blocks by source line number is unreliable.
Instead the scanner replaces each recognized directive with a unique marker
(`SCRIBADIRK<n>` / `SCRIBADIRB<n>`), and the renderer — which knows block
boundaries natively — strips the tokens and attaches the class
(`scriba-keep` / `scriba-page-break`) to the next top-level block's opening tag.

The classes are styled only in `resources/print-base.css`; the preview CSS
deliberately has no `.scriba-*` rules, so the preview never shows a directive
artifact beyond the blank line where the comment was.

**Quoteblocks keep like admonitions in print.** `blockquote` carries
`page-break-inside: avoid` in `resources/print-base.css` (same keep rule as
`.admonition`), and the "Keep figures and quotes together" toggle relaxes both
together (`PrintOptions::buildCss` emits
`.mermaid,.katex-display,.admonition,blockquote,pre{break-inside:auto;…}` when
off). The keep applies to the outer blockquote, so nested quotes stay together
as one unit.

## `@page` parsing: the *last* block wins

`ExportPdfDialog::parsePageSize` / `parsePageMargins` iterate **all**
`@page { ... }` blocks and use the **last** one. `print-base.css` ships
`@page { margin: 15mm; }` on its first line, and the per-export typesetting
override (`PrintOptions::buildPageOverrideCss`) is appended *after* all the
custom CSS, so a last-match scan is what makes the override beat the base block.
(Falling back to a first-match scan silently reverts to base 15mm/A4 even when
the user set a custom size/margin — do not "simplify" it back.)

The override's `margin:` is typically the final declaration with no trailing
semicolon (`@page{size:A4;margin:18mm}`), so the margin regex must stop at `}`
as well as `;`.

## Code splitting is measured in pixels, not lines

The "split code blocks" option's thresholds (50/100 lines) are a *label*; the
prepare-print JS pass in `ExportPdfDialog::onPageLoaded` decides by measuring
each `<pre>` with `getBoundingClientRect()` against the page content box (page
height minus margins). Blocks taller than the content box get the mode's
`scriba-split-*` class (→ `break-inside:auto`); everything else keeps the base
`break-inside:avoid`. Line counting can't work reliably because soft-wrapped
code lines don't appear in `textContent`.

## Page-break mode only separates what must not flow

"Show Page Breaks" (`Ctrl+Shift+B`, `Preferences::PreviewShowPageBreaks`)
rebuilds the preview in print layout via `src/PreviewPagination.cpp`: the same
merged print CSS + `PrintOptions` geometry as the PDF export (`PrintOptions::
parsePageSize`/`parsePageMargins`, last `@page` wins — see above), a grey canvas
with a white page in `#center-css`, and a paginator script (`window.
scribaPaginate`) injected before `</body>`.

The paginator inserts a `Page N` separator only where content **cannot** flow:
`scriba-keep` blocks and (with the options on) tables/figures/heading+first
line, plus explicit `scriba-page-break` directives. Plain paragraphs and code
blocks that are allowed to split just flow across the page boundary like in a
real printout — no separator is drawn, because the text simply continues on the
next page. Don't "fix" this by giving flowing blocks separators; only kept
content and explicit breaks get them (asserted by `tests/test_preview_pagination.cpp`).

Re-pagination happens after every render pass by patching the preview template's
two chains (`patchIncrementalPaginate`): the `DOMContentLoaded` pass ends in
`scribaEndRender()` and every `scribaUpdate` heavy pass ends in `restoreScroll()`,
both of which are followed by `if(window.scribaPaginate){window.scribaPaginate();}`.
The hooks are guarded on `window.scribaPaginate` existing, so the normal preview
is unaffected. While page-break mode is on, `refreshPreviewCss`,
`applyPreviewSplitWidth` and `setPreviewState`'s `center-css` writes are skipped
and theme/geometry changes force a full page rebuild; the print CSS is sent as
`#base-css` and the theme as empty, so the on-screen page matches the printout
(and the pane background, not the theme, colours the canvas).

Page geometry changes (print page size/margin or any option in
`PrintOptions::fromSettings`) are detected by a merged-CSS fingerprint
(`m_printLayoutFp`); when it changes, `m_previewInitialized` is reset to force
the full-build path so the new page box and paginator take effect.

## Relative images resolve against the shared preview page's base URL

The live preview is **one `QWebEngine` page shared across all tabs**
(`src/Preview.cpp`). Relative markdown images (`![img](pic.png)`) are never
rewritten in C++ — the browser resolves them against the page's base location,
which is fixed at the last full `setHtml(html, baseUrl)` and then re-asserted
per render by a `<base href>` element that `scribaUpdate` injects/updates.

That `<base>` element must be (re)asserted on **every** render — tab switches
*and* plain edits. If an update passes an empty base URL, the JS *removes* the
element and relative images fall back to the page's document URL from the last
full page load: if that was another tab's (or a pre-Save-As) directory, the
images render broken until the next full load or tab switch. So in
`MainWindow::updatePreview` the base URL is sent on every `scribaUpdate` call,
not just when `tabSwitch == true` (regression test:
`tests/test_scroll_sync.cpp` `BaseUrlSurvivesEdits`).

Two corollaries:

- `saveFile`/`renameCurrentFile` into a **different directory** must trigger a
  re-render, otherwise the base stays stale until the next edit/tab switch
  (`saveFile`/`renameCurrentFile` only signal when the base dir actually
  changed). This mirrors the same "base desync" failure mode.
- Untitled documents (no file path) legitimately pass an empty base URL and
  drop the element — there's nothing for relative paths to anchor to.


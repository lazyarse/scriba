# Chart Assistants

Scriba includes form-based assistants for building **Mermaid** diagrams and **ECharts** charts without writing code by hand. Each assistant provides a split-pane UI: form controls on the left, a live preview on the right, and Insert/Copy buttons at the bottom.

## Mermaid Assistants (13 diagram types)

Accessed from **Tools > Mermaid Diagrams**. Each assistant generates a fenced ` ```mermaid ` code block.

Every table-based assistant has per-row delete buttons (trash icon) and an add-row button.

### Pie Chart

A simple pie with labeled slices. Fields: **Title**, and a table of **Label** / **Value** pairs.

### Flowchart

Node-and-edge diagrams with direction control. Two tables:

- **Nodes** -- ID, display text, and shape (box, round, stadium, diamond, hexagon).
- **Edges** -- From/To (dropdowns populated from node IDs), label, and arrow type (6 styles: `-->`, `---`, `-.->`, `==>`, `--o`, `--x`).

Deleting a node refreshes the edge dropdowns automatically.

### Sequence Diagram

Message exchanges between participants. Two tables:

- **Participants** -- Name and optional alias.
- **Messages** -- From/To (dropdowns from participant names), label, and arrow type (6 styles: `->>`, `-->>`, `-x`, `--)`, `->`, `-->`).

Deleting a participant refreshes the message dropdowns.

### Gantt Chart

Project timeline. Fields: **Title**, **Date format** (YYYY-MM-DD, DD/MM/YYYY, MM-DD-YYYY), **Exclude weekends** toggle, and a task table with ID, description, start/duration, **Status** (done/active/crit/milestone), and section grouping.

### Class Diagram

Object-oriented class hierarchies. Four tables:

- **Classes** -- Class name, stereotype, and note.
- **Fields** -- Name, type, and visibility (public/private/protected/package).
- **Methods** -- Name, return type, parameters, and visibility.
- **Relations** -- From/To class, type (inheritance, composition, aggregation, association, dependency, realization), and label.

Deleting a class re-indexes the internal data map.

### Entity-Relationship Diagram

Database schemas. Three tables:

- **Entities** -- Entity name.
- **Attributes** -- Name, type, and key type (PK/FK/none), tied to the selected entity.
- **Relations** -- From/To entity (dropdowns), relationship type (`||--o{`, `||--||`, `}|--||`, `}|--o{`), and label.

Deleting an entity re-indexes the attributes map.

### State Diagram

State machines. Two tables:

- **States** -- Name and description.
- **Transitions** -- From/To (dropdowns from state names) and label.

Deleting a state refreshes the transition dropdowns.

### Mindmap

Hierarchical concept maps. Uses a **tree widget** (not a table) with Add Child, Add Sibling, and Delete buttons. Each node has editable text.

### Timeline

Chronological events. Fields: **Title**, and a table of **Section** / **Event** pairs. Rows with the same section are grouped together in the output.

### Journey

User experience maps. Fields: **Title**, and a table of **Section**, **Task Name**, **Score** (1-7 spinner), and **Actors**. Like Timeline, rows with the same section are grouped.

### Quadrant Chart

Two-axis scatter plots with quadrant labels. Fields: **Title**, **X-axis** labels (left/right), **Y-axis** labels (bottom/top), **Quadrant labels** (Q1-Q4), and a point table with Label, X (0-1), and Y (0-1).

### Sankey Diagram

Flow quantity diagrams. A table of **Source** / **Target** / **Value** triples. Uses the `sankey-beta` syntax.

### Git Graph

Commit/branch visualization from a local git repository. Reads the repo with the bundled libgit2 (no `git` binary needed) and emits a `gitGraph` block.

- **Repo** — directory path of the local repository (or one of its subdirectories; the nearest enclosing repo is used).
- **Branch** — optional: restrict the graph to a single branch. Defaults to the current branch, or all local branches.
- **Limit** — how many commits to include (`All`, or a cap of 10/25/50).
- **Date range** — optional from/to dates; only commits within the range appear.

Branch topology follows first-parent chains: each branch is assigned the commits reachable from its tip, walking first parents; merge commits are emitted as `merge <branch>`. Remote refs are ignored. If the path is not a git repository, an error message is shown instead.

---

## Mermaid Types NOT Covered

The following Mermaid diagram types have no assistant in Scriba. They can still be written by hand in a ` ```mermaid ` code block:

| Type | Keyword | Notes |
|---|---|---|
| XY Chart | `xychart-beta` | Simple bar/line charts (beta) |
| Block Diagram | `block-beta` | Grid-based system layouts (beta) |
| Architecture | `architecture-beta` | Cloud/infrastructure topology (beta) |
| Requirement Diagram | `requirementDiagram` | Requirements traceability |
| C4 Diagrams | `C4Context`, `C4Container`, `C4Component`, `C4Deployment`, `C4Dynamic` | Software architecture (5 sub-types) |
| Kanban | `kanban` | Task board columns |
| Packet Diagram | `packet-beta` | Network protocol layouts (beta) |
| Radar Chart | `radar-beta` | Spider/star multi-axis scoring (beta) |
| Treemap | `treemap-beta` | Hierarchical proportion rectangles (beta) |
| TreeView | `treeView-beta` | Indentation-based tree (beta) |
| Venn Diagram | `venn-beta` | Set relationships (beta) |
| Ishikawa | `ishikawa-beta` | Fishbone cause-and-effect (beta) |
| Wardley Map | `wardley-beta` | Strategy/value-chain mapping (beta) |
| ZenUML | `zenuml` | Code-style sequence diagrams (plugin) |

Additionally, the existing assistants do not cover some advanced features within supported diagram types (e.g., subgraphs in flowcharts, notes in sequence diagrams, lifecycle pseudostates in state diagrams).

---

## Chart Builder (ECharts)

Accessed from **Tools > Chart Builder**. Generates an ECharts option object wrapped in a fenced ` ```ec ` code block. Charts render in the preview via the bundled Apache ECharts library (SVG renderer).

### Supported Chart Types (7)

| Type | Description |
|---|---|
| Bar | Categorical or numeric X axis |
| Line | Connected data points |
| Area | Line with filled area |
| Scatter | Numeric X/Y point pairs |
| Effect Scatter | Scatter with animated ripples around each point (ripple toggle) |
| Pictorial Bar | Bar built from repeated symbols like `rect` (repeat toggle) |
| Pie | Categorical name/value slices (no axes) |
| Donut | Pie via the `radius` option (not exposed in the dialog) |
| Funnel / Gauge | Categorical name/value or single-value gauges |
| Radar | Indicator/value/max table over a radar grid |
| Heatmap (matrix + calendar) | X/Y/Value triples, or Date/Value pairs on a calendar |

### Features

- **Data input**: Editable table with CSV and JSON paste support. Rows/columns can be added or removed.
- **Fields**: X and Y field dropdowns populated from the table columns. A numeric X field produces a value axis; otherwise a category axis is used.
- **Tooltip**: Checkbox to add an axis-triggered tooltip.
- **Animation**: Toggle chart animation on/off (off by default, emitting `"animation": false` in the options).
- **Options**: Chart title.
- **Preview**: Live rendering via ECharts (SVG renderer) in QWebEngineView.
- **Output**: Insert or copy a fenced ` ```ec ` JSON block.
- **Type-specific toggles**: `Effect Scatter` gains a "Ripple" checkbox (emits `rippleEffect`/`symbolSize`); `Pictorial Bar` gains a "Repeat" checkbox (emits `symbolRepeat` styling). Both round-trip through the spec: unchecking writes the spec without the key, so re-opening sees it off.
- **Pencil edit**: A rendered chart whose type matches a builder type opens **pre-filled** (data + title + toggles recovered) — see `docs/gotchas.md` for what is *not* preserved.

### Chart Builder Features NOT Covered

**Encodings**: No color/size/shape channels, multi-series grouping, or stacked values.

**Mark properties**: No constant styling (color, opacity, stroke, bar width, symbol type), axis/legend customization, or number/date formatting.

**Data**: No URL-based data or transforms (filter, aggregate, sort, bin, etc.). Data is always inline values.

**Composition**: No multiple series, grids, or chart composition. Single-series, single-view only.

---

## Stock Chart Builder (ECharts)

Accessed from **Tools > Stock Chart**. Builds candlestick charts from OHLC data and wraps the result in a fenced ` ```ec ` code block.

### Features

- **Data input**: Paste or open CSV with columns `date, open, high, low, close[, volume]`. Column names are matched case-insensitively (aliases: `time`/`day` for date, `o`/`h`/`l`/`c`/`vol`); if the names don't match, columns are used positionally. Invalid rows are skipped.
- **Volume pane**: Optional bar series on its own grid below the candles, sharing the time axis.
- **Moving averages**: Optional MA5 / MA10 / MA20 / MA50 line overlays, computed from closing prices.
- **Zoom / pan**: Optional inside-scroll zoom plus a slider (dataZoom).
- **Animation**: Toggle chart animation on/off (off by default, emitting `"animation": false` in the options).
- **Options**: Chart title.
- **Preview**: Live rendering via ECharts (SVG renderer) in QWebEngineView.
- **Output**: Insert or copy a fenced ` ```ec ` JSON block with candlestick, volume, and MA series.

### Stock Chart Builder Features NOT Covered

No chart-type indicators beyond moving averages (no RSI, MACD, Bollinger bands), no intraday scale handling, and no data transformations — prices are used as pasted.

---

## Advanced Charts Builder (ECharts)

Accessed from **Tools > Advanced Charts**. One-panel-per-type builder for the
chart families that don't fit the Chart Builder's X/Y cartesian model. Each
panel has its own table editor, a live ECharts preview (SVG renderer), and
wraps the result in a fenced ` ```ec ` JSON block.

### Supported Chart Types (7)

| Type | Table columns | Panel |
|---|---|---|
| Sankey Flow | Source / Target / Weight | single links table |
| Box Plot | Category + Min / Q1 / Median / Q3 / Max | one row per category |
| Parallel Coordinates | one numeric column per dimension | `+Col` adds a dimension |
| Theme River | Date / Value / Category | one row per triplet |
| Treemap | Level 1 / Level 2 / ... / Value | shared tree editor |
| Sunburst | Level 1 / Level 2 / ... / Value | same tree editor |
| Graph (Network) | Nodes (Name/Value) + Links (Source/Target/Value) | two tables |

For the tree editor (Treemap/Sunburst) each table row is one *leaf*: the Level
columns name the path from the root and the final cell is the leaf value. Add
more levels with +Col; `-Col` drops the deepest non-value column.

### Features

- **+Row / -Row / +Col / -Col**: add/remove table rows and columns (Parallel and
  the tree editor grow extra dimensions/levels).
- **Options**: Chart title and an animation toggle.
- **Preview**: live rendering via ECharts (SVG renderer) in QWebEngineView.
- **Output**: Insert or copy a fenced ` ```ec ` JSON block.
- **Pencil edit**: an existing chart re-opens in the matching panel with its
  data recovered into the table (see `docs/gotchas.md` for what is *not*
  preserved).

### Advanced Charts Builder Features NOT Covered

No node/edge attributes beyond name+value (no graph categories, `draggable`,
`label` settings, `layout` control beyond force, or `edgeLabel`), no boxplot
outliers or live box statistics (the five numbers are entered directly), no
parallel axis types/ranges/inverted dims, and no Sankey node sizing by value.
Only the tablular data survives re-opening hand-written charts.

# Chart Helpers

Scriba includes form-based helpers for building **Mermaid** diagrams and **Vega-Lite** charts without writing code by hand. Each helper provides a split-pane UI: form controls on the left, a live preview on the right, and Insert/Copy buttons at the bottom.

## Mermaid Helpers (12 diagram types)

Accessed from **Tools > Mermaid Charts**. Each helper generates a fenced ` ```mermaid ` code block.

Every table-based helper has per-row delete buttons (trash icon) and an add-row button.

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

---

## Mermaid Types NOT Covered

The following Mermaid diagram types have no helper in Scriba. They can still be written by hand in a ` ```mermaid ` code block:

| Type | Keyword | Notes |
|---|---|---|
| Git Graph | `gitGraph` | Branch/commit visualization |
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

Additionally, the existing helpers do not cover some advanced features within supported diagram types (e.g., subgraphs in flowcharts, notes in sequence diagrams, lifecycle pseudostates in state diagrams).

---

## Vega-Lite Helper

Accessed from **Tools > Vega-Lite Chart**. Generates a Vega-Lite JSON spec wrapped in a fenced ` ```vega-lite ` code block.

### Supported Mark Types (15)

| Mark | Description | Encodings |
|---|---|---|
| bar | Bar chart | X, Y, Color, Tooltip |
| line | Line chart | X, Y, Color, Tooltip |
| point | Scatter plot | X, Y, Color, Size, Shape, Tooltip |
| area | Area chart | X, Y, Color, Tooltip |
| rect | Rectangular (heatmap-like) | X, Y, Color, Tooltip |
| tick | Tick marks | X, Y, Color, Size, Tooltip |
| rule | Reference lines | X, Y, Color, Tooltip |
| circle | Circle marks | X, Y, Color, Size, Shape, Tooltip |
| square | Square marks | X, Y, Color, Size, Shape, Tooltip |
| text | Text marks | X, Y, Color, Text, Tooltip |
| trail | Variable-width lines | X, Y, Color, Size, Tooltip |
| boxplot | Box plot | X, Y, Color, Size, Tooltip |
| errorband | Error band | X, Y, Color, Tooltip |
| errorbar | Error bar | X, Y, Color, Tooltip |
| geoshape | Geographic shape | X, Y, Color, Tooltip |

### Features

- **Data input**: Editable table with CSV and JSON paste support. Rows/columns can be added or removed.
- **Encodings**: Each channel (X, Y, Color, Size, Shape, Text) has a field dropdown (populated from table columns) and a type selector (nominal, ordinal, quantitative, temporal). Which channels are visible depends on the selected mark type.
- **Tooltip**: Checkbox to add a tooltip encoding using all columns.
- **Options**: Chart title, fill-available-width toggle.
- **Preview**: Live rendering via vega-embed in QWebEngineView.
- **Output**: Insert or copy the JSON spec.

### Vega-Lite Features NOT Covered

**Encoding gaps**: No opacity, angle, stroke, X2/Y2 (for ranged marks), or href channels.

**Mark properties**: No constant color, opacity, stroke, corner radius, filled/unfilled toggle, line interpolation, point-on-line toggle, bar width, or text styling (font, size, alignment).

**Data**: No URL-based data, named data sources, or transforms (filter, calculate, fold, window, bin, aggregate, regression, loess, impute, lookup, etc.). Data is always inline values.

**Aggregation**: No aggregation functions (count, sum, mean, median, min, max, etc.). Raw data only.

**Composition**: No layer, facet, repeat, concat, hconcat, or vconcat. Single-view only.

**Interactivity**: No selections (point, interval, legend), parameters, or conditional encodings.

**Configuration**: No axis/legend/scale customization, sort order, stack control, bin settings, color schemes, number/date formatting, explicit pixel sizing, padding, background, or title subtitles.

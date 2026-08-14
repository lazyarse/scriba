# md4c Parser Flags

Scriba's Markdown parser is the vendored copy of md4c (`vendor/md4c/`). Its flag
set is assembled in `MarkdownParser::toHtml()` (`src/MarkdownParser.cpp:116`),
and the `MD_FLAG_*` constants are defined at `vendor/md4c/src/md4c.h:372`. This
is the reference for which flags Scriba enables and why the rest are left off —
consult it when upgrading the vendored parser or adding a parsing feature.

## Enabled flags

| Flag | Bit | What it renders | Where used |
|------|-----|-----------------|------------|
| `MD_FLAG_TABLES` | 0x100 | Pipe tables (`<table>`) | always |
| `MD_FLAG_STRIKETHROUGH` | 0x200 | `~~x~~` → `<del>` | always |
| `MD_FLAG_TASKLISTS` | 0x800 | `- [ ]` checkboxes | always |
| `MD_FLAG_PERMISSIVEAUTOLINKS` | 0x4\|0x8\|0x400 | bare `url`, `mail@x.com`, `www.x.com` → `<a>` | always |
| `MD_FLAG_ADMONITIONS` | 0x80000 | `> [!note]` callouts | always |
| `MD_FLAG_HIGHLIGHT` | 0x200000 | `==x==` → `<mark>` | always |
| `MD_FLAG_SUPERSCRIPTS` | 0x20000 | `^x^` → `<sup>` | always |
| `MD_FLAG_SUBSCRIPTS` | 0x40000 | `~x~` → `<sub>` | always |
| `MD_FLAG_FOOTNOTES` | 0x100000 | `[^n]` → footnotes section | always |
| `MD_FLAG_LATEXMATHSPANS` | 0x1000 | `$…$` / `$$…$$` → `MD_SPAN_LATEXMATH` math spans | always — MdRenderer emits semantic `<span class="katex" data-tex=…>` nodes that `initKaTeX()` feeds to `katex.render()` (auto-render removed); also protects math bodies from superscript/subscript parsing |
| `MD_FLAG_DEFINITIONLISTS` | 0x400000 | definition lists: `Term` + `: `/`~ ` lines → `<dl>/<dt>/<dd>` | always — requires the `definition-lists.patch` (Scriba ships with the flag on); see `docs/gotchas.md` for semantics and constraints |
| `MD_FLAG_HARD_SOFT_BREAKS` | 0x8000 | single newline → `<br>` | `Preferences::HardSoftBreaks` (default off) |
| `MD_FLAG_NOHTML` | 0x20\|0x40 | blocks raw HTML | `toHtml(…, noHtml)` path (preview/export "block raw HTML" prefs) |

## Disabled flags (and why)

These md4c flags are **not** enabled by Scriba. Keep this table in sync with
`vendor/md4c/src/md4c.h` when upgrading the vendored parser.

| Flag | Bit | Description | Why not enabled / potential use |
|------|-----|-------------|----------------------------------|
| `MD_FLAG_COLLAPSEWHITESPACE` | 0x1 | Collapse runs of non-trivial whitespace in normal text to a single space | Fights Scriba's line-preserving `data-line` mapping and would change rendering in `<pre>`/code contexts; users expect their whitespace kept. |
| `MD_FLAG_PERMISSIVEATXHEADERS` | 0x2 | Allow ATX headings without a space (`###header`) | Breaks the `MarkdownChecker` `HashNoSpace` rule (`src/MarkdownChecker.cpp`), which deliberately flags `#foo`; also diverges from CommonMark. |
| `MD_FLAG_NOINDENTEDCODEBLOCKS` | 0x10 | Disable 4-space indented code blocks (fenced only) | Indented blocks are a CommonMark staple; turning them off would surprise users who rely on them. |
| `MD_FLAG_SPOILERS` | 0x10000 | `\|\|x\|\|` → `<span class="spoiler">` | Removed — no useful purpose for Scriba. |
| `MD_FLAG_WIKILINKS` | 0x2000 | `[[target]]` wiki links (`MD_SPAN_WIKILINK`) | `[[…]]` is commonly used for non-link content (math, bracket notation); the checker/link-validator has no wiki-link rules, so links would be silently dead. |
| `MD_FLAG_UNDERLINE` | 0x4000 | `_x_` → `<u>` (and **disables** `_` as emphasis) | Directly conflicts with CommonMark `_italic_` emphasis that Scriba supports; would silently change emphasis rendering in existing documents. |

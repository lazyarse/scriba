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

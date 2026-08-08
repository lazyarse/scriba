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

# Security Notes

## HTML Export — Image Handling

When exporting a Markdown document as HTML, Scriba embeds all images inline as
`data:` URIs. This applies to both local images (resolved relative to the
source `.md` file) and external images (fetched over HTTPS at export time).

### Why fetch external images?

Leaving external URLs in the exported HTML would cause the viewer's browser to
contact third-party servers on every open — enabling tracking, fingerprinting,
and the potential serving of different (possibly malicious) content on
subsequent requests.

By fetching and embedding at export time:

- No runtime network requests remain in the exported file.
- The exported HTML is fully self-contained and works offline.
- The image content is static and immutable once embedded.

### Validation

Downloaded images are validated before embedding:

- The MIME type is checked via `QMimeDatabase` — only recognised image types
  (`image/*`) are embedded.
- If the fetch fails (network error, non-image response, timeout), the original
  URL is preserved as a graceful fallback.

### Limitations

- Large images significantly increase file size.
- Export requires network access when external images are present in the
  document.
- Embedded images are frozen at export time — updates to the source image are
  not reflected in previously exported files.

## HTML Export — Script Handling

Raw HTML in markdown can include `<script>` tags that reference external
JavaScript files. During export, the user can choose how these are handled:

### Strip (default)

All `<script>` tags — both inline and external — are removed from the exported
HTML. This is the safest option: no JavaScript runs in the exported file, and
there are zero external network requests.

### Embed external

External `<script src="URL">` tags are fetched at export time, and the JS
content is inlined into the `<body>` of the exported HTML. The `src` attribute
is removed. Inline `<script>` blocks (without a `src` attribute) are left
unchanged.

**Security note:** Inline scripts may still contain code that makes network
requests at view time (e.g. `fetch()`, `XMLHttpRequest`, dynamic `<script>`
injection). Choosing "Embed external" does not eliminate this risk — it only
prevents the external-script-to-inline conversion vector. If you require zero
network requests at view time, use "Strip".

## HTML Export — External CSS (`<link>`)

External stylesheets referenced via `<link rel="stylesheet" href="URL">` are
**always** fetched and inlined as `<style>` blocks during export. This applies
to both HTML and DOCX export and is not affected by the script-handling option.
CSS is declarative and cannot execute code, so embedding it carries no
execution risk.

## Preview — Script Stripping

A preference option **Strip `<script>` tags from markdown content in preview**
(Preferences → General) is enabled by default. When active, any `<script>`
tags in the raw markdown source are removed before the HTML reaches the
in-app preview.

This prevents user-injected scripts — whether malicious, accidental, or
tracking-related — from executing while editing. Scriba's own scripts
(highlight.js, Mermaid, KaTeX, Vega, twemoji) are unaffected because they are
injected in the `<head>` of the preview template, not in the markdown body.

Disabling this option allows `<script>` tags in markdown to run in the
preview. This is intended for users who embed interactive widgets or
self-contained JS snippets in their documents.

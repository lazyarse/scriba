# mathml2omml (vendored)

Vendored copy of [lazyarse/mathml2omml](https://github.com/lazyarse/mathml2omml),
pinned to tag **v3.0.0** (commit `a46f1910769dd557c0b3ebb65622876c9991b2a7`).

Zero-dependency C++23 library converting between MathML and OMML (Office Math
Markup Language). Used by `src/HtmlToOoxml.cpp` for DOCX export.

## This copy

- `mathml2omml.{cpp,h}` — upstream v3.0.0 sources.
- This directory is the **working copy**: local code changes are made here and
  may diverge from upstream (no patch-file workflow, unlike `vendor/md4c`).
- To upgrade: replace the two source files from the new upstream tag, then adapt
  `src/HtmlToOoxml.cpp` to any API changes.

## Status

Upstream v3.0.0 introduced breaking API changes (`XmlSink` methods and the
`convert()` parameters now use `std::string_view`; the string-returning
`convert()` now returns `std::expected<std::string, std::string>`).
`src/HtmlToOoxml.cpp` (`QtOmmlSink`) has been adapted to the new API.

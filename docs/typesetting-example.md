# Print Typesetting Example

A printable tour of Scriba's PDF typesetting controls. Open this file in Scriba
and **export it to PDF** — the page breaks and keep-together behavior in the
output come from two places:

- **Defaults** saved in **Preferences → Printing**, and
- the two in-source directives `<!-- keep -->` and `<!-- page-break -->`
  baked into this document.

Everything shown here is also available per export in the **Typesetting** group
of the **Export PDF** dialog, where **Reset to saved defaults** discards the
override and returns to your saved Preferences.

## The controls

| Option | Default | What it does |
|---|---|---|
| **Split code blocks** | Never | Code blocks either stay whole (`Never`) or are allowed to split across pages when taller than the page's content area (`Over 50 lines`, `Over 100 lines`). |
| **Keep tables together** | On | A table stays on one page where it fits. |
| **Keep headings with following text** | On | A heading is not left stranded at the bottom of a page. |
| **Keep figures and quotes together** | On | Mermaid, KaTeX, ECharts, admonitions, blockquotes and code blocks stay on one page where possible. |
| **Avoid orphan/widow lines** | On | Prevents a lone paragraph line dangling at the top or bottom of a page. Chromium's support is best-effort. |
| **Margin / Page size** | Base | Free-form CSS values, e.g. `18mm`, `0`, or `A4 landscape`. |

## In-source directives

Two HTML comments control pagination at specific points in a document. They
never appear in the preview or in the PDF:

```text
<!-- keep -->

<!-- page-break -->
```

- `<!-- keep -->` pins the **next block** — table, code block, figure, heading,
  quote, paragraph — to one page.
- `<!-- page-break -->` starts a **new page** at the next block.

A directive must sit **flush-left on its own line** and form its own block:
keep a blank line before it when it follows paragraph text, and a blank line
after it when the target is a paragraph. A directive merged into a paragraph is
silently ignored. The demonstration blocks below follow exactly that form.

## Keep tables together

<!-- keep -->

| Project | Language | Lines | Status |
|---|---|---|---|
| Scriba | C++23 / Qt6 | 28,000 | active |
| md4c | C | 9,400 | vendored |
| mathml2omml | C++ | 3,100 | vendored |
| stoppard | C++ | 1,900 | vendored |

The `<!-- keep -->` above this table keeps it on one page where it fits.

## Keep code blocks that fit

<!-- keep -->

```python
def short():
    return "this block stays on one page"
```

A short block like this never splits under any **Split code blocks** mode — the
splitting applies only to blocks taller than the page's content area.

## Figures stay together

<!-- keep -->

> [!note]
> Admonitions count as "figures" too: with **Keep figures and quotes together**
> on, this block is kept intact where it fits, along with Mermaid diagrams,
> display math, ECharts charts — and plain `>` blockquotes, which follow the
> same keep rule.

## Splitting code blocks

This section starts with a `<!-- page-break -->`, so the tall block below
begins on a fresh page. Export this file with each **Split code blocks** mode
to see the difference:

- **Never** — the block stays whole where it fits; a block taller than the
  page has to split anyway (the browser cannot fit it on one page).
- **Over 50 lines** / **Over 100 lines** — blocks taller than the page's
  content area are allowed to split across pages; everything else stays
  together.

<!-- page-break -->

```cpp
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

struct Row {
    std::vector<std::string> fields;
};

std::vector<std::string> split(std::string_view line, char sep) {
    std::vector<std::string> out;
    std::string field;
    for (char c : line) {
        if (c == sep) {
            out.push_back(field);
            field.clear();
        } else {
            field.push_back(c);
        }
    }
    out.push_back(field);
    return out;
}

std::optional<Row> parseCsvLine(std::string_view line, char sep = ',') {
    if (line.empty()) return std::nullopt;
    Row row;
    for (auto& field : split(line, sep)) {
        if (field.size() >= 2 && field.front() == '"' && field.back() == '"') {
            field = field.substr(1, field.size() - 2);
        }
        row.fields.push_back(field);
    }
    return row;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: csvstats <file.csv>\n";
        return 1;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open " << argv[1] << '\n';
        return 1;
    }
    std::size_t rows = 0, fields = 0, chars = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (auto row = parseCsvLine(line)) {
            ++rows;
            fields += row->fields.size();
            for (const auto& f : row->fields) chars += f.size();
        }
    }
    std::cout << "rows:   " << rows << '\n'
              << "fields: " << fields << '\n'
              << "chars:  " << chars << '\n';
    return 0;
}
```

With **Over 50 lines** selected, the `//` comments let you count the break:
lines above the cut stay on the first page, the rest flow onto the next.

## Headings keep with following text

A heading is kept with the paragraph that follows it, so it is never the last
thing on a page — no stranded title above a page of white space.

### This heading travels with the text below it

Because **Keep headings with following text** is on, the renderer asks the
browser to avoid a page break immediately after this heading. If the heading
lands near the bottom of a page, it moves to the next page together with this
paragraph.

## Orphans and widows

<!-- page-break -->

The paragraph below is long enough to wrap across a page boundary, which is
exactly where orphan and widow control shows up. An **orphan** is the first
line of a paragraph left alone at the bottom of a page; a **widow** is the
last line of a paragraph pushed alone onto the top of the next page. With
**Avoid orphan/widow lines** on, Scriba tells the browser to keep at least a
couple of lines together at each break (`orphans`/`widows`), so a page never
ends or begins with a single dangling line. The effect is subtle and easy to
miss, but it is the difference between a document that looks typeset and one
that looks like a long text file. Note that Chromium's support for these two
CSS properties in print layout is only partial, so the control is best-effort
— the preference is applied, and on engines that honor it the paragraph keeps
its lines together at every page boundary it crosses.

## Page geometry

The **Margin** and **Page size** fields accept free-form CSS values: margins
like `18mm` or `0`, and sizes like `A4`, `A5`, `Letter`, `A4 landscape`, or an
explicit `size: 210mm 297mm`. The override is applied to the `@page` rule last
in the cascade, so it wins over the base `15mm` default and the custom print
CSS. A smaller margin fits more per page; a wider page (with **Split code
blocks** on) gives code blocks more room before they split.

## Quick reference

| Directive | Effect | Applies to |
|---|---|---|
| `<!-- keep -->` | keep the next block on one page | table, code block, figure, heading, paragraph |
| `<!-- page-break -->` | start a new page at the next block | any top-level block |

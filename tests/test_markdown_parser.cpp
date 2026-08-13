// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include <gtest/gtest.h>
#include "MarkdownParser.h"
#include "MdTable.h"
#include "Preferences.h"
#include <QSettings>

TEST(MarkdownParserTest, EmptyInput) {
    QString html = MarkdownParser::toHtml("");
    EXPECT_TRUE(html.isEmpty() || html.contains("<p"));
}

TEST(MarkdownParserTest, BasicHeading) {
    QString html = MarkdownParser::toHtml("# Hello");
    EXPECT_TRUE(html.contains("<h1"));
    EXPECT_TRUE(html.contains("data-line=\"1\""));
    EXPECT_TRUE(html.contains("Hello"));
    EXPECT_TRUE(html.contains("</h1>"));
}

TEST(MarkdownParserTest, HeadingLevels) {
    QString html = MarkdownParser::toHtml("## H2\n### H3\n#### H4");
    EXPECT_TRUE(html.contains("<h2"));
    EXPECT_TRUE(html.contains("<h3"));
    EXPECT_TRUE(html.contains("<h4"));
}

TEST(MarkdownParserTest, BoldAndItalic) {
    QString html = MarkdownParser::toHtml("**bold** and *italic*");
    EXPECT_TRUE(html.contains("<strong>"));
    EXPECT_TRUE(html.contains("</strong>"));
    EXPECT_TRUE(html.contains("<em>"));
    EXPECT_TRUE(html.contains("</em>"));
    EXPECT_TRUE(html.contains("bold"));
    EXPECT_TRUE(html.contains("italic"));
}

TEST(MarkdownParserTest, InlineCode) {
    QString html = MarkdownParser::toHtml("Use `code` here");
    EXPECT_TRUE(html.contains("<code>"));
    EXPECT_TRUE(html.contains("code"));
}

TEST(MarkdownParserTest, FencedCodeBlock) {
    QString html = MarkdownParser::toHtml("```cpp\nint x = 1;\n```");
    EXPECT_TRUE(html.contains("<pre"));
    EXPECT_TRUE(html.contains("language-cpp"));
    EXPECT_TRUE(html.contains("data-lang=\"cpp\""));
    EXPECT_TRUE(html.contains("data-line=\"1\""));
    EXPECT_TRUE(html.contains("int x = 1;"));
}

TEST(MarkdownParserTest, FencedCodeBlockNoLang) {
    QString html = MarkdownParser::toHtml("```\nplain code\n```");
    EXPECT_TRUE(html.contains("<pre"));
    EXPECT_TRUE(html.contains("language-"));
    EXPECT_FALSE(html.contains("data-lang"));
    EXPECT_TRUE(html.contains("plain code"));
}

TEST(MarkdownParserTest, Table) {
    QString md = "| A | B |\n|---|---|\n| 1 | 2 |";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<thead>"));
    EXPECT_TRUE(html.contains("<tbody>"));
    EXPECT_TRUE(html.contains("<th"));
    EXPECT_TRUE(html.contains("<td"));
    EXPECT_TRUE(html.contains("A"));
    EXPECT_TRUE(html.contains("2"));
}

TEST(MarkdownParserTest, PaddingZeroFormattedTableStillRenders) {
    // formatMdTable with padding 0 must never emit a separator cell without a
    // dash (`:` or `::`), or md4c rejects the delimiter row and the whole table
    // collapses into plain paragraphs. The floor-3 body keeps `|:-:|` valid.
    QString table = MdTable::formatMdTable(
        {"| h | c |", "|:--:|:--:|", "| a | bb |"}, 0);
    EXPECT_EQ(table,
        "| h | c |\n"
        "|:-:|:-:|\n"
        "| a |bb |");
    QString html = MarkdownParser::toHtml(table);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<th"));
    EXPECT_TRUE(html.contains("<td"));
    EXPECT_TRUE(html.contains("bb"));
}

TEST(MarkdownParserTest, PaddedBorderlessEmptyRowStaysInTable) {
    // The app caps a borderless empty row's leading padding at three spaces.
    // md4c's indented-code check runs before its table-continuation check, so a
    // continuation line with four or more leading spaces silently ends the
    // table (GFM keeps the row); padding≥2 would otherwise push borderless rows
    // past the limit — see docs/gotchas.md.
    QString md = "foo | bar\n--- | ---\na | b\n   |    ";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<tbody>"));
    EXPECT_FALSE(html.contains("<pre"));
}

TEST(MarkdownParserTest, PaddedBorderedEmptyRowStaysInTable) {
    // Bordered rows start with a pipe, so padding never produces leading spaces
    // and there is no md4c indented-code risk (regression guard).
    QString md = "| foo | bar |\n|-----|-----|\n| a | b |\n|     |     |";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<tbody>"));
    EXPECT_FALSE(html.contains("<pre"));
}

TEST(MarkdownParserTest, TableEndsAtAtxHeader) {
    // GFM: a table is broken at the start of another block-level structure.
    // A "# " line following table rows must become a header, not a table row.
    QString md = "| A | B |\n|---|---|\n| 1 | 2 |\n# Next Section";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<h1"));
    EXPECT_TRUE(html.contains("Next Section"));
    // The header must NOT be swallowed into a 3rd table row.
    EXPECT_FALSE(html.contains("<td>1</td><td>2</td>\n<td>Next"));
}

TEST(MarkdownParserTest, TableEndsAtFencedCodeBlock) {
    QString md = "| A | B |\n|---|---|\n| 1 | 2 |\n```\ncode\n```";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<code"));
    EXPECT_FALSE(html.contains("<td>NextTest"));
}

TEST(MarkdownParserTest, TableEndsAtHtmlBlock) {
    QString md = "| A | B |\n|---|---|\n| 1 | 2 |\n<div>x</div>";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<div>"));
}

TEST(MarkdownParserTest, TableEndsAtBlankLine) {
    QString md = "| A | B |\n|---|---|\n| 1 | 2 |\n\ntext after";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<table>"));
    EXPECT_TRUE(html.contains("<p"));
    EXPECT_FALSE(html.contains("<td>NextTest"));
}

TEST(MarkdownParserTest, TaskList) {
    QString md = "- [x] done\n- [ ] todo";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("task-list-item"));
    EXPECT_TRUE(html.contains("task-list-item-checkbox"));
    EXPECT_TRUE(html.contains("checked"));
    EXPECT_TRUE(html.contains("done"));
    EXPECT_TRUE(html.contains("todo"));
}

TEST(MarkdownParserTest, Strikethrough) {
    QString html = MarkdownParser::toHtml("~~deleted~~");
    EXPECT_TRUE(html.contains("<del>"));
    EXPECT_TRUE(html.contains("deleted"));
}

TEST(MarkdownParserTest, Highlight) {
    QString html = MarkdownParser::toHtml("==highlighted==");
    EXPECT_TRUE(html.contains("<mark>"));
    EXPECT_TRUE(html.contains("highlighted"));
}

TEST(MarkdownParserTest, Link) {
    QString html = MarkdownParser::toHtml("[click](https://example.com)");
    EXPECT_TRUE(html.contains("<a href="));
    EXPECT_TRUE(html.contains("https://example.com"));
    EXPECT_TRUE(html.contains("click"));
}

TEST(MarkdownParserTest, LinkWithTitle) {
    QString md = R"([click](https://example.com "My Title"))";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("title="));
    EXPECT_TRUE(html.contains("My Title"));
}

TEST(MarkdownParserTest, Image) {
    QString html = MarkdownParser::toHtml("![alt text](image.png)");
    EXPECT_TRUE(html.contains("<img"));
    EXPECT_TRUE(html.contains("src="));
    EXPECT_TRUE(html.contains("image.png"));
    EXPECT_TRUE(html.contains("alt="));
    EXPECT_TRUE(html.contains("alt text"));
}

TEST(MarkdownParserTest, ImageWithTitle) {
    QString md = R"(![alt](pic.jpg "Photo Title"))";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<img"));
    EXPECT_TRUE(html.contains("title="));
    EXPECT_TRUE(html.contains("Photo Title"));
}

TEST(MarkdownParserTest, ImageWithDimensions) {
    QString html = MarkdownParser::toHtml("![alt](img.png#400x200)");
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("max-width: 400px"));
    EXPECT_TRUE(html.contains("max-height: 200px"));
    EXPECT_TRUE(html.contains("alt=\"alt\""));
}

TEST(MarkdownParserTest, ImageWithWidthOnly) {
    QString html = MarkdownParser::toHtml("![alt](img.png#400x)");
    EXPECT_TRUE(html.contains("max-width: 400px"));
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_FALSE(html.contains("max-height"));
}

TEST(MarkdownParserTest, ImageWithHeightOnly) {
    QString html = MarkdownParser::toHtml("![alt](img.png#x200)");
    EXPECT_TRUE(html.contains("max-height: 200px"));
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_FALSE(html.contains("max-width"));
}

TEST(MarkdownParserTest, ImageWithoutDimensions) {
    QString html = MarkdownParser::toHtml("![alt](img.png)");
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_FALSE(html.contains("style=\""));
    EXPECT_FALSE(html.contains("max-width"));
    EXPECT_FALSE(html.contains("max-height"));
}

TEST(MarkdownParserTest, SvgWithDimensionUpscales) {
    QString html = MarkdownParser::toHtml("![alt](img.svg#400x)");
    EXPECT_TRUE(html.contains("src=\"img.svg\""));
    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("width: 400px"));
    EXPECT_FALSE(html.contains("max-width"));
    EXPECT_TRUE(html.contains("alt=\"alt\""));
}

TEST(MarkdownParserTest, SvgWithDimensions) {
    QString html = MarkdownParser::toHtml("![alt](img.svg#400x100)");
    EXPECT_TRUE(html.contains("width: 400px"));
    EXPECT_TRUE(html.contains("height: 100px"));
    EXPECT_FALSE(html.contains("max-"));
}

TEST(MarkdownParserTest, SvgWithoutDimensions) {
    QString html = MarkdownParser::toHtml("![alt](img.svg)");
    EXPECT_TRUE(html.contains("src=\"img.svg\""));
    EXPECT_FALSE(html.contains("style=\""));
    EXPECT_FALSE(html.contains("width:"));
    EXPECT_FALSE(html.contains("height:"));
}

TEST(MarkdownParserTest, ImageWithDimensionsAndTitle) {
    QString html = MarkdownParser::toHtml("![alt](img.png#400x200 \"title\")");
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("max-width: 400px"));
    EXPECT_TRUE(html.contains("max-height: 200px"));
    EXPECT_TRUE(html.contains("title=\"title\""));
}

TEST(MarkdownParserTest, RawHtmlPassthrough) {
    QString html = MarkdownParser::toHtml("<div>raw html</div>");
    EXPECT_TRUE(html.contains("<div>raw html</div>"));
}

TEST(MarkdownParserTest, Blockquote) {
    QString md = "> quoted text";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<blockquote>"));
    EXPECT_TRUE(html.contains("quoted text"));
}

TEST(MarkdownParserTest, HorizontalRule) {
    QString html = MarkdownParser::toHtml("---");
    EXPECT_TRUE(html.contains("<hr"));
    EXPECT_TRUE(html.contains("data-line="));
}

TEST(MarkdownParserTest, ParagraphDataLine) {
    QString md = "line1\n\nline2";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("data-line=\"1\""));
    EXPECT_TRUE(html.count("data-line=") >= 2);
}

TEST(MarkdownParserTest, HtmlEscaping) {
    QString md = "`<script>alert(1)</script>`";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("&lt;script&gt;"));
    EXPECT_FALSE(html.contains("<script>"));
}

TEST(MarkdownParserTest, OrderedList) {
    QString md = "1. First\n2. Second\n3. Third";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<ol>"));
    EXPECT_TRUE(html.contains("<li"));
    EXPECT_TRUE(html.contains("First"));
    EXPECT_TRUE(html.contains("Third"));
}

TEST(MarkdownParserTest, OrderedListParen) {
    QString md = "1) First\n2) Second\n3) Third";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<ol>"));
    EXPECT_TRUE(html.contains("<li"));
    EXPECT_TRUE(html.contains("First"));
    EXPECT_TRUE(html.contains("Third"));
}

TEST(MarkdownParserTest, OrderedListStart) {
    QString md = "3. Third\n4. Fourth";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<ol start=\"3\">"));
    EXPECT_TRUE(html.contains("Third"));
    EXPECT_TRUE(html.contains("Fourth"));
}

TEST(MarkdownParserTest, OrderedListMarkerStyles) {
    QSettings settings;
    const struct { const char *key; const char *cls; } cases[] = {
        { "decimal", "" },
        { "decimal-paren", "md-list-decimal-paren" },
        { "alpha", "md-list-alpha" },
        { "alpha-paren", "md-list-alpha-paren" },
        { "roman", "md-list-roman" },
        { "roman-paren", "md-list-roman-paren" },
    };
    for (const auto &c : cases) {
        settings.setValue(Preferences::OrderedListMarker, c.key);
        QString html = MarkdownParser::toHtml("1. one\n2. two");
        if (c.cls[0])
            EXPECT_TRUE(html.contains(QString("<ol class=\"%1\">").arg(c.cls)))
                << "key " << c.key << " must emit class " << c.cls;
        else
            EXPECT_TRUE(html.contains("<ol>"))
                << "key " << c.key << " must emit a bare <ol>";
    }
    settings.setValue(Preferences::OrderedListMarker, Preferences::defaultOrderedListMarker());
}

TEST(MarkdownParserTest, OrderedListStartAndMarkerStyleCombine) {
    QSettings settings;
    settings.setValue(Preferences::OrderedListMarker, "alpha-paren");
    QString html = MarkdownParser::toHtml("3. three\n4. four");
    EXPECT_TRUE(html.contains("<ol start=\"3\" class=\"md-list-alpha-paren\">"));
    settings.setValue(Preferences::OrderedListMarker, Preferences::defaultOrderedListMarker());
}

TEST(MarkdownParserTest, UnorderedList) {
    QString md = "- one\n- two\n- three";
    QString html = MarkdownParser::toHtml(md);
    EXPECT_TRUE(html.contains("<ul>"));
    EXPECT_TRUE(html.contains("one"));
    EXPECT_TRUE(html.contains("three"));
}

TEST(MarkdownParserTest, SoftBreakIncrementsLine) {
    QString md = "line1\nline2\nline3";
    QString html = MarkdownParser::toHtml(md);
    int count = html.count("data-line=");
    EXPECT_GE(count, 1);
}

TEST(MarkdownParserTest, RawHtmlBlockedWhenNoHtmlFlag) {
    QString html = MarkdownParser::toHtml("<div>raw html</div>", true);
    EXPECT_FALSE(html.contains("<div>"));
    EXPECT_FALSE(html.contains("</div>"));
}

TEST(MarkdownParserTest, NoHtmlFlagPreservesMarkdown) {
    QString html = MarkdownParser::toHtml("# Hello\n\n**bold**", true);
    EXPECT_TRUE(html.contains("<h1"));
    EXPECT_TRUE(html.contains("<strong>"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksScriptTags) {
    QString html = MarkdownParser::toHtml("<script>alert('xss')</script>", true);
    EXPECT_FALSE(html.contains("<script>"));
    EXPECT_TRUE(html.contains("&lt;script&gt;"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksInlineEventHandlers) {
    QString html = MarkdownParser::toHtml("<img src=x onerror=\"alert(1)\">", true);
    EXPECT_FALSE(html.contains("<img"));
    EXPECT_TRUE(html.contains("&lt;img"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksIframe) {
    QString html = MarkdownParser::toHtml("<iframe src=\"https://evil.com\"></iframe>", true);
    EXPECT_FALSE(html.contains("<iframe"));
    EXPECT_TRUE(html.contains("&lt;iframe"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksObject) {
    QString html = MarkdownParser::toHtml("<object data=\"evil.swf\"></object>", true);
    EXPECT_FALSE(html.contains("<object"));
    EXPECT_TRUE(html.contains("&lt;object"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksEmbed) {
    QString html = MarkdownParser::toHtml("<embed src=\"evil.svg\">", true);
    EXPECT_FALSE(html.contains("<embed"));
    EXPECT_TRUE(html.contains("&lt;embed"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksStyleBlocks) {
    QString html = MarkdownParser::toHtml("<style>body{display:none}</style>", true);
    EXPECT_FALSE(html.contains("<style>"));
    EXPECT_TRUE(html.contains("&lt;style&gt;"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksExternalScript) {
    QString html = MarkdownParser::toHtml("<script src=\"https://evil.com/hook.js\"></script>", true);
    EXPECT_FALSE(html.contains("<script"));
    EXPECT_TRUE(html.contains("&lt;script"));
}

TEST(MarkdownParserTest, NoHtmlFlagBlocksNestedRawHtml) {
    QString html = MarkdownParser::toHtml("<div><span style=\"color:red\">nested</span></div>", true);
    EXPECT_FALSE(html.contains("<div>"));
    EXPECT_TRUE(html.contains("&lt;div"));
    EXPECT_TRUE(html.contains("&lt;span"));
    EXPECT_TRUE(html.contains("nested"));
}

TEST(MarkdownParserTest, NoHtmlFlagDoesNotEscapeNormalMarkdownLinks) {
    QString html = MarkdownParser::toHtml("[click](https://example.com?a=1&b=2)", true);
    EXPECT_TRUE(html.contains("<a"));
    EXPECT_TRUE(html.contains("href"));
}

TEST(MarkdownParserTest, NoHtmlFlagDoesNotEscapeCodeFence) {
    QString html = MarkdownParser::toHtml("```\n<div>escaped in code</div>\n```", true);
    EXPECT_TRUE(html.contains("<code"));
    EXPECT_TRUE(html.contains("&lt;div&gt;"));
}

TEST(MarkdownParserTest, FootnoteReference) {
    QString html = MarkdownParser::toHtml("Text with a note.[^1]\n\n[^1]: The note body.");
    EXPECT_TRUE(html.contains("<sup><a href=\"#fn-1\" id=\"fnref-1-1\">1</a></sup>"));
    EXPECT_TRUE(html.contains("<section class=\"footnotes\"><ol>"));
    EXPECT_TRUE(html.contains("<li id=\"fn-1\">"));
    EXPECT_TRUE(html.contains("</ol></section>"));
    EXPECT_TRUE(html.contains("The note body."));
}

TEST(MarkdownParserTest, FootnoteNamedLabel) {
    QString html = MarkdownParser::toHtml("See the note.[^my-note]\n\n[^my-note]: Body text.");
    EXPECT_TRUE(html.contains("id=\"fnref-1-1\""));
    EXPECT_TRUE(html.contains("Body text."));
}

TEST(MarkdownParserTest, FootnoteMultipleRefs) {
    QString html = MarkdownParser::toHtml("[^a] one and [^a] two.\n\n[^a]: Same note.");
    EXPECT_TRUE(html.contains("id=\"fnref-1-1\""));
    EXPECT_TRUE(html.contains("id=\"fnref-1-2\""));
    EXPECT_TRUE(html.contains("class=\"footnote-backref\""));
}

TEST(MarkdownParserTest, PermissiveEmailAutolink) {
    QString html = MarkdownParser::toHtml("Mail someone@example.com today.");
    EXPECT_TRUE(html.contains("<a href="));
    EXPECT_TRUE(html.contains("someone@example.com"));
}

TEST(MarkdownParserTest, PermissiveWwwAutolink) {
    QString html = MarkdownParser::toHtml("See www.example.com for details.");
    EXPECT_TRUE(html.contains("<a href="));
    EXPECT_TRUE(html.contains("www.example.com"));
}

TEST(MarkdownParserTest, PermissiveUrlAutolink) {
    QString html = MarkdownParser::toHtml("Go to https://example.com now.");
    EXPECT_TRUE(html.contains("<a href="));
    EXPECT_TRUE(html.contains("https://example.com"));
}

TEST(MarkdownParserTest, Superscript) {
    QString html = MarkdownParser::toHtml("E = mc^2^");
    EXPECT_TRUE(html.contains("<sup>"));
    EXPECT_TRUE(html.contains("</sup>"));
    EXPECT_TRUE(html.contains("2"));
}

TEST(MarkdownParserTest, Subscript) {
    QString html = MarkdownParser::toHtml("H~2~O");
    EXPECT_TRUE(html.contains("<sub>"));
    EXPECT_TRUE(html.contains("</sub>"));
    EXPECT_TRUE(html.contains("2"));
}

TEST(MarkdownParserTest, InlineMathProducesKatexSpanNoSupSub) {
    // Math spans must protect their body from superscript/subscript parsing:
    // md4c recognizes $...$ and disables inner marks (e.g. ^2 must not become
    // <sup>), and the renderer emits a semantic .katex span with the raw TeX.
    QString html = MarkdownParser::toHtml("Inline $a^2$ and $b_3$.");
    EXPECT_TRUE(html.contains("<span class=\"katex\" data-tex=\"a^2\""));
    EXPECT_TRUE(html.contains("<span class=\"katex\" data-tex=\"b_3\""));
    EXPECT_FALSE(html.contains("<sup>")) << "^ inside math must not become <sup>";
    EXPECT_FALSE(html.contains("<sub>")) << "_ inside math must not become <sub>";
    EXPECT_TRUE(html.contains("data-line=\"1\""));
}

TEST(MarkdownParserTest, InlineMathPreservesDelimiterFreeTex) {
    // The data-tex value must be the inner TeX without $ delimiters.
    QString html = MarkdownParser::toHtml("$E = mc^2$");
    EXPECT_TRUE(html.contains("data-tex=\"E = mc^2\""));
}

TEST(MarkdownParserTest, DisplayMathWrapsKatexDisplay) {
    QString html = MarkdownParser::toHtml("$$E=mc^2$$");
    EXPECT_TRUE(html.contains("<span class=\"katex-display\">"));
    EXPECT_TRUE(html.contains("<span class=\"katex\" data-tex=\"E=mc^2\""));
}

TEST(MarkdownParserTest, SuperscriptOutsideMathUnchanged) {
    QString html = MarkdownParser::toHtml("Volume m^3^ without math.");
    EXPECT_TRUE(html.contains("<sup>"));
    EXPECT_TRUE(html.contains("</sup>"));
    EXPECT_TRUE(html.contains("3"));
}

TEST(MarkdownParserTest, HardSoftBreaksDefaultOff) {
    QString html = MarkdownParser::toHtml("line one\nline two");
    EXPECT_FALSE(html.contains("<br"));
    EXPECT_TRUE(html.contains("line one"));
    EXPECT_TRUE(html.contains("line two"));
}

TEST(MarkdownParserTest, HardSoftBreaksPrefOn) {
    QSettings settings;
    settings.setValue(Preferences::HardSoftBreaks, true);
    QString html = MarkdownParser::toHtml("line one\nline two");
    EXPECT_TRUE(html.contains("<br>"));
    settings.setValue(Preferences::HardSoftBreaks, false);
}

// ---- In-source directives: <!-- keep --> / <!-- page-break --> ----
// Task 0 contract (DR-1): tokens are replaced by the scanner, stripped by the
// renderer, and the class lands on the next top-level block. Directive lines
// keep their line count, so the block after a directive carries its real
// source line (the local md4c block-start-offset patch feeds data-line).

TEST(MarkdownParserTest, DirectiveKeepAboveCodeBlock) {
    QString html = MarkdownParser::toHtml("<!-- keep -->\n```cpp\nint x = 1;\n```");
    EXPECT_TRUE(html.contains("<pre data-line=\"2\" data-lang=\"cpp\" class=\"scriba-keep\">"));
    EXPECT_FALSE(html.contains("<!--"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

TEST(MarkdownParserTest, DirectiveKeepAboveCodeBlockNoHtmlMode) {
    // Default app mode passes noHtml=true (MD_FLAG_NOHTML); tokens must still
    // survive and be stripped so the directive is invisible in the preview.
    QString html = MarkdownParser::toHtml("<!-- keep -->\n```\ncode\n```", true);
    EXPECT_TRUE(html.contains("class=\"scriba-keep\""));
    EXPECT_FALSE(html.contains("<!--"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

TEST(MarkdownParserTest, DirectivePageBreakAboveHeading) {
    QString html = MarkdownParser::toHtml("<!-- page-break -->\n## Heading");
    EXPECT_TRUE(html.contains("<h2 data-line=\"2\" class=\"scriba-page-break\">"));
    EXPECT_FALSE(html.contains("<!--"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

TEST(MarkdownParserTest, DirectiveAboveParagraph) {
    QString html = MarkdownParser::toHtml("<!-- keep -->\n\nSome paragraph.");
    EXPECT_TRUE(html.contains("<p data-line=\"3\" class=\"scriba-keep\">"));
    EXPECT_TRUE(html.contains("Some paragraph."));
    EXPECT_FALSE(html.contains("<!--"));
}

TEST(MarkdownParserTest, DirectiveAboveTable) {
    QString html = MarkdownParser::toHtml(
        "<!-- keep -->\n\n| A | B |\n|---|---|\n| 1 | 2 |");
    EXPECT_TRUE(html.contains("<table class=\"scriba-keep\">"));
    EXPECT_FALSE(html.contains("<!--"));
}

TEST(MarkdownParserTest, DirectiveStackedAdjacentCombine) {
    // Adjacent lines merge into one token paragraph via softbreak (which also
    // advances the line counter, so do not assert data-line here — DR-1).
    QString html = MarkdownParser::toHtml("<!-- keep -->\n<!-- page-break -->\n# Title");
    EXPECT_TRUE(html.contains("<h1 ") && html.contains("class=\"scriba-keep scriba-page-break\">"));
    EXPECT_FALSE(html.contains("<!--"));
}

TEST(MarkdownParserTest, DirectiveStackedBlankSeparatedCombine) {
    // Blank-line-separated directives form two token paragraphs that must
    // accumulate their classes (never apply one to the other).
    QString html = MarkdownParser::toHtml("<!-- keep -->\n\n<!-- page-break -->\n# Title");
    EXPECT_TRUE(html.contains("<h1 data-line=\"4\" class=\"scriba-keep scriba-page-break\">"));
    EXPECT_FALSE(html.contains("<!--"));
}

TEST(MarkdownParserTest, DirectiveAliasesMapToClasses) {
    QString html = MarkdownParser::toHtml(
        "<!-- keep-together -->\n\n| A | B |\n|---|---|\n| 1 | 2 |");
    EXPECT_TRUE(html.contains("<table class=\"scriba-keep\">"));
}

TEST(MarkdownParserTest, DirectiveInsideFenceStaysLiteral) {
    QString html = MarkdownParser::toHtml("```\n<!-- keep -->\n```");
    EXPECT_TRUE(html.contains("&lt;!-- keep --&gt;"));
    EXPECT_FALSE(html.contains("scriba-keep"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

TEST(MarkdownParserTest, DirectiveInlineNotRecognized) {
    QString html = MarkdownParser::toHtml("para <!-- keep --> text");
    EXPECT_FALSE(html.contains("scriba-keep"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

TEST(MarkdownParserTest, DirectiveIndentedInListItemNotRecognized) {
    QString html = MarkdownParser::toHtml("- item\n  <!-- keep -->");
    EXPECT_FALSE(html.contains("scriba-keep"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

TEST(MarkdownParserTest, DirectiveAtEndDropped) {
    QString html = MarkdownParser::toHtml("Some text.\n\n<!-- page-break -->");
    EXPECT_FALSE(html.contains("page-break"));
    EXPECT_FALSE(html.contains("<!--"));
    EXPECT_TRUE(html.contains("Some text."));
}

TEST(MarkdownParserTest, DirectiveAfterParagraphNoBlankIgnored) {
    // A directive line with no blank line before it merges into the preceding
    // paragraph; the token is stripped but no class is applied (DR-1).
    QString html = MarkdownParser::toHtml("para\n<!-- keep -->");
    EXPECT_TRUE(html.contains("<p"));
    EXPECT_TRUE(html.contains("para"));
    EXPECT_FALSE(html.contains("scriba-keep"));
    EXPECT_FALSE(html.contains("SCRIBADIR"));
}

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
#include <QApplication>
#include <QDir>
#include <QUrl>
#include <QFile>

#include "TestConfig.h"
#include "io/HtmlToMarkdown.h"
#include "preview/MarkdownParser.h"

// Runs turndown.js in a real QWebEnginePage, so these tests exercise the
// exact conversion path used by the app (HTML -> Markdown).
static QString convert(const QString &html, const QUrl &baseUrl = {})
{
    return HtmlToMarkdown::convert(html, baseUrl);
}

// Collapses all whitespace runs (including newlines) to single spaces so
// assertions are insensitive to turndown's exact line-wrapping conventions.
static QString norm(const QString &s)
{
    return s.simplified();
}

TEST(HtmlToMarkdown, HeadingsAndEmphasis)
{
    const QString md = convert(
        "<h1>Title</h1><p>Some <strong>bold</strong>, <em>italic</em> and "
        "<code>mono</code> text.</p><h2>Sub</h2>");
    EXPECT_EQ(norm(md), "# Title Some **bold**, *italic* and `mono` text. ## Sub");
}

TEST(HtmlToMarkdown, LinksAndImages)
{
    const QString md = convert(
        "<p>See <a href=\"https://example.com/a\">the docs</a> or "
        "<img src=\"https://example.com/pic.png\" alt=\"diagram\">.</p>");
    EXPECT_EQ(norm(md),
              "See [the docs](https://example.com/a) or ![diagram](https://example.com/pic.png).");
}

TEST(HtmlToMarkdown, ResolvesRelativeUrlsAgainstBase)
{
    const QString base = QUrl::fromLocalFile(QDir::tempPath() + "/docs/page.html").toString();
    const QString md = convert(
        "<p><a href=\"guide.html\">guide</a> "
        "<a href=\"../up.md\">up</a> <img src=\"images/x.png\" alt=\"x\"> "
        "<a href=\"#anchor\">anchor</a></p>",
        QUrl(base));
    EXPECT_EQ(norm(md),
              "[guide](" + QUrl::fromLocalFile(QDir::tempPath() + "/docs/guide.html").toString() + ") "
              "[up](" + QUrl::fromLocalFile(QDir::tempPath() + "/up.md").toString() + ") "
              "![x](" + QUrl::fromLocalFile(QDir::tempPath() + "/docs/images/x.png").toString() + ") "
              "[anchor](#anchor)");
}

TEST(HtmlToMarkdown, Lists)
{
    const QString md = convert(
        "<ul><li>one</li><li>two<ul><li>nested</li></ul></li></ul>"
        "<ol><li>first</li><li>second</li></ol>");
    EXPECT_EQ(norm(md), "- one - two - nested 1. first 2. second");
}

TEST(HtmlToMarkdown, FencedCodeBlock)
{
    const QString md = convert(
        "<pre><code class=\"language-cpp\">int main() { return 0; }</code></pre>");
    EXPECT_EQ(norm(md), "```cpp int main() { return 0; } ```");
}

TEST(HtmlToMarkdown, Table)
{
    const QString md = convert(
        "<table><thead><tr><th>A</th><th>B</th></tr></thead>"
        "<tbody><tr><td>1</td><td>2</td></tr></tbody></table>");
    EXPECT_EQ(norm(md), "| A | B | | --- | --- | | 1 | 2 |");
}

TEST(HtmlToMarkdown, Blockquote)
{
    const QString md = convert(
        "<blockquote><p>To be or not to be</p></blockquote>");
    EXPECT_EQ(norm(md), "> To be or not to be");
}

TEST(HtmlToMarkdown, StrikethroughAndTaskList)
{
    const QString md = convert(
        "<ul><li><input type=\"checkbox\" checked> done</li>"
        "<li><input type=\"checkbox\"> todo</li></ul>"
        "<p>Strike <s>this</s>.</p>");
    EXPECT_EQ(norm(md), "- [x] done - [ ] todo Strike ~~this~~.");
}

TEST(HtmlToMarkdown, StripsScriptsAndStyles)
{
    const QString md = convert(
        "<script>alert('x')</script><style>p{color:red}</style>"
        "<p>Keep <script>bad()</script>me.</p>");
    EXPECT_EQ(norm(md), "Keep me.");
}

TEST(HtmlToMarkdown, EmptyInputGivesEmpty)
{
    EXPECT_EQ(convert("<script>alert(1)</script>"), QString());
    EXPECT_EQ(convert("<style>p{}</style>"), QString());
    EXPECT_EQ(convert(""), QString());
}

TEST(HtmlToMarkdown, PlainTextFallback)
{
    EXPECT_EQ(norm(convert("<div>just some text</div>")), "just some text");
}

TEST(HtmlToMarkdown, FullDocumentIgnoresHead)
{
    const QString md = convert(
        "<!DOCTYPE html><html><head><title>Ignored</title></head>"
        "<body><p>Body text</p></body></html>");
    EXPECT_EQ(norm(md), "Body text");
}

// Footnote reference/definition syntax is Scriba's own marker convention.
// Turndown escapes the brackets ([\^1]), which must be undone so the
// markers survive the HTML -> Markdown round trip.
TEST(HtmlToMarkdown, FootnotesPassThrough)
{
    const QString md = convert(
        "<p>Text with a footnote [^1] and another [^f2].</p>"
        "<p>[^1]: Definition one.</p>"
        "<p>[^f2]: Definition two.</p>");
    EXPECT_EQ(norm(md), "Text with a footnote [^1] and another [^f2]. [^1]: Definition one. [^f2]: Definition two.");
}

// Math delimiters ($...$, $$...$$) are plain text as far as turndown is
// concerned; MathML -> LaTeX output must keep them intact.
TEST(HtmlToMarkdown, MathDollarSurvives)
{
    const QString md = convert(
        "<p>Inline $a^2$ and display $$\\frac{a}{b}$$ both survive.</p>");
    EXPECT_EQ(norm(md), "Inline $a^2$ and display $$\\\\frac{a}{b}$$ both survive.");
}

// Definition lists round-trip to Scriba's `term\n: def` syntax (the same
// markers MarkdownParser::toHtml emits). Structure matters here — norm()
// would collapse the newlines — so assert on the raw trimmed output.
static QString defConvert(const QString &html)
{
    return HtmlToMarkdown::convert(html).trimmed();
}

TEST(HtmlToMarkdown, DefinitionListBasic)
{
    EXPECT_EQ(defConvert("<dl><dt>Apple</dt><dd>A fruit.</dd></dl>"),
              "Apple\n: A fruit.");
}

TEST(HtmlToMarkdown, DefinitionListMultipleGroups)
{
    // A <dt> after a <dd> starts a new term group; groups are separated by a
    // blank line so the second term isn't swallowed as a lazy continuation.
    EXPECT_EQ(defConvert(
                  "<dl><dt>Apple</dt><dd>A fruit.</dd>"
                  "<dt>Car</dt><dd>A vehicle.</dd></dl>"),
              "Apple\n: A fruit.\n\nCar\n: A vehicle.");
}

TEST(HtmlToMarkdown, DefinitionListMultiTermMultiDef)
{
    EXPECT_EQ(defConvert(
                  "<dl><dt>Term A</dt><dt>Term B</dt>"
                  "<dd>Def one</dd><dd>Def two</dd></dl>"),
              "Term A\nTerm B\n: Def one\n: Def two");
}

TEST(HtmlToMarkdown, DefinitionListInlineMarkup)
{
    EXPECT_EQ(defConvert(
                  "<dl><dt>Apple</dt><dd>A <em>fruit</em> with "
                  "<code>seeds</code>.</dd></dl>"),
              "Apple\n: A *fruit* with `seeds`.");
}

TEST(HtmlToMarkdown, DefinitionListRoundTripThroughParser)
{
    // The <dl> HTML emitted by the renderer imports back to the same syntax.
    const QString rendered = MarkdownParser::toHtml(
        QStringLiteral("Apple\n: A fruit.\n\nCar\n: A vehicle."));
    const QString md = defConvert(rendered);
    EXPECT_EQ(md, "Apple\n: A fruit.\n\nCar\n: A vehicle.");
}

TEST(HtmlToMarkdown, ImageStyleSizeBecomesSuffix)
{
    const QString md = convert(
        "<p><img src=\"pic.png\" alt=\"diagram\" "
        "style=\"width:150px;height:120px\"></p>");
    EXPECT_EQ(norm(md), "![diagram](pic.png#150x120)");
}

TEST(HtmlToMarkdown, ImageAttributeSizeBecomesSuffix)
{
    const QString md = convert(
        "<p><img src=\"pic.png\" alt=\"diagram\" "
        "width=\"150\" height=\"120\"></p>");
    EXPECT_EQ(norm(md), "![diagram](pic.png#150x120)");
}

TEST(HtmlToMarkdown, ImageSingleDimensionBecomesSuffix)
{
    EXPECT_EQ(norm(convert(
                  "<p><img src=\"a.png\" alt=\"a\" style=\"width:400px\"></p>")),
              "![a](a.png#400x)");
    EXPECT_EQ(norm(convert(
                  "<p><img src=\"b.png\" alt=\"b\" style=\"height:300px\"></p>")),
              "![b](b.png#x300)");
}

TEST(HtmlToMarkdown, ImageNonPixelSizeUnchanged)
{
    const QString md = convert(
        "<p><img src=\"pic.png\" alt=\"diagram\" "
        "style=\"width:50%;height:auto\"></p>");
    EXPECT_EQ(norm(md), "![diagram](pic.png)");
}

TEST(HtmlToMarkdown, ImageRoundTripPreservesSize)
{
    // The renderer emits inline style sizes from the #WxH suffix — exact
    // width/height for SVG, max-width/max-height caps for raster — and the
    // import must restore the suffix.
    const QString rendered = MarkdownParser::toHtml(
        QStringLiteral("![a](a.png#150x120) ![b](b.svg#400x300)"));
    const QString md = convert(rendered);
    EXPECT_EQ(norm(md), "![a](a.png#150x120) ![b](b.svg#400x300)");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

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

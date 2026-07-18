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
    EXPECT_TRUE(html.contains("data-line=\"1\""));
    EXPECT_TRUE(html.contains("int x = 1;"));
}

TEST(MarkdownParserTest, FencedCodeBlockNoLang) {
    QString html = MarkdownParser::toHtml("```\nplain code\n```");
    EXPECT_TRUE(html.contains("<pre"));
    EXPECT_TRUE(html.contains("language-"));
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
    EXPECT_TRUE(html.contains("width=\"400\""));
    EXPECT_TRUE(html.contains("height=\"200\""));
    EXPECT_TRUE(html.contains("alt=\"alt\""));
}

TEST(MarkdownParserTest, ImageWithWidthOnly) {
    QString html = MarkdownParser::toHtml("![alt](img.png#400x)");
    EXPECT_TRUE(html.contains("width=\"400\""));
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_FALSE(html.contains("height="));
}

TEST(MarkdownParserTest, ImageWithHeightOnly) {
    QString html = MarkdownParser::toHtml("![alt](img.png#x200)");
    EXPECT_TRUE(html.contains("height=\"200\""));
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_FALSE(html.contains("width="));
}

TEST(MarkdownParserTest, ImageWithoutDimensions) {
    QString html = MarkdownParser::toHtml("![alt](img.png)");
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_FALSE(html.contains("width="));
    EXPECT_FALSE(html.contains("height="));
}

TEST(MarkdownParserTest, ImageWithDimensionsAndTitle) {
    QString html = MarkdownParser::toHtml("![alt](img.png#400x200 \"title\")");
    EXPECT_TRUE(html.contains("src=\"img.png\""));
    EXPECT_TRUE(html.contains("width=\"400\""));
    EXPECT_TRUE(html.contains("height=\"200\""));
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

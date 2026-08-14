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
#include "preview/MdRenderer.h"
#include <md4c.h>

TEST(MdRendererEscapeTest, HtmlEntitiesInCode) {
    MdRenderer renderer;
    QString input = "```\n<div>test</div>\n```";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("&lt;div&gt;test&lt;/div&gt;"));
    EXPECT_FALSE(html.contains("<div>"));
}

TEST(MdRendererAlignmentTest, DefaultTableAlignment) {
    QString md = "| A |\n|---|\n| 1 |";
    QByteArray utf8 = md.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    MdRenderer renderer;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("<th>"));
    EXPECT_TRUE(html.contains("<td>"));
}

TEST(MdRendererListTest, OrderedListStartPreserved) {
    QString md = "3. three\n4. four";
    QByteArray utf8 = md.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    MdRenderer renderer;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("<ol start=\"3\">"))
        << "list starting at 3 must emit start=\"3\"; got: " << html.toStdString();
    EXPECT_TRUE(html.contains("three"));
    EXPECT_TRUE(html.contains("four"));
}

TEST(MdRendererListTest, OrderedListDefaultStartOmitsAttribute) {
    QString md = "1. one\n2. two";
    QByteArray utf8 = md.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    MdRenderer renderer;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("<ol>")) << "start=1 must not emit start attr";
    EXPECT_FALSE(html.contains("start="));
}

TEST(MdRendererListTest, OrderedListStyleClassEmitted) {
    const struct { MdRenderer::OrderedListStyle style; const char *cls; } cases[] = {
        { MdRenderer::OrderedListStyle::Decimal, "" },
        { MdRenderer::OrderedListStyle::DecimalParen, "md-list-decimal-paren" },
        { MdRenderer::OrderedListStyle::LowerAlpha, "md-list-alpha" },
        { MdRenderer::OrderedListStyle::LowerAlphaParen, "md-list-alpha-paren" },
        { MdRenderer::OrderedListStyle::LowerRoman, "md-list-roman" },
        { MdRenderer::OrderedListStyle::LowerRomanParen, "md-list-roman-paren" },
    };
    for (const auto &c : cases) {
        QString md = "1. one\n2. two";
        QByteArray utf8 = md.toUtf8();
        unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
        MdRenderer renderer;
        renderer.setOrderedListStyle(c.style);
        QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

        if (c.cls[0])
            EXPECT_TRUE(html.contains(QString("<ol class=\"%1\">").arg(c.cls)))
                << "expected class " << c.cls << "; got: " << html.toStdString();
        else
            EXPECT_TRUE(html.contains("<ol>"))
                << "decimal style must emit a bare <ol>; got: " << html.toStdString();
    }
}

TEST(MdRendererListTest, OrderedListStartAndStyleCombine) {
    QString md = "3. three\n4. four";
    QByteArray utf8 = md.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    MdRenderer renderer;
    renderer.setOrderedListStyle(MdRenderer::OrderedListStyle::LowerAlphaParen);
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("<ol start=\"3\" class=\"md-list-alpha-paren\">"))
        << "start and style class must coexist; got: " << html.toStdString();
}

TEST(MdRendererEscapeTest, AmpersandInCodeSpan) {
    MdRenderer renderer;
    QString input = "`a & b < c`";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("a &amp; b &lt; c"));
    EXPECT_FALSE(html.contains("a & b < c"));
}

TEST(MdRendererEscapeTest, QuotesInCodeSpan) {
    MdRenderer renderer;
    QString input = R"(`say "hello"`)";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("&quot;"));
    EXPECT_FALSE(html.contains(R"(say "hello")"));
}

TEST(MdRendererImageTest, ImageDimensionHxW) {
    MdRenderer renderer;
    QString input = R"(![resized](img.png#200x150))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("max-width: 200px"));
    EXPECT_TRUE(html.contains("max-height: 150px"));
    EXPECT_TRUE(html.contains("img.png"));
}

TEST(MdRendererImageTest, ImageDimensionHeightOnly) {
    MdRenderer renderer;
    QString input = R"(![hlimit](img.png#x200))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("max-height: 200px"));
    EXPECT_FALSE(html.contains("max-width"));
    EXPECT_TRUE(html.contains("img.png"));
}

TEST(MdRendererImageTest, ImageDimensionWidthOnly) {
    MdRenderer renderer;
    QString input = R"(![wlimit](img.png#300x))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("max-width: 300px"));
    EXPECT_FALSE(html.contains("max-height"));
    EXPECT_TRUE(html.contains("img.png"));
}

TEST(MdRendererImageTest, SvgDimensionWidthUpscales) {
    MdRenderer renderer;
    QString input = R"(![upscaled](img.svg#400x))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("width: 400px"));
    EXPECT_FALSE(html.contains("max-width"));
    EXPECT_TRUE(html.contains("img.svg"));
}

TEST(MdRendererImageTest, SvgDimensionHeightOnly) {
    MdRenderer renderer;
    QString input = R"(![hsvg](img.svg#x100))";
    QByteArray utf8 = input.toUtf8();
    unsigned int flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("height: 100px"));
    EXPECT_FALSE(html.contains("max-height"));
    EXPECT_FALSE(html.contains("width"));
    EXPECT_TRUE(html.contains("img.svg"));
}

TEST(MdRendererImageTest, SvgDimensionBoth) {
    MdRenderer renderer;
    QString input = R"(![both](img.svg#400x100))";
    QByteArray utf8 = input.toUtf8();
    unsigned int flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("width: 400px"));
    EXPECT_TRUE(html.contains("height: 100px"));
    EXPECT_FALSE(html.contains("max-"));
    EXPECT_TRUE(html.contains("img.svg"));
}

TEST(MdRendererImageTest, SvgNoDimension) {
    MdRenderer renderer;
    QString input = R"(![plain](img.svg))";
    QByteArray utf8 = input.toUtf8();
    unsigned int flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_FALSE(html.contains("style=\""));
    EXPECT_FALSE(html.contains("width:"));
    EXPECT_FALSE(html.contains("height:"));
    EXPECT_TRUE(html.contains("img.svg"));
}

TEST(MdRendererImageTest, ImageNoDimension) {
    MdRenderer renderer;
    QString input = R"(![plain](img.png))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_FALSE(html.contains("style=\""));
    EXPECT_TRUE(html.contains("img.png"));
}

TEST(MdRendererImageTest, ImageEscaping) {
    MdRenderer renderer;
    QString input = R"(!["alt" & quote](img.png "title"))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("<img"));
    EXPECT_TRUE(html.contains("img.png"));
    EXPECT_TRUE(html.contains("&amp;"));
    EXPECT_TRUE(html.contains("&quot;"));
}

TEST(MdRendererLinkTest, LinkHrefEscaping) {
    MdRenderer renderer;
    QString input = R"([click](https://example.com?a=1&b=2))";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("https://example.com?a=1&amp;b=2"));
}

TEST(MdRendererBlockquoteTest, NestedBlockquote) {
    MdRenderer renderer;
    QString input = "> quoted text";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("<blockquote>"));
    EXPECT_TRUE(html.contains("quoted text"));
    EXPECT_TRUE(html.contains("</blockquote>"));
}

TEST(MdRendererTaskListTest, CheckedAndUnchecked) {
    MdRenderer renderer;
    QString input = "- [x] done\n- [ ] pending";
    QByteArray utf8 = input.toUtf8();
    unsigned long flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
    QString html = renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), flags);

    EXPECT_TRUE(html.contains("task-list-item"));
    EXPECT_TRUE(html.contains("task-list-item-checkbox"));
    EXPECT_TRUE(html.contains("checked"));
    EXPECT_TRUE(html.contains("done"));
    EXPECT_TRUE(html.contains("pending"));
}

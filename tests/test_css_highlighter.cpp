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

#include "CssHighlighter.h"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

namespace {

const char *kSampleCss = R"css(/* a comment */
body {
    color: #ff0000;
    font-size: 16px;
    background: url("img.png") !important;
}
@media screen {
    h1 { color: blue; }
}
)css";

class CssHighlighterTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        delete m_highlighter;
        delete m_doc;
    }

    void highlight(bool dark, const QString &text)
    {
        delete m_highlighter;
        m_highlighter = new CssHighlighter(dark, m_doc);
        m_doc->setPlainText(text);
        m_highlighter->rehighlight();
        m_doc->markContentsDirty(0, m_doc->characterCount());
        m_doc->documentLayout()->documentSize();
    }

    QTextLayout::FormatRange rangeAt(int block, int pos) const
    {
        const auto formats = m_doc->findBlockByNumber(block).layout()->formats();
        for (const auto &f : formats) {
            if (pos >= f.start && pos < f.start + f.length
                && f.format.foreground().style() != Qt::NoBrush) {
                return f;
            }
        }
        return {};
    }

    QColor fgAt(int block, int pos) const
    {
        return rangeAt(block, pos).format.foreground().color();
    }

    bool italicAt(int block, int pos) const
    {
        return rangeAt(block, pos).format.fontItalic();
    }

    bool boldAt(int block, int pos) const
    {
        return rangeAt(block, pos).format.fontWeight() >= QFont::Bold;
    }

    int idxOf(int block, const QString &needle) const
    {
        return m_doc->findBlockByNumber(block).text().indexOf(needle);
    }

    QTextDocument *m_doc = new QTextDocument;
    CssHighlighter *m_highlighter = nullptr;
};

TEST_F(CssHighlighterTest, CommentItalicInBothModes)
{
    for (bool dark : {false, true}) {
        highlight(dark, kSampleCss);
        int pos = idxOf(0, "a comment");
        EXPECT_EQ(fgAt(0, pos), CssHighlighter::paletteFor(dark).comment);
        EXPECT_TRUE(italicAt(0, pos));
    }
}

TEST_F(CssHighlighterTest, StringFormatted)
{
    highlight(false, kSampleCss);
    int pos = idxOf(4, "\"img.png\"");
    EXPECT_EQ(fgAt(4, pos), CssHighlighter::paletteFor(false).string);
}

TEST_F(CssHighlighterTest, ImportantBoldKeyword)
{
    for (bool dark : {false, true}) {
        highlight(dark, kSampleCss);
        int pos = idxOf(4, "!important");
        EXPECT_EQ(fgAt(4, pos), CssHighlighter::paletteFor(dark).keyword);
        EXPECT_TRUE(boldAt(4, pos));
    }
}

TEST_F(CssHighlighterTest, AtRuleKeywordColor)
{
    for (bool dark : {false, true}) {
        highlight(dark, kSampleCss);
        EXPECT_EQ(fgAt(6, 0), CssHighlighter::paletteFor(dark).keyword);
    }
}

TEST_F(CssHighlighterTest, PropertyColor)
{
    for (bool dark : {false, true}) {
        highlight(dark, kSampleCss);
        int pos = idxOf(2, "color");
        EXPECT_EQ(fgAt(2, pos), CssHighlighter::paletteFor(dark).property);
    }
}

TEST_F(CssHighlighterTest, HexColorFormatted)
{
    highlight(true, kSampleCss);
    int pos = idxOf(2, "#ff0000");
    EXPECT_EQ(fgAt(2, pos), CssHighlighter::paletteFor(true).hexColor);
}

TEST_F(CssHighlighterTest, NumberFormatted)
{
    highlight(false, kSampleCss);
    int pos = idxOf(3, "16px");
    EXPECT_EQ(fgAt(3, pos), CssHighlighter::paletteFor(false).number);
}

TEST_F(CssHighlighterTest, SelectorBold)
{
    for (bool dark : {false, true}) {
        highlight(dark, kSampleCss);
        int pos = idxOf(7, "h1");
        EXPECT_EQ(fgAt(7, pos), CssHighlighter::paletteFor(dark).selector);
        EXPECT_TRUE(boldAt(7, pos));
    }
}

TEST_F(CssHighlighterTest, MultiLineCommentSpansBlocks)
{
    for (bool dark : {false, true}) {
        highlight(dark, "/* start\n   still comment */\nbody {}");
        EXPECT_TRUE(italicAt(0, 3));
        EXPECT_TRUE(italicAt(1, 2));
        int sel = idxOf(2, "body");
        EXPECT_EQ(fgAt(2, sel), CssHighlighter::paletteFor(dark).selector);
    }
}

TEST(CssHighlighterPaletteTest, BackgroundMatchesDarkOrLight)
{
    EXPECT_LT(CssHighlighter::paletteFor(true).background.lightness(), 128);
    EXPECT_GT(CssHighlighter::paletteFor(false).background.lightness(), 128);
}

TEST(CssHighlighterPaletteTest, RolesContrastWithBackground)
{
    for (bool dark : {false, true}) {
        auto p = CssHighlighter::paletteFor(dark);
        EXPECT_GT(qAbs(p.foreground.lightness() - p.background.lightness()), 80);
        for (QColor role : {p.comment, p.string, p.keyword, p.property,
                            p.hexColor, p.number, p.selector}) {
            EXPECT_GT(qAbs(role.lightness() - p.background.lightness()), 60)
                << (dark ? "dark" : "light") << " role lightness " << role.lightness();
        }
    }
}

TEST(CssHighlighterPaletteTest, DarkAndLightDiffer)
{
    auto dark = CssHighlighter::paletteFor(true);
    auto light = CssHighlighter::paletteFor(false);
    EXPECT_NE(dark.background.name(), light.background.name());
    EXPECT_NE(dark.comment.name(), light.comment.name());
    EXPECT_NE(dark.string.name(), light.string.name());
    EXPECT_NE(dark.keyword.name(), light.keyword.name());
}

} // namespace

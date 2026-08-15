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
#include <QPair>
#include <QStringList>
#include "FrontMatterParser.h"

TEST(FrontMatterParserTest, LineRangeValidBlock) {
    const QPair<int, int> r = FrontMatterParser::lineRange(
        QStringLiteral("---\ntitle: x\n---\n# H1"));
    EXPECT_EQ(r.first, 0);
    EXPECT_EQ(r.second, 2);
}

TEST(FrontMatterParserTest, LineRangeAbsent) {
    EXPECT_EQ(FrontMatterParser::lineRange(QStringLiteral("# H1\n\ntext")),
              (QPair<int, int>(-1, -1)));
    EXPECT_EQ(FrontMatterParser::lineRange(QString()), (QPair<int, int>(-1, -1)));
}

TEST(FrontMatterParserTest, LineRangeUnterminatedDashIsNotFrontmatter) {
    // A lone leading `---` is an HR, not an opened frontmatter block.
    EXPECT_EQ(FrontMatterParser::lineRange(QStringLiteral("---\ntitle: x")),
              (QPair<int, int>(-1, -1)));
}

TEST(FrontMatterParserTest, LineRangeNotAtStart) {
    EXPECT_EQ(FrontMatterParser::lineRange(QStringLiteral("text\n---\ntitle\n---")),
              (QPair<int, int>(-1, -1)));
}

TEST(FrontMatterParserTest, LineRangeEmptyBlock) {
    // `---\n---` is an empty frontmatter block, not two HRs.
    EXPECT_EQ(FrontMatterParser::lineRange(QStringLiteral("---\n---")),
              (QPair<int, int>(0, 1)));
}

TEST(FrontMatterParserTest, LineRangeSkipsLeadingBlankLines) {
    EXPECT_EQ(FrontMatterParser::lineRange(
                  QStringLiteral("\n\n---\na: b\n---")),
              (QPair<int, int>(2, 4)));
}

TEST(FrontMatterParserTest, ValuePlain) {
    EXPECT_EQ(FrontMatterParser::value(
                  QStringLiteral("---\ntoc-description: A short description\n---"), "toc-description"),
              QStringLiteral("A short description"));
}

TEST(FrontMatterParserTest, ValueDoubleQuoted) {
    EXPECT_EQ(FrontMatterParser::value(
                  QStringLiteral("---\ntoc-description: \"A short description\"\n---"), "toc-description"),
              QStringLiteral("A short description"));
}

TEST(FrontMatterParserTest, ValueSingleQuoted) {
    EXPECT_EQ(FrontMatterParser::value(
                  QStringLiteral("---\ntoc-description: 'A short description'\n---"), "toc-description"),
              QStringLiteral("A short description"));
}

TEST(FrontMatterParserTest, ValueMissingKey) {
    EXPECT_EQ(FrontMatterParser::value(
                  QStringLiteral("---\ntitle: x\n---"), "toc-description"), QString());
}

TEST(FrontMatterParserTest, ValueNoFrontmatter) {
    EXPECT_EQ(FrontMatterParser::value(QStringLiteral("# H1"), "toc-description"), QString());
}

TEST(FrontMatterParserTest, ValueLeadingWhitespaceKey) {
    EXPECT_EQ(FrontMatterParser::value(
                  QStringLiteral("---\n  toc-description: indented\n---"), "toc-description"),
              QStringLiteral("indented"));
}

TEST(FrontMatterParserTest, ValueQuotedNotMatchingPairKeepsQuotes) {
    EXPECT_EQ(FrontMatterParser::value(
                  QStringLiteral("---\ntoc-description: \"half\n---"), "toc-description"),
              QStringLiteral("\"half"));
}

TEST(FrontMatterParserTest, BlankOutPreservesLineCount) {
    const QString md = QStringLiteral("---\ntitle: x\n---\n# H1\n\ntext");
    const QString blanked = FrontMatterParser::blankOut(md);
    EXPECT_EQ(blanked.split(QLatin1Char('\n')).size(), md.split(QLatin1Char('\n')).size());
    EXPECT_FALSE(blanked.contains(QLatin1String("title:")));
    EXPECT_FALSE(blanked.contains(QLatin1String("---")));
    EXPECT_TRUE(blanked.contains(QLatin1String("# H1")));
    EXPECT_TRUE(blanked.contains(QLatin1String("text")));
}

TEST(FrontMatterParserTest, BlankOutTrailingNewlinePreserved) {
    const QString md = QStringLiteral("---\na: b\n---\n\nbody\n");
    const QString blanked = FrontMatterParser::blankOut(md);
    EXPECT_TRUE(blanked.endsWith(QLatin1Char('\n')));
    EXPECT_EQ(blanked.split(QLatin1Char('\n')).size(), md.split(QLatin1Char('\n')).size());
    EXPECT_TRUE(blanked.contains(QLatin1String("body")));
}

TEST(FrontMatterParserTest, BlankOutNoFrontmatterReturnsSame) {
    const QString md = QStringLiteral("# H1\n\ntext");
    EXPECT_EQ(FrontMatterParser::blankOut(md), md);
}

TEST(FrontMatterParserTest, StripRemovesBlock) {
    EXPECT_EQ(FrontMatterParser::strip(QStringLiteral("---\ntitle: x\n---\n# H1")),
              QStringLiteral("# H1"));
}

TEST(FrontMatterParserTest, StripNoFrontmatterReturnsSame) {
    const QString md = QStringLiteral("# H1\n");
    EXPECT_EQ(FrontMatterParser::strip(md), md);
}

TEST(FrontMatterParserTest, TocInfoDescriptionExtracted) {
    const FrontMatterParser::TocInfo info = FrontMatterParser::tocInfo(
        QStringLiteral("---\ntoc-description: A short description\n---\n# Alpha"));
    EXPECT_EQ(info.description, QStringLiteral("A short description"));
}

TEST(FrontMatterParserTest, TocInfoNoFrontmatterEmpty) {
    EXPECT_TRUE(FrontMatterParser::tocInfo(QStringLiteral("# Alpha")).description.isEmpty());
}

TEST(FrontMatterParserTest, HasFrontMatter) {
    EXPECT_TRUE(FrontMatterParser::hasFrontMatter(QStringLiteral("---\n---")));
    EXPECT_FALSE(FrontMatterParser::hasFrontMatter(QStringLiteral("---\ntitle: x")));
    EXPECT_FALSE(FrontMatterParser::hasFrontMatter(QStringLiteral("# H1")));
}
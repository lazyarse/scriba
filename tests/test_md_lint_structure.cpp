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
// Parity cases adapted from tests/test_markdown_checker.cpp
#include <gtest/gtest.h>

#include "validation/MdLintConfig.h"
#include "validation/MdLintEngine.h"

#include <algorithm>

namespace {

QVector<MdLintIssue> lint(const QString &text, const MdLintConfig &cfg = MdLintConfig::defaults())
{
    return MdLintEngine::lint(text, cfg);
}

TEST(MdLintStructure, FlagSpacelessHashHeading)
{
    const auto issues = lint(QStringLiteral("#foo\n##bar\n"));
    const auto md018 = [&] {
        QVector<MdLintIssue> out;
        for (const auto &i : issues)
            if (i.rule == QLatin1String("MD018"))
                out << i;
        return out;
    }();
    ASSERT_EQ(2, md018.size());
    for (const auto &i : md018)
        EXPECT_EQ(1, i.col) << "span starts at the '#': " << i.detail.toStdString();
}

TEST(MdLintStructure, DoesNotFlagValidAtxHeading)
{
    EXPECT_TRUE(lint(QStringLiteral("## Foo\n")).isEmpty());
}

TEST(MdLintStructure, FlagsHeadingLevelSkip)
{
    const auto issues = lint(QStringLiteral("# Level one\n### Level three\n"));
    bool found = false;
    for (const auto &i : issues)
        if (i.rule == QLatin1String("MD001") && i.line == 2)
            found = true;
    EXPECT_TRUE(found);
}

TEST(MdLintStructure, FlagsDuplicateHeadings)
{
    const auto issues = lint(QStringLiteral("# Title\n## Title\n"));
    EXPECT_EQ(1, std::count_if(issues.begin(), issues.end(),
                               [](const MdLintIssue &i) { return i.rule == QLatin1String("MD024"); }));
}

TEST(MdLintStructure, SetextHeadingIsNotASkip)
{
    EXPECT_TRUE(lint(QStringLiteral("# Title\nSubheading\n-------\n")).isEmpty());
}

TEST(MdLintStructure, FlagsTrailingWhitespace)
{
    const auto issues = lint(QStringLiteral("text  \nmore\t\n"));
    int n = 0;
    for (const auto &i : issues)
        if (i.rule == QLatin1String("MD009"))
            ++n;
    EXPECT_EQ(2, n);
}

TEST(MdLintStructure, FlagsConsecutiveBlankLines)
{
    const auto issues = lint(QStringLiteral("one\n\n\n\ntwo\n"));
    EXPECT_EQ(1, std::count_if(issues.begin(), issues.end(),
                               [](const MdLintIssue &i) { return i.rule == QLatin1String("MD012"); }));
}

TEST(MdLintStructure, FlagsOverlongLines)
{
    const QString line = QString(150, QLatin1Char('a'));
    const auto issues = lint(line + QLatin1Char('\n'));
    EXPECT_EQ(1, std::count_if(issues.begin(), issues.end(),
                               [](const MdLintIssue &i) { return i.rule == QLatin1String("MD013"); }));
}

TEST(MdLintStructure, FlagsUnmatchedFootnoteReference)
{
    const auto issues = lint(
        QStringLiteral("Text[^1] and [^note].\n\n[^note]: the definition\n"));
    bool flagged1 = false, flaggedNote = false;
    for (const auto &i : issues) {
        if (i.rule == QLatin1String("MD900") && i.detail.contains(QLatin1String("[^1]")))
            flagged1 = true;
        if (i.rule == QLatin1String("MD900") && i.detail.contains(QLatin1String("[^note]")))
            flaggedNote = true;
    }
    EXPECT_TRUE(flagged1);
    EXPECT_FALSE(flaggedNote);
}

TEST(MdLintStructure, SkipsFootnoteLikeTextInInlineCode)
{
    const auto issues = lint(QStringLiteral("Use `[^1]` inline and a real one[^2].\n"));
    bool flagged1 = false, flagged2 = false;
    for (const auto &i : issues) {
        if (i.rule == QLatin1String("MD900") && i.detail.contains(QLatin1String("[^1]")))
            flagged1 = true;
        if (i.rule == QLatin1String("MD900") && i.detail.contains(QLatin1String("[^2]")))
            flagged2 = true;
    }
    EXPECT_FALSE(flagged1);
    EXPECT_TRUE(flagged2);
}

TEST(MdLintStructure, SkipsFencedCodeAndFrontMatter)
{
    const auto issues = lint(QStringLiteral(
        "# Heading\n"
        "```\n"
        "# code\n"
        "#nospace\n"
        "```\n"
        "# Heading\n"
        "\n\n\n"));
    const int dupes = std::count_if(issues.begin(), issues.end(),
                                    [](const MdLintIssue &i) { return i.rule == QLatin1String("MD024"); });
    const int blanks = std::count_if(issues.begin(), issues.end(),
                                     [](const MdLintIssue &i) { return i.rule == QLatin1String("MD012"); });
    const int md018 = std::count_if(issues.begin(), issues.end(),
                                    [](const MdLintIssue &i) { return i.rule == QLatin1String("MD018"); });
    EXPECT_EQ(1, dupes);
    EXPECT_EQ(1, blanks);
    EXPECT_EQ(0, md018);
}

TEST(MdLintStructure, EmptyConfigDisablesEverything)
{
    const auto issues = lint(QStringLiteral("#oops\n\n\n\n#### deep\n## deep\n"), MdLintConfig());
    EXPECT_TRUE(issues.isEmpty());
}

} // namespace

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

MdLintConfig withRule(const char *id)
{
    return MdLintConfig::fromJson(QStringLiteral("{\"%1\": true}").arg(QLatin1String(id)));
}

MdLintConfig withRuleAndParams(const char *id, const QString &paramsJson)
{
    return MdLintConfig::fromJson(
        QStringLiteral("{\"%1\": {\"enabled\": true, \"params\": %2}}")
            .arg(QLatin1String(id), paramsJson));
}

int countRule(const QVector<MdLintIssue> &issues, const char *id)
{
    const QString want = QLatin1String(id);
    return std::count_if(issues.begin(), issues.end(),
                         [&want](const MdLintIssue &i) { return i.rule == want; });
}

// ---- MD003 ----------------------------------------------------------------

TEST(MdLintStructure, Md003AtxStyle)
{
    EXPECT_EQ(0, countRule(lint(QStringLiteral("# A\n## B\n"), withRule("MD003")), "MD003"));
    const auto issues = lint(QStringLiteral("# A\nB\n===\n"), withRule("MD003"));
    ASSERT_EQ(1, countRule(issues, "MD003"));
    EXPECT_EQ(2, issues.at(0).line);
}

TEST(MdLintStructure, Md003ConsistentStyle)
{
    const auto issues = lint(QStringLiteral("# A\nA\n---\n"), withRule("MD003"));
    ASSERT_EQ(1, countRule(issues, "MD003"));
    EXPECT_EQ(2, issues.at(0).line);
}

// ---- MD004 ----------------------------------------------------------------

TEST(MdLintStructure, Md004ConsistentStyle)
{
    EXPECT_EQ(1, countRule(lint(QStringLiteral("- a\n* b\n"), withRule("MD004")), "MD004"));
    EXPECT_EQ(0, countRule(lint(QStringLiteral("* a\n* b\n"), withRule("MD004")), "MD004"));
}

TEST(MdLintStructure, Md004SublistUsesDifferentMarker)
{
    const auto issues = lint(QStringLiteral("* a\n  - b\n"),
                             withRuleAndParams("MD004", QStringLiteral(R"({"style": "sublist"})")));
    ASSERT_EQ(1, countRule(issues, "MD004"));
    EXPECT_EQ(2, issues.at(0).line);
}

// ---- MD005 ----------------------------------------------------------------

TEST(MdLintStructure, Md005ConsistentIndentPerLevel)
{
    EXPECT_EQ(0, countRule(lint(QStringLiteral("- a\n  - b\n- c\n"), withRule("MD005")), "MD005"));
    const auto issues = lint(QStringLiteral("- a\n  - b\n   - c\n"), withRule("MD005"));
    ASSERT_EQ(1, countRule(issues, "MD005"));
    EXPECT_EQ(3, issues.at(0).line);
}

// ---- MD007 ----------------------------------------------------------------

TEST(MdLintStructure, Md007IndentNestedUl)
{
    EXPECT_EQ(0, countRule(lint(QStringLiteral("- a\n  - b\n"), withRule("MD007")), "MD007"));
    const auto issues = lint(QStringLiteral("- a\n    - b\n"), withRule("MD007"));
    ASSERT_EQ(1, countRule(issues, "MD007"));
    EXPECT_EQ(2, issues.at(0).line);
}

// ---- MD024 nesting variants ----------------------------------------------

TEST(MdLintStructure, Md024SiblingsOnlyIgnoresNested)
{
    const auto issues = lint(QStringLiteral("# A\n\n> # A\n"),
                             withRuleAndParams("MD024", QStringLiteral(R"({"siblings_only": true})")));
    EXPECT_EQ(0, countRule(issues, "MD024"));
    EXPECT_EQ(1, countRule(lint(QStringLiteral("# A\n\n> # A\n"), withRule("MD024")), "MD024"));
}

// ---- MD040 ----------------------------------------------------------------

TEST(MdLintStructure, Md040FenceNeedsLanguage)
{
    EXPECT_EQ(1, countRule(lint(QStringLiteral("```\ncode\n```\n"), withRule("MD040")), "MD040"));
    EXPECT_EQ(0, countRule(lint(QStringLiteral("```cpp\ncode\n```\n"), withRule("MD040")), "MD040"));
}

// ---- MD041 ----------------------------------------------------------------

TEST(MdLintStructure, Md041FirstLineHeading)
{
    EXPECT_EQ(1, countRule(lint(QStringLiteral("text\n"), withRule("MD041")), "MD041"));
    EXPECT_EQ(0, countRule(lint(QStringLiteral("# A\n\ntext\n"), withRule("MD041")), "MD041"));
}

// ---- MD043 ----------------------------------------------------------------

TEST(MdLintStructure, Md043RequiredHeadingStructure)
{
    const auto issues = lint(QStringLiteral("# A\n## B\n"),
                             withRuleAndParams("MD043",
                                               QStringLiteral(R"({"headings": ["# A", "## C"]})")));
    EXPECT_EQ(1, countRule(issues, "MD043"));
}

// ---- MD046 ----------------------------------------------------------------

TEST(MdLintStructure, Md046CodeBlockStyle)
{
    const auto issues = lint(QStringLiteral("    indented\n\n```\nfenced\n```\n"),
                             withRule("MD046"));
    EXPECT_EQ(1, countRule(issues, "MD046"));
    const auto fenced = lint(QStringLiteral("    indented\n\n```\nfenced\n```\n"),
                             withRuleAndParams("MD046", QStringLiteral(R"({"style": "fenced"})")));
    ASSERT_EQ(1, countRule(fenced, "MD046"));
    EXPECT_EQ(1, fenced.at(0).line);
}

// ---- MD048 ----------------------------------------------------------------

TEST(MdLintStructure, Md048FenceStyle)
{
    const auto consistent = lint(QStringLiteral("```\n```\n\n~~~\n~~~\n"), withRule("MD048"));
    ASSERT_EQ(1, countRule(consistent, "MD048"));
    EXPECT_EQ(4, consistent.at(0).line);
    const auto backtick = lint(QStringLiteral("```\n```\n\n~~~\n~~~\n"),
                               withRuleAndParams("MD048", QStringLiteral(R"({"style": "backtick"})")));
    ASSERT_EQ(1, countRule(backtick, "MD048"));
    EXPECT_EQ(4, backtick.at(0).line);
}

// ---- MD058 ----------------------------------------------------------------

TEST(MdLintStructure, Md058BlanksAroundTables)
{
    const auto issues = lint(QStringLiteral("| a |\n|---|\n| b |\ntext\n"), withRule("MD058"));
    ASSERT_EQ(1, countRule(issues, "MD058"));
    EXPECT_EQ(3, issues.at(0).line);
    EXPECT_EQ(0, countRule(lint(QStringLiteral("text\n\n| a |\n|---|\n"), withRule("MD058")),
                           "MD058"));
}

} // namespace

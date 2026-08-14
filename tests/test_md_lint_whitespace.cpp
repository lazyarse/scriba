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

#include "validation/MdLintConfig.h"
#include "validation/MdLintEngine.h"

#include <algorithm>

namespace {

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

// ---- MD010 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md010FlagsHardTabs)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("a\tb\n"), withRule("MD010"));
    ASSERT_EQ(1, countRule(issues, "MD010"));
    EXPECT_EQ(2, issues.at(0).col);
}

TEST(MdLintWhitespace, Md010SkipsCodeByDefault)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("```\n\ta\n```\n"), withRule("MD010"));
    EXPECT_EQ(0, countRule(issues, "MD010"));
}

// ---- MD011 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md011AllowsNormalLinks)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("[text](url)\n"), withRule("MD011"));
    EXPECT_EQ(0, countRule(issues, "MD011"));
}

TEST(MdLintWhitespace, Md011FlagsReversedLinks)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("(text)[url]\n"), withRule("MD011"));
    ASSERT_EQ(1, countRule(issues, "MD011"));
    EXPECT_EQ(1, issues.at(0).col);
}

TEST(MdLintWhitespace, Md011SkipsInlineCode)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("`(text)[url]`\n"), withRule("MD011"));
    EXPECT_EQ(0, countRule(issues, "MD011"));
}

// ---- MD014 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md014FlagsDollarCommandsWithoutOutput)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("```\n$ echo hi\n$ echo again\n```\n"), withRule("MD014"));
    ASSERT_EQ(1, countRule(issues, "MD014"));
    EXPECT_EQ(2, issues.at(0).line);
}

TEST(MdLintWhitespace, Md014AllowsDollarCommandWithOutput)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("```\n$ echo hi\noutput\n```\n"), withRule("MD014"));
    EXPECT_EQ(0, countRule(issues, "MD014"));
}

// ---- MD019/020/021 --------------------------------------------------------

TEST(MdLintWhitespace, Md019FlagsMultipleSpacesAfterHash)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("##  Title\n"), withRule("MD019"));
    ASSERT_EQ(1, countRule(issues, "MD019"));
}

TEST(MdLintWhitespace, Md020FlagsClosedAtxWithoutSpaceBeforeClosingHashes)
{
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("## Title ##\n"), withRule("MD020")),
                           "MD020"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("## Title##\n"), withRule("MD020")),
                           "MD020"));
}

TEST(MdLintWhitespace, Md021FlagsMultipleSpacesBeforeClosingHashes)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("## Title  ##\n"), withRule("MD021"));
    EXPECT_EQ(1, countRule(issues, "MD021"));
}

// ---- MD022 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md022FlagsAdjacentHeadings)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("# A\n## B\n"), withRule("MD022"));
    // Both headings lack a separating blank line: the first has no blank
    // below, the second none above.
    ASSERT_EQ(2, countRule(issues, "MD022"));
    EXPECT_EQ(1, issues.at(0).line);
    EXPECT_EQ(2, issues.at(1).line);
}

TEST(MdLintWhitespace, Md022AllowsBlankLineAfterHeading)
{
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("# A\n\ntext\n"), withRule("MD022")),
                           "MD022"));
}

// ---- MD023 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md023FlagsIndentedHeading)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("  # A\n"), withRule("MD023"));
    ASSERT_EQ(1, countRule(issues, "MD023"));
    EXPECT_EQ(1, issues.at(0).col);
    EXPECT_EQ(2, issues.at(0).length);
}

// ---- MD025 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md025FlagsMultipleTitles)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("# A\n# B\n"), withRule("MD025"));
    ASSERT_EQ(1, countRule(issues, "MD025"));
    EXPECT_EQ(2, issues.at(0).line);
}

// ---- MD026 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md026FlagsTrailingPunctuation)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("# Title!\n"), withRule("MD026"));
    ASSERT_EQ(1, countRule(issues, "MD026"));
    EXPECT_EQ(8, issues.at(0).col);
}

TEST(MdLintWhitespace, Md026AllowsPlainHeading)
{
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("# Title\n"), withRule("MD026")),
                           "MD026"));
}

// ---- MD027 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md027FlagsMultipleSpacesAfterBlockquoteMarker)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral(">  x\n"), withRule("MD027")),
                           "MD027"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("> x\n"), withRule("MD027")),
                           "MD027"));
}

TEST(MdLintWhitespace, Md027SkipsCode)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral(">  ```\n>  y\n>  ```\n"), withRule("MD027"));
    EXPECT_EQ(0, countRule(issues, "MD027"));
}

// ---- MD028 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md028FlagsBlankLineInsideBlockquote)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("> a\n\n> b\n"), withRule("MD028"));
    ASSERT_EQ(1, countRule(issues, "MD028"));
    EXPECT_EQ(2, issues.at(0).line);
}

TEST(MdLintWhitespace, Md028AllowsBlankLineAfterBlockquote)
{
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("> a\n\nplain\n"), withRule("MD028")),
                           "MD028"));
}

// ---- MD029 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md029StyleOneRequiresAllOnes)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("1. a\n2. b\n"),
        withRuleAndParams("MD029", QStringLiteral(R"({"style": "one"})")));
    ASSERT_EQ(1, countRule(issues, "MD029"));
    EXPECT_EQ(2, issues.at(0).line);
}

TEST(MdLintWhitespace, Md029StyleOrderedAllowsSequence)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("1. a\n2. b\n"),
        withRuleAndParams("MD029", QStringLiteral(R"({"style": "ordered"})")));
    EXPECT_EQ(0, countRule(issues, "MD029"));
}

// ---- MD030 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md030FlagsExtraSpacesAfterMarker)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("-  a\n"), withRule("MD030")),
                           "MD030"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("- a\n"), withRule("MD030")),
                           "MD030"));
}

// ---- MD031 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md031FlagsFenceWithoutBlankBelow)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("```\ncode\n```\ntext\n"), withRule("MD031"));
    ASSERT_EQ(1, countRule(issues, "MD031"));
    EXPECT_EQ(3, issues.at(0).line);
}

TEST(MdLintWhitespace, Md031AllowsBlankLinesAroundFence)
{
    EXPECT_EQ(0, countRule(
        MdLintEngine::lint(QStringLiteral("text\n\n```\ncode\n```\n"), withRule("MD031")),
        "MD031"));
}

// ---- MD032 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md032FlagsListWithoutBlankBelow)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("- a\n- b\ntext\n"), withRule("MD032"));
    ASSERT_EQ(1, countRule(issues, "MD032"));
    EXPECT_EQ(2, issues.at(0).line);
}

TEST(MdLintWhitespace, Md032AllowsBlankLineBeforeList)
{
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("text\n\n- a\n"), withRule("MD032")),
                           "MD032"));
}

// ---- MD035 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md035ConsistentStyleUsesFirstHr)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("---\n***\n"), withRule("MD035"));
    ASSERT_EQ(1, countRule(issues, "MD035"));
    EXPECT_EQ(2, issues.at(0).line);
}

TEST(MdLintWhitespace, Md035ExplicitStyle)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("***\n***\n"),
        withRuleAndParams("MD035", QStringLiteral(R"({"style": "***"})")));
    EXPECT_EQ(0, countRule(issues, "MD035"));
}

// ---- MD047 ----------------------------------------------------------------

TEST(MdLintWhitespace, Md047RequiresSingleTrailingNewline)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("# A"), withRule("MD047")), "MD047"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("# A\n"), withRule("MD047")), "MD047"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("# A\n\n"), withRule("MD047")), "MD047"));
}

// ---- Inline config directives ----------------------------------------------

TEST(MdLintDirectives, DisableAll)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-disable -->\n#oops\n#oops\n"),
        withRule("MD018"));
    EXPECT_EQ(0, countRule(issues, "MD018"));
}

TEST(MdLintDirectives, DisableLine)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("#oops\n#oops <!-- markdownlint-disable-line -->\n"),
        withRule("MD018"));
    ASSERT_EQ(1, countRule(issues, "MD018"));
    EXPECT_EQ(1, issues.at(0).line);
}

TEST(MdLintDirectives, DisableNextLine)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-disable-next-line MD018 -->\n#oops\n#oops\n"),
        withRule("MD018"));
    ASSERT_EQ(1, countRule(issues, "MD018"));
    EXPECT_EQ(3, issues.at(0).line);
}

TEST(MdLintDirectives, DisableEnableRegion)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-disable MD018 -->\n#oops\n#oops\n"
                       "<!-- markdownlint-enable MD018 -->\n#oops\n"),
        withRule("MD018"));
    ASSERT_EQ(1, countRule(issues, "MD018"));
    EXPECT_EQ(5, issues.at(0).line);
}

TEST(MdLintDirectives, CaptureRestore)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-capture -->\n<!-- markdownlint-disable -->\n"
                       "#oops\n<!-- markdownlint-restore -->\n#oops\n"),
        withRule("MD018"));
    ASSERT_EQ(1, countRule(issues, "MD018"));
    EXPECT_EQ(5, issues.at(0).line);
}

TEST(MdLintDirectives, DisableFile)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-disable-file -->\n#oops\n"), withRule("MD018"));
    EXPECT_EQ(0, countRule(issues, "MD018"));
}

TEST(MdLintDirectives, ConfigureFile)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-configure-file {\"MD018\": false} -->\n#oops\n"),
        withRule("MD018"));
    EXPECT_EQ(0, countRule(issues, "MD018"));
}

TEST(MdLintDirectives, DisableByAlias)
{
    const auto issues = MdLintEngine::lint(
        QStringLiteral("<!-- markdownlint-disable no-missing-space-atx -->\n#oops\n"),
        withRule("MD018"));
    EXPECT_EQ(0, countRule(issues, "MD018"));
}

} // namespace
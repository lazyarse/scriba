// Copyright (C) 2026 LazyArse
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

MdLintIssue firstIssue(const QVector<MdLintIssue> &issues, const QString &rule)
{
    for (const auto &i : issues)
        if (i.rule == rule)
            return i;
    return MdLintIssue{};
}

}   // namespace

TEST(MdLintTables, Md055ConsistentPipeStyle)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("| a |\n| --- |\n| b |\n"),
                                           withRule("MD055"));
    EXPECT_EQ(0, countRule(issues, "MD055"));
}

TEST(MdLintTables, Md055FlagsMissingTrailingPipe)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("| a |\n| --- |\n| b\n"),
                                           withRule("MD055"));
    ASSERT_EQ(1, countRule(issues, "MD055"));
    EXPECT_EQ(3, firstIssue(issues, "MD055").line);
}

TEST(MdLintTables, Md055LeadingAndTrailingStyle)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("a |\n--- |\nb |\n"),
                                           withRuleAndParams(
                                               "MD055",
                                               R"({"style": "leading_and_trailing"})"));
    EXPECT_EQ(3, countRule(issues, "MD055"));
}

TEST(MdLintTables, Md055NoLeadingOrTrailingStyle)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("a\n---\nb\n"),
                                           withRuleAndParams(
                                               "MD055",
                                               R"({"style": "no_leading_or_trailing"})"));
    EXPECT_EQ(0, countRule(issues, "MD055"));
    const auto bad = MdLintEngine::lint(QStringLiteral("a\n|---|\nb\n"),
                                        withRuleAndParams(
                                            "MD055",
                                            R"({"style": "no_leading_or_trailing"})"));
    EXPECT_EQ(1, countRule(bad, "MD055"));
}

TEST(MdLintTables, Md056WellFormed)
{
    const auto issues =
        MdLintEngine::lint(QStringLiteral("| a | b |\n| --- | --- |\n| c | d |\n"),
                           withRule("MD056"));
    EXPECT_EQ(0, countRule(issues, "MD056"));
}

TEST(MdLintTables, Md056FlagsColumnCountMismatch)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("| a | b |\n| --- | --- |\n| c |\n"),
                                           withRule("MD056"));
    ASSERT_EQ(1, countRule(issues, "MD056"));
    EXPECT_EQ(3, firstIssue(issues, "MD056").line);
}

TEST(MdLintTables, Md060ConsistentColumnStyle)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("| a |\n| --- |\n| b |\n"),
                                           withRule("MD060"));
    EXPECT_EQ(0, countRule(issues, "MD060"));
}

TEST(MdLintTables, Md060FlagsUnpaddedHeader)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("| a |\n|---|\n| b |\n"),
                                           withRule("MD060"));
    ASSERT_EQ(1, countRule(issues, "MD060"));
    EXPECT_EQ(2, firstIssue(issues, "MD060").line);
}
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

// ---- MD033 ----------------------------------------------------------------

TEST(MdLintLinks, Md033FlagsInlineHtml)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("<div>\n"), withRule("MD033")),
                           "MD033"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(
                             QStringLiteral("<span>x</span>\n"),
                             withRuleAndParams("MD033",
                                               QStringLiteral(R"({"allowed_elements": ["span"]})"))),
                           "MD033"));
}

// ---- MD034 ----------------------------------------------------------------

TEST(MdLintLinks, Md034FlagsBareUrls)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("https://example.com\n"),
                                              withRule("MD034")),
                           "MD034"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("<https://example.com>\n"),
                                              withRule("MD034")),
                           "MD034"));
}

// ---- MD036 ----------------------------------------------------------------

TEST(MdLintLinks, Md036FlagsEmphasisAsHeading)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("*heading*\n"), withRule("MD036")),
                           "MD036"));
    EXPECT_EQ(0, countRule(
        MdLintEngine::lint(QStringLiteral("*heading* and more\n"), withRule("MD036")), "MD036"));
}

// ---- MD037 ----------------------------------------------------------------

TEST(MdLintLinks, Md037FlagsSpaceInEmphasis)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("* foo*\n"), withRule("MD037")),
                           "MD037"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("*foo*\n"), withRule("MD037")),
                           "MD037"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("* foo *\n"), withRule("MD037")),
                           "MD037"));
}

// ---- MD038 ----------------------------------------------------------------

TEST(MdLintLinks, Md038FlagsSpaceInCode)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("` foo `\n"), withRule("MD038")),
                           "MD038"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("`foo`\n"), withRule("MD038")),
                           "MD038"));
}

// ---- MD039 ----------------------------------------------------------------

TEST(MdLintLinks, Md039FlagsSpaceInLinkText)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[ foo ](url)\n"),
                                              withRule("MD039")),
                           "MD039"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("[foo](url)\n"), withRule("MD039")),
                           "MD039"));
}

// ---- MD042 ----------------------------------------------------------------

TEST(MdLintLinks, Md042FlagsEmptyLinks)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[](url)\n"), withRule("MD042")),
                           "MD042"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[ ](url)\n"), withRule("MD042")),
                           "MD042"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("[text](url)\n"), withRule("MD042")),
                           "MD042"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("`[](url)`\n"), withRule("MD042")),
                           "MD042"));
}

// ---- MD044 ----------------------------------------------------------------

TEST(MdLintLinks, Md044FlagsWrongCaseName)
{
    const auto cfg = withRuleAndParams("MD044", QStringLiteral(R"({"names": ["GitHub"]})"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("github\n"), cfg), "MD044"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("GitHub\n"), cfg), "MD044"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("`github`\n"), cfg), "MD044"));
}

// ---- MD045 ----------------------------------------------------------------

TEST(MdLintLinks, Md045FlagsImageWithoutAlt)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("![](img.png)\n"), withRule("MD045")),
                           "MD045"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("![alt](img.png)\n"),
                                              withRule("MD045")),
                           "MD045"));
}

// ---- MD049/MD050 ----------------------------------------------------------

TEST(MdLintLinks, Md049EmphasisStyle)
{
    const auto consistent =
        MdLintEngine::lint(QStringLiteral("*a*\n_b_\n"), withRule("MD049"));
    ASSERT_EQ(1, countRule(consistent, "MD049"));
    EXPECT_EQ(2, consistent.at(0).line);
    const auto underscore = MdLintEngine::lint(
        QStringLiteral("*a*\n_b_\n"),
        withRuleAndParams("MD049", QStringLiteral(R"({"style": "underscore"})")));
    ASSERT_EQ(1, countRule(underscore, "MD049"));
    EXPECT_EQ(1, underscore.at(0).line);
}

TEST(MdLintLinks, Md050StrongStyle)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("**a**\n__b__\n"), withRule("MD050"));
    ASSERT_EQ(1, countRule(issues, "MD050"));
    EXPECT_EQ(2, issues.at(0).line);
}

// ---- MD051 ----------------------------------------------------------------

TEST(MdLintLinks, Md051FlagsMissingFragment)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[a](#missing)\n"), withRule("MD051")),
                           "MD051"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("# Found\n\n[a](#found)\n"),
                                              withRule("MD051")),
                           "MD051"));
}

// ---- MD052/MD053 ----------------------------------------------------------

TEST(MdLintLinks, Md052FlagsMissingReference)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[text][def]\n"), withRule("MD052")),
                           "MD052"));
    EXPECT_EQ(0, countRule(
        MdLintEngine::lint(QStringLiteral("[text][def]\n\n[def]: url\n"), withRule("MD052")),
        "MD052"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[text]\n"), withRule("MD052")),
                           "MD052"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[text][]\n"), withRule("MD052")),
                           "MD052"));
}

TEST(MdLintLinks, Md053FlagsUnusedDefinition)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[def]: url\n"), withRule("MD053")),
                           "MD053"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("[def]: url\n[text][def]\n"),
                                              withRule("MD053")),
                           "MD053"));
}

// ---- MD054 ----------------------------------------------------------------

TEST(MdLintLinks, Md054ConsistentLinkStyle)
{
    const auto issues = MdLintEngine::lint(QStringLiteral("[a](u)\n[b]\n"), withRule("MD054"));
    ASSERT_EQ(1, countRule(issues, "MD054"));
    EXPECT_EQ(2, issues.at(0).line);
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("[a](u)\n[b](v)\n"),
                                              withRule("MD054")),
                           "MD054"));
}

// ---- MD059 ----------------------------------------------------------------

TEST(MdLintLinks, Md059FlagsNonDescriptiveLinkText)
{
    EXPECT_EQ(1, countRule(MdLintEngine::lint(QStringLiteral("[here](url)\n"), withRule("MD059")),
                           "MD059"));
    EXPECT_EQ(0, countRule(MdLintEngine::lint(QStringLiteral("[read more](url)\n"),
                                              withRule("MD059")),
                           "MD059"));
    EXPECT_EQ(1, countRule(MdLintEngine::lint(
                             QStringLiteral("[read more](url)\n"),
                             withRuleAndParams("MD059",
                                               QStringLiteral(R"({"test": "markdownlint-github"})"))),
                           "MD059"));
}

} // namespace
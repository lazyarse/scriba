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

#include "MarkdownChecker.h"

#include <QSet>
#include <QString>

namespace {

QStringList messages(const QVector<MarkdownChecker::Issue> &issues)
{
    QStringList out;
    for (const auto &issue : issues)
        out << issue.message;
    return out;
}

// ---- heading `#` space check ----

TEST(MarkdownChecker, FlagSpacelessHashHeading)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("#foo\n##bar\n"));
    const QStringList msgs = messages(issues);
    ASSERT_EQ(2, msgs.count(
                  QStringLiteral("Heading missing space after '#' (not rendered as a heading)")));
    // The offending span starts at the '#', covers the whole run.
    const auto hashIssues = issues;
    for (const auto &issue : hashIssues) {
        if (issue.message.contains(QLatin1String("Heading missing space")))
            EXPECT_EQ(0, issue.start) << "span starts at the '#': " << issue.message.toStdString();
    }
}

TEST(MarkdownChecker, DoesNotFlagValidAtxHeading)
{
    // `## Foo` has a space after the `#` run; the possessive quantifier must
    // not report it. Regression for a greedy-regex false positive.
    const auto issues = MarkdownChecker::scan(QStringLiteral("## Foo\n"));
    for (const auto &issue : issues)
        EXPECT_FALSE(issue.message.contains(QLatin1String("Heading missing space")))
            << issue.message.toStdString();
    EXPECT_TRUE(messages(issues).isEmpty());
}

TEST(MarkdownChecker, DoesNotFlagHashInsideLinkTarget)
{
    // A `#` that is not at line start (a URL fragment) is never flagged.
    const auto issues = MarkdownChecker::scan(
        QStringLiteral("See [section](#foo) for details.\n"));
    for (const auto &issue : issues)
        EXPECT_FALSE(issue.message.contains(QLatin1String("Heading missing space")))
            << issue.message.toStdString();
}

// ---- headings ----

TEST(MarkdownChecker, FlagsHeadingLevelSkip)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("# Level one\n### Level three\n"));
    EXPECT_TRUE(messages(issues).contains(
        QStringLiteral("Heading level skipped (#3 after #1)")));
}

TEST(MarkdownChecker, FlagsDuplicateHeadings)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("# Title\n## Title\n"));
    EXPECT_TRUE(messages(issues).contains(
        QStringLiteral("Duplicate heading (anchor #title already used)")));
}

TEST(MarkdownChecker, SetextHeadingIsNotASkip)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("# Title\nSubheading\n-------\n"));
    EXPECT_TRUE(messages(issues).isEmpty());
}

// ---- whitespace / length ----

TEST(MarkdownChecker, FlagsTrailingWhitespace)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("text  \nmore\t\n"));
    ASSERT_EQ(2, messages(issues).count(QStringLiteral("Trailing whitespace")));
#if 0
    static_assert(true, "");
#endif
    // The span covers only the trailing runs.
    for (const auto &issue : issues) {
        if (issue.message == QLatin1String("Trailing whitespace"))
            EXPECT_GT(issue.length, 0);
    }
}

TEST(MarkdownChecker, FlagsConsecutiveBlankLines)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("one\n\n\n\ntwo\n"));
    EXPECT_TRUE(messages(issues).contains(QStringLiteral("Consecutive blank lines (3 or more)")));
}

TEST(MarkdownChecker, FlagsOverlongLines)
{
    const QString line = QString(150, QLatin1Char('a'));
    const auto issues = MarkdownChecker::scan(line + QLatin1Char('\n'));
    EXPECT_TRUE(messages(issues).contains(QStringLiteral("Line too long (150 characters)")));
}

// ---- footnotes ----

TEST(MarkdownChecker, FlagsUnmatchedFootnoteReference)
{
    const auto issues = MarkdownChecker::scan(
        QStringLiteral("Text[^1] and [^note].\n\n[^note]: the definition\n"));
    const QStringList msgs = messages(issues);
    EXPECT_TRUE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^1]")));
    EXPECT_FALSE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^note]")));
}

TEST(MarkdownChecker, SkipsFootnoteLikeTextInInlineCode)
{
    const auto issues = MarkdownChecker::scan(
        QStringLiteral("Use `[^1]` inline and a real one[^2].\n"));
    const QStringList msgs = messages(issues);
    EXPECT_TRUE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^2]")));
    EXPECT_FALSE(msgs.contains(QStringLiteral("Unmatched footnote reference: [^1]")));
}

// ---- block skipping ----

TEST(MarkdownChecker, SkipsFencedCodeAndFrontMatter)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral(
        "# Heading\n"
        "```\n"
        "# code\n"
        "#nospace\n"
        "```\n"
        "# Heading\n"
        "\n\n\n"));
    const QStringList msgs = messages(issues);
    EXPECT_EQ(1, msgs.count(QStringLiteral("Duplicate heading (anchor #heading already used)")));
    EXPECT_TRUE(msgs.contains(QStringLiteral("Consecutive blank lines (3 or more)")));
    EXPECT_FALSE(msgs.contains(
        QStringLiteral("Heading missing space after '#' (not rendered as a heading)")));
}

// ---- check selection ----

TEST(MarkdownChecker, EmptyCheckSetDisablesEverything)
{
    const auto issues = MarkdownChecker::scan(QStringLiteral("#oops\n\n\n\n#### deep\n## deep\n"), {});
    EXPECT_TRUE(issues.isEmpty());
}

TEST(MarkdownChecker, SingleCheckOnlyCheckSelectedCheck)
{
    const auto issues = MarkdownChecker::scan(
        QStringLiteral("# Title\n## Title\n#oops\n"), {MarkdownChecker::Check::DuplicateHeading});
    EXPECT_EQ(1, issues.size());
}

} // namespace
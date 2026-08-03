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
//
// R13 dedicated suite (SPEC §18.4): reordering cases like "us one" ->
// "one of us" produce insert-only LCS runs, so they cannot live in the
// rule-case harness; here the exact start/length/suggestion is asserted.
#include <gtest/gtest.h>

#include "rule_test_util.h"

using namespace stoppard;

namespace {

void expectFix(const std::u16string &wrong, size_t start, size_t length,
               const std::u16string &suggestion)
{
    const std::vector<Issue> issues = checkAll(wrong, Dialect::American);
    for (const Issue &iss : issues) {
        if (iss.start == static_cast<int>(start)
            && iss.length == static_cast<int>(length)) {
            ASSERT_FALSE(iss.suggestions.empty());
            EXPECT_EQ(iss.suggestions[0].kind, SuggestionKind::Replace);
            EXPECT_EQ(iss.suggestions[0].text, suggestion);
            return;
        }
    }
    FAIL() << "no issue at (" << start << "," << length << ") for: "
           << std::string(wrong.begin(), wrong.end());
}

} // namespace

TEST(R13, ForUsOne) {
    expectFix(u"for us one", 4, 2, u"one of us");
}

TEST(R13, ForThemOne) {
    expectFix(u"for them one", 4, 4, u"one of them");
}

TEST(R13, WithUsOne) {
    expectFix(u"with us one", 5, 2, u"one of us");
}

TEST(R13, WeOneWent) {
    expectFix(u"we one went", 0, 2, u"one of us");
}

TEST(R13, TheyOneLeft) {
    expectFix(u"they one left", 0, 4, u"one of them");
}

TEST(R13, WeOneShouldGo) {
    expectFix(u"we one should go", 0, 2, u"one of us");
}

TEST(R13, MatchCaseSentenceInitial) {
    expectFix(u"Us one asked", 0, 2, u"One of us");
}

TEST(R13, GiveUsOneStaysClean) {
    // Ditransitive "Give us one." is correct English; no preposition before.
    const std::vector<Issue> issues = checkAll(u"Give us one.", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

TEST(R13, OneFollowedByNounStaysClean) {
    const std::vector<Issue> issues =
        checkAll(u"for us one cat", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

TEST(R13, OneOfUsStaysClean) {
    const std::vector<Issue> issues = checkAll(u"one of us", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

TEST(R13, OneOfThemWentStaysClean) {
    const std::vector<Issue> issues =
        checkAll(u"One of them went home.", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

TEST(R13, YouOneStaysClean) {
    // "you" is excluded (ambiguous number).
    const std::vector<Issue> issues = checkAll(u"for you one", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

TEST(R13, UsTwoStaysClean) {
    // Numerals other than "one" are idiomatic ("for us two").
    const std::vector<Issue> issues = checkAll(u"for us two", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

TEST(R13, MeOneStaysClean) {
    // Singular object pronouns are excluded.
    const std::vector<Issue> issues = checkAll(u"for me one", Dialect::American);
    EXPECT_TRUE(issues.empty());
}

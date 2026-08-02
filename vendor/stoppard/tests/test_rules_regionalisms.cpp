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
#include "rule_test_util.h"
#include "rules_regionalisms.h"

TEST(R9, Cards) {
    expectIssue(checkAll(u"It's in the cards.", Dialect::British), 5, 2, u"on");
    expectClean(checkAll(u"It's in the cards.", Dialect::American));
    expectIssue(checkAll(u"It's on the cards.", Dialect::American), 5, 2, u"in");
}
TEST(R9, Gotten) {
    expectIssue(checkAll(u"I have gotten it.", Dialect::British), 7, 6, u"got");
    expectClean(checkAll(u"I have gotten it.", Dialect::American));
    expectClean(checkAll(u"I have got it.", Dialect::British));
}
TEST(R9, Weekend) {
    expectIssue(checkAll(u"See you at the weekend.", Dialect::American), 8, 2, u"on");
    expectClean(checkAll(u"See you at the weekend.", Dialect::British));
    expectIssue(checkAll(u"See you on the weekend.", Dialect::British), 8, 2, u"at");
}
TEST(R9, DiscussAbout) {
    auto issues = checkAll(u"We discuss about this.", Dialect::Indian);
    ASSERT_EQ(issues.size(), 1u);
    ASSERT_EQ(issues[0].start, 11);
    ASSERT_EQ(issues[0].length, 5);
    ASSERT_EQ(issues[0].suggestions.size(), 1u);
    EXPECT_EQ(issues[0].suggestions[0].kind, SuggestionKind::Remove);
    expectClean(checkAll(u"We discuss about this.", Dialect::American));
}
TEST(R9, ProfileAgreement) {
    EXPECT_TRUE(profileFor(Dialect::British).collectivePluralAgreement);
    EXPECT_FALSE(profileFor(Dialect::American).collectivePluralAgreement);
    EXPECT_FALSE(profileFor(Dialect::Canadian).collectivePluralAgreement);
    EXPECT_TRUE(profileFor(Dialect::NewZealand).collectivePluralAgreement);
}

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

TEST(R7, Flags) {
    expectIssue(checkAll(u"I can could go."), 6, 5);                    // Remove
    expectIssue(checkAll(u"She will can do it."), 9, 3);
    expectIssue(checkAll(u"You must might go."), 9, 5);
    expectIssue(checkAll(u"I should would ask."), 9, 5);
    expectIssue(checkAll(u"I could not can go."), 12, 3);
}
TEST(R7, Clean) {
    expectClean(checkAll(u"I can go."));
    expectClean(checkAll(u"I should have gone."));                      // have = aux
    expectClean(checkAll(u"He cannot go."));
    expectClean(checkAll(u"I could not go."));
}
TEST(R7, RemoveKind) {
    auto issues = checkAll(u"I can could go.");
    ASSERT_EQ(issues.size(), 1u);
    ASSERT_EQ(issues[0].suggestions.size(), 1u);
    EXPECT_EQ(issues[0].suggestions[0].kind, SuggestionKind::Remove);
    EXPECT_EQ(issues[0].suggestions[0].text, u"");
}

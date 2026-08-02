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
#include "rules.h"
using namespace stoppard;

TEST(Framework, RegistryHasNineRules) {
    EXPECT_EQ(allRules().size(), 9u);
}

TEST(Framework, EmptyTextNoIssues) {
    EXPECT_TRUE(runAll(u"", Dialect::American).empty());
}

TEST(Framework, CleanTextNoIssues) {
    EXPECT_TRUE(runAll(u"The cat sat on the mat.", Dialect::American).empty());
}

TEST(Framework, DedupKeepsEarlierStart) {
    // R1 fires on "has" (modal rule); dedup must keep exactly that one issue.
    auto issues = runAll(u"I can has a cat.", Dialect::American);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].start, 6);
    EXPECT_EQ(issues[0].length, 3);
}

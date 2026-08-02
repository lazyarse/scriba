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
#include "rule_test_util.h"

TEST(R3, Adjacent) {
    expectIssue(checkAll(u"I has a cat."), 2, 3, u"have");
    expectIssue(checkAll(u"They goes home."), 5, 4, u"go");
    expectIssue(checkAll(u"He walk to school."), 3, 4, u"walks");
    expectIssue(checkAll(u"She run fast."), 4, 3, u"runs");
    expectClean(checkAll(u"I have a cat."));
    expectClean(checkAll(u"She runs fast."));
    expectClean(checkAll(u"Watch it break."));                       // "it"+base guard
    expectClean(checkAll(u"It breaks."));
}
TEST(R3, AcrossAuxiliaries) {
    expectIssue(checkAll(u"I has been there."), 2, 3, u"have");
    expectIssue(checkAll(u"They is going."), 5, 2, u"are");
    expectIssue(checkAll(u"He have a cat."), 3, 4, u"has");
    expectClean(checkAll(u"I have been there."));
    expectClean(checkAll(u"They are going."));
    expectClean(checkAll(u"I am happy."));
    expectClean(checkAll(u"She was here."));
    expectClean(checkAll(u"She will do it."));                         // modal governs base form
    expectClean(checkAll(u"She can do it."));
    expectClean(checkAll(u"He must walk."));
    { auto issues = checkAll(u"I can has"); ASSERT_EQ(issues.size(), 1u); }  // R1 wins on overlap (dedup)
}
TEST(R3, AcrossPPModifiers) {
    expectIssue(checkAll(u"The list of items are here."), 18, 3, u"is");
    expectIssue(checkAll(u"The boxes on the table is here."), 23, 2, u"are");
    expectClean(checkAll(u"The list of items is here."));
    expectClean(checkAll(u"There is a cat."));                       // expletive: inert in v1
    expectClean(checkAll(u"There are many cats."));
}
TEST(R3, CollectiveNouns) {                                          // §14.3
    expectIssue(checkAll(u"The team are winning.", Dialect::American), 9, 3, u"is");
    expectClean(checkAll(u"The team are winning.", Dialect::British));
    expectClean(checkAll(u"The team is winning.", Dialect::American));
    expectIssue(checkAll(u"The police is here."), 11, 2, u"are");     // always-plural, any dialect
}

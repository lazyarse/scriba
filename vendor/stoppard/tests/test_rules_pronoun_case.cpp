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

TEST(R6, SubjectPosition) {
    expectIssue(checkAll(u"Me went home."), 0, 2, u"I");
    expectIssue(checkAll(u"Him did it."), 0, 3, u"He");
    expectIssue(checkAll(u"Her said no."), 0, 3, u"She");
    expectIssue(checkAll(u"Us saw them."), 0, 2, u"We");
    expectClean(checkAll(u"I went home."));
    expectClean(checkAll(u"Me and John went home."));                   // coordination skip (M4.5)
}
TEST(R6, AfterPreposition) {
    expectIssue(checkAll(u"Give it to she."), 11, 3, u"her");
    expectIssue(checkAll(u"For I, it is hard."), 4, 1, u"me");
    expectIssue(checkAll(u"With he, we go."), 5, 2, u"him");
    expectClean(checkAll(u"I saw her."));
    expectClean(checkAll(u"Between you and I."));                       // PP-internal: M4.5
}

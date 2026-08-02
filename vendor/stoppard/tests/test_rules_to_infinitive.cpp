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

TEST(R4, Flags) {
    expectIssue(checkAll(u"I want to has a cat."), 10, 3, u"have");
    expectIssue(checkAll(u"She tried to went home."), 13, 4, u"go");
    expectIssue(checkAll(u"I need to goes now."), 10, 4, u"go");
    expectIssue(checkAll(u"Ought to has."), 9, 3, u"have");           // §14.4
}
TEST(R4, Clean) {
    expectClean(checkAll(u"I want to go."));
    expectClean(checkAll(u"I look forward to going."));               // gerund never flagged
    expectClean(checkAll(u"I went to the store."));
}

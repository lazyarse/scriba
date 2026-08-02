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

TEST(R1, Flags) {
    expectIssue(checkAll(u"I can has a cat."), 6, 3, u"have");
    expectIssue(checkAll(u"She can goes to the store."), 8, 4, u"go");
    expectIssue(checkAll(u"We could went there."), 9, 4, u"go");
    expectIssue(checkAll(u"He might gone home."), 9, 4, u"go");
    expectIssue(checkAll(u"I can going now."), 6, 5, u"go");
    expectIssue(checkAll(u"I can't has a dog."), 8, 3, u"have");   // decomposed n't
    expectIssue(checkAll(u"You must not has it."), 13, 3, u"have");
    expectIssue(checkAll(u"can has been"), 4, 3, u"have");          // modal + non-base aux
}
TEST(R1, Clean) {
    expectClean(checkAll(u"I can have a cat."));
    expectClean(checkAll(u"We will go soon."));
    expectClean(checkAll(u"I could go."));
    expectClean(checkAll(u"I may be going."));                      // be is auxiliary, base
    expectClean(checkAll(u"I must not go."));
    expectClean(checkAll(u"He can wait."));
}
TEST(R1, CasePreservingSuggestion) {
    expectIssue(checkAll(u"Can Goes?"), 4, 4, u"Go");
}

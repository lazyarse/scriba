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

TEST(R2, DoAuxiliaries) {
    expectIssue(checkAll(u"He did went."), 7, 4, u"go");
    expectIssue(checkAll(u"She does goes."), 9, 4, u"go");
    expectClean(checkAll(u"He did go."));
    expectClean(checkAll(u"She does not go."));
    expectClean(checkAll(u"Did you go?"));                          // no verb directly after did
}
TEST(R2, HaveAuxiliaries) {
    expectIssue(checkAll(u"She has go home."), 8, 2, u"gone");
    expectIssue(checkAll(u"I have went there."), 7, 4, u"gone");
    expectClean(checkAll(u"He has gone."));
    expectClean(checkAll(u"She has walked."));                      // regular past==participle
    expectClean(checkAll(u"I have to go."));                        // obligation idiom guard
    expectClean(checkAll(u"I have got a cat."));                    // have-got idiom guard (§14.4)
}
TEST(R2, Been) {
    expectIssue(checkAll(u"He has been go."), 12, 2, u"going");
    expectIssue(checkAll(u"It has been take."), 12, 4, u"taking");
    expectClean(checkAll(u"He has been gone."));                    // passive
    expectClean(checkAll(u"She has been walking."));
}
TEST(R2, BeForms) {
    expectIssue(checkAll(u"I am has a cat."), 5, 3, u"having");
    expectIssue(checkAll(u"They are went."), 9, 4, u"going");
    expectClean(checkAll(u"I am having a cat."));
    expectClean(checkAll(u"It is gone."));
    expectClean(checkAll(u"She was taken there."));
}

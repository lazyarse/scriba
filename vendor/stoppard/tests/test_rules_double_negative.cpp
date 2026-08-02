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

TEST(R8, Flags) {
    expectIssue(checkAll(u"I don't never go."), 8, 5, u"ever");
    expectIssue(checkAll(u"I don't know nothing."), 13, 7, u"anything");
    expectIssue(checkAll(u"He doesn't see nobody."), 15, 6, u"anybody");
    expectIssue(checkAll(u"I am not nobody."), 9, 6, u"anybody");
    expectIssue(checkAll(u"She didn't go nowhere."), 14, 7, u"anywhere");
}
TEST(R8, Clean) {
    expectClean(checkAll(u"I don't think anyone saw me."));
    expectClean(checkAll(u"I can't not laugh."));                       // can't-not guard
    expectClean(checkAll(u"There is no doubt."));                       // no-doubt guard
    expectClean(checkAll(u"No way."));
    expectClean(checkAll(u"I no longer go."));
    expectClean(checkAll(u"No one came."));
    expectClean(checkAll(u"I never go."));
    expectClean(checkAll(u"I didn't go, and nobody saw me."));          // comma boundary
}

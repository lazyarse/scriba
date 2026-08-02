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

TEST(R5, Flags) {
    expectIssue(checkAll(u"These cat are cute."), 6, 3, u"cats");
    expectIssue(checkAll(u"Many child were there."), 5, 5, u"children");
    expectIssue(checkAll(u"I saw those box."), 12, 3, u"boxes");
    expectIssue(checkAll(u"This dogs bark."), 5, 4, u"dog");
    expectIssue(checkAll(u"Each cats has a toy."), 5, 4, u"cat");
    expectIssue(checkAll(u"These big cat is cute."), 10, 3, u"cats");  // adjective between
}
TEST(R5, Clean) {
    expectClean(checkAll(u"These cats are cute."));
    expectClean(checkAll(u"This is a dog."));
    expectClean(checkAll(u"These water is cold."));                     // mass-noun guard
    expectClean(checkAll(u"Each cat has a toy."));
}

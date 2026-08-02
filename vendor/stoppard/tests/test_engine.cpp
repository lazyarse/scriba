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

#include "stoppard/stoppard.h"

using namespace stoppard;

TEST(Engine, EmptyAndUnicode) {
    EXPECT_TRUE(Engine().check(u"").empty());
    EXPECT_TRUE(Engine().check(u"kōrero Māori ki Aotearoa").empty());   // all Unknown → no rules fire
}
TEST(Engine, UnpairedSurrogateDoesNotCrash) {
    std::u16string s = u"a ";
    s.push_back(static_cast<char16_t>(0xD800));   // lone high surrogate
    s += u" b";
    EXPECT_TRUE(Engine().check(s).empty());
}
TEST(Engine, DialectState) {
    Engine e(Dialect::British);
    EXPECT_EQ(e.dialect(), Dialect::British);
    e.setDialect(Dialect::American);
    EXPECT_TRUE(e.check(u"I have gotten it.").empty());                 // AmE: clean
    e.setDialect(Dialect::British);
    EXPECT_EQ(e.check(u"I have gotten it.").size(), 1u);
}
TEST(Engine, SortedByStart) {
    auto issues = Engine().check(u"He have this cats and I can could go.");
    // R7 fires at "could" (later), R3 at "have"/"cats" (earlier) — must be sorted
    for (size_t i = 1; i < issues.size(); ++i)
        EXPECT_LE(issues[i - 1].start, issues[i].start);
}
TEST(Engine, DedupKeepsHigherPriority) {
    auto issues = Engine().check(u"I can has a cat.");
    ASSERT_EQ(issues.size(), 1u);                                        // R1 and R3 both fire on "has"
    EXPECT_EQ(issues[0].start, 6);
    EXPECT_EQ(issues[0].message, u"The form of the verb must agree with the modal.");
}
TEST(Engine, SpansWithinBounds) {
    auto issues = Engine().check(u"Me went home and I can has a cat.");
    for (const auto& it : issues) {
        EXPECT_GE(it.start, 0);
        EXPECT_LE(it.start + it.length, 32);
    }
}

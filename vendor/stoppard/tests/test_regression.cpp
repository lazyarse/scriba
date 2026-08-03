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
//
// SPEC §9 regression anchors (PLAN Task 4.3): canonical errors must keep
// producing the exact span + suggestion, known-clean input must stay silent.
// These act as a stability net across rule/API refactors.
#include <gtest/gtest.h>

#include "stoppard/stoppard.h"

using namespace stoppard;

TEST(Regression, Utf16Anchors) {
    auto issues = Engine().check(u"I has a cat.");
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].start, 2);
    EXPECT_EQ(issues[0].length, 3);
    ASSERT_EQ(issues[0].suggestions.size(), 1u);
    EXPECT_EQ(issues[0].suggestions[0].text, u"have");
    EXPECT_TRUE(Engine().check(u"This is helo wrking text.").empty());   // never flagged for spelling
    EXPECT_TRUE(Engine().check(u"The cat sat on the mat.").empty());
}

TEST(Regression, ModalAndAuxiliaryAnchors) {
    EXPECT_EQ(Engine().check(u"She has go home.")[0].suggestions[0].text, u"gone")
        << "clean-corpus 4.2 fixed R2 'go' boost; keep the anchor";
    EXPECT_EQ(Engine().check(u"He has been go.")[0].suggestions[0].text, u"going")
        << "'been' takes the gerund; bare-verb boost must not regress this";
    EXPECT_EQ(Engine().check(u"She can goes to the store.")[0].suggestions[0].text, u"go");
}

TEST(Regression, PronounAgreementAnchors) {
    EXPECT_EQ(Engine().check(u"I has a cat.")[0].suggestions[0].text, u"have");
    EXPECT_EQ(Engine().check(u"He have a cat.")[0].suggestions[0].text, u"has");
    EXPECT_TRUE(Engine().check(u"I can read.").empty());
}

TEST(Regression, DeterminerAnchors) {
    EXPECT_EQ(Engine().check(u"These cat can run.")[0].suggestions[0].text, u"cats");
    EXPECT_TRUE(Engine().check(u"These water is cold.").empty());   // mass noun
}
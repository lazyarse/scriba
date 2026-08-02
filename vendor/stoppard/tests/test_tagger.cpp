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
#include "tagger.h"
using namespace stoppard;

TEST(Tagger, ClosedClassDeterministic) {
    auto t = tag(u"I can has a cat.");
    ASSERT_EQ(t.size(), 6u);   // whitespace dropped
    EXPECT_EQ(t[0].pos, PosTag::Pronoun); EXPECT_EQ(t[0].pcase, PronounCase::Subject);
    EXPECT_EQ(t[1].pos, PosTag::Modal);
    EXPECT_EQ(t[2].pos, PosTag::Auxiliary);      // has = auxiliary (Have, 3ps)
    EXPECT_EQ(t[2].auxVerb, AuxVerb::Have); EXPECT_EQ(t[2].auxForm, AuxForm::ThirdSingular);
    EXPECT_EQ(t[3].pos, PosTag::Determiner);     EXPECT_EQ(t[3].number, -1);
    EXPECT_EQ(t[4].pos, PosTag::Noun);           EXPECT_EQ(t[4].number, -1);
    EXPECT_EQ(t[5].pos, PosTag::Punctuation);
}

TEST(Tagger, ContractionDecomposition) {
    auto t = tag(u"don't");
    ASSERT_EQ(t.size(), 2u);
    EXPECT_EQ(t[0].pos, PosTag::Auxiliary); EXPECT_EQ(t[0].token.start, 0); EXPECT_EQ(t[0].token.length, 2);
    EXPECT_EQ(t[1].pos, PosTag::Negator);   EXPECT_EQ(t[1].token.start, 2); EXPECT_EQ(t[1].token.length, 3);
    t = tag(u"she's");
    EXPECT_EQ(t[1].pos, PosTag::Auxiliary);   // 's = is (Be, 3ps)
    EXPECT_EQ(t[1].auxVerb, AuxVerb::Be);
}

TEST(Tagger, ContextResolvesAmbiguity) {
    auto t = tag(u"the cats");
    EXPECT_EQ(t[1].pos, PosTag::Noun);  EXPECT_EQ(t[1].number, +1);      // after determiner
    t = tag(u"cats");
    EXPECT_EQ(t[0].pos, PosTag::Unknown);                                 // alone: 3ps-or-plural
    t = tag(u"they goes");
    EXPECT_EQ(t[1].pos, PosTag::Verb);  EXPECT_EQ(t[1].verbForm, VerbForm::ThirdSingular);
    EXPECT_EQ(t[1].lemma, u"go");                                         // irregular table
    t = tag(u"walking");
    EXPECT_EQ(t[0].pos, PosTag::Verb);  EXPECT_EQ(t[0].verbForm, VerbForm::Gerund);
    t = tag(u"the walking");
    EXPECT_EQ(t[0].pos, PosTag::Determiner); EXPECT_EQ(t[1].pos, PosTag::Noun);
    t = tag(u"decision");
    EXPECT_EQ(t[0].pos, PosTag::Noun);                                     // -tion
    t = tag(u"quickly");
    EXPECT_EQ(t[0].pos, PosTag::Adverb);                                   // -ly
    t = tag(u"went");
    EXPECT_EQ(t[0].pos, PosTag::Verb);  EXPECT_EQ(t[0].verbForm, VerbForm::Past);
    EXPECT_EQ(t[0].lemma, u"go");
}

TEST(Tagger, ContextBoostAfterModalAndTo) {
    auto t = tag(u"can has");
    EXPECT_EQ(t[1].pos, PosTag::Auxiliary);                                // lexicon beats context
    t = tag(u"can walk");
    EXPECT_EQ(t[1].pos, PosTag::Verb);  EXPECT_EQ(t[1].verbForm, VerbForm::Base);
    t = tag(u"to store");
    EXPECT_EQ(t[1].pos, PosTag::Unknown);                                  // bare after "to": ambiguous
    t = tag(u"to went");
    EXPECT_EQ(t[1].pos, PosTag::Verb);  EXPECT_EQ(t[1].verbForm, VerbForm::Past);
}

TEST(Tagger, UnknownConservatism) {
    auto t = tag(u"helo wrking text");
    for (const auto &tk : t) EXPECT_EQ(tk.pos, PosTag::Unknown);
    t = tag(u"kōrero Māori ki Aotearoa");
    for (const auto &tk : t) EXPECT_EQ(tk.pos, PosTag::Unknown);
}

TEST(Tagger, CodeIsSkipped) {
    auto t = tag(u"Use `rm -rf` now");
    EXPECT_EQ(t[1].pos, PosTag::Code);
}

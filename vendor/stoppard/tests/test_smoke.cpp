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
#include "tokenizer.h"
#include "lexicon.h"
#include "morphology.h"
#include "tagger.h"
#include "chunker.h"
using namespace stoppard;

// M1 exit criteria (SPEC.md 12): "I can has a cat." tokenizes and `has` resolves via paradigms.
TEST(Smoke, M1ExitCriteria) {
    auto t = tokenize(u"I can has a cat.");
    ASSERT_EQ(t.size(), 10u);
    EXPECT_EQ(t[0].start, 0);          // I
    EXPECT_EQ(t[2].start, 2);          // can
    EXPECT_EQ(t[4].start, 6);          // has
    EXPECT_EQ(t[8].length, 3);         // cat
    EXPECT_EQ(t[9].kind, TokenKind::Punctuation);

    EXPECT_EQ(lookupWord(u"can")->tag, WordTag::Modal);
    auto f = classifyVerbForm(u"has");
    EXPECT_TRUE(f.known);
    EXPECT_EQ(f.lemma, u"have");
    EXPECT_EQ(f.form, VerbForm::ThirdSingular);
}

// M2 exit criteria (SPEC.md 12): "I can has a cat." tags and chunks deterministically.
TEST(Smoke, M2ExitCriteria) {
    auto t = tag(u"I can has a cat.");
    ASSERT_EQ(t.size(), 6u);
    EXPECT_EQ(t[0].pos, PosTag::Pronoun);            // I
    EXPECT_EQ(t[1].pos, PosTag::Modal);              // can
    EXPECT_EQ(t[2].pos, PosTag::Auxiliary);          // has
    EXPECT_EQ(t[2].auxVerb, AuxVerb::Have);
    EXPECT_EQ(t[2].auxForm, AuxForm::ThirdSingular);
    EXPECT_EQ(t[3].pos, PosTag::Determiner);         // a
    EXPECT_EQ(t[4].pos, PosTag::Noun);               // cat
    EXPECT_EQ(t[4].number, -1);
    EXPECT_EQ(t[5].pos, PosTag::Punctuation);

    const auto toks = tag(u"I can has");
    const auto chunks = chunk(toks);
    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0].kind, ChunkKind::NP);
    EXPECT_EQ(findSubjectHead(toks, 2), 0);  // I is the subject of has
}

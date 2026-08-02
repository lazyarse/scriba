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
#include "lexicon.h"
using namespace stoppard;

TEST(Lexicon, SpecClosedClasses) {
    // every word in SPEC.md 6.2 must resolve to its tag
    EXPECT_EQ(lookupWord(u"the")->tag, WordTag::Determiner);
    EXPECT_EQ(lookupWord(u"many")->tag, WordTag::Determiner);
    EXPECT_EQ(lookupWord(u"they")->tag, WordTag::Pronoun);
    EXPECT_EQ(lookupWord(u"could")->tag, WordTag::Modal);
    EXPECT_EQ(lookupWord(u"had")->tag, WordTag::Auxiliary);
    EXPECT_EQ(lookupWord(u"between")->tag, WordTag::Preposition);
    EXPECT_EQ(lookupWord(u"although")->tag, WordTag::Conjunction);
    EXPECT_EQ(lookupWord(u"never")->tag, WordTag::Negator);
    EXPECT_EQ(lookupWord(u"whose")->tag, WordTag::Interrogative);
}

TEST(Lexicon, CaseInsensitive) {
    EXPECT_EQ(lookupWord(u"The")->tag, WordTag::Determiner);
    EXPECT_EQ(lookupWord(u"THE")->tag, WordTag::Determiner);
}

TEST(Lexicon, ContentWordIsNull) { EXPECT_EQ(lookupWord(u"cat"), nullptr); }

TEST(Lexicon, DeterminerNumber) {
    EXPECT_EQ(lookupWord(u"this")->number, -1);
    EXPECT_EQ(lookupWord(u"every")->number, -1);
    EXPECT_EQ(lookupWord(u"these")->number, +1);
    EXPECT_EQ(lookupWord(u"few")->number, +1);
    EXPECT_EQ(lookupWord(u"the")->number, 0);
    EXPECT_EQ(lookupWord(u"any")->number, 0);
}

TEST(Lexicon, PronounFeatures) {
    EXPECT_EQ(lookupWord(u"me")->person, 1);  EXPECT_EQ(lookupWord(u"me")->pcase, PronounCase::Object);
    EXPECT_EQ(lookupWord(u"we")->person, 1);  EXPECT_EQ(lookupWord(u"we")->number, +1);
    EXPECT_EQ(lookupWord(u"them")->person, 3); EXPECT_EQ(lookupWord(u"them")->number, +1);
    EXPECT_EQ(lookupWord(u"my")->pcase, PronounCase::PossessiveDeterminer);
    EXPECT_EQ(lookupWord(u"mine")->pcase, PronounCase::Possessive);
}

TEST(Lexicon, DecomposedParticles) {   // splitContraction outputs must be taggable
    EXPECT_EQ(lookupWord(u"'ll")->tag, WordTag::Modal);
    EXPECT_EQ(lookupWord(u"n't")->tag, WordTag::Negator);
}

TEST(Lexicon, AuxiliaryForms) {
    EXPECT_EQ(lookupWord(u"am")->auxVerb, AuxVerb::Be);
    EXPECT_EQ(lookupWord(u"am")->auxForm, AuxForm::Base);       // am = be+base, person 1
    EXPECT_EQ(lookupWord(u"was")->auxForm, AuxForm::Past);
    EXPECT_EQ(lookupWord(u"been")->auxForm, AuxForm::PastParticiple);
    EXPECT_EQ(lookupWord(u"doing")->auxForm, AuxForm::Gerund);
}

TEST(Lexicon, FullPlanWordLists) {
    // every word listed in PLAN.md Task 3 must resolve (guards the sorted-array coverage)
    for (const auto w : {u"a", u"an", u"this", u"that", u"each", u"every", u"another",
                         u"either", u"neither", u"these", u"those", u"many", u"several",
                         u"few", u"both", u"various", u"numerous", u"the", u"some",
                         u"any", u"all", u"no", u"in", u"on", u"at", u"of", u"with",
                         u"by", u"for", u"to", u"from", u"about", u"after", u"before",
                         u"between", u"under", u"over", u"through", u"without", u"behind",
                         u"beside", u"beyond", u"despite", u"during", u"inside", u"into",
                         u"near", u"onto", u"out", u"outside", u"past", u"since",
                         u"throughout", u"toward", u"towards", u"underneath", u"until",
                         u"upon", u"within", u"among", u"amongst", u"around", u"across",
                         u"along", u"against", u"off", u"above", u"below", u"per",
                         u"plus", u"minus", u"via", u"except", u"including", u"regarding",
                         u"can", u"could", u"will", u"would", u"shall", u"should",
                         u"may", u"might", u"must", u"cannot", u"am", u"is", u"are",
                         u"was", u"were", u"have", u"has", u"had", u"been", u"being",
                         u"having", u"doing", u"done", u"not", u"never", u"nobody",
                         u"nothing", u"nowhere", u"and", u"or", u"but", u"because",
                         u"although", u"though", u"if", u"unless", u"while", u"whereas",
                         u"so", u"yet", u"as", u"than", u"what", u"who", u"whom",
                         u"whose", u"which", u"when", u"where", u"why", u"how",
                         u"my", u"your", u"his", u"her", u"its", u"our", u"their",
                         u"mine", u"yours", u"hers", u"ours", u"theirs", u"i", u"me",
                         u"we", u"us", u"you", u"he", u"him", u"she", u"it", u"they",
                         u"them", u"n't"}) {
        EXPECT_NE(lookupWord(w), nullptr);
    }
}

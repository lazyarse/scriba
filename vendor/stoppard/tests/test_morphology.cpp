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
#include "morphology.h"
using namespace stoppard;

TEST(Morphology, IrregularVerbClassification) {
    auto f = classifyVerbForm(u"went");   EXPECT_EQ(f.form, VerbForm::Past);           EXPECT_EQ(f.lemma, u"go");
    f = classifyVerbForm(u"has");         EXPECT_EQ(f.form, VerbForm::ThirdSingular);  EXPECT_EQ(f.lemma, u"have");
    f = classifyVerbForm(u"been");        EXPECT_EQ(f.form, VerbForm::PastParticiple); EXPECT_EQ(f.lemma, u"be");
    f = classifyVerbForm(u"took");        EXPECT_EQ(f.lemma, u"take");
    f = classifyVerbForm(u"seen");        EXPECT_EQ(f.lemma, u"see");
    f = classifyVerbForm(u"gotten");      EXPECT_EQ(f.lemma, u"get");                 // AmE participle
}

TEST(Morphology, RegularVerbClassification) {
    auto f = classifyVerbForm(u"walking"); EXPECT_EQ(f.form, VerbForm::Gerund);        EXPECT_EQ(f.lemma, u"walk");
    f = classifyVerbForm(u"jumped");       EXPECT_EQ(f.form, VerbForm::Past);          EXPECT_EQ(f.lemma, u"jump");
    f = classifyVerbForm(u"carries");      EXPECT_EQ(f.form, VerbForm::ThirdSingular); EXPECT_EQ(f.lemma, u"carry");
    f = classifyVerbForm(u"hoped");        EXPECT_EQ(f.lemma, u"hope");
    EXPECT_FALSE(classifyVerbForm(u"cat").known);
}

TEST(Morphology, Paradigm) {
    auto v = lookupVerbForms(u"walk");
    EXPECT_TRUE(v.known);
    EXPECT_EQ(v.third, u"walks");  EXPECT_EQ(v.past, u"walked");
    EXPECT_EQ(v.participle, u"walked");  EXPECT_EQ(v.gerund, u"walking");
    v = lookupVerbForms(u"carry");
    EXPECT_EQ(v.third, u"carries");  EXPECT_EQ(v.past, u"carried");  EXPECT_EQ(v.gerund, u"carrying");
    v = lookupVerbForms(u"hop");
    EXPECT_EQ(v.past, u"hopped");  EXPECT_EQ(v.gerund, u"hopping");
    v = lookupVerbForms(u"hope");
    EXPECT_EQ(v.gerund, u"hoping");
    v = lookupVerbForms(u"die");
    EXPECT_EQ(v.gerund, u"dying");
    v = lookupVerbForms(u"agree");
    EXPECT_EQ(v.gerund, u"agreeing");
    v = lookupVerbForms(u"take");
    EXPECT_EQ(v.past, u"took");  EXPECT_EQ(v.participle, u"taken");
}

TEST(Morphology, NounPlurals) {
    EXPECT_EQ(pluralize(u"cat"), u"cats");
    EXPECT_EQ(pluralize(u"box"), u"boxes");
    EXPECT_EQ(pluralize(u"baby"), u"babies");
    EXPECT_EQ(pluralize(u"key"), u"keys");
    EXPECT_EQ(pluralize(u"knife"), u"knives");
    EXPECT_EQ(pluralize(u"potato"), u"potatoes");
    EXPECT_EQ(pluralize(u"piano"), u"pianos");
    EXPECT_EQ(pluralize(u"child"), u"children");
    EXPECT_EQ(pluralize(u"mouse"), u"mice");
    EXPECT_EQ(pluralize(u"sheep"), u"sheep");
}

TEST(Morphology, NounSingulars) {
    EXPECT_EQ(singularize(u"cats"), u"cat");
    EXPECT_EQ(singularize(u"boxes"), u"box");
    EXPECT_EQ(singularize(u"babies"), u"baby");
    EXPECT_EQ(singularize(u"knives"), u"knife");
    EXPECT_EQ(singularize(u"children"), u"child");
    EXPECT_EQ(singularize(u"mice"), u"mouse");
    EXPECT_EQ(singularize(u"sheep"), u"sheep");
}

TEST(Morphology, NounNumber) {
    EXPECT_EQ(nounNumber(u"cat"), -1);
    EXPECT_EQ(nounNumber(u"cats"), +1);
    EXPECT_EQ(nounNumber(u"gas"), -1);     // -as/-ss/-us/-is endings stay singular
    EXPECT_EQ(nounNumber(u"sheep"), 0);
    EXPECT_EQ(nounNumber(u"children"), +1);
}

TEST(Morphology, CasePreserving) {
    EXPECT_EQ(pluralize(u"Cat"), u"Cats");
    EXPECT_EQ(pluralize(u"CAT"), u"CATS");
    EXPECT_EQ(pluralize(u"Child"), u"Children");
    EXPECT_EQ(singularize(u"Cats"), u"Cat");
    EXPECT_EQ(classifyVerbForm(u"Went").lemma, u"go");
}

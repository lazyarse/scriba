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
// Spelling-pass behaviour (SPEC §19): REP pairs, generator order, the
// ngram gate, token policy, user words, and dialect/Māori handling.
#include <string>

#include <gtest/gtest.h>

#include "stoppard/stoppard.h"

using namespace stoppard;

namespace {

void configure(Engine &e, Dialect d = Dialect::American, Language l = Language::American)
{
    e.setDialect(d);
    e.setLanguage(l);
    e.setDictionaryPaths(std::string(TESTS_DICT_DIR) + "/en-US.txt",
                         std::string(TESTS_DICT_DIR) + "/en-GB.txt",
                         std::string(TESTS_DICT_DIR) + "/maori-nz.txt",
                         std::string(TESTS_DICT_DIR) + "/canadian-en.txt");
}

// Suggestion list for a bare word, or empty if it is not flagged.
std::vector<std::u16string> suggestionsFor(const Engine &e, const char16_t *word)
{
    const auto issues = e.check(word);
    for (const auto &iss : issues) {
        if (iss.start == 0 && iss.length == static_cast<int>(std::u16string_view(word).size())) {
            std::vector<std::u16string> out;
            for (const auto &s : iss.suggestions)
                out.push_back(s.text);
            return out;
        }
    }
    return {};
}

bool flagsWord(const Engine &e, const char16_t *word)
{
    return !suggestionsFor(e, word).empty();
}

} // namespace

TEST(Spell, RepPairSplitsAndRanksFirst) {
    Engine e(Dialect::American);
    configure(e);
    auto s = suggestionsFor(e, u"alot");
    ASSERT_FALSE(s.empty());
    EXPECT_EQ(s.front(), u"a lot");   // REP "alot" -> "a lot"
}
TEST(Spell, SwapCharTop1) {
    Engine e(Dialect::American);
    configure(e);
    auto s = suggestionsFor(e, u"recieve"); ASSERT_FALSE(s.empty()); EXPECT_EQ(s.front(), u"receive");
}
TEST(Spell, ExtraCharTop1) {
    Engine e(Dialect::American);
    configure(e);
    auto s = suggestionsFor(e, u"seperate"); ASSERT_FALSE(s.empty()); EXPECT_EQ(s.front(), u"separate");
}
TEST(Spell, NgramContributesGrammar) {
    Engine e(Dialect::American);
    configure(e);
    const auto s = suggestionsFor(e, u"grammer");
    ASSERT_FALSE(s.empty());
    bool found = false;
    for (const auto &cand : s) {
        if (cand == u"grammar") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);   // in top-5 (measured 2nd after "glammer")
}
TEST(Spell, RepPinOutranksNgram) {
    Engine e(Dialect::American);
    configure(e);
    // M11 added REP "mmorow" -> "tomorrow" (reference-set pair). The REP pin
    // now wins top-1, and the ngram scan is skipped because a generator
    // found something good (hunspell-faithful: "try ngram approach since
    // found nothing good"). "morrow" (the pre-M11 ngram-only winner) is no
    // longer produced; NgramContributesGrammar covers the ngram path.
    auto s = suggestionsFor(e, u"mmorow"); ASSERT_FALSE(s.empty()); EXPECT_EQ(s.front(), u"tomorrow");
    for (const auto &cand : s)
        EXPECT_NE(cand, u"morrow");
}
TEST(Spell, LanguageNoneSkipsSpelling) {
    Engine e(Dialect::American);
    EXPECT_TRUE(e.check(u"alot recieve seperate").empty());
}
TEST(Spell, UserWordsSuppressFlag) {
    Engine e(Dialect::American);
    configure(e);
    e.setUserWords({u"qwxz"});
    EXPECT_TRUE(e.check(u"qwxz").empty());
    e.setUserWords({});
    EXPECT_TRUE(flagsWord(e, u"qwxz"));
}
TEST(Spell, CaseMatchedSuggestion) {
    Engine e(Dialect::American);
    configure(e);
    auto s = suggestionsFor(e, u"Recieve"); ASSERT_FALSE(s.empty()); EXPECT_EQ(s.front(), u"Receive");
}
TEST(Spell, TokenPolicySkipsCapsFragmentChecks) {
    Engine e(Dialect::American);
    configure(e);
    EXPECT_TRUE(e.check(u"RECIEVE").empty());          // all-caps skip (§19.5)
    EXPECT_TRUE(e.check(u"well-known").empty());       // both fragments are real words
    EXPECT_FALSE(e.check(u"recieve-this").empty());  // "recieve" fragment is flagged
    EXPECT_FALSE(e.check(u"2recieve").empty());      // digit-split fragment too
}
TEST(Spell, PossessiveFlagsBadBase) {
    Engine e(Dialect::American);
    configure(e);
    EXPECT_TRUE(flagsWord(e, u"recieve's"));
}
TEST(Spell, MaoriExemptOnlyUnderNewZealand) {
    Engine us(Dialect::American);
    configure(us);
    Engine nz(Dialect::NewZealand);
    configure(nz, Dialect::NewZealand, Language::British);
    EXPECT_TRUE(flagsWord(us, u"whanau"));
    EXPECT_FALSE(flagsWord(nz, u"whanau"));
}
TEST(Spell, BritishDictionaryDiffers) {
    Engine us(Dialect::American);
    configure(us);
    Engine gb(Dialect::British);
    configure(gb, Dialect::British, Language::British);
    EXPECT_TRUE(flagsWord(us, u"colour"));   // not an en-US word
    EXPECT_EQ(flagsWord(gb, u"colour"), false);
}
TEST(Spell, CanadianAllowsCentre) {
    Engine us(Dialect::American);
    configure(us);
    Engine ca(Dialect::Canadian);
    configure(ca, Dialect::Canadian, Language::American);
    EXPECT_TRUE(flagsWord(us, u"centre"));   // not en-US
    EXPECT_FALSE(flagsWord(ca, u"centre"));  // Canadian allowance (§19.3)
}
TEST(Spell, GrammarStillFiresAlongsideSpelling) {
    Engine e(Dialect::American);
    configure(e);
    const auto issues = e.check(u"I can has a cat.");
    ASSERT_EQ(issues.size(), 1u);   // grammar only; all tokens are real words
    EXPECT_EQ(issues[0].start, 6);  // R1/R3 on "has"
}
TEST(Spell, SortedAndSpansBounded) {
    Engine e(Dialect::American);
    configure(e);
    const auto issues = e.check(u"Recieve seperate alot grammer.");
    int prev = -1;
    for (const auto &iss : issues) {
        EXPECT_GT(iss.start, prev);
        prev = iss.start;
        EXPECT_GE(iss.start, 0);
        EXPECT_LE(iss.start + iss.length, 30);
    }
}

// Per-word API (SPEC §19.5 / M10): isMisspelled + spellSuggestions mirror
// the spelling pass's per-token policy and case-matched suggestions.
TEST(Spell, PerWordPolicyAndCase) {
    Engine e(Dialect::American);
    configure(e);
    EXPECT_FALSE(e.isMisspelled(u"cat"));
    EXPECT_TRUE(e.isMisspelled(u"recieve"));
    ASSERT_FALSE(e.spellSuggestions(u"recieve").empty());
    EXPECT_EQ(e.spellSuggestions(u"recieve").front(), u"receive");
    EXPECT_FALSE(e.isMisspelled(u"2recieve"));      // digit -> skip
    EXPECT_FALSE(e.isMisspelled(u"recieve-this"));  // hyphen -> skip
    EXPECT_FALSE(e.isMisspelled(u"RECIEVE"));       // all-caps -> skip
    EXPECT_TRUE(e.spellSuggestions(u"RECIEVE").empty());
    EXPECT_FALSE(e.isMisspelled(u"don't"));         // closed contraction
    EXPECT_FALSE(e.isMisspelled(u"cat's"));         // possessive of clean base
    EXPECT_TRUE(e.isMisspelled(u"recieve's"));      // possessive of bad base
    ASSERT_FALSE(e.spellSuggestions(u"Recieve").empty());
    EXPECT_EQ(e.spellSuggestions(u"Recieve").front(), u"Receive");  // case-matched
}
TEST(Spell, PerWordLanguageNoneAndUserWords) {
    Engine none;   // Language::None
    EXPECT_FALSE(none.isMisspelled(u"recieve"));
    EXPECT_TRUE(none.spellSuggestions(u"recieve").empty());

    Engine e(Dialect::American);
    configure(e);
    EXPECT_TRUE(e.isMisspelled(u"qwxz"));
    e.setUserWords({u"qwxz"});
    EXPECT_FALSE(e.isMisspelled(u"qwxz"));
    EXPECT_TRUE(e.spellSuggestions(u"qwxz").empty());
    e.setUserWords({});
    EXPECT_TRUE(e.isMisspelled(u"qwxz"));
}
TEST(Spell, PerWordDialectAllowances) {
    Engine us(Dialect::American);
    configure(us);
    EXPECT_TRUE(us.isMisspelled(u"centre"));
    Engine ca(Dialect::Canadian);
    configure(ca, Dialect::Canadian, Language::American);
    EXPECT_FALSE(ca.isMisspelled(u"centre"));       // §19.3 allowance
    Engine nz(Dialect::NewZealand);
    configure(nz, Dialect::NewZealand, Language::British);
    EXPECT_FALSE(nz.isMisspelled(u"whanau"));       // §19.7 Māori exemption
    EXPECT_FALSE(nz.isMisspelled(u"whānau"));       // folded lookup
    EXPECT_TRUE(nz.spellSuggestions(u"whanau").empty());
}

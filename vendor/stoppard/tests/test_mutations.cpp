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
// Mutation corpus (PLAN Task 4.3 / SPEC §9): every clean corpus line is
// treated as a seed. For each seed we apply a guarded error-operator that
// flips exactly one anchor into its defective twin, then require the engine
// to produce exactly ONE issue at the mutated token whose suggestion
// restores the original word. Guards exclude seeds that are not simple
// declaratives (commas, conjunctions, inversion) since multi-clause
// sentences can seat unexpected secondary rules.
//
// Operators (anchor -> mutation -> expected rule):
//   [modal]+Verb-Base                verb -> 3ps      R1
//   [I/you/we/they]+Verb-Base        verb -> 3ps      R3
//   [he/she]+Verb-3ps                verb -> Base     R3
//   [sing det]+[sing noun] (not mass) noun -> plural  R5
//   [prep]+[obj pronoun]             pronoun -> subj  R6
// Negation-flip / modal-insertion / tense-shift operators are deferred to
// M4.5/M8 (their guard tables arrive with those rules) — see
// docs/mutations.md.
#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "morphology.h"
#include "tagger.h"
#include "rule_test_util.h"
#include "data_util.h"

using namespace stoppard;

namespace {

std::u16string wordOf(const std::u16string &text, const TaggedToken &t)
{
    return text.substr(t.token.start, t.token.length);
}

std::u16string toLowerCase(std::u16string s)
{
    for (auto &c : s)
        if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
    return s;
}

// A candidate edit: the mutated token + the body of the mutated line.
struct Mutation {
    int start;                 // token start in the mutated line
    int length;               // mutated-word length
    std::u16string fix;       // expected suggestion (restores the seed)
    std::u16string line;      // mutated line, one error injected
};

// Determine whether the seed is a simple declarative (PLAN 4.3 guard).
bool isMutatable(const std::u16string &text)
{
    if (text.find(u',') != std::u16string::npos) return false;
    if (text.find(u'?') != std::u16string::npos) return false;
    if (text.find(u'!') != std::u16string::npos) return false;
    constexpr std::array<std::u16string_view, 9> kForbidden = {
        u" and ", u" or ", u" but ", u" because ", u" although ",
        u" while ", u" if ", u" unless ", u" whereas ",
    };
    for (const auto &f : kForbidden)
        if (text.find(f) != std::u16string::npos) return false;
    return true;
}

// Mass nouns are exempt from the determiner-pluralization operator exactly as
// R5 exempts them. Kept in sync with src/rules_determiner.cpp kMassNouns.
bool isMassNoun(const std::u16string &w)
{
    static const std::array<std::u16string, 31> kMass = {
        u"water", u"milk", u"air", u"rice", u"money", u"music", u"furniture",
        u"information", u"advice", u"homework", u"news", u"luggage", u"equipment",
        u"software", u"bread", u"butter", u"cheese", u"sugar", u"salt", u"coffee",
        u"tea", u"oil", u"sand", u"dust", u"grass", u"traffic", u"weather",
        u"knowledge", u"literature", u"research", u"evidence",
    };
    return std::find(kMass.begin(), kMass.end(), w) != kMass.end();
}

bool isSubjPlural(const TaggedToken &t, const std::u16string &w)
{
    // I/you/we/they and other plural/2-person subjects take the base verb.
    return (t.number == +1) || (t.number == 0 && (w == u"you")) || t.person == 1;
}

std::u16string replaceLine(std::u16string line, const TaggedToken &t, const std::u16string &with)
{
    line.replace(t.token.start, t.token.length, with);
    return line;
}

} // namespace

TEST(Mutation, OneIssuePerOperator) {
    const std::vector<std::string> seeds =
        readLines(std::string(TESTS_DATA_DIR) + "/clean_corpus.txt");
    ASSERT_GT(seeds.size(), 100u);
    size_t applied = 0;
    size_t byOp[5] = {0, 0, 0, 0, 0};
    const char *opName[5] = {"R1-modal", "R3-plural-subj", "R3-3sg-subj", "R5-det-noun", "R6-prep-pro"};

    for (const auto &seed : seeds) {
        const std::u16string s = utf8ToUtf16(seed);
        if (!isMutatable(s)) continue;
        const auto toks = tag(s);

        Mutation m;   // one mutation per seed (first operator that anchors)
        bool have = false;

        // R1: modal + Verb-Base -> verb 3ps.
        for (size_t i = 0; i + 1 < toks.size() && !have; ++i) {
            if (toks[i].pos != PosTag::Modal) continue;
            size_t j = i + 1;
            while (j < toks.size()
                   && (toks[j].pos == PosTag::Adverb || toks[j].pos == PosTag::Negator))
                ++j;
            if (j >= toks.size() || toks[j].pos != PosTag::Verb) continue;
            if (toks[j].verbForm != VerbForm::Base) continue;
            const VerbForms vf = lookupVerbForms(toks[j].lemma);
            if (vf.third.empty() || vf.third == wordOf(s, toks[j])) continue;
            // The suggestion is built from the mutated token's lemma. If that
            // reverse-map is not deterministic (comes -> com, ambiguous sans
            // dictionary), the fix does not restore the seed, so such seeds
            // are skipped.
            if (classifyVerbForm(vf.third).lemma != toks[j].lemma) continue;
            m.start = toks[j].token.start;
            m.length = static_cast<int>(vf.third.size());
            m.fix = wordOf(s, toks[j]);
            m.line = replaceLine(s, toks[j], vf.third);
            have = true;
            ++byOp[0];
        }

        // R3: [I/you/we/they]+Verb-Base -> verb 3ps.
        for (size_t i = 0; i + 1 < toks.size() && !have; ++i) {
            if (toks[i].pos != PosTag::Pronoun || toks[i].pcase != PronounCase::Subject)
                continue;
            if (toks[i + 1].pos != PosTag::Verb || toks[i + 1].verbForm != VerbForm::Base)
                continue;
            const std::u16string w = toLowerCase(wordOf(s, toks[i]));
            if (!isSubjPlural(toks[i], w) || w == u"it") continue;
            const VerbForms vf = lookupVerbForms(toks[i + 1].lemma);
            if (vf.third.empty() || vf.third == wordOf(s, toks[i + 1])) continue;
            if (classifyVerbForm(vf.third).lemma != toks[i + 1].lemma) continue;
            m.start = toks[i + 1].token.start;
            m.length = static_cast<int>(vf.third.size());
            m.fix = wordOf(s, toks[i + 1]);
            m.line = replaceLine(s, toks[i + 1], vf.third);
            have = true;
            ++byOp[1];
        }

        // R3: [he/she]+Verb-3ps -> verb Base.
        for (size_t i = 0; i + 1 < toks.size() && !have; ++i) {
            if (toks[i].pos != PosTag::Pronoun || toks[i].pcase != PronounCase::Subject)
                continue;
            const std::u16string w = toLowerCase(wordOf(s, toks[i]));
            if (w != u"he" && w != u"she") continue;
            if (toks[i + 1].pos != PosTag::Verb) continue;
            if (toks[i + 1].verbForm != VerbForm::ThirdSingular) continue;
            const std::u16string base = toks[i + 1].lemma;
            if (base.empty() || base == wordOf(s, toks[i + 1])) continue;
            // Some base lemmas are re-tagged as non-Base by the classifier
            // (e.g. whose own surface lemmas like "need" mis-suffix as -ed).
            if (classifyVerbForm(base).form != VerbForm::Base) continue;
            m.start = toks[i + 1].token.start;
            m.length = static_cast<int>(base.size());
            m.fix = wordOf(s, toks[i + 1]);
            m.line = replaceLine(s, toks[i + 1], base);
            have = true;
            ++byOp[2];
        }

        // R5: [sing det]+[sing noun] (not mass) -> noun plural. Only when the
        // noun is NOT the clause subject: pluralizing a subject head also
        // breaks subject-verb agreement further on, which would seat R3 as a
        // second issue ("This books is interesting."). Prefer an object/"a
        // coin in the street" style noun with no verb after it.
        for (size_t i = 0; i + 1 < toks.size() && !have; ++i) {
            if (toks[i].pos != PosTag::Determiner || toks[i].number != -1) continue;
            if (toks[i + 1].pos != PosTag::Noun || toks[i + 1].number != -1) continue;
            const std::u16string noun = wordOf(s, toks[i + 1]);
            if (isMassNoun(noun)) continue;
            bool verbAfter = false;
            for (size_t k = i + 2; k < toks.size(); ++k) {
                if (toks[k].pos == PosTag::Verb || toks[k].pos == PosTag::Auxiliary) {
                    verbAfter = true;
                    break;
                }
                if (toks[k].pos == PosTag::Punctuation) break;
            }
            if (verbAfter) continue;
            const std::u16string pl = pluralize(noun);
            if (pl.empty() || pl == noun) continue;
            // Only plurals that singularize back deterministically: "houses"
            // cannot decide between hous/house, so its fix would not restore
            // the seed.
            if (singularize(pl) != noun) continue;
            m.start = toks[i + 1].token.start;
            m.length = static_cast<int>(pl.size());
            m.fix = noun;
            m.line = replaceLine(s, toks[i + 1], pl);
            have = true;
            ++byOp[3];
        }

        // R6: [prep]+[object pronoun] -> subject form.
        for (size_t i = 0; i + 1 < toks.size() && !have; ++i) {
            if (toks[i].pos != PosTag::Preposition) continue;
            if (toks[i + 1].pos != PosTag::Pronoun || toks[i + 1].pcase != PronounCase::Object)
                continue;
            const std::u16string w = wordOf(s, toks[i + 1]);
            const std::u16string subj =
                w == u"me" ? u"I" : w == u"us" ? u"we"
                : w == u"him" ? u"he" : w == u"her" ? u"she"
                : w == u"them" ? u"they" : u"";
            if (subj.empty()) continue;
            m.start = toks[i + 1].token.start;
            m.length = static_cast<int>(subj.size());
            m.fix = w;
            m.line = replaceLine(s, toks[i + 1], subj);
            have = true;
            ++byOp[4];
        }

        if (!have) continue;
        ++applied;

        SCOPED_TRACE(seed + "  (mutated: " + std::string(m.line.begin(), m.line.end()) + ")");
        const auto issues = checkAll(m.line);
        ASSERT_EQ(issues.size(), 1u) << "exactly one issue expected";
        EXPECT_EQ(issues[0].start, m.start);
        EXPECT_EQ(issues[0].length, m.length);
        ASSERT_FALSE(issues[0].suggestions.empty());
        EXPECT_EQ(issues[0].suggestions[0].text, m.fix);
    }
EXPECT_GT(applied, 30u);
    for (int i = 0; i < 5; ++i)
        EXPECT_GT(byOp[i], 0u) << opName[i] << " never anchored; corpus coverage gap";
}
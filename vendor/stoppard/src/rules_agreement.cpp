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
// R3: subject-verb agreement.
// - Adjacent: subject Pronoun + Verb. {I,you,we,they}+3ps -> Base;
//   {he,she,it}+Base -> Third (skip "it"+Base: object-case ambiguity).
// - Across auxiliaries: subject Pronoun + (Modal|Aux|Neg)*; the first
//   inflecting auxiliary must agree with the subject ("I has been" -> "have").
// - Across PP modifiers: NP head Noun + (Prep/Noun/Adverb)* + is/are/was/were;
//   collective-noun and always-plural tables (§14.3).
#include "rules_agreement.h"

#include <algorithm>
#include <array>

#include "morphology.h"

namespace stoppard {
namespace {

using std::u16string_view;

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

Issue makeIssue(const Analysis &a, const TaggedToken &t, u16string_view message,
                std::u16string suggestion)
{
    Issue it;
    it.start = t.token.start;
    it.length = t.token.length;
    it.message = std::u16string(message);
    it.suggestions.push_back(
        {SuggestionKind::Replace, matchCase(wordText(a, t), std::move(suggestion))});
    return it;
}

int pronounPerson(const TaggedToken &t)
{
    if (t.pos != PosTag::Pronoun || t.pcase != PronounCase::Subject) return 0;
    return t.person;   // 1..3; 0 = you
}

bool takesBase(const TaggedToken &t)
{
    // I/you/we/they take the base; only 3rd-person singular takes 3ps.
    return !(pronounPerson(t) == 3 && t.number == -1);
}

// Does this inflecting auxiliary agree with the subject?
bool auxAgrees(const Analysis &a, const TaggedToken &aux, int person, int number)
{
    const u16string_view w = wordText(a, aux);
    const bool thirdSing = person == 3 && number == -1;
    switch (aux.auxVerb) {
    case AuxVerb::Be:
        if (w == u"am") return person == 1 && number == -1;
        if (w == u"is") return thirdSing;
        if (w == u"are") return !thirdSing;
        if (w == u"was") return (person == 1 || person == 3) && number == -1;
        if (w == u"were") return !((person == 1 || person == 3) && number == -1);
        return true;                    // be/been/being: uninflected
    case AuxVerb::Have:
        if (w == u"had") return true;           // past: uninflected
        return w == u"has" ? thirdSing : !thirdSing;
    case AuxVerb::Do:
        if (w == u"did") return true;           // past: uninflected
        return w == u"does" ? thirdSing : !thirdSing;
    }
    return true;
}

// Correct inflecting form for this subject.
u16string_view auxFor(int person, int number, u16string_view currentWord)
{
    const bool thirdSing = person == 3 && number == -1;
    if (currentWord == u"was" || currentWord == u"were")
        return thirdSing ? u"was" : u"were";
    if (currentWord == u"is" || currentWord == u"are" || currentWord == u"am")
        return thirdSing ? u"is" : u"are";
    if (currentWord == u"has" || currentWord == u"have")
        return thirdSing ? u"has" : u"have";
    return thirdSing ? u"does" : u"do";
}

constexpr std::array<u16string_view, 24> kCollective = {
    u"team", u"government", u"committee", u"staff", u"family", u"audience",
    u"crowd", u"public", u"board", u"council", u"press", u"army", u"navy",
    u"club", u"company", u"firm", u"group", u"union", u"jury", u"parliament",
    u"class", u"orchestra", u"choir", u"faculty",
};
constexpr std::array<u16string_view, 8> kAlwaysPlural = {
    u"police", u"cattle", u"clergy", u"people", u"vermin", u"poultry",
    u"militia", u"gentry",
};

bool isCollective(u16string_view w)
{
    return std::find(kCollective.begin(), kCollective.end(), w) != kCollective.end();
}
bool isAlwaysPlural(u16string_view w)
{
    return std::find(kAlwaysPlural.begin(), kAlwaysPlural.end(), w) != kAlwaysPlural.end();
}

class RulesAgreement final : public Rule {
public:
    RulesAgreement() : Rule{u"R3", u"subject-verb agreement"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        adjacent(a, toks, out);
        acrossAuxiliaries(a, toks, out);
        acrossPPModifiers(a, toks, a.chunks, out);
    }

private:
    static void adjacent(const Analysis &a, const std::vector<TaggedToken> &toks,
                         std::vector<Issue> &out)
    {
        for (size_t i = 0; i + 1 < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Pronoun || toks[i].pcase != PronounCase::Subject)
                continue;
            if (toks[i + 1].pos != PosTag::Verb) continue;
            const auto &subj = toks[i];
            const auto &verb = toks[i + 1];
            if (verb.verbForm == VerbForm::ThirdSingular && takesBase(subj)) {
                out.push_back(makeIssue(a, verb, u"The verb must agree with the subject.", verb.lemma));
            } else if (verb.verbForm == VerbForm::Base && pronounPerson(subj) == 3
                       && wordText(a, subj) != u"it") {
                const VerbForms vf = lookupVerbForms(verb.lemma);
                out.push_back(makeIssue(a, verb, u"The verb must agree with the subject.", vf.third));
            } else if (verb.verbForm == VerbForm::PastParticiple && pronounPerson(subj) == 3
                       && wordText(a, subj) != u"it") {
                // run/cut/hit: participle spelling == base, but the token could
                // be a base ("she run fast"). Only when the past differs ("she
                // set the table" stays clean: set == past).
                const VerbForms vf = lookupVerbForms(verb.lemma);
                if (vf.past != wordText(a, verb))
                    out.push_back(makeIssue(a, verb, u"The verb must agree with the subject.", vf.third));
            }
        }
    }

    static void acrossAuxiliaries(const Analysis &a, const std::vector<TaggedToken> &toks,
                                  std::vector<Issue> &out)
    {
        for (size_t i = 0; i < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Pronoun || toks[i].pcase != PronounCase::Subject)
                continue;
            // First inflecting auxiliary in the Modal/Aux/Neg run. A modal
            // governs the base form ("She will do it."), so when the run
            // contains one the auxiliary after it is not subject-inflected;
            // modal+verb is R1's domain.
            size_t k = i + 1;
            bool sawModal = false;
            while (k < toks.size()
                   && (toks[k].pos == PosTag::Modal || toks[k].pos == PosTag::Negator)) {
                if (toks[k].pos == PosTag::Modal) sawModal = true;
                ++k;
            }
            if (k >= toks.size() || toks[k].pos != PosTag::Auxiliary || sawModal) continue;
            const auto &aux = toks[k];
            if (aux.auxForm != AuxForm::Base && aux.auxForm != AuxForm::ThirdSingular
                && aux.auxForm != AuxForm::Past)
                continue;
            if (auxAgrees(a, aux, toks[i].person, toks[i].number)) continue;
            out.push_back(makeIssue(a, aux, u"The verb must agree with the subject.",
                                    std::u16string(auxFor(toks[i].person, toks[i].number, wordText(a, aux)))));
        }
    }

    static void acrossPPModifiers(const Analysis &a, const std::vector<TaggedToken> &toks,
                                  const std::vector<Chunk> &chunks, std::vector<Issue> &out)
    {
        // Expletive "There" subjects are inert in v1.
        if (!toks.empty() && toks[0].pos == PosTag::Unknown
            && wordText(a, toks[0]) == u"There")
            return;

        for (const Chunk &c : chunks) {
            if (c.kind != ChunkKind::NP) continue;
            const auto &head = toks[c.endToken];
            if (head.pos != PosTag::Noun) continue;
            // An NP inside a PP is a complement, not a subject: "The men in
            // the room are talking." must not match "room" against "are".
            bool insidePP = false;
            for (const Chunk &p : chunks)
                if (p.kind == ChunkKind::PP && c.endToken >= p.startToken
                    && c.endToken <= p.endToken) {
                    insidePP = true;
                    break;
                }
            if (insidePP) continue;
            const u16string_view headText = wordText(a, head);

            // Scan forward to is/are/was/were; only Prep/Noun/Adverb and the
            // Determiner(+noun) of the prepositional NP in between.
            size_t j = c.endToken + 1;
            bool foundBe = false;
            while (j < toks.size()) {
                const auto &t = toks[j];
                if (t.pos == PosTag::Preposition || t.pos == PosTag::Noun
                    || t.pos == PosTag::Adverb || t.pos == PosTag::Determiner) {
                    ++j;
                    continue;
                }
                if (t.pos == PosTag::Auxiliary && t.auxVerb == AuxVerb::Be) {
                    // "are" is AuxForm::Base in the lexicon, so match by word.
                    const u16string_view w = wordText(a, t);
                    if (w == u"is" || w == u"are" || w == u"was" || w == u"were") {
                        foundBe = true;
                        break;
                    }
                }
                break;   // anything else (conjunction, determiner, ...) -> skip
            }
            if (!foundBe) continue;
            const auto &verb = toks[j];
            const u16string_view vw = wordText(a, verb);
            const bool pluralVerb = (vw == u"are" || vw == u"were");

            if (isAlwaysPlural(headText)) {
                if (!pluralVerb)
                    out.push_back(makeIssue(a, verb, u"The verb must agree with the subject.",
                                            vw == u"was" ? u"were" : u"are"));
                continue;
            }
            if (head.number == 0) continue;
            if (isCollective(headText)) {
                if (pluralVerb && !a.profile.collectivePluralAgreement)
                    out.push_back(makeIssue(a, verb, u"The verb must agree with the subject.",
                                            vw == u"were" ? u"was" : u"is"));
                continue;
            }
            const bool pluralHead = head.number == +1;
            if (pluralVerb == pluralHead) continue;   // agrees
            out.push_back(makeIssue(a, verb, u"The verb must agree with the subject.",
                                    pluralHead ? (vw == u"was" ? u"were" : u"are")
                                               : (vw == u"were" ? u"was" : u"is")));
        }
    }
};

} // namespace

const Rule& agreementRule()
{
    static const RulesAgreement rule;
    return rule;
}

} // namespace stoppard

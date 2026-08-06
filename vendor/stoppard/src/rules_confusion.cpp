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
// R12: word-confusion (SPEC §17). The closed-lexicon member of a homophone
// pair is flagged when it occurs in a text slot that provably requires its
// sibling: "I like swimming to." -> "too", "its going to rain" -> "it's",
// "your going to be late" -> "you're", "their is a problem" -> "there",
// "bigger then him" -> "than", "I want too eat." -> "to". Each entry carries
// a guard predicate that encodes the provable slot (tag context + sentence
// position + closed-table content-word POS signals from pos.h).
//
// Rows that need the sibling to be a known content word are powered by
// src/pos.h (v1): too->to (base-verb complement), it's->its / who's->whose
// (contraction + possessed noun), mid-sentence to->too (degree word).
#include "rules_confusion.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "morphology.h"
#include "pos.h"

namespace stoppard {
namespace {

using std::u16string_view;

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

std::u16string folded(u16string_view w)
{
    std::u16string out;
    out.reserve(w.size());
    for (char16_t c : w)
        out.push_back(c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c);
    return out;
}

bool endsWithEr(u16string_view w)
{
    return w.size() >= 2 && (w[w.size() - 2] == u'e' || w[w.size() - 2] == u'E')
        && (w[w.size() - 1] == u'r' || w[w.size() - 1] == u'R');
}

// to -> too: sentence-final "to" after a gerund. Excludes the elliptical
// infinitive ("I want to.", "I'm going to.") where "to" is a stranded
// infinitive marker: the token before the gerund is an auxiliary or modal,
// or the gerund is "going" (future marker).
bool sentenceFinalTo(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (toks[i].pos != PosTag::Preposition) return false;
    if (folded(wordText(a, toks[i])) != u"to") return false;
    if (i + 1 < toks.size() && toks[i + 1].pos != PosTag::Punctuation)
        return false;   // must be sentence-final
    if (i == 0) return false;
    const auto &prev = toks[i - 1];
    if (prev.pos != PosTag::Verb || prev.verbForm != VerbForm::Gerund) return false;
    // "going to" as a future marker stays clean ("I'm going to.").
    if (folded(wordText(a, prev)) == u"going") return false;
    if (i >= 2) {
        const auto &prev2 = toks[i - 2];
        if (prev2.pos == PosTag::Auxiliary || prev2.pos == PosTag::Modal)
            return false;   // "I am trying to." — elliptical, keep clean
    }
    return true;
}

// its -> it's: "its" before a Verb or be/do/have auxiliary. Non-gerunds fire
// directly ("its been a while" — been is an Auxiliary). A gerund (Verb or
// Auxiliary form, e.g. being/having) only fires when it heads the going-to
// future ("its going to rain"): the possessive gerund is legitimate ("its
// being here surprised me"), and "its going to the store" (PP) stays clean
// via the Determiner check on the word after "to".
bool itsBeforeVerbAux(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (i + 1 >= toks.size()) return false;
    const auto &next = toks[i + 1];
    if (next.pos != PosTag::Verb && next.pos != PosTag::Auxiliary) return false;
    const bool gerund = next.pos == PosTag::Verb
        ? next.verbForm == VerbForm::Gerund
        : next.auxForm == AuxForm::Gerund;
    if (!gerund) return true;
    if (i + 2 >= toks.size()) return false;
    if (folded(wordText(a, toks[i + 2])) != u"to") return false;
    return i + 3 >= toks.size() || toks[i + 3].pos != PosTag::Determiner;
}

// your -> you're: "your" + gerund + "to" + non-Determiner. The determiner
// check keeps the possessive gerund clean ("I appreciate your going to the
// store" — "to the" is a PP) while firing on the going-to future ("your
// going to be late" — "to be").
bool yourBeforeGerundTo(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (i + 2 >= toks.size()) return false;
    const auto &gerund = toks[i + 1];
    if (gerund.pos != PosTag::Verb || gerund.verbForm != VerbForm::Gerund)
        return false;
    if (folded(wordText(a, toks[i + 2])) != u"to") return false;
    if (i + 3 < toks.size() && toks[i + 3].pos == PosTag::Determiner)
        return false;   // "going to the store" — possessive gerund, keep clean
    return true;
}

// their -> there: "their" before be/aux/modal ("their is a problem").
// Gerund auxiliaries are excluded: "their being here matters" and "their
// having left" are legitimate possessive gerunds.
bool theirBeforeBeAuxModal(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (i + 1 >= toks.size()) return false;
    const auto &next = toks[i + 1];
    if (next.pos == PosTag::Modal) return true;
    return next.pos == PosTag::Auxiliary && next.auxForm != AuxForm::Gerund;
}

// then -> than: comparative + "then" + Pronoun/Determiner. The Pronoun/
// Determiner requirement separates comparative ("bigger then him") from a
// content-noun "-er" word followed by a clause ("water then sleep" stays
// clean). "more"/"less" are unambiguous (a comma would break adjacency).
bool thenAfterComparative(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (i == 0 || i + 1 >= toks.size()) return false;
    const auto &prev = toks[i - 1];
    const std::u16string prevWord = folded(wordText(a, prev));
    const bool strong = prevWord == u"more" || prevWord == u"less";
    const bool erForm = endsWithEr(prevWord);
    if (!strong && !erForm) return false;
    if (erForm && !strong) {
        const auto &next = toks[i + 1];
        if (next.pos != PosTag::Pronoun && next.pos != PosTag::Determiner)
            return false;
    }
    return true;
}

// too -> to: "too" + base verb, where the verb is a to-infinitive complement
// of the preceding word ("I want too eat."). Both ends are closed-table
// signals (pos::isInfinitiveTakingVerb / pos::isBaseVerb), never Unknown
// tags, so the over-tagging of misspellings can't trigger it.
bool tooBeforeBaseVerb(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (toks[i].pos != PosTag::Adverb) return false;
    if (folded(wordText(a, toks[i])) != u"too") return false;
    if (i == 0 || i + 1 >= toks.size()) return false;
    const auto &prev = toks[i - 1];
    if (prev.token.kind != TokenKind::Word) return false;
    // Resolve inflected forms ("plans", "hopes") to their lemma so the
    // closed infinitive-taking table matches. A base verb uses its surface
    // form directly: classifyVerbForm over-eagerly reads "need" as the past
    // "ne" (any -ed ending).
    const std::u16string prevWord = folded(wordText(a, prev));
    const std::u16string prevKey = pos::isBaseVerb(prevWord)
        ? prevWord
        : (classifyVerbForm(prevWord).known ? classifyVerbForm(prevWord).lemma : prevWord);
    if (!pos::isInfinitiveTakingVerb(prevKey)) return false;
    const auto &next = toks[i + 1];
    if (next.token.kind != TokenKind::Word) return false;
    return pos::isBaseVerb(folded(wordText(a, next)));
}

// it's -> its / who's -> whose: the contraction stem (it/who) + "'s" when the
// word after the contraction is a possessed noun (closed-table signal). The
// exclusion is fromLexicon, not the heuristic pos: after a subject pronoun +
// auxiliary "'s" the tagger's verbCtx over-tags the possessed noun as a Verb
// ("it's tail" -> tail=Verb), so a pos-based skip would reject the legitimate
// possessive. fromLexicon exactly separates the closed-class copula
// continuations ("it's been", "it's a", "it's my", "it's five", "who's there")
// from content words; the that/which/who skip excludes clefts ("it's value
// that matters"). Mirrors harper's open false-positive bugs ("It's time to
// go.", "It's been stuck in my head.") plus Merriam-Webster/Grammarly's
// possessive-determiner semantics (its + noun).
bool contractionBeforePossessedNoun(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (i + 1 >= toks.size() || folded(wordText(a, toks[i + 1])) != u"'s")
        return false;
    if (i + 2 >= toks.size()) return false;
    const auto &noun = toks[i + 2];
    if (noun.token.kind != TokenKind::Word) return false;
    if (noun.fromLexicon) return false;   // "it's been / a / my / five / there"
    if (!pos::isPossessedNoun(folded(wordText(a, noun)))) return false;
    if (i + 3 < toks.size()) {
        const std::u16string nxt = folded(wordText(a, toks[i + 3]));
        if (nxt == u"that" || nxt == u"which" || nxt == u"who") return false;
    }
    return true;
}

// to -> too (degree words): "to" + degree word (many/much/few/little) heading
// a phrase, e.g. "I ate to many cookies." Both ends are closed-table signals
// (pos::isDegreeWord / the noun-head requirement); the to-taking-verb guard
// keeps prepositional verbs clean ("give to many charities", "connected to
// many servers", "listen to much music" — harper's documented FP sources).
bool toBeforeDegreeWord(const Analysis &a, size_t i)
{
    const auto &toks = a.tokens;
    if (toks[i].pos != PosTag::Preposition) return false;
    if (folded(wordText(a, toks[i])) != u"to") return false;
    if (i + 2 >= toks.size()) return false;
    const auto &deg = toks[i + 1];
    if (deg.token.kind != TokenKind::Word) return false;
    if (!pos::isDegreeWord(folded(wordText(a, deg)))) return false;
    // The degree word must head a phrase: a following noun-ish word, not a
    // verb or clause boundary.
    const auto &head = toks[i + 2];
    if (head.token.kind != TokenKind::Word) return false;
    switch (head.pos) {
    case PosTag::Verb: case PosTag::Auxiliary: case PosTag::Modal:
    case PosTag::Conjunction: case PosTag::Preposition:
    case PosTag::Punctuation:
        return false;
    default: break;
    }
    // "to" must not be governed by a to-taking/prepositional verb.
    if (i >= 1 && toks[i - 1].token.kind == TokenKind::Word) {
        const std::u16string prevWord = folded(wordText(a, toks[i - 1]));
        const std::u16string prevKey = pos::isBaseVerb(prevWord)
            ? prevWord
            : (classifyVerbForm(prevWord).known ? classifyVerbForm(prevWord).lemma : prevWord);
        if (pos::isToTakingVerb(prevKey)) return false;
    }
    return true;
}

struct Entry {
    u16string_view wrong;
    u16string_view correction;
    bool (*guard)(const Analysis &, size_t);
    u16string_view message;
};

constexpr std::array<Entry, 7> kEntries = {{
    {u"to", u"too", sentenceFinalTo, u"Did you mean \"too\" (also)?"},
    {u"to", u"too", toBeforeDegreeWord, u"Did you mean \"too\" (also)?"},
    {u"too", u"to", tooBeforeBaseVerb, u"Did you mean \"to\"?"},
    {u"its", u"it's", itsBeforeVerbAux, u"Did you mean \"it's\" (it is)?"},
    {u"your", u"you're", yourBeforeGerundTo, u"Did you mean \"you're\" (you are)?"},
    {u"their", u"there", theirBeforeBeAuxModal, u"Did you mean \"there\"?"},
    {u"then", u"than", thenAfterComparative, u"Did you mean \"than\"?"},
}};

struct ContractionEntry {
    u16string_view stem;
    u16string_view correction;
    bool (*guard)(const Analysis &, size_t);
    u16string_view message;
};

constexpr std::array<ContractionEntry, 2> kContractionEntries = {{
    {u"it", u"its", contractionBeforePossessedNoun, u"Did you mean \"its\" (possessive)?"},
    {u"who", u"whose", contractionBeforePossessedNoun, u"Did you mean \"whose\"?"},
}};

class RulesConfusion final : public Rule {
public:
    RulesConfusion() : Rule{u"R12", u"word-confusion"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (const auto &entry : kEntries) {
            for (size_t i = 0; i < toks.size(); ++i) {
                if (folded(wordText(a, toks[i])) != entry.wrong) continue;
                if (!entry.guard(a, i)) continue;
                const TaggedToken &t = toks[i];
                const std::u16string correction =
                    matchCase(wordText(a, t), std::u16string(entry.correction));
                out.push_back({t.token.start, t.token.length,
                               std::u16string(entry.message),
                               {{SuggestionKind::Replace, correction}}});
            }
        }
        // Contraction pairs ("it's", "who's") span two tokens; emit one issue
        // covering stem + "'s" with the sibling correction.
        for (const auto &entry : kContractionEntries) {
            for (size_t i = 0; i + 1 < toks.size(); ++i) {
                if (folded(wordText(a, toks[i])) != entry.stem) continue;
                if (folded(wordText(a, toks[i + 1])) != u"'s") continue;
                if (!entry.guard(a, i)) continue;
                const int start = toks[i].token.start;
                const int length = toks[i + 1].token.start
                    + toks[i + 1].token.length - start;
                const std::u16string correction =
                    matchCase(a.text.substr(start, length), std::u16string(entry.correction));
                out.push_back({start, length, std::u16string(entry.message),
                               {{SuggestionKind::Replace, correction}}});
            }
        }
    }
};

} // namespace

const Rule& confusionRule()
{
    static const RulesConfusion rule;
    return rule;
}

} // namespace stoppard

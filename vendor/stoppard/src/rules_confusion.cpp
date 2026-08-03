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
// "bigger then him" -> "than". Each entry carries a guard predicate that
// encodes the provable slot (tag context + sentence position).
//
// Rows deferred to M9 (dictionary pass — the sibling must be known to be a
// noun/verb before firing): too->to, it's->its, who's->whose.
#include "rules_confusion.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "morphology.h"

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

struct Entry {
    u16string_view wrong;
    u16string_view correction;
    bool (*guard)(const Analysis &, size_t);
    u16string_view message;
};

constexpr std::array<Entry, 5> kEntries = {{
    {u"to", u"too", sentenceFinalTo, u"Did you mean \"too\" (also)?"},
    {u"its", u"it's", itsBeforeVerbAux, u"Did you mean \"it's\" (it is)?"},
    {u"your", u"you're", yourBeforeGerundTo, u"Did you mean \"you're\" (you are)?"},
    {u"their", u"there", theirBeforeBeAuxModal, u"Did you mean \"there\"?"},
    {u"then", u"than", thenAfterComparative, u"Did you mean \"than\"?"},
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
    }
};

} // namespace

const Rule& confusionRule()
{
    static const RulesConfusion rule;
    return rule;
}

} // namespace stoppard

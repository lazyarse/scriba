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
// R2: auxiliary-driven verb forms.
// - (do|does|did) [neg]* [Verb]  -> non-Base flagged, suggest Base. Auxiliary
//   after do skipped (inversion, precision).
// - (have|has|had) [neg]* [Verb] -> non-participle flagged; "have to"
//   obligation and "have got" idioms are guarded. "has been [Verb]" requires a
//   gerund after been.
// - (am|is|are|was|were) [Verb|Auxiliary] -> 3ps/Past with participle!=token
//   flagged, suggest Gerund ("I am has" -> "I am having").
// Only confident tags fire; Unknown verbs never flagged.
#include "rules_aux.h"

#include <algorithm>
#include <array>

#include "morphology.h"

namespace stoppard {
namespace {

using std::u16string_view;

// "been there/here/home": bare words tag as Verb Base after an auxiliary;
// these common adverbs must not trigger the gerund rule.
constexpr std::array<u16string_view, 8> kBeenAdverbs = {
    u"there", u"here", u"home", u"away", u"back", u"upstairs", u"downstairs", u"inside",
};

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

VerbForm formOf(const TaggedToken &t)
{
    if (t.pos == PosTag::Verb) return t.verbForm;
    if (t.pos == PosTag::Auxiliary) {
        switch (t.auxForm) {
        case AuxForm::Base: return VerbForm::Base;
        case AuxForm::ThirdSingular: return VerbForm::ThirdSingular;
        case AuxForm::Past: return VerbForm::Past;
        case AuxForm::PastParticiple: return VerbForm::PastParticiple;
        case AuxForm::Gerund: return VerbForm::Gerund;
        }
    }
    return VerbForm::Base;
}

std::u16string lemmaOf(const TaggedToken &t)
{
    if (t.pos == PosTag::Auxiliary) {
        switch (t.auxVerb) {
        case AuxVerb::Be: return u"be";
        case AuxVerb::Have: return u"have";
        case AuxVerb::Do: return u"do";
        }
    }
    return t.lemma;
}

Issue makeIssue(const Analysis &a, const TaggedToken &t, std::u16string_view message,
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

// Next content token after i, skipping Negator/Adverb. Null index on end.
size_t nextContent(const std::vector<TaggedToken> &toks, size_t i)
{
    size_t j = i + 1;
    while (j < toks.size()
           && (toks[j].pos == PosTag::Negator || toks[j].pos == PosTag::Adverb))
        ++j;
    return j;
}

class RulesAux final : public Rule {
public:
    RulesAux() : Rule{u"R2", u"auxiliary requires matching verb form"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Auxiliary) continue;
            const size_t j = nextContent(toks, i);
            if (j >= toks.size()) continue;
            const auto &aux = toks[i];
            const auto &verb = toks[j];

            switch (aux.auxVerb) {
            case AuxVerb::Do: {
                if (verb.pos == PosTag::Auxiliary) continue;   // inversion: precision
                if (verb.pos != PosTag::Verb) continue;
                if (verb.verbForm != VerbForm::Base)
                    out.push_back(makeIssue(a, verb, u"After an auxiliary, use the base form of the verb.", verb.lemma));
                break;
            }
            case AuxVerb::Have: {
                if (verb.pos == PosTag::Auxiliary) {
                    // "has been X": been requires a gerund after it.
                    if (verb.auxVerb == AuxVerb::Be && verb.auxForm == AuxForm::PastParticiple) {
                        const size_t j2 = nextContent(toks, j);
                        if (j2 < toks.size() && toks[j2].pos == PosTag::Verb
                            && (toks[j2].verbForm == VerbForm::Base
                                || toks[j2].verbForm == VerbForm::ThirdSingular)
                            && std::find(kBeenAdverbs.begin(), kBeenAdverbs.end(),
                                         toks[j2].lemma) == kBeenAdverbs.end()) {
                            const VerbForms vf = lookupVerbForms(toks[j2].lemma);
                            out.push_back(makeIssue(a, toks[j2], u"Auxiliary \"been\" takes the gerund of the verb.", vf.gerund));
                        }
                    }
                    continue;
                }
                if (verb.pos != PosTag::Verb) continue;
                if (wordText(a, verb) == u"to") continue;       // obligation idiom
                if (verb.lemma == u"get") continue;             // have-got idiom
                const VerbForms vf = lookupVerbForms(verb.lemma);
                if (!vf.known) continue;
                if (verb.verbForm == VerbForm::PastParticiple) continue;
                if (verb.verbForm == VerbForm::Past && vf.participle == wordText(a, verb))
                    continue;
                out.push_back(makeIssue(a, verb, u"Auxiliary \"have\" takes the past participle of the verb.", vf.participle));
                break;
            }
            case AuxVerb::Be: {
                if (verb.pos != PosTag::Verb && verb.pos != PosTag::Auxiliary) continue;
                const VerbForm f = formOf(verb);
                if (f == VerbForm::ThirdSingular || f == VerbForm::Past) {
                    const VerbForms vf = lookupVerbForms(lemmaOf(verb));
                    if (vf.known && vf.participle != wordText(a, verb))
                        out.push_back(makeIssue(a, verb, u"Auxiliary \"be\" takes the gerund of the verb.", vf.gerund));
                }
                break;
            }
            }
        }
    }
};

} // namespace

const Rule& auxiliaryVerbFormRule()
{
    static const RulesAux rule;
    return rule;
}

} // namespace stoppard

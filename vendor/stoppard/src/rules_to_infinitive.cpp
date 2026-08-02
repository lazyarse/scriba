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
// R4: "to" + verb. "to" takes the base form: "I want to has" -> "to have".
// Gerund/Base/participle after "to" are clean.
#include "rules_to_infinitive.h"

#include "morphology.h"

namespace stoppard {
namespace {

using std::u16string_view;

class RulesToInfinitive final : public Rule {
public:
    RulesToInfinitive() : Rule{u"R4", u"to takes the base verb form"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i + 1 < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Preposition
                || a.text.substr(toks[i].token.start, toks[i].token.length) != u"to")
                continue;
            const auto &verb = toks[i + 1];
            if (verb.pos != PosTag::Verb && verb.pos != PosTag::Auxiliary) continue;
            const VerbForm f = verb.pos == PosTag::Verb ? verb.verbForm : [&]() {
                switch (verb.auxForm) {
                case AuxForm::Base: return VerbForm::Base;
                case AuxForm::ThirdSingular: return VerbForm::ThirdSingular;
                case AuxForm::Past: return VerbForm::Past;
                case AuxForm::PastParticiple: return VerbForm::PastParticiple;
                case AuxForm::Gerund: return VerbForm::Gerund;
                }
                return VerbForm::Base;
            }();
            if (f != VerbForm::ThirdSingular && f != VerbForm::Past) continue;
            std::u16string base;
            if (verb.pos == PosTag::Auxiliary) {
                switch (verb.auxVerb) {
                case AuxVerb::Be: base = u"be"; break;
                case AuxVerb::Have: base = u"have"; break;
                case AuxVerb::Do: base = u"do"; break;
                }
            } else {
                base = verb.lemma;
            }
            const u16string_view src = a.text.substr(verb.token.start, verb.token.length);
            Issue it;
            it.start = verb.token.start;
            it.length = verb.token.length;
            it.message = u"The preposition \"to\" takes the base form of the verb.";
            it.suggestions.push_back({SuggestionKind::Replace, matchCase(src, std::move(base))});
            out.push_back(it);
        }
    }
};

} // namespace

const Rule& toInfinitiveRule()
{
    static const RulesToInfinitive rule;
    return rule;
}

} // namespace stoppard

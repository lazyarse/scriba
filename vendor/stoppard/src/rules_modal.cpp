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
// R1: after a modal, the next content verb must be the base form. "I can has"
// -> "I can have". Non-base auxiliaries after a modal are flagged too
// ("can has been" -> "can have been"). Suggestions match-case the source.
#include "rules_modal.h"

#include "morphology.h"

namespace stoppard {
namespace {

bool isNonBaseVerb(const TaggedToken &t)
{
    if (t.pos == PosTag::Verb)
        return t.verbForm != VerbForm::Base;
    if (t.pos == PosTag::Auxiliary)
        return t.auxForm != AuxForm::Base;   // 3ps/past/participle/gerund
    return false;
}

std::u16string baseForm(const TaggedToken &t)
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

class RulesModal final : public Rule {
public:
    RulesModal() : Rule{u"R1", u"modal requires base verb form"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Modal) continue;
            size_t j = i + 1;
            while (j < toks.size()
                   && (toks[j].pos == PosTag::Adverb || toks[j].pos == PosTag::Negator))
                ++j;
            if (j >= toks.size() || !isNonBaseVerb(toks[j])) continue;
            const std::u16string_view src = a.text.substr(
                toks[j].token.start, toks[j].token.length);
            Issue it;
            it.start = toks[j].token.start;
            it.length = toks[j].token.length;
            it.message = u"The form of the verb must agree with the modal.";
            it.suggestions.push_back({SuggestionKind::Replace,
                                      matchCase(src, baseForm(toks[j]))});
            out.push_back(it);
        }
    }
};

} // namespace

const Rule& modalVerbFormRule()
{
    static const RulesModal rule;
    return rule;
}

} // namespace stoppard

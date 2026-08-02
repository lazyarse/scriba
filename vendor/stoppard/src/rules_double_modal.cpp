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
// R7: two modals in a row ("I can could go"). The second is redundant;
// the fix is to remove it (spec §7).
#include "rules_double_modal.h"

namespace stoppard {
namespace {

class RulesDoubleModal final : public Rule {
public:
    RulesDoubleModal() : Rule{u"R7", u"double modal"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i + 1 < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Modal) continue;
            size_t j = i + 1;
            while (j < toks.size() && toks[j].pos == PosTag::Negator) ++j;
            if (j < toks.size() && toks[j].pos == PosTag::Modal) {
                out.push_back({toks[j].token.start, toks[j].token.length,
                               u"Two modal verbs in a row.",
                               {{SuggestionKind::Remove, u""}}});
            }
        }
    }
};

} // namespace

const Rule& doubleModalRule()
{
    static const RulesDoubleModal rule;
    return rule;
}

} // namespace stoppard

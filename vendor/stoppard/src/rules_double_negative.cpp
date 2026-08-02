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
// R8: a clause negated twice ("I don't never go"). Only fires when the
// first negation is the explicit "not"/"n't" and the second is a negative
// word, within 3 tokens, without a clause boundary between.
#include "rules_double_negative.h"

#include <algorithm>
#include <array>
#include <string>

namespace stoppard {
namespace {

using std::u16string_view;

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

struct NegativeWord { u16string_view word; u16string_view positive; };
constexpr std::array<NegativeWord, 6> kNegatives = {{
    {u"never", u"ever"}, {u"no", u"any"}, {u"none", u"any"},
    {u"nobody", u"anybody"}, {u"nothing", u"anything"}, {u"nowhere", u"anywhere"},
}};

// "no doubt/way/longer/one" are fixed expressions, not double negatives.
constexpr std::array<u16string_view, 4> kNoGuards = {
    u"doubt", u"way", u"longer", u"one",
};

class RulesDoubleNegative final : public Rule {
public:
    RulesDoubleNegative() : Rule{u"R8", u"double negative"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i < toks.size(); ++i) {
            const u16string_view first = wordText(a, toks[i]);
            if (first != u"n't" && first != u"not") continue;
            if (toks[i].pos != PosTag::Negator) continue;
            for (size_t j = i + 1; j < toks.size() && j <= i + 3; ++j) {
                const auto &t = toks[j];
                if (t.pos == PosTag::Conjunction || t.pos == PosTag::Punctuation)
                    break;
                const u16string_view w = wordText(a, t);
                const auto *neg = std::find_if(
                    kNegatives.begin(), kNegatives.end(),
                    [w](const NegativeWord &e) { return e.word == w; });
                if (neg == kNegatives.end()) continue;
                if (w == u"no") {
                    // "no doubt/way/longer/one" (adjacent) are fixed phrases.
                    if (j + 1 < toks.size()
                        && std::find(kNoGuards.begin(), kNoGuards.end(),
                                     wordText(a, toks[j + 1])) != kNoGuards.end())
                        break;
                }
                out.push_back({t.token.start, t.token.length,
                               u"Only one negative is allowed in a clause.",
                               {{SuggestionKind::Replace, std::u16string(neg->positive)}}});
                break;
            }
        }
    }
};

} // namespace

const Rule& doubleNegativeRule()
{
    static const RulesDoubleNegative rule;
    return rule;
}

} // namespace stoppard

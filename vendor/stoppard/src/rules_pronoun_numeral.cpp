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
// R13: pronoun + numeral word order (SPEC §18). A plural pronoun directly
// followed by the numeral "one" where the numeral must precede the pronoun
// with "of" between: "for us one" -> "for one of us", "we one went" ->
// "one of us went". Fires on the closed pronoun (we/us/they/them) with a
// surface-text check of the adjacent "one" (it is not in the lexicon, so
// rules never fire on it directly).
#include "rules_pronoun_numeral.h"

#include <string>
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

class RulesPronounNumeral final : public Rule {
public:
    RulesPronounNumeral() : Rule{u"R13", u"pronoun + numeral"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i + 1 < toks.size(); ++i) {
            const TaggedToken &pron = toks[i];
            if (pron.pos != PosTag::Pronoun || pron.number != +1) continue;
            const bool subject = pron.pcase == PronounCase::Subject;
            if (!subject && pron.pcase != PronounCase::Object) continue;
            // Only the four seed pronouns: we/us/they/them.
            const std::u16string w = folded(wordText(a, pron));
            if (w != u"we" && w != u"us" && w != u"they" && w != u"them") continue;

            const TaggedToken &one = toks[i + 1];
            if (folded(wordText(a, one)) != u"one") continue;

            // Conservative skip: "one" followed by a content word ("us one
            // cat" is nonsense, not a fixable error).
            if (i + 2 < toks.size() && toks[i + 2].pos == PosTag::Unknown) continue;

            // Object pronouns need a preposition before them: "for us one" is
            // the error; "Give us one." is correct English (ditransitive).
            // Sentence-initial "Us one asked" has no prev token and fires.
            if (!subject && i > 0 && toks[i - 1].pos != PosTag::Preposition)
                continue;

            const std::u16string correction = matchCase(
                wordText(a, pron),
                w == u"us" || w == u"we" ? std::u16string(u"one of us")
                                         : std::u16string(u"one of them"));
            const std::u16string message =
                u"Did you mean \"" + correction + u"\"?";
            out.push_back({pron.token.start, pron.token.length, message,
                           {{SuggestionKind::Replace, correction}}});
        }
    }
};

} // namespace

const Rule& pronounNumeralRule()
{
    static const RulesPronounNumeral rule;
    return rule;
}

} // namespace stoppard

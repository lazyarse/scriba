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
// R15: sentence-start capitalization. The first word of each sentence must
// start with an uppercase letter. Fires on lowercase word tokens at sentence
// boundaries identified by the tokenizer's splitSentences.
//
// R16: pronoun "I" capitalization. The standalone pronoun "I" must always
// be uppercase. Fires on single-character "i" tokens tagged as Pronoun.
#include "rules_capitalization.h"

namespace stoppard {
namespace {

using std::u16string_view;

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

bool isLowerCase(char16_t c)
{
    return c >= u'a' && c <= u'z';
}

// R15: Sentence-start capitalization
class RulesSentenceCapitalization final : public Rule {
public:
    RulesSentenceCapitalization() : Rule{u"R15", u"sentence-start capitalization"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        const auto &sentences = a.sentences;

        for (const auto &sent : sentences) {
            for (const auto &t : toks) {
                if (t.token.start + t.token.length <= sent.start) continue;
                if (t.token.start >= sent.start + sent.length) break;
                if (t.token.kind != TokenKind::Word) continue;
                // Precision-first: only fire on words recognised by the closed-
                // class lexicon. Heuristically tagged content words (-ing/-ed/-ly
                // -ment…) misfire on misspellings, and tagging those as sentence
                // starts would mask the spelling suggestion on the same span.
                if (!t.fromLexicon) break;

                u16string_view text = wordText(a, t);

                if (text.empty() || !isLowerCase(text[0])) break;

                std::u16string suggestion(text);
                suggestion[0] = static_cast<char16_t>(suggestion[0] - u'a' + u'A');

                Issue it;
                it.start = t.token.start;
                it.length = t.token.length;
                it.message = u"Sentence should start with a capital letter.";
                it.suggestions.push_back({SuggestionKind::Replace, std::move(suggestion)});
                out.push_back(it);
                break;
            }
        }
    }
};

} // namespace

const Rule& sentenceCapitalizationRule()
{
    static const RulesSentenceCapitalization rule;
    return rule;
}

// R16: Pronoun "I" capitalization. The first-person subject pronoun must be
// uppercase wherever it appears (not just sentence-initial).
class RulesPronounCapitalization final : public Rule {
public:
    RulesPronounCapitalization() : Rule{u"R16", u"pronoun I capitalization"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        for (const TaggedToken &t : a.tokens) {
            if (t.pos != PosTag::Pronoun) continue;
            if (wordText(a, t) != u"i") continue;

            Issue it;
            it.start = t.token.start;
            it.length = t.token.length;
            it.message = u"The pronoun \"I\" must be capitalized.";
            it.suggestions.push_back({SuggestionKind::Replace, std::u16string(u"I")});
            out.push_back(it);
        }
    }
};

const Rule& pronounCapitalizationRule()
{
    static const RulesPronounCapitalization rule;
    return rule;
}

} // namespace stoppard

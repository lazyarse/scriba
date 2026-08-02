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
// R6: pronoun case. Object-case pronouns cannot head a subject ("me went"),
// subject-case pronouns cannot follow a preposition ("to she"). Coordinated
// pronouns ("me and John") are out of scope in v1.
#include "rules_pronoun_case.h"

#include <array>
#include <string>

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

// me -> I etc. Only object-case pronouns that have a distinct subject form.
// The subject forms are always capitalized: the rule only fires when the
// pronoun heads a clause ("Me went" -> "I went").
const std::u16string& subjectForm(std::u16string_view w)
{
    static const std::array<std::pair<u16string_view, std::u16string>, 5> kMap = {{
        {u"me", u"I"}, {u"him", u"He"}, {u"her", u"She"},
        {u"us", u"We"}, {u"them", u"They"},
    }};
    const std::u16string f = folded(w);
    for (const auto &[obj, subj] : kMap)
        if (f == obj) return subj;
    static const std::u16string empty;
    return empty;
}

// I -> me etc. "it" and "you" are excluded: same form in both cases.
// Object forms are lowercase: they follow a preposition mid-clause.
const std::u16string& objectForm(std::u16string_view w)
{
    static const std::array<std::pair<u16string_view, std::u16string>, 5> kMap = {{
        {u"i", u"me"}, {u"he", u"him"}, {u"she", u"her"},
        {u"we", u"us"}, {u"they", u"them"},
    }};
    const std::u16string f = folded(w);
    for (const auto &[subj, obj] : kMap)
        if (f == subj) return obj;
    static const std::u16string empty;
    return empty;
}

size_t nextContent(const std::vector<TaggedToken> &toks, size_t i)
{
    while (i < toks.size()
           && (toks[i].pos == PosTag::Adverb || toks[i].pos == PosTag::Unknown))
        ++i;
    return i;
}

class RulesPronounCase final : public Rule {
public:
    RulesPronounCase() : Rule{u"R6", u"pronoun case"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i < toks.size(); ++i) {
            if (toks[i].pos != PosTag::Pronoun) continue;
            const u16string_view w = wordText(a, toks[i]);

            if (toks[i].pcase == PronounCase::Object) {
                // Subject position: next content token must be Verb/Auxiliary.
                // Coordination ("me and John") is out of scope.
                if (i + 1 < toks.size() && toks[i + 1].pos == PosTag::Conjunction)
                    continue;
                const size_t j = nextContent(toks, i + 1);
                if (j >= toks.size()) continue;
                if (toks[j].pos != PosTag::Verb && toks[j].pos != PosTag::Auxiliary)
                    continue;
                const std::u16string &subj = subjectForm(w);
                if (subj.empty()) continue;
                out.push_back({toks[i].token.start, toks[i].token.length,
                               u"Use the subject form of the pronoun here.",
                               {{SuggestionKind::Replace, subj}}});
            } else if (toks[i].pcase == PronounCase::Subject && w != u"it"
                       && w != u"you") {
                // After a preposition: adjacent subject-case pronoun.
                if (i == 0 || toks[i - 1].pos != PosTag::Preposition) continue;
                const std::u16string &obj = objectForm(w);
                if (obj.empty()) continue;
                out.push_back({toks[i].token.start, toks[i].token.length,
                               u"Use the object form of the pronoun after a preposition.",
                               {{SuggestionKind::Replace, obj}}});
            }
        }
    }
};

} // namespace

const Rule& pronounCaseRule()
{
    static const RulesPronounCase rule;
    return rule;
}

} // namespace stoppard

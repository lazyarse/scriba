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
// R9: dialect-specific regional expressions (SPEC §14.2). A phrase that is
// standard in one dialect ("in the cards" in AmE) is flagged in another
// ("It's on the cards" is the BrE form). The dialect profile carries the
// list of wrong phrases for the active dialect; the flag is the minimal
// edit (token-level diff) from the wrong phrase to the correction.
#include "rules_regionalisms.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "morphology.h"

namespace stoppard {
namespace {

using std::u16string_view;

struct Entry {
    u16string_view wrong;        // space-joined lowercase tokens
    u16string_view correction;
    std::array<bool, 6> wrongIn; // indexed by Dialect
};

constexpr bool kYes = true, kNo = false;
// American, British, Australian, Indian, Canadian, NewZealand
constexpr std::array<Entry, 6> kEntries = {{
    {u"in the cards", u"on the cards", {kNo, kYes, kYes, kYes, kYes, kYes}},
    {u"on the cards", u"in the cards", {kYes, kNo, kNo, kNo, kNo, kNo}},
    {u"gotten", u"got", {kNo, kYes, kYes, kYes, kYes, kYes}},   // only as get-participle
    {u"at the weekend", u"on the weekend", {kYes, kNo, kNo, kNo, kNo, kNo}},
    {u"on the weekend", u"at the weekend", {kNo, kYes, kYes, kNo, kYes, kYes}},
    {u"discuss about", u"discuss", {kNo, kNo, kNo, kYes, kNo, kNo}},
}};

// The minimal token-level edit from `wrong` to `correction` (LCS alignment).
struct Diff {
    size_t wrongStart, wrongEnd;      // wrong-token index range (inclusive)
    std::vector<std::u16string> right; // replacement tokens; empty = Remove
};

std::vector<u16string_view> splitTokens(std::u16string_view s)
{
    std::vector<u16string_view> out;
    size_t p = 0;
    while (p < s.size()) {
        const size_t q = s.find(u' ', p);
        out.push_back(s.substr(p, q == u16string_view::npos ? s.size() - p : q - p));
        if (q == u16string_view::npos) break;
        p = q + 1;
    }
    return out;
}

Diff diffPhrase(std::u16string_view wrong, std::u16string_view correction)
{
    const auto w = splitTokens(wrong);
    const auto c = splitTokens(correction);
    // LCS table.
    std::vector<std::vector<size_t>> lcs(w.size() + 1,
                                         std::vector<size_t>(c.size() + 1, 0));
    for (size_t i = 1; i <= w.size(); ++i)
        for (size_t j = 1; j <= c.size(); ++j)
            lcs[i][j] = w[i - 1] == c[j - 1]
                ? lcs[i - 1][j - 1] + 1
                : std::max(lcs[i - 1][j], lcs[i][j - 1]);
    // Walk back; mismatched wrong tokens form the flagged range.
    size_t i = w.size(), j = c.size();
    std::vector<bool> matched(w.size(), false);
    while (i > 0 && j > 0) {
        if (w[i - 1] == c[j - 1]) { matched[i - 1] = true; --i; --j; }
        else if (lcs[i - 1][j] >= lcs[i][j - 1]) --i;
        else --j;
    }
    size_t first = w.size(), last = w.size();
    for (size_t k = 0; k < w.size(); ++k)
        if (!matched[k]) { if (first == w.size()) first = k; last = k; }
    Diff d{first, last, {}};
    for (size_t k = 0; k < c.size(); ++k) {
        // Correction tokens not consumed by a match, in order.
        // Recompute alignment output: collect c-tokens that are unmatched.
    }
    // Unmatched correction tokens: walk the same backtrack and collect.
    i = w.size(); j = c.size();
    std::vector<bool> cmatched(c.size(), false);
    while (i > 0 && j > 0) {
        if (w[i - 1] == c[j - 1]) { cmatched[j - 1] = true; --i; --j; }
        else if (lcs[i - 1][j] >= lcs[i][j - 1]) --i;
        else --j;
    }
    for (size_t k = 0; k < c.size(); ++k)
        if (!cmatched[k]) d.right.push_back(std::u16string(c[k]));
    return d;
}

std::u16string folded(u16string_view w)
{
    std::u16string out;
    out.reserve(w.size());
    for (char16_t c : w)
        out.push_back(c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c);
    return out;
}

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

// "gotten" only counts as the past participle of "get", not any other word.
bool isGottenParticiple(const TaggedToken &t)
{
    return t.pos == PosTag::Verb && t.verbForm == VerbForm::PastParticiple
        && t.lemma == u"get";
}

class RulesRegionalisms final : public Rule {
public:
    RulesRegionalisms() : Rule{u"R9", u"regionalisms"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (const auto &reg : a.profile.regionalisms) {
            const auto phrase = splitTokens(reg.wrong);
            const Diff diff = diffPhrase(reg.wrong, reg.correction);
            if (phrase.empty()) continue;

            for (size_t i = 0; i + phrase.size() <= toks.size(); ++i) {
                if (folded(wordText(a, toks[i])) != phrase[0]) continue;
                bool match = true;
                for (size_t k = 1; k < phrase.size(); ++k) {
                    if (folded(wordText(a, toks[i + k])) != phrase[k]) { match = false; break; }
                }
                if (!match) continue;
                if (phrase.size() == 1 && phrase[0] == u"gotten"
                    && !isGottenParticiple(toks[i]))
                    continue;
                const auto &first = toks[i + diff.wrongStart];
                const auto &last = toks[i + diff.wrongEnd];
                std::u16string suggestionText;
                for (size_t k = 0; k < diff.right.size(); ++k) {
                    if (k) suggestionText += u' ';
                    suggestionText += diff.right[k];
                }
                if (!diff.right.empty()) {
                    const std::u16string cased =
                        matchCase(wordText(a, first), std::move(suggestionText));
                    const std::u16string message =
                        u"Regional expression: use \"" + cased + u"\" in this dialect.";
                    out.push_back({first.token.start,
                                   last.token.start + last.token.length - first.token.start,
                                   message, {{SuggestionKind::Replace, cased}}});
                } else {
                    out.push_back({first.token.start,
                                   last.token.start + last.token.length - first.token.start,
                                   u"Regional expression: not used in this dialect.",
                                   {{SuggestionKind::Remove, u""}}});
                }
                i += phrase.size() - 1;
            }
        }
    }
};

} // namespace

const Rule& regionalismsRule()
{
    static const RulesRegionalisms rule;
    return rule;
}

DialectProfile profileFor(Dialect dialect)
{
    DialectProfile profile;
    switch (dialect) {
    case Dialect::British:
    case Dialect::Australian:
    case Dialect::Indian:
    case Dialect::NewZealand:
        profile.collectivePluralAgreement = true;   // "the team are..." is standard
        break;
    default:
        break;   // US/CA: collective nouns take a singular verb
    }
    for (const Entry &e : kEntries)
        if (e.wrongIn[static_cast<int>(dialect)])
            profile.regionalisms.push_back({std::u16string(e.wrong), std::u16string(e.correction)});
    return profile;
}

} // namespace stoppard

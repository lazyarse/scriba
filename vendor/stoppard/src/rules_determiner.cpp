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
// R5: determiner-noun number agreement. Only confident tags fire: the
// determiner and noun must both carry a number, and the noun must not be a
// mass noun ("these water" is fine).
#include "rules_determiner.h"

#include <algorithm>
#include <array>
#include <string>

#include "morphology.h"

namespace stoppard {
namespace {

using std::u16string_view;

// Mass nouns take no number: "these water/rice/music" are not flagged.
constexpr std::array<u16string_view, 31> kMassNouns = {
    u"water", u"milk", u"air", u"rice", u"money", u"music", u"furniture",
    u"information", u"advice", u"homework", u"news", u"luggage", u"equipment",
    u"software", u"bread", u"butter", u"cheese", u"sugar", u"salt", u"coffee",
    u"tea", u"oil", u"sand", u"dust", u"grass", u"traffic", u"weather",
    u"knowledge", u"literature", u"research", u"evidence",
};

bool isMassNoun(u16string_view w)
{
    return std::find(kMassNouns.begin(), kMassNouns.end(), w) != kMassNouns.end();
}

u16string_view wordText(const Analysis &a, const TaggedToken &t)
{
    return a.text.substr(t.token.start, t.token.length);
}

bool isDetModifier(const TaggedToken &t)
{
    return t.pos == PosTag::Adjective || t.pos == PosTag::Unknown
        || t.pos == PosTag::Adverb;
}

class RulesDeterminer final : public Rule {
public:
    RulesDeterminer() : Rule{u"R5", u"determiner-noun agreement"} {}

    void run(const Analysis &a, std::vector<Issue> &out) const override
    {
        const auto &toks = a.tokens;
        for (size_t i = 0; i < toks.size(); ++i) {
            const auto &det = toks[i];
            if (det.pos != PosTag::Determiner || det.number == 0) continue;
            size_t j = i + 1;
            while (j < toks.size() && isDetModifier(toks[j])) ++j;
            if (j >= toks.size()) continue;
            const auto &noun = toks[j];
            if (noun.pos != PosTag::Noun || noun.number == 0) continue;
            if (isMassNoun(wordText(a, noun))) continue;
            if (det.number == noun.number) continue;   // agrees

            const u16string_view nounText = wordText(a, noun);
            std::u16string suggestion = det.number == +1
                ? pluralize(nounText)          // "These cat" -> cats
                : singularize(nounText);       // "This dogs" -> dog
            if (suggestion.empty() || suggestion == nounText) continue;
            const std::u16string message =
                u"The determiner '" + std::u16string(wordText(a, det))
                + u"' requires a " + (det.number == +1 ? u"plural" : u"singular")
                + u" noun.";
            out.push_back({static_cast<int>(noun.token.start),
                           noun.token.length, message,
                           {{SuggestionKind::Replace, std::move(suggestion)}}});
        }
    }
};

} // namespace

const Rule& determinerNounAgreementRule()
{
    static const RulesDeterminer rule;
    return rule;
}

} // namespace stoppard

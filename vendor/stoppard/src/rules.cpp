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
#include <algorithm>

#include "rules.h"
#include "rules_capitalization.h"
#include "rules_confusion.h"
#include "rules_double_modal.h"
#include "rules_double_negative.h"
#include "rules_regionalisms.h"
#include "rules_to_infinitive.h"
#include "rules_agreement.h"
#include "rules_aux.h"
#include "rules_determiner.h"
#include "rules_modal.h"
#include "rules_pronoun_case.h"
#include "rules_pronoun_numeral.h"

namespace stoppard {
namespace {

// Explicit registry in priority order (R1..R16). Overlapping issues: earlier
// rule wins, so R1 must sort before R3 (both fire on "I can has"), and R13
// before R6 ("Us one asked": R6's "We" would shadow R13's "One of us"). R15
// (sentence-start capitalization) sorts before R16 so a sentence-initial "i"
// keeps the R15 message and R16 is deduped.
const std::vector<const Rule*>& registry()
{
    static const std::vector<const Rule*> rules = {
        &modalVerbFormRule(),
        &auxiliaryVerbFormRule(),
        &agreementRule(),
        &toInfinitiveRule(),
        &determinerNounAgreementRule(),
        &pronounNumeralRule(),
        &pronounCaseRule(),
        &doubleModalRule(),
        &doubleNegativeRule(),
        &regionalismsRule(),
        &confusionRule(),
        &sentenceCapitalizationRule(),
        &pronounCapitalizationRule(),
    };
    return rules;
}

} // namespace

const std::vector<const Rule*>& allRules() { return registry(); }

std::vector<Issue> runAll(std::u16string_view text, Dialect dialect)
{
    Analysis a;
    a.text = text;
    a.tokens = tag(text);
    a.chunks = chunk(a.tokens);
    a.sentences = splitSentences(text);
    a.profile = profileFor(dialect);

    std::vector<Issue> issues;
    for (const Rule *r : registry())
        r->run(a, issues);

    return dedupIssues(std::move(issues));
}

std::vector<Issue> dedupIssues(std::vector<Issue> issues)
{
    // Dedup: sort by start (stable), drop any issue whose start falls
    // inside the span of a higher-priority kept issue (start < last.end).
    std::stable_sort(issues.begin(), issues.end(),
                     [](const Issue &x, const Issue &y) { return x.start < y.start; });
    std::vector<Issue> kept;
    for (const Issue &it : issues) {
        if (!kept.empty() && it.start < kept.back().start + kept.back().length)
            continue;
        kept.push_back(it);
    }
    return kept;
}

} // namespace stoppard

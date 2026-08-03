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
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "chunker.h"
#include "tokenizer.h"
#include "stoppard/stoppard.h"

namespace stoppard {

struct Analysis {
    std::u16string_view text;
    std::vector<TaggedToken> tokens;
    std::vector<Chunk> chunks;
    std::vector<SentenceSpan> sentences;
    DialectProfile profile;
};

struct Rule {
    Rule(std::u16string id, std::u16string description)
        : id(std::move(id)), description(std::move(description)) {}
    std::u16string id;
    std::u16string description;
    virtual void run(const Analysis&, std::vector<Issue>&) const = 0;
    virtual ~Rule() = default;
};

// Priority order (R1..R9). Overlapping issues: earlier rule wins.
const std::vector<const Rule*>& allRules();

// Full pipeline: tokenize -> tag -> chunk -> rules -> dedup (overlapping starts).
std::vector<Issue> runAll(std::u16string_view text, Dialect dialect);

// Sort by start (stable) and drop any issue whose start falls inside the span
// of a previously kept issue. Grammar issues precede spelling issues in
// Engine::check, so on overlap the grammar rule wins.
std::vector<Issue> dedupIssues(std::vector<Issue> issues);

} // namespace stoppard

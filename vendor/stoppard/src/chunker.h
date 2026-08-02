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
#include <vector>

#include "tagger.h"

namespace stoppard {

enum class ChunkKind { NP, PP, VP };
struct Chunk { ChunkKind kind; int startToken; int endToken; };   // inclusive token range

std::vector<Chunk> chunk(const std::vector<TaggedToken>& tokens);

// Subject head (noun or subject pronoun) of the clause containing the verb,
// scanning left within the sentence; -1 when none (imperative, expletive).
int findSubjectHead(const std::vector<TaggedToken>& tokens, int verbTokenIndex);

} // namespace stoppard

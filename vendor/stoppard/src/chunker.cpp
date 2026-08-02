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
// Design: NP = Determiner/Number + (Adverb|Unknown|Adjective)* + Noun (head =
// last token); lone subject/object pronoun -> 1-token NP. PP = Preposition +
// immediately-following NP. VP = (Modal|Auxiliary|Negator)* + Verb. Indices are
// content-token indices (tag() drops whitespace).
#include "chunker.h"

namespace stoppard {
namespace {

bool isDeterminer(const TaggedToken &t)
{
    return t.pos == PosTag::Determiner || t.pos == PosTag::Number
        || (t.pos == PosTag::Pronoun && t.pcase == PronounCase::PossessiveDeterminer);
}

bool isPronoun(const TaggedToken &t)
{
    return t.pos == PosTag::Pronoun
        && (t.pcase == PronounCase::Subject || t.pcase == PronounCase::Object);
}

bool isNPModifier(const TaggedToken &t)
{
    return t.pos == PosTag::Unknown || t.pos == PosTag::Adverb
        || t.pos == PosTag::Adjective;
}

bool isVerbHead(const TaggedToken &t) { return t.pos == PosTag::Verb; }

bool isVPHead(const TaggedToken &t)
{
    return t.pos == PosTag::Modal || t.pos == PosTag::Auxiliary || t.pos == PosTag::Negator;
}

bool isNounHead(const TaggedToken &t) { return t.pos == PosTag::Noun; }

// NP span starting at i, or -1 if none. Returns end index (inclusive).
int npEnd(const std::vector<TaggedToken> &ts, int i)
{
    if (isPronoun(ts[i])) return i;
    if (!isDeterminer(ts[i])) return -1;
    int j = i + 1;
    while (j < static_cast<int>(ts.size()) && isNPModifier(ts[j])) ++j;
    if (j < static_cast<int>(ts.size()) && isNounHead(ts[j])) {
        // Bare words in noun context tag as Noun, so stacked modifiers are
        // consecutive Nouns ("the big cat" -> the,big,cat); head = last noun.
        while (j + 1 < static_cast<int>(ts.size()) && isNounHead(ts[j + 1])) ++j;
        return j;
    }
    return -1;
}

} // namespace

std::vector<Chunk> chunk(const std::vector<TaggedToken>& tokens)
{
    std::vector<Chunk> out;
    const int n = static_cast<int>(tokens.size());
    for (int i = 0; i < n; ++i) {
        if (tokens[i].pos == PosTag::Preposition) {
            if (i + 1 < n) {
                const int end = npEnd(tokens, i + 1);
                if (end >= 0) out.push_back({ChunkKind::PP, i, end});
            }
        } else if (isVPHead(tokens[i])) {
            int j = i + 1;
            while (j < n && isVPHead(tokens[j])) ++j;
            if (j < n && isVerbHead(tokens[j]))
                out.push_back({ChunkKind::VP, i, j});
        } else if (isDeterminer(tokens[i])) {
            const int end = npEnd(tokens, i);
            if (end >= 0) out.push_back({ChunkKind::NP, i, end});
        } else if (isPronoun(tokens[i])) {
            out.push_back({ChunkKind::NP, i, i});
        }
    }
    return out;
}

int findSubjectHead(const std::vector<TaggedToken>& tokens, int verbTokenIndex)
{
    if (verbTokenIndex <= 0) return -1;
    for (int i = verbTokenIndex - 1; i >= 0; --i) {
        const auto &t = tokens[i];
        if (t.pos == PosTag::Noun) return i;
        if (t.pos == PosTag::Pronoun && t.pcase == PronounCase::Subject) return i;
        if (t.pos == PosTag::Conjunction || t.pos == PosTag::Punctuation) return -1;
    }
    return -1;
}

} // namespace stoppard

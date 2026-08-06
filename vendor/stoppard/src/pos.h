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
// Precision-first content-word POS signals (SPEC §17.8). Each predicate is a
// closed, curated membership test on case-folded text — never a consult of a
// heuristic/Unknown tag — so a true is always a confident classification.
#pragma once
#include <string_view>

namespace stoppard {
namespace pos {

// True when `word` is a base-form verb: an irregular base (morphology's closed
// irregular table) or a curated common regular base. Enables too->to ("I want
// too eat.") without a full verb dictionary.
bool isBaseVerb(std::u16string_view word);

// True when `word` is a common verb that takes a to-infinitive complement
// ("want", "need", "like"...). Half of the too->to guard.
bool isInfinitiveTakingVerb(std::u16string_view word);

// True when `word` is a noun that is prototypically possessed (body parts, kin,
// belongings) or carries a productive noun suffix (-tion/-sion/-ness/-ment/-ity).
// The set deliberately excludes predicate/idiom heads (time, fun, fact, ...) so
// "it's time to go." stays clean. Enables it's->its and who's->whose.
bool isPossessedNoun(std::u16string_view word);

// True when `word` is a degree word (many/much/few/little). Half of the
// mid-sentence to->too pattern ("I ate to many cookies.").
bool isDegreeWord(std::u16string_view word);

// True when `word` (its lemma) licenses a to + NP complement ("give to many
// charities", "connected to many servers"). Guards the to->too degree pattern.
bool isToTakingVerb(std::u16string_view word);

} // namespace pos
} // namespace stoppard

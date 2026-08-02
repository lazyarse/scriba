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
#include <string_view>

namespace stoppard {

enum class WordTag { Determiner, Pronoun, Modal, Auxiliary, Preposition, Conjunction, Negator, Interrogative, Adjective, Adverb };
enum class PronounCase { Subject, Object, PossessiveDeterminer, Possessive };
enum class AuxVerb { Be, Have, Do };
enum class AuxForm { Base, ThirdSingular, Past, PastParticiple, Gerund };

struct Lexeme {
    WordTag tag;
    int number = 0;          // -1 singular, +1 plural, 0 both/unknown (determiners, pronouns)
    int person = 0;          // pronouns: 1, 2, 3
    PronounCase pcase = PronounCase::Subject;
    AuxVerb auxVerb = AuxVerb::Be;
    AuxForm auxForm = AuxForm::Base;
};

const Lexeme* lookupWord(std::u16string_view word);   // lowercase lookup; nullptr if not closed-class

} // namespace stoppard

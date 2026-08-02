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

#include "lexicon.h"
#include "morphology.h"
#include "tokenizer.h"

namespace stoppard {

enum class PosTag {
    Determiner, Pronoun, Modal, Auxiliary, Preposition, Conjunction, Negator,
    Interrogative, Noun, Verb, Adjective, Adverb, Number, Punctuation, Code, Unknown
};

struct TaggedToken {
    Token token;
    PosTag pos = PosTag::Unknown;
    int number = 0;                  // -1 singular, +1 plural, 0 both/unknown
    int person = 0;                  // pronouns
    PronounCase pcase = PronounCase::Subject;
    AuxVerb auxVerb = AuxVerb::Be;
    AuxForm auxForm = AuxForm::Base;
    VerbForm verbForm = VerbForm::Base;
    std::u16string lemma;            // lowercase lemma when known
};

std::vector<TaggedToken> tag(std::u16string_view text);

} // namespace stoppard

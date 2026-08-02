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

namespace stoppard {

enum class VerbForm { Base, ThirdSingular, Past, PastParticiple, Gerund };

struct VerbForms {
    std::u16string base, third, past, participle, gerund;
    bool known = false;
};

struct VerbFormInfo {
    std::u16string lemma;
    VerbForm form = VerbForm::Base;
    bool known = false;
};

VerbForms lookupVerbForms(std::u16string_view lemma);      // full paradigm (regular rules + irregular table)
VerbFormInfo classifyVerbForm(std::u16string_view word);   // word -> (lemma, form); unknown for bare non-table words
std::u16string pluralize(std::u16string_view noun);
std::u16string singularize(std::u16string_view noun);
int nounNumber(std::u16string_view noun);                  // -1, +1, 0 unknown/both
std::u16string matchCase(std::u16string_view source, std::u16string out);

} // namespace stoppard

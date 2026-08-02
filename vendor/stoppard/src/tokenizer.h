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

namespace stoppard {

enum class TokenKind { Word, Number, Punctuation, Whitespace, Code };

struct Token { TokenKind kind = TokenKind::Word; int start = 0; int length = 0; };   // UTF-16 code units

std::vector<Token> tokenize(std::u16string_view text);

std::vector<std::u16string> splitContraction(std::u16string_view word);

struct SentenceSpan { int start; int length; };
std::vector<SentenceSpan> splitSentences(std::u16string_view text);

} // namespace stoppard

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

enum class SuggestionKind { Replace, Remove, InsertAfter };

struct Suggestion { SuggestionKind kind = SuggestionKind::Replace; std::u16string text; };

struct Issue {
    int start = 0;
    int length = 0;
    std::u16string message;
    std::vector<Suggestion> suggestions;
};

enum class Dialect { American, British, Australian, Indian, Canadian, NewZealand };

struct Regionalism {
    std::u16string wrong;       // lowercase phrase
    std::u16string correction;
};

struct DialectProfile {
    bool collectivePluralAgreement = false;
    std::vector<Regionalism> regionalisms;
};
DialectProfile profileFor(Dialect dialect);   // impl lands in M3

class Engine {
public:
    explicit Engine(Dialect dialect = Dialect::American);
    std::vector<Issue> check(std::u16string_view text) const;  // impl lands in M4
    Dialect dialect() const;
    void setDialect(Dialect d);

private:
    Dialect m_dialect;
};

} // namespace stoppard

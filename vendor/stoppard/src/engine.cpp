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
#include "stoppard/stoppard.h"

#include "rules.h"

namespace stoppard {

Engine::Engine(Dialect dialect) : m_dialect(dialect) {}

Dialect Engine::dialect() const { return m_dialect; }

void Engine::setDialect(Dialect d) { m_dialect = d; }

std::vector<Issue> Engine::check(std::u16string_view text) const
{
    return runAll(text, m_dialect);   // stateless pipeline; const -> thread-safe by construction
}

} // namespace stoppard

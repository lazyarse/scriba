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

#include "GrammarChecker.h"

#include <QString>

#include <stoppard/stoppard.h>

// GrammarChecker backed by the vendored stoppard grammar engine
// (vendor/stoppard). Spelling is deliberately NOT handled here — the app owns
// spelling via Hunspell.
//
// Unlike HarperEngine, the engine is stateless: stoppard::Engine holds only
// the dialect and check() is const with no mutable state, so a single instance
// may be called from any thread without a mutex.
class StoppardEngine : public GrammarChecker
{
public:
    explicit StoppardEngine(const QString &dialect = QStringLiteral("American"));

    QList<Issue> check(const QString &text) override;

    // Rebuilds the dialect ("American", "British", "Australian", "Indian",
    // "Canadian", "New Zealand"). Unknown names fall back to American.
    void setDialect(const QString &dialect);

    stoppard::Dialect stoppardDialect() const { return m_engine.dialect(); }

private:
    stoppard::Engine m_engine;
};
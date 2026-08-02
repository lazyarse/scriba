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

// GrammarChecker backed by the vendored harper-core engine exposed through
// the C ABI in vendor/harper-ffi. Spelling is deliberately NOT handled here —
// the app owns spelling via Hunspell (harper's SpellCheck rule is disabled).
//
// Creating an engine loads harper's curated dictionary, so a single shared
// instance should be reused (see Editor).
class HarperEngine : public GrammarChecker
{
public:
    HarperEngine();
    ~HarperEngine() override;

    QList<Issue> check(const QString &text) override;

    // Whether the underlying engine could be initialised.
    bool isAvailable() const { return m_engine != nullptr; }

    // Rebuilds the engine for the given English dialect ("American",
    // "British", "Australian", "Indian", "Canadian"). Unknown names fall
    // back to American. No-op when the dialect is unchanged.
    void setDialect(const QString &dialect);

private:
    void *m_engine = nullptr; // opaque HarperEngine* from harper_init()
    QString m_dialect;
};

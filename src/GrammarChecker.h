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

#include <QList>
#include <QMetaType>
#include <QString>

// Interface for grammar checking. Implementations are expected to be
// expensive (e.g. harper) — callers should debounce invocations.
class GrammarChecker
{
public:
    virtual ~GrammarChecker();

    struct Issue {
        int start = 0;   // offset relative to the start of the checked text
        int length = 0;
        QString message;
    };

    // Runs the check over `text` and returns all issues found.
    // May be called from a background thread; implementations must be
    // thread-safe (harper serializes access to its engine with a mutex).
    virtual QList<Issue> check(const QString &text) = 0;
};

Q_DECLARE_METATYPE(GrammarChecker::Issue)

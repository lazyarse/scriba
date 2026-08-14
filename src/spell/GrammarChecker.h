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
#include <QVector>

// Interface for grammar checking. Implementations are expected to be
// expensive — callers should debounce invocations.
class GrammarChecker
{
public:
    virtual ~GrammarChecker();

    struct Issue {
        // How a suggestion fixes the issue. All kinds apply to the issue's
        // own [start, start + length) span.
        enum class SuggestionKind { Replace, Remove, InsertAfter };

        struct Suggestion {
            SuggestionKind kind = SuggestionKind::Replace;
            QString text;
        };

        int start = 0;   // offset relative to the start of the checked text
        int length = 0;
        QString message;
        QVector<Suggestion> suggestions;
    };

    // Runs the check over `text` and returns all issues found.
    // May be called from a background thread; implementations must be
    // thread-safe.
    virtual QList<Issue> check(const QString &text) = 0;
};

Q_DECLARE_METATYPE(GrammarChecker::Issue)

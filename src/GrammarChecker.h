#pragma once

#include <QList>
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
    virtual QList<Issue> check(const QString &text) = 0;
};

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

#include <QSet>
#include <QString>
#include <QVector>

// The single home for markdown-consistency checks (heading-level skips,
// duplicate headings, trailing whitespace, runs of blank lines, overlong
// lines, `#`-headings without a space, unmatched footnote references). It is
// a pure, headless scanner so it can be shared by the in-editor underlines
// (SpellHighlighter) and the Validation Report, keeping both in lock-step.
class MarkdownChecker
{
public:
    enum class Check {
        HeadingLevelSkip,
        DuplicateHeading,
        TrailingWhitespace,
        ConsecutiveBlankLines,
        OverlongLine,
        HashNoSpace,
        FootnoteReference,
    };

    // One finding. `line` is 1-based; `start`/`length` are offsets within the
    // line (so callers can both locate and underline the span). For whole-line
    // findings (e.g. consecutive blank lines) length is 0.
    struct Issue {
        int line = 1;
        int start = 0;
        int length = 0;
        QString message;
    };

    // The full set of checks, used as the default and by the in-editor pass.
    static QSet<Check> defaultChecks();

    // Scan `text` for the enabled checks, in line order. Fenced code blocks
    // and YAML front matter are skipped via a per-line state machine. Pure,
    // deterministic, and cheap.
    static QVector<Issue> scan(const QString &text, QSet<Check> checks = defaultChecks());
};
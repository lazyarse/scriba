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

#include "MdLintConfig.h"

#include <QString>
#include <QVector>

struct MdLintIssue {
    QString rule;        // "MD013"
    QString alias;       // "line-length"
    QString description; // "Line length"
    Severity severity = Severity::Error;
    int line = 1;        // 1-based
    int col = 1;         // 1-based character column
    int length = 0;      // characters (0 = whole-line/point finding)
    QString detail;
};

// Pure, headless, synchronous markdown linter: runs md4c with the preview's
// exact flag set, collects a document model, then runs the enabled rules.
// Shared by the in-editor underlines (SpellHighlighter) and the issue-summary
// pane so both stay in lock-step.
class MdLintEngine
{
public:
    static QVector<MdLintIssue> lint(const QString &text, const MdLintConfig &config);
};

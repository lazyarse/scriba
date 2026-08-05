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
#include <QMap>
#include <QString>
#include <QVector>

class SpellChecker;

// Generates a validation report over one or more markdown documents: spelling
// typos, grammar issues, broken file/URL links, missing heading anchors and
// markdown-consistency problems (heading-level skips, duplicate headings,
// trailing whitespace, runs of blank lines, overlong lines, `#` headings
// without a space, unmatched footnote references).
//
// The checks reuse the same engines the editor's underlines use:
// SpellHighlighter::scanDocument (spelling) and scanLinkIssues (links and
// anchors) plus LinkValidator for target classification; grammar runs through
// GrammarChecker. The class is headless and deterministic so it can be unit
// tested directly.
//
// Grammar checking is whole-document and expensive, so MainWindow runs that
// category on a background thread and merges the results with
// grammarIssuesToLineIssues() after scan() returns; renderMarkdown() renders
// the collected per-document reports to a markdown string for a new tab.
class ValidationReport
{
public:
    struct DocumentSource {
        QString filePath; // empty for untitled tabs
        QString text;
    };

    enum class Category { Spelling, Grammar, Links, Markdown };

    // One finding. line and column are 1-based; column is 0 when the whole
    // line is the unit (most markdown-consistency checks).
    struct Issue {
        int line = 0;
        int column = 0;
        int length = 0;
        QString message;
        QString suggestion; // optional suggested fix
    };

    struct DocumentReport {
        QString label;                         // file name or "(Untitled)"
        QString filePath;
        QMap<Category, QVector<Issue>> issues; // in scan order
    };

    // Scan `sources` and return one report per source, in order. Runs
    // spelling (only when a loaded spell checker is supplied), links and
    // markdown-consistency checks. Grammar is merged in by the caller from
    // the async worker via grammarIssuesToLineIssues().
    QVector<DocumentReport> scan(const QVector<DocumentSource> &sources,
                                 SpellChecker *spellChecker) const;

    // Convert raw grammar issues (offsets relative to `text`) into line/col
    // report issues, keeping the first replace-with suggestion as the fix.
    static QVector<Issue> grammarIssuesToLineIssues(
        const QString &text, const QList<GrammarChecker::Issue> &issues);

    // The markdown-consistency scan. Pure; exposed for tests.
    static QVector<Issue> scanMarkdownIssues(const QString &text);

    // Render collected reports to markdown for display in a new tab.
    static QString renderMarkdown(const QVector<DocumentReport> &reports,
                                  const QString &generatedAt);
};

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

#include "spell/GrammarChecker.h"
#include "MarkdownChecker.h"
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
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

    // The individual markdown-consistency checks, so the report dialog can
    // toggle each one independently. Alias of MarkdownChecker::Check: the two
    // scans share the same engine, so the report and the editor's in-document
    // underlines can never drift apart.
    using MarkdownCheck = MarkdownChecker::Check;

    // Which categories and markdown sub-checks to run. Empty sets disable the
    // corresponding scans entirely.
    struct ValidationOptions {
        QSet<Category> categories =
            {Category::Spelling, Category::Grammar, Category::Links, Category::Markdown};
        QSet<MarkdownCheck> markdown = {
            MarkdownCheck::HeadingLevelSkip,
            MarkdownCheck::DuplicateHeading,
            MarkdownCheck::TrailingWhitespace,
            MarkdownCheck::ConsecutiveBlankLines,
            MarkdownCheck::OverlongLine,
            MarkdownCheck::HashNoSpace,
            MarkdownCheck::FootnoteReference,
        };

        static ValidationOptions all() { return ValidationOptions{}; }
    };

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
        QStringList sourceLines;               // source text split on '\n'
        QMap<Category, QVector<Issue>> issues; // in scan order
    };

    // Scan `sources` and return one report per source, in order. Runs the
    // checks enabled by `options` (spelling only when a loaded spell checker
    // is supplied). Grammar is merged in by the caller from the async worker
    // via grammarIssuesToLineIssues().
    QVector<DocumentReport> scan(const QVector<DocumentSource> &sources,
                                 SpellChecker *spellChecker,
                                 const ValidationOptions &options = ValidationOptions::all()) const;

    // Convert raw grammar issues (offsets relative to `text`) into line/col
    // report issues, keeping the first replace-with suggestion as the fix.
    static QVector<Issue> grammarIssuesToLineIssues(
        const QString &text, const QList<GrammarChecker::Issue> &issues);

    // The markdown-consistency scan. Pure; exposed for tests. When `checks`
    // is non-empty only the listed sub-checks run; an empty set disables all
    // of them.
    static QVector<Issue> scanMarkdownIssues(
        const QString &text,
        const QSet<MarkdownCheck> &checks = {
            MarkdownCheck::HeadingLevelSkip,
            MarkdownCheck::DuplicateHeading,
            MarkdownCheck::TrailingWhitespace,
            MarkdownCheck::ConsecutiveBlankLines,
            MarkdownCheck::OverlongLine,
            MarkdownCheck::HashNoSpace,
            MarkdownCheck::FootnoteReference,
        });

    // Render collected reports to markdown for display in a new tab. Only the
    // categories present in `categories` produce a summary column and report
    // section.
    static QString renderMarkdown(
        const QVector<DocumentReport> &reports,
        const QString &generatedAt,
        const QSet<Category> &categories = {Category::Spelling, Category::Grammar,
                                            Category::Links, Category::Markdown});
};

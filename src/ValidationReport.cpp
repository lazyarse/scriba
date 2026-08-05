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
#include "ValidationReport.h"

#include "LinkValidator.h"
#include "SpellChecker.h"
#include "SpellHighlighter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextDocument>

#include <algorithm>

namespace {

// Converts a document offset to 1-based line/column.
void lineCol(const QString &text, int offset, int *line, int *column)
{
    int lineNumber = 1;
    int lastNewline = -1;
    const int upto = qBound(0, offset, text.size());
    for (int i = 0; i < upto; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++lineNumber;
            lastNewline = i;
        }
    }
    *line = lineNumber;
    *column = offset - lastNewline;
}

const char *categoryName(ValidationReport::Category category)
{
    switch (category) {
    case ValidationReport::Category::Spelling: return "Spelling";
    case ValidationReport::Category::Grammar: return "Grammar";
    case ValidationReport::Category::Links: return "Links & anchors";
    case ValidationReport::Category::Markdown: return "Markdown consistency";
    }
    return "";
}

QString contextQuote(const QStringList &lines, const ValidationReport::Issue &issue);

QString renderCategory(const char *name, const QVector<ValidationReport::Issue> &issues,
                      const QStringList &lines)
{
    QString md = QLatin1String("\n### ") + QLatin1String(name) + QLatin1String("\n\n");
    if (issues.isEmpty())
        return md + QLatin1String("No issues found.\n");
    for (const auto &issue : issues) {
        md += QLatin1String("- L") + QString::number(issue.line);
        if (issue.column > 0)
            md += QLatin1String(" C") + QString::number(issue.column);
        md += QLatin1String(": ") + issue.message;
        if (!issue.suggestion.isEmpty())
            md += QLatin1String(" -> ") + issue.suggestion;
        md += QLatin1Char('\n');
        const QString quote = contextQuote(lines, issue);
        if (!quote.isEmpty())
            md += quote + QLatin1Char('\n');
    }
    return md;
}

// Builds a short blockquote of the offending source line, with the issue's span
// wrapped in `==…==` so the preview renders it as a highlighted <mark>.
// Returns an empty string when there is no meaningful span to quote (blank
// line, whole-line span, or no source lines).
QString contextQuote(const QStringList &lines, const ValidationReport::Issue &issue)
{
    const int lineIndex = issue.line - 1; // Issue::line is 1-based
    if (lineIndex < 0 || lineIndex >= lines.size())
        return QString();
    const QString full = lines.at(lineIndex);
    if (issue.column <= 0 || issue.length <= 0)
        return QStringLiteral("> ") + (full.isEmpty() ? QStringLiteral(" ") : full);
    const int spanStart = qBound(0, issue.column - 1, qMax(0, full.size() - 1));
    const int spanLen = qMin(issue.length, full.size() - spanStart);
    if (spanLen <= 0)
        return QStringLiteral("> ") + (full.isEmpty() ? QStringLiteral(" ") : full);
    const int spanEnd = spanStart + spanLen;
    const int kMax = 140;
    if (full.size() <= kMax) {
        return QStringLiteral("> ") + full.left(spanStart) + QLatin1String("==")
            + full.mid(spanStart, spanLen) + QLatin1String("==") + full.mid(spanEnd);
    }
    // Truncate the line around the span, keeping the whole span visible.
    const int window = qMin(kMax, full.size());
    const int extra = window - spanLen;       // budget for context either side
    const int before = spanStart;             // chars available before the span
    const int after = full.size() - spanEnd;  // chars available after the span
    int lead = qMin(extra / 2, before);
    int trail = qMin(extra - lead, after);
    lead = qMin(extra - trail, before);       // use up any unused trailing budget
    const int begin = spanStart - lead;
    const int finish = spanEnd + trail;
    QString out;
    if (begin > 0)
        out += QLatin1String("…");
    out += full.mid(begin, spanStart - begin) + QLatin1String("==")
        + full.mid(spanStart, spanLen) + QLatin1String("==") + full.mid(spanEnd, finish - spanEnd);
    if (finish < full.size())
        out += QLatin1String("…");
    return QStringLiteral("> ") + out;
}

} // namespace

QVector<ValidationReport::DocumentReport>
ValidationReport::scan(const QVector<DocumentSource> &sources,
                       SpellChecker *spellChecker,
                       const ValidationOptions &options) const
{
    QVector<DocumentReport> reports;
    reports.reserve(sources.size());
    for (const DocumentSource &source : sources) {
        DocumentReport report;
        report.label = source.filePath.isEmpty()
            ? QStringLiteral("(Untitled)")
            : QFileInfo(source.filePath).fileName();
        report.filePath = source.filePath;
        report.sourceLines = source.text.split(QLatin1Char('\n'));

        if (options.categories.contains(Category::Spelling)
            && spellChecker && spellChecker->isLoaded()) {
            QTextDocument doc;
            doc.setPlainText(source.text);
            const auto spellIssues = SpellHighlighter::scanDocument(&doc, spellChecker);
            for (const auto &si : spellIssues) {
                Issue issue;
                issue.line = si.blockNumber + 1;
                issue.column = si.start + 1;
                issue.length = si.length;
                issue.message = si.word;
                const QStringList suggestions = spellChecker->suggestions(si.word);
                if (!suggestions.isEmpty())
                    issue.suggestion = suggestions.first();
                report.issues[Category::Spelling].append(issue);
            }
        }

        const QString baseDir = source.filePath.isEmpty()
            ? QString() : QFileInfo(source.filePath).absolutePath();
        if (options.categories.contains(Category::Links)) {
            const auto linkHits = SpellHighlighter::scanLinkIssues(source.text, baseDir);
            for (const auto &lh : linkHits)
                report.issues[Category::Links].append({lh.line, lh.col, lh.length, lh.message, {}});
        }

        if (options.categories.contains(Category::Markdown)) {
            const auto mdIssues = MarkdownChecker::scan(source.text, options.markdown);
            QVector<Issue> issues;
            issues.reserve(mdIssues.size());
            for (const auto &mi : mdIssues)
                issues.append({mi.line, mi.start + 1, mi.length, mi.message, {}});
            report.issues[Category::Markdown] = issues;
        }

        reports.append(report);
    }
    return reports;
}

QVector<ValidationReport::Issue>
ValidationReport::grammarIssuesToLineIssues(
    const QString &text, const QList<GrammarChecker::Issue> &issues)
{
    QVector<Issue> out;
    out.reserve(issues.size());
    for (const auto &gi : issues) {
        Issue issue;
        lineCol(text, gi.start, &issue.line, &issue.column);
        issue.length = gi.length;
        issue.message = gi.message;
        for (const auto &suggestion : gi.suggestions) {
            if (suggestion.kind == GrammarChecker::Issue::SuggestionKind::Replace
                && !suggestion.text.isEmpty()) {
                issue.suggestion = suggestion.text;
                break;
            }
        }
        out.append(issue);
    }
    return out;
}

QVector<ValidationReport::Issue>
ValidationReport::scanMarkdownIssues(const QString &text, const QSet<MarkdownCheck> &checks)
{
    const auto mdIssues = MarkdownChecker::scan(text, checks);
    QVector<Issue> issues;
    issues.reserve(mdIssues.size());
    for (const auto &mi : mdIssues)
        issues.append({mi.line, mi.start + 1, mi.length, mi.message, {}});
    return issues;
}

QString ValidationReport::renderMarkdown(const QVector<DocumentReport> &reports,
                                         const QString &generatedAt,
                                         const QSet<Category> &categories)
{
    // Active categories in fixed display order.
    const QVector<Category> order = {
        Category::Spelling, Category::Grammar, Category::Links, Category::Markdown};
    QVector<Category> active;
    for (Category c : order)
        if (categories.contains(c))
            active.append(c);

    // Simulate the preview's heading-anchor assignment (JsSnippets headingIdJs)
    // over the headings we emit, in DOM order, so the summary-table links
    // resolve to real anchors when the report is opened in a tab.
    QSet<QString> used;
    auto allocateAnchor = [&used](const QString &text) {
        const QString base = LinkValidator::headingSlug(text);
        if (base.isEmpty())
            return QString();
        QString id = base;
        int n = 1;
        while (used.contains(id))
            id = base + QLatin1Char('-') + QString::number(n++);
        used.insert(id);
        return id;
    };

    QString md;
    md += "# Validation Report\n\n";
    allocateAnchor(QStringLiteral("Validation Report"));
    md += "Generated: " + generatedAt + "\n\n";

    md += "## Summary\n\n";
    allocateAnchor(QStringLiteral("Summary"));
    md += "| Document |";
    for (Category c : active)
        md += " " + QString::fromUtf8(categoryName(c)) + " |";
    md += "\n| --- |";
    for (qsizetype i = 0; i < active.size(); ++i)
        md += " --- |";
    md += "\n";

    QVector<int> totals(active.size(), 0);
    for (const DocumentReport &report : reports) {
        const QString docAnchor = allocateAnchor(report.label);
        QStringList catAnchors;
        for (Category c : active)
            catAnchors.append(allocateAnchor(QString::fromUtf8(categoryName(c))));
        md += "| " + (docAnchor.isEmpty()
                          ? report.label
                          : QStringLiteral("[%1](#%2)").arg(report.label, docAnchor));
        for (qsizetype i = 0; i < active.size(); ++i) {
            const int count = static_cast<int>(report.issues.value(active.at(i)).size());
            totals[i] += count;
            const QString &anchor = catAnchors.at(i);
            if (anchor.isEmpty())
                md += " | " + QString::number(count);
            else
                md += " | [" + QString::number(count) + "](#" + anchor + ")";
        }
        md += " |\n";
    }
    md += "| **Total** |";
    int grandTotal = 0;
    for (int t : totals) {
        md += " **" + QString::number(t) + "** |";
        grandTotal += t;
    }
    md += "\n\nTotal issues: " + QString::number(grandTotal) + "\n\n---\n";

    for (const DocumentReport &report : reports) {
        md += "\n## " + report.label + "\n";
        if (!report.filePath.isEmpty())
            md += "\nPath: `" + report.filePath + "`\n";
        for (Category c : active)
            md += renderCategory(categoryName(c), report.issues.value(c), report.sourceLines);
    }
    return md;
}

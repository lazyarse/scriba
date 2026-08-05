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

// Marks a line as fenced code / front matter, mirroring the state machine in
// SpellHighlighter::blockContext so the checks skip the same content the
// editor's underlines ignore. Kept in sync deliberately (a short, stable
// function; the highlighter's copy is private to its translation unit).
struct BlockContext {
    int state = 0; // 0 plain, 1 fenced code, 2 front matter
    bool checkable = true;
};

BlockContext blockContext(int blockNumber, const QString &text, int previousState)
{
    static const QRegularExpression fenceRe(R"(^\s*(?:```+|~~~+))");
    static const QRegularExpression frontMatterRe(R"(^\s*---\s*$)");
    const bool opener = fenceRe.match(text).hasMatch();
    const bool boundary = frontMatterRe.match(text).hasMatch();
    BlockContext ctx;
    if (previousState == 1) {
        ctx.state = opener ? 0 : 1;
    } else if (previousState == 2) {
        ctx.state = boundary ? 0 : 2;
    } else if (blockNumber == 0 && boundary) {
        ctx.state = 2;
    } else if (opener) {
        ctx.state = 1;
    }
    ctx.checkable = previousState != 1 && previousState != 2 && !opener
        && !(blockNumber == 0 && boundary);
    return ctx;
}

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

QString renderCategory(const char *name, const QVector<ValidationReport::Issue> &issues)
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
    }
    return md;
}

// Records a duplicate-heading finding when `title`'s slug has been seen.
void checkDuplicateHeading(QVector<ValidationReport::Issue> &issues, int line,
                           const QString &title, QSet<QString> &seenSlugs)
{
    const QString slug = LinkValidator::headingSlug(title);
    if (slug.isEmpty())
        return;
    if (seenSlugs.contains(slug)) {
        issues.append({line, 0, 0,
                       QStringLiteral("Duplicate heading (anchor #%1 already used)").arg(slug),
                       {}});
    } else {
        seenSlugs.insert(slug);
    }
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

        if (options.categories.contains(Category::Markdown))
            report.issues[Category::Markdown] = scanMarkdownIssues(source.text, options.markdown);

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
    const QStringList lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression atxRe(R"(^(\#{1,6})\s+(.+))");
    static const QRegularExpression noSpaceHashRe(R"(^\#{1,6}\S)");
    static const QRegularExpression setextRe(R"(^(={3,}|-{3,})\s*$)");
    static const QRegularExpression footnoteDefRe(R"(^\[\^([^\]]+)\]:)");
    static const QRegularExpression footnoteUseRe(R"(\[\^([^\]\n]+)\])");
    static const QRegularExpression inlineCodeRe(R"(`[^`\n]*`)");
    static const int kMaxLineLength = 120;

    QVector<Issue> issues;
    QSet<QString> footnoteDefs;
    QVector<QPair<int, QString>> footnoteUses; // (1-based line, reference name)

    int state = 0;
    int prevHeadingLevel = 0;
    QSet<QString> seenHeadingSlugs;
    int blankRun = 0;
    bool prevCheckable = false; // previous checkable line could carry setext text
    bool prevWasHeading = false;
    QString prevLine;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const BlockContext ctx = blockContext(i, line, state);
        state = ctx.state;
        if (!ctx.checkable) {
            blankRun = 0;
            prevCheckable = false;
            prevWasHeading = false;
            prevLine.clear();
            continue;
        }

        if (line.trimmed().isEmpty()) {
            ++blankRun;
            prevCheckable = false;
            prevWasHeading = false;
            prevLine.clear();
            if (blankRun == 3 && checks.contains(MarkdownCheck::ConsecutiveBlankLines))
                issues.append({i + 1, 0, 0,
                               QStringLiteral("Consecutive blank lines (3 or more)"), {}});
            continue;
        }
        blankRun = 0;

        if (checks.contains(MarkdownCheck::TrailingWhitespace)
            && (line.endsWith(QLatin1Char(' ')) || line.endsWith(QLatin1Char('\t'))))
            issues.append({i + 1, 0, 0, QStringLiteral("Trailing whitespace"), {}});

        if (checks.contains(MarkdownCheck::OverlongLine) && line.length() > kMaxLineLength)
            issues.append({i + 1, 0, 0,
                           QStringLiteral("Line too long (%1 characters)").arg(line.length()),
                           {}});

        if (checks.contains(MarkdownCheck::HashNoSpace) && noSpaceHashRe.match(line).hasMatch())
            issues.append({i + 1, 0, 0,
                           QStringLiteral("Heading missing space after '#' (not rendered as a heading)"),
                           {}});

        // Footnotes: a definition line records the name; usages are checked
        // against the collected definitions at the end. Inline code is
        // blanked so `[^x]` inside backticks is not reported.
        const QRegularExpressionMatch defMatch = footnoteDefRe.match(line);
        if (defMatch.hasMatch()) {
            footnoteDefs.insert(defMatch.captured(1));
        } else {
            QString scanLine = line;
            scanLine.replace(inlineCodeRe, QString());
            QRegularExpressionMatchIterator it = footnoteUseRe.globalMatch(scanLine);
            while (it.hasNext())
                footnoteUses.append({i + 1, it.next().captured(1)});
        }

        // ATX heading `# Title`.
        const QRegularExpressionMatch atx = atxRe.match(line);
        if (atx.hasMatch()) {
            const int level = atx.captured(1).length();
            QString title = atx.captured(2).trimmed();
            while (title.endsWith(QLatin1Char('#')))
                title.chop(1);
            title = title.trimmed();
            if (prevHeadingLevel > 0 && level > prevHeadingLevel + 1
                && checks.contains(MarkdownCheck::HeadingLevelSkip))
                issues.append({i + 1, 0, 0,
                               QStringLiteral("Heading level skipped (#%1 after #%2)")
                                   .arg(level).arg(prevHeadingLevel),
                               {}});
            prevHeadingLevel = level;
            if (checks.contains(MarkdownCheck::DuplicateHeading))
                checkDuplicateHeading(issues, i + 1, title, seenHeadingSlugs);
            prevCheckable = true;
            prevWasHeading = true;
            prevLine = line;
            continue;
        }

        // Setext heading underline (`====` / `----`) — or a horizontal rule
        // when the previous line is blank or itself a heading.
        const QRegularExpressionMatch setext = setextRe.match(line);
        if (setext.hasMatch()) {
            const bool isHr = !(prevCheckable && !prevWasHeading);
            if (isHr) {
                prevCheckable = false;
                prevWasHeading = false;
                prevLine.clear();
                continue;
            }
            const int level = setext.captured(1).startsWith(QLatin1Char('=')) ? 1 : 2;
            if (prevHeadingLevel > 0 && level > prevHeadingLevel + 1
                && checks.contains(MarkdownCheck::HeadingLevelSkip))
                issues.append({i + 1, 0, 0,
                               QStringLiteral("Heading level skipped (#%1 after #%2)")
                                   .arg(level).arg(prevHeadingLevel),
                               {}});
            prevHeadingLevel = level;
            if (checks.contains(MarkdownCheck::DuplicateHeading))
                checkDuplicateHeading(issues, i + 1, prevLine.trimmed(), seenHeadingSlugs);
            prevCheckable = false;
            prevWasHeading = false;
            prevLine.clear();
            continue;
        }

        prevCheckable = true;
        prevWasHeading = false;
        prevLine = line;
    }

    for (const auto &use : footnoteUses) {
        if (!footnoteDefs.contains(use.second)
            && checks.contains(MarkdownCheck::FootnoteReference))
            issues.append({use.first, 0, 0,
                           QStringLiteral("Unmatched footnote reference: [^%1]").arg(use.second),
                           {}});
    }

    std::sort(issues.begin(), issues.end(), [](const Issue &a, const Issue &b) {
        if (a.line != b.line)
            return a.line < b.line;
        return a.column < b.column;
    });
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

    QString md;
    md += "# Validation Report\n\n";
    md += "Generated: " + generatedAt + "\n\n";

    md += "## Summary\n\n";
    md += "| Document |";
    for (Category c : active)
        md += " " + QString::fromUtf8(categoryName(c)) + " |";
    md += "\n| --- |";
    for (qsizetype i = 0; i < active.size(); ++i)
        md += " --- |";
    md += "\n";

    QVector<int> totals(active.size(), 0);
    for (const DocumentReport &report : reports) {
        md += "| " + report.label;
        for (qsizetype i = 0; i < active.size(); ++i) {
            const int count = static_cast<int>(report.issues.value(active.at(i)).size());
            totals[i] += count;
            md += " | " + QString::number(count);
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
            md += renderCategory(categoryName(c), report.issues.value(c));
    }
    return md;
}

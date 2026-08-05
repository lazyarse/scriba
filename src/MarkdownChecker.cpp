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
#include "MarkdownChecker.h"

#include "LinkValidator.h"

#include <QRegularExpression>
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

// Number of leading `#` characters in an ATX heading line (the title span
// starts right after them).
int headingTitleStart(const QString &line)
{
    int i = 0;
    while (i < line.size() && line.at(i) == QLatin1Char('#'))
        ++i;
    return i;
}

// Records a duplicate-heading finding when `headingText`'s slug has been seen.
void checkDuplicateHeading(QVector<MarkdownChecker::Issue> &issues, int line,
                           int start, int length,
                           const QString &headingText, QSet<QString> &seenSlugs)
{
    const QString slug = LinkValidator::headingSlug(headingText);
    if (slug.isEmpty())
        return;
    if (seenSlugs.contains(slug)) {
        issues.append({line, start, length,
                       QStringLiteral("Duplicate heading (anchor #%1 already used)").arg(slug)});
    } else {
        seenSlugs.insert(slug);
    }
}

// A footnote usage found on a checkable line: 1-based line, offset within the
// line, length of the `[^name]` span, and the reference name.
struct FootnoteUse {
    int line = 0;
    int start = 0;
    int length = 0;
    QString name;
};

} // namespace

QSet<MarkdownChecker::Check> MarkdownChecker::defaultChecks()
{
    return {Check::HeadingLevelSkip, Check::DuplicateHeading, Check::TrailingWhitespace,
            Check::ConsecutiveBlankLines, Check::OverlongLine, Check::HashNoSpace,
            Check::FootnoteReference};
}

QVector<MarkdownChecker::Issue> MarkdownChecker::scan(const QString &text, QSet<Check> checks)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression atxRe(R"(^(\#{1,6})\s+(.+))");
    // Possessive quantifier: the `#` run must be followed by a non-space with
    // no backtracking, so a valid heading like `## Foo` (space after the run)
    // is NOT flagged; only true space-less runs like `#foo` / `##no` are.
    static const QRegularExpression noSpaceHashRe(R"(^\#{1,6}+\S)");
    static const QRegularExpression setextRe(R"(^(={3,}|-{3,})\s*$)");
    static const QRegularExpression footnoteDefRe(R"(^\[\^([^\]]+)\]:)");
    static const QRegularExpression footnoteUseRe(R"(\[\^([^\]\n]+)\])");
    static const QRegularExpression inlineCodeRe(R"(`[^`\n]*`)");
    static const int kMaxLineLength = 120;

    QVector<Issue> issues;
    QSet<QString> footnoteDefs;
    QVector<FootnoteUse> footnoteUses;

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
            if (blankRun == 3 && checks.contains(Check::ConsecutiveBlankLines))
                issues.append({i + 1, 0, 0, QStringLiteral("Consecutive blank lines (3 or more)")});
            continue;
        }
        blankRun = 0;

        if (checks.contains(Check::TrailingWhitespace)) {
            int ws = line.size();
            while (ws > 0) {
                const QChar c = line.at(ws - 1);
                if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
                    break;
                --ws;
            }
            if (ws < line.size())
                issues.append({i + 1, ws, static_cast<int>(line.size() - ws),
                               QStringLiteral("Trailing whitespace")});
        }

        if (checks.contains(Check::OverlongLine) && line.size() > kMaxLineLength) {
            const int overflowStart = qMin(kMaxLineLength, line.size());
            issues.append({i + 1, overflowStart, static_cast<int>(line.size() - overflowStart),
                           QStringLiteral("Line too long (%1 characters)").arg(line.size())});
        }

        const QRegularExpressionMatch noSpaceMatch = noSpaceHashRe.match(line);
        if (checks.contains(Check::HashNoSpace) && noSpaceMatch.hasMatch())
            issues.append({i + 1, 0, static_cast<int>(noSpaceMatch.capturedLength()),
                           QStringLiteral("Heading missing space after '#' (not rendered as a heading)")});

        // Footnotes: a definition line records the name; usages are checked
        // against the collected definitions at the end. Inline code is skipped
        // (so `[^x]` inside backticks or link targets is not reported), but
        // usages are located on the original line so offsets stay accurate.
        const QRegularExpressionMatch defMatch = footnoteDefRe.match(line);
        if (defMatch.hasMatch()) {
            footnoteDefs.insert(defMatch.captured(1));
        } else if (checks.contains(Check::FootnoteReference)) {
            QVector<QPair<int, int>> codeRanges;
            QRegularExpressionMatchIterator cit = inlineCodeRe.globalMatch(line);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                codeRanges.append({cm.capturedStart(), cm.capturedLength()});
            }
            QRegularExpressionMatchIterator it = footnoteUseRe.globalMatch(line);
            while (it.hasNext()) {
                const auto um = it.next();
                const int useStart = um.capturedStart();
                bool inCode = false;
                for (const auto &r : codeRanges) {
                    if (useStart >= r.first && useStart < r.first + r.second) {
                        inCode = true;
                        break;
                    }
                }
                if (!inCode)
                    footnoteUses.append({i + 1, useStart, static_cast<int>(um.capturedLength()),
                                         um.captured(1)});
            }
        }

        // ATX heading `# Title`.
        const QRegularExpressionMatch atx = atxRe.match(line);
        if (atx.hasMatch()) {
            const int level = atx.captured(1).length();
            QString title = atx.captured(2).trimmed();
            while (title.endsWith(QLatin1Char('#')))
                title.chop(1);
            title = title.trimmed();
            const int titleStart = headingTitleStart(line);
            const int titleLength = qMax(1, line.size() - titleStart);
            if (prevHeadingLevel > 0 && level > prevHeadingLevel + 1
                && checks.contains(Check::HeadingLevelSkip))
                issues.append({i + 1, titleStart, titleLength,
                               QStringLiteral("Heading level skipped (#%1 after #%2)")
                                   .arg(level).arg(prevHeadingLevel)});
            prevHeadingLevel = level;
            if (checks.contains(Check::DuplicateHeading))
                checkDuplicateHeading(issues, i + 1, titleStart, titleLength,
                                      title, seenHeadingSlugs);
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
            const int lineSize = line.size();
            const int level = setext.captured(1).startsWith(QLatin1Char('=')) ? 1 : 2;
            if (prevHeadingLevel > 0 && level > prevHeadingLevel + 1
                && checks.contains(Check::HeadingLevelSkip))
                issues.append({i + 1, 0, lineSize,
                               QStringLiteral("Heading level skipped (#%1 after #%2)")
                                   .arg(level).arg(prevHeadingLevel)});
            prevHeadingLevel = level;
            if (checks.contains(Check::DuplicateHeading))
                checkDuplicateHeading(issues, i + 1, 0, lineSize, prevLine.trimmed(),
                                      seenHeadingSlugs);
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
        if (!footnoteDefs.contains(use.name)) {
            issues.append({use.line, use.start, use.length,
                           QStringLiteral("Unmatched footnote reference: [^%1]").arg(use.name)});
        }
    }

    std::sort(issues.begin(), issues.end(), [](const Issue &a, const Issue &b) {
        if (a.line != b.line)
            return a.line < b.line;
        return a.start < b.start;
    });
    return issues;
}
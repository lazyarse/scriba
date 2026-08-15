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
#include "FrontMatterParser.h"

#include <QRegularExpression>

namespace {

QStringList splitLines(const QString &text)
{
    return text.split(QLatin1Char('\n'));
}

} // namespace

QPair<int, int> FrontMatterParser::lineRange(const QString &markdown)
{
    const QStringList lines = splitLines(markdown);

    // The first non-empty line must open the block. Blank leading lines are
    // skipped, so `\n---\n...\n---` is still frontmatter.
    int first = 0;
    while (first < lines.size() && lines[first].trimmed().isEmpty())
        ++first;
    if (first >= lines.size()
        || !lines[first].trimmed().startsWith(QLatin1String("---")))
        return {-1, -1};

    // Scan for a closing `---` line after the opener.
    for (int i = first + 1; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith(QLatin1String("---")))
            return {first, i};
    }
    // Unterminated: a lone leading `---` is an HR, not frontmatter.
    return {-1, -1};
}

bool FrontMatterParser::hasFrontMatter(const QString &markdown)
{
    return lineRange(markdown).first >= 0;
}

QString FrontMatterParser::value(const QString &markdown, const QString &key)
{
    const auto range = lineRange(markdown);
    if (range.first < 0)
        return QString();

    const QStringList lines = splitLines(markdown);
    const QRegularExpression re(
        QStringLiteral("^[ \t]*%1:[ \t]*(.*)$").arg(QRegularExpression::escape(key)));
    for (int i = range.first + 1; i < range.second; ++i) {
        const QRegularExpressionMatch m = re.match(lines[i]);
        if (!m.hasMatch())
            continue;
        QString v = m.captured(1).trimmed();
        // Strip a matching pair of surrounding single/double quotes.
        if (v.size() >= 2
            && ((v.front() == QLatin1Char('"') && v.back() == QLatin1Char('"'))
                || (v.front() == QLatin1Char('\'') && v.back() == QLatin1Char('\''))))
            v = v.mid(1, v.size() - 2);
        return v;
    }
    return QString();
}

FrontMatterParser::TocInfo FrontMatterParser::tocInfo(const QString &markdown)
{
    TocInfo info;
    info.description = value(markdown, QStringLiteral("toc-description"));
    return info;
}

QString FrontMatterParser::blankOut(const QString &markdown)
{
    const auto range = lineRange(markdown);
    if (range.first < 0)
        return markdown;

    QStringList out = splitLines(markdown);
    for (int i = range.first; i <= range.second; ++i)
        out[i] = QString();
    // Join with '\n' (a trailing newline round-trips through split/join as the
    // final empty element), preserving the exact line structure.
    return out.join(QLatin1Char('\n'));
}

QString FrontMatterParser::strip(const QString &markdown)
{
    const auto range = lineRange(markdown);
    if (range.first < 0)
        return markdown;

    const QStringList lines = splitLines(markdown);
    QStringList out;
    for (int i = 0; i < lines.size(); ++i) {
        if (i < range.first || i > range.second)
            out.append(lines[i]);
    }
    return out.join(QLatin1Char('\n'));
}
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
#include "MarkdownParser.h"
#include "MdRenderer.h"
#include "Preferences.h"
#include "Typography.h"
#include <md4c.h>
#include <QRegularExpression>
#include <QSettings>

QString MarkdownParser::substituteDirectives(const QString &markdown)
{
    static const QRegularExpression directiveRe(
        QStringLiteral(R"(^<!--\s*(keep|keep-together|no-break|page-break|break|new-page)\s*-->\s*$)"));

    // Length of a leading code-fence run (` ``` ` / ` ~~~ `, up to 3 leading
    // spaces) of the given char, or 0 if the line is not such a fence.
    auto fenceRun = [](const QString &line, QChar ch) -> int {
        int i = 0;
        while (i < 3 && i < line.size() && line[i] == QLatin1Char(' '))
            ++i;
        int len = 0;
        while (i < line.size() && line[i] == ch) {
            ++i;
            ++len;
        }
        return len >= 3 ? len : 0;
    };

    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QStringList out;
    out.reserve(lines.size());

    bool inFence = false;
    QChar fenceChar;
    int fenceLen = 0;
    int keepCounter = 0;
    int breakCounter = 0;

    for (const QString &line : lines) {
        bool fenceLine = false;
        if (inFence) {
            // Closing fence: same char, at least as long, then whitespace only.
            int i = 0;
            while (i < 3 && i < line.size() && line[i] == QLatin1Char(' '))
                ++i;
            int len = 0;
            while (i < line.size() && line[i] == fenceChar) {
                ++i;
                ++len;
            }
            bool onlyWs = true;
            while (i < line.size()) {
                if (line[i] != QLatin1Char(' ') && line[i] != QLatin1Char('\t')) {
                    onlyWs = false;
                    break;
                }
                ++i;
            }
            if (len >= fenceLen && onlyWs)
                inFence = false;
            fenceLine = true;
        } else {
            for (QChar ch : {QLatin1Char('`'), QLatin1Char('~')}) {
                if (int len = fenceRun(line, ch)) {
                    inFence = true;
                    fenceChar = ch;
                    fenceLen = len;
                    fenceLine = true;
                    break;
                }
            }
        }

        if (fenceLine || inFence) {
            out.append(line);
            continue;
        }

        const QRegularExpressionMatch m = directiveRe.match(line);
        if (m.hasMatch()) {
            const QString word = m.captured(1);
            if (word == QLatin1String("page-break")
                || word == QLatin1String("break")
                || word == QLatin1String("new-page")) {
                out.append(QStringLiteral("SCRIBADIRB%1").arg(++breakCounter));
            } else {
                out.append(QStringLiteral("SCRIBADIRK%1").arg(++keepCounter));
            }
        } else {
            out.append(line);
        }
    }

    // Preserve the trailing newline (no trimmed()): data-line behavior must
    // stay identical to today.
    return out.join(QLatin1Char('\n'));
}

QString MarkdownParser::toHtml(const QString &markdown, bool noHtml)
{
    QByteArray utf8 = substituteDirectives(markdown).toUtf8();

    unsigned long parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH
                              | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEAUTOLINKS
                              | MD_FLAG_ADMONITIONS | MD_FLAG_HIGHLIGHT
                              | MD_FLAG_SUPERSCRIPTS | MD_FLAG_SUBSCRIPTS
                              | MD_FLAG_FOOTNOTES | MD_FLAG_LATEXMATHSPANS;

    QSettings settings;
    if (settings.value(Preferences::HardSoftBreaks, false).toBool())
        parserFlags |= MD_FLAG_HARD_SOFT_BREAKS;

    if (noHtml)
        parserFlags |= MD_FLAG_NOHTML;

    MdRenderer renderer;
    renderer.setTypography(Typography::optionsFromSettings());
    return renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), parserFlags);
}

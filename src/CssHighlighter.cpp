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
#include "CssHighlighter.h"
#include <QTextDocument>

CssHighlighter::CssHighlighter(const QString &themeCss, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    auto extractHljsColor = [&](const QString &className) -> QColor {
        QRegularExpression re(
            QStringLiteral("\\.hljs-%1[^{]*\\{[^}]*?color\\s*:\\s*([^;}\"]+)").arg(className));
        QRegularExpressionMatchIterator it = re.globalMatch(themeCss);
        QColor result;
        while (it.hasNext())
            result = QColor(it.next().captured(1).trimmed());
        return result;
    };

    QColor commentCol   = extractHljsColor("comment");
    QColor stringCol    = extractHljsColor("string");
    QColor keywordCol   = extractHljsColor("keyword");
    QColor attrCol      = extractHljsColor("attr");
    QColor numberCol    = extractHljsColor("number");
    QColor symbolCol    = extractHljsColor("symbol");
    QColor nameCol      = extractHljsColor("name");
    QColor builtInCol   = extractHljsColor("built_in");
    QColor punctCol     = extractHljsColor("punctuation");

    auto hasColor = [](const QColor &c) { return c.isValid() && c.alpha() > 0; };

    m_commentFormat.setForeground(hasColor(commentCol) ? commentCol : QColor("#999999"));
    m_commentFormat.setFontItalic(true);

    m_punctFormat.setForeground(hasColor(punctCol) ? punctCol : QColor("#999999"));

    auto makeFormat = [](const QColor &col, bool bold = false) {
        QTextCharFormat f;
        f.setForeground(col);
        if (bold) f.setFontWeight(QFont::Bold);
        return f;
    };

    // Multi-line comments: /* ... */
    // (rules are empty; handled entirely in highlightBlock via block state)

    // Strings: "..." or '...'
    if (hasColor(stringCol))
        m_rules.append({QRegularExpression(R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')"),
                        makeFormat(stringCol)});

    // !important
    if (hasColor(keywordCol))
        m_rules.append({QRegularExpression(QStringLiteral("!important")),
                        makeFormat(keywordCol, true)});

    // At-rules: @media, @font-face, etc.
    if (hasColor(keywordCol))
        m_rules.append({QRegularExpression(QStringLiteral("[@][a-zA-Z][\\w-]*")),
                        makeFormat(keywordCol)});

    // Properties: word followed by :
    if (hasColor(attrCol))
        m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z][\\w-]*(?=\\s*:)")),
                        makeFormat(attrCol)});

    // Hex colors: # followed by 3-8 hex digits
    if (hasColor(symbolCol))
        m_rules.append({QRegularExpression(QStringLiteral("#(?:[0-9a-fA-F]{3,8})\\b")),
                        makeFormat(symbolCol)});

    // Numbers with units: e.g. 16px, 2.5em, 100%, 1.45
    if (hasColor(numberCol))
        m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+\\.?\\d*(?:px|em|rem|%|pt|cm|mm|in|vh|vw|vmin|vmax|fr|s|ms|deg|rad|turn)?\\b")),
                        makeFormat(numberCol)});

    // Selectors: identifiers not followed by : (so properties are excluded)
    if (hasColor(nameCol)) {
        QTextCharFormat selectorFmt = makeFormat(nameCol);
        selectorFmt.setFontWeight(QFont::Bold);
        m_rules.append({QRegularExpression(QStringLiteral("(?<=^|[{,\\s])\\s*([a-zA-Z][\\w-]*)(?!\\s*:)")),
                        selectorFmt});
    }
}

void CssHighlighter::highlightBlock(const QString &text)
{
    // Handle multi-line comments /* ... */
    int startIndex = 0;
    if (previousBlockState() != 1) {
        int idx = text.indexOf("/*");
        if (idx >= 0) {
            startIndex = idx;
        } else {
            // No comment start — apply rules to entire text
            for (const auto &rule : m_rules) {
                QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
                while (it.hasNext()) {
                    QRegularExpressionMatch match = it.next();
                    setFormat(match.capturedStart(), match.capturedLength(), rule.format);
                }
            }
            return;
        }
    }

    // We're inside or entering a comment
    if (previousBlockState() == 1) {
        startIndex = 0;
    }

    int endIndex = text.indexOf("*/", startIndex);
    if (endIndex < 0) {
        // Comment continues to next block
        setCurrentBlockState(1);
        setFormat(startIndex, text.length() - startIndex, m_commentFormat);
    } else {
        // Comment ends on this line
        setFormat(startIndex, endIndex + 2 - startIndex, m_commentFormat);
        // Apply rules to text after the comment end
        for (const auto &rule : m_rules) {
            QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text, endIndex + 2);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

    // Also apply rules to text before the comment
    if (startIndex > 0) {
        for (const auto &rule : m_rules) {
            QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text, 0);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                if (match.capturedEnd() <= startIndex)
                    setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }
}

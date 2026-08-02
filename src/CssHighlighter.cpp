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

CssHighlighter::Palette CssHighlighter::paletteFor(bool dark)
{
    if (dark) {
        return {
            QColor("#1e1e1e"), // background
            QColor("#d4d4d4"), // foreground
            QColor("#6a9955"), // comment
            QColor("#8c8c8c"), // punct
            QColor("#ce9178"), // string
            QColor("#569cd6"), // keyword / at-rule
            QColor("#9cdcfe"), // property
            QColor("#d19a66"), // hexColor
            QColor("#b5cea8"), // number
            QColor("#d7ba7d"), // selector
        };
    }
    return {
        QColor("#ffffff"), // background
        QColor("#24292e"), // foreground
        QColor("#6a737d"), // comment
        QColor("#6a737d"), // punct
        QColor("#032f62"), // string
        QColor("#d73a49"), // keyword / at-rule
        QColor("#005cc5"), // property
        QColor("#6f42c1"), // hexColor
        QColor("#098658"), // number
        QColor("#22863a"), // selector
    };
}

CssHighlighter::CssHighlighter(bool dark, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    const Palette p = paletteFor(dark);

    auto makeFormat = [](const QColor &col, bool bold = false) {
        QTextCharFormat f;
        f.setForeground(col);
        if (bold) f.setFontWeight(QFont::Bold);
        return f;
    };

    m_commentFormat.setForeground(p.comment);
    m_commentFormat.setFontItalic(true);

    // Strings: "..." or '...'
    m_rules.append({QRegularExpression(R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')"),
                    makeFormat(p.string)});

    // !important
    m_rules.append({QRegularExpression(QStringLiteral("!important")),
                    makeFormat(p.keyword, true)});

    // At-rules: @media, @font-face, etc.
    m_rules.append({QRegularExpression(QStringLiteral("[@][a-zA-Z][\\w-]*")),
                    makeFormat(p.keyword)});

    // Properties: word followed by :
    m_rules.append({QRegularExpression(QStringLiteral("\\b[a-zA-Z][\\w-]*(?=\\s*:)")),
                    makeFormat(p.property)});

    // Hex colors: # followed by 3-8 hex digits
    m_rules.append({QRegularExpression(QStringLiteral("#(?:[0-9a-fA-F]{3,8})\\b")),
                    makeFormat(p.hexColor)});

    // Numbers with units: e.g. 16px, 2.5em, 100%, 1.45
    m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+\\.?\\d*(?:px|em|rem|%|pt|cm|mm|in|vh|vw|vmin|vmax|fr|s|ms|deg|rad|turn)?\\b")),
                    makeFormat(p.number)});

    // Selectors: identifiers not followed by : (so properties are excluded)
    QTextCharFormat selectorFmt = makeFormat(p.selector, true);
    m_rules.append({QRegularExpression(QStringLiteral("(?<=^|[{,\\s])\\s*([a-zA-Z][\\w-]*)\\b(?!\\s*:)")),
                    selectorFmt});
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

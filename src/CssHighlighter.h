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

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class CssHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    struct Palette {
        QColor background;
        QColor foreground;
        QColor comment;
        QColor punct;
        QColor string;
        QColor keyword;
        QColor property;
        QColor hexColor;
        QColor number;
        QColor selector;
    };

    static Palette paletteFor(bool dark);

    explicit CssHighlighter(bool dark, QTextDocument *parent);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;
    QTextCharFormat m_commentFormat;
};

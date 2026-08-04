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

#include <QString>
#include <QUrl>

class HtmlToMarkdown
{
public:
    // Converts an HTML fragment or full document to Markdown. Relative
    // href/src URLs are resolved against baseUrl when it is valid. Returns
    // the Markdown text, or a plain-text fallback when nothing convertible
    // is found.
    static QString convert(const QString &html, const QUrl &baseUrl = {},
                           int timeoutMs = 30000);
};

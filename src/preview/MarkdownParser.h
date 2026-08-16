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

class MarkdownParser
{
public:
    static QString toHtml(const QString &markdown, bool noHtml = false);

private:
    // Replaces recognized `<!-- keep -->` / `<!-- break -->` comment lines
    // with SCRIBADIRK<n> / SCRIBADIRB<n> marker tokens (flush-left, own-line,
    // never inside fenced code). The MdRenderer strips the tokens and turns the
    // markers into scriba-* classes on the next top-level block.
    static QString substituteDirectives(const QString &markdown);
};


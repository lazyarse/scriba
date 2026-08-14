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

#include <QHash>
#include <QList>
#include <QString>

class Corpus;

// Renders a markdown Table of Contents for a Corpus as a virtual document
// (editable tab, not part of the saved corpus).
class CorpusIndex
{
public:
    struct Heading { int level = 0; QString title; };
    // Fence-aware ATX heading scan (skips ``` fences and ~~~ blocks).
    static QList<Heading> extractHeadings(const QString &markdown);
    // Markdown TOC. pageLinkByAbs maps each document's absolute path to the
    // link used on the FILE row (source-relative path in-app, exported page
    // name during export). Out-of-root docs map to an absolute file:/// URL.
    static QString renderToc(const Corpus &corpus,
                             const QHash<QString, QString> &pageLinkByAbs);
};

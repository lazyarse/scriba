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
    // The two Scriba-managed markers. Everything between the start and end
    // marker lines is Scriba-owned and regenerated; text outside is the user's.
    static QString tocStartMarker();
    static QString tocEndMarker();
    // The marker region filled with the rendered links (no "# Table of
    // Contents" heading / "Corpus:" line — the user owns those). `fullText`
    // is the current toc.md content; if both markers are present the region
    // between them is replaced (text before/after preserved), if only the
    // start marker is present it is replaced through end-of-file and the end
    // marker appended, and if neither is present a fresh block is appended at
    // EOF. Returns the input unchanged when the result would be identical.
    static QString replaceTocBlock(const QString &fullText, const QString &linksMd);
    // Built-in template used when the user's preference template is empty.
    static QString defaultTocTemplate();
    // The links-only markdown (bullets + optional "## External documents"
    // section) that lives inside the marker region.
    static QString renderTocLinks(const Corpus &corpus,
                                  const QHash<QString, QString> &pageLinkByAbs);
};

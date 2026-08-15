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

#include <QPair>
#include <QString>

// YAML frontmatter parsing for Scriba documents.
//
// A YAML frontmatter block is recognized only when it starts the document: the
// first non-empty line must be `---`, the block runs to the next `---` line,
// and an unterminated opener is NOT frontmatter (it is a horizontal rule) —
// this mirrors the lint engine's frontMatter convention.
//
// Value extraction is scalar-only: `key: value` lines where the value is
// trimmed and a matching surrounding pair of single/double quotes is stripped.
// Block scalars (`|`, `>`) and nested maps are out of scope.
class FrontMatterParser
{
public:
    struct TocInfo {
        QString description;   // from toc-description:
    };

    // 0-based inclusive line range of the leading `--- ... ---` block, or
    // {-1, -1} when there is no frontmatter.
    static QPair<int, int> lineRange(const QString &markdown);
    static bool hasFrontMatter(const QString &markdown);
    // Trimmed, dequoted value of `key:` inside the block; empty when absent.
    static QString value(const QString &markdown, const QString &key);
    static TocInfo tocInfo(const QString &markdown);
    // Replace the block's lines with blank lines, preserving the line count
    // (renderers that emit per-line data-line attributes keep scroll-sync and
    // click-navigation alignment; md4c emits nothing for blank lines).
    static QString blankOut(const QString &markdown);
    // Remove the block lines entirely (for consumers that need no line numbers).
    static QString strip(const QString &markdown);
};
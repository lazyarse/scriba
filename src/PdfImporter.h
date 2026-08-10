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
#include "StaticHelpers.h"

struct PdfImportResult {
    bool ok = false;
    QString markdown;
    int pages = 0;
    QString error;
};

// Converts a PDF file to Markdown by driving bundled pdf.js (V4, standalone
// build) inside a hidden QWebEnginePage. Text runs and their font metrics are
// extracted per page and run through pdf2md.js, which heuristically rebuilds
// headings, paragraphs, code blocks, lists, tables and cross-line hyphenation.
class PdfImporter
{
public:
    static PdfImportResult convert(const QString &filePath,
                                   int timeoutMs = Timeout::RenderTimeoutMs);
};
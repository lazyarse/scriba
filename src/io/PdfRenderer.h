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

// Headless PDF rendering for batch export (the interactive Print / Export PDF
// dialog owns its own generation in ExportPdfDialog). Used by the corpus
// exporter, which must not prompt one dialog per file.
class PdfRenderer
{
public:
    // Renders a complete <html> document to a PDF at outPath. Returns false on
    // load, JS-timeout, or write failure. Layout: A4 portrait, 15 mm margins
    // (matches the base print defaults).
    static bool render(const QString &fullHtml, const QString &baseUrl,
                       const QString &outPath);
};

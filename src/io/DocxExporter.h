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

enum class DocxMathMode {
    Images,
    Omml
};

struct DocxExportOptions {
    DocxMathMode mathMode = DocxMathMode::Images;
    bool landscape = false;
    double marginTopCm = 2.54;
    double marginBottomCm = 2.54;
    double marginLeftCm = 2.54;
    double marginRightCm = 2.54;
    bool pageNumbers = false;
};

class DocxExporter
{
public:
    static bool exportToDocx(const QString &html, const QString &outputPath,
                             const QString &css = QString(),
                             const DocxExportOptions &options = {});
};

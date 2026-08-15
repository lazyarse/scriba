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
    // Optional user .docx template. When set, the template owns ALL styling and
    // page setup (styles.xml, theme, headers/footers, sectPr); landscape/margins/
    // pageNumbers below are ignored. When empty, Scriba's fixed default styles
    // are used (see kDefaultTemplateCss).
    QString templatePath;
};

class DocxExporter
{
public:
    static bool exportToDocx(const QString &html, const QString &outputPath,
                             const DocxExportOptions &options = {});
    // Write a .docx template whose styles.xml is generated from themeCss (hex
    // colors are captured; rgb()/hsl()/CSS vars fall back to defaults) with an
    // empty body — a starting point users open in Word, customize, and reuse
    // via DocxExportOptions::templatePath.
    static bool saveAsTemplate(const QString &outputPath, const QString &themeCss);
};

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

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// A binary image extracted from the package's word/media/ directory, keyed by
// the relationship id used in the document ("docximg://<rId>" placeholders in
// the produced HTML). The importer resolves each placeholder to a file.
struct OoxmlImportedImage {
    QString rId;
    QString fileName;    // e.g. "image1.png"
    QString contentType; // MIME type inferred from the extension
    QByteArray data;
};

struct OoxmlToHtmlResult {
    QString html;          // body fragment; images referenced as docximg://<rId>
    QVector<OoxmlImportedImage> images;
    QStringList errors;    // non-fatal warnings (dropped EMF/WMF, math fallbacks)
    bool ok = false;
};

// Converts the parsed parts of a .docx (OPC) package into an HTML body
// fragment that can be fed to HtmlToMarkdown. This is the structural inverse
// of HtmlToOoxml: paragraphs/runs/styles/tables/lists/links/images come out as
// HTML, and Office Math (OMML) becomes LaTeX ($...$ / $$...$$) via the
// vendored OmmlToMathml -> MathmlToLatex pipeline.
class OoxmlToHtml {
public:
    // `parts` maps package entry names (e.g. "word/document.xml",
    // "word/styles.xml", "word/media/image1.png") to their raw bytes.
    static OoxmlToHtmlResult convert(const QHash<QString, QByteArray> &parts);
};
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
#include <QStringList>

struct DocxImportOptions {
    enum class ImageLocation { CurrentDir, CustomDir, TempDir, Ask };
    ImageLocation imageLocation = ImageLocation::CurrentDir;
    QString customImageDir;  // for CustomDir
    QString documentDir;     // where the .md will live (for relative refs)
};

struct DocxImportResult {
    QString markdown;
    QStringList writtenImages;  // files written during import
    QStringList warnings;       // dropped EMF/WMF, math fallbacks, skipped images
    bool ok = false;
    QString error;
};

// Reads a Word (.docx) package, converts the body OOXML to Markdown and
// resolves embedded images according to the caller's preference. Orchestrates
// ZipReader + OoxmlToHtml + HtmlToMarkdown.
class DocxImporter {
public:
    static DocxImportResult import(const QString &docxPath, const DocxImportOptions &opts);
};

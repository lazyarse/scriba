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
#include "DocxImporter.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUrl>

#include "HtmlToMarkdown.h"
#include "OoxmlToHtml.h"
#include "ZipReader.h"

namespace {

// Extension for an image MIME type, matching the OOXML part extension.
QString extensionFor(const QString &contentType)
{
    if (contentType == QLatin1String("image/png")) return QStringLiteral("png");
    if (contentType == QLatin1String("image/jpeg")) return QStringLiteral("jpg");
    if (contentType == QLatin1String("image/gif")) return QStringLiteral("gif");
    if (contentType == QLatin1String("image/svg+xml")) return QStringLiteral("svg");
    if (contentType == QLatin1String("image/webp")) return QStringLiteral("webp");
    if (contentType == QLatin1String("image/bmp")) return QStringLiteral("bmp");
    if (contentType == QLatin1String("image/tiff")) return QStringLiteral("tiff");
    return QString();
}

} // namespace

DocxImportResult DocxImporter::import(const QString &docxPath, const DocxImportOptions &opts)
{
    DocxImportResult res;

    ZipReader zip(docxPath);
    QString zipError;
    if (!zip.open(&zipError)) {
        res.error = "Could not open " + docxPath + ":\n" + zipError;
        return res;
    }

    QHash<QString, QByteArray> parts;
    for (const QString &entry : zip.entryNames())
        parts.insert(entry, zip.readEntry(entry));
    if (!parts.contains(QStringLiteral("word/document.xml"))) {
        res.error = "Not a valid Word document (missing word/document.xml).";
        return res;
    }

    OoxmlToHtmlResult converted = OoxmlToHtml::convert(parts);
    if (!converted.ok) {
        res.error = converted.errors.isEmpty()
            ? "Could not convert the Word document."
            : converted.errors.join('\n');
        return res;
    }
    res.warnings = converted.errors;
    QString html = converted.html;

    // Decide where images are written before touching the HTML.
    DocxImportOptions::ImageLocation location = opts.imageLocation;
    if (location == DocxImportOptions::ImageLocation::CurrentDir
        && opts.documentDir.isEmpty()) {
        // No document on disk yet; a relative "next to the doc" path cannot be
        // expressed, so escalate to asking.
        location = DocxImportOptions::ImageLocation::Ask;
    }

    QString targetDir;
    bool useRelative = false;
    QString referenceRoot;  // absolute dir refs are relative to

    switch (location) {
    case DocxImportOptions::ImageLocation::CurrentDir: {
        targetDir = QDir(opts.documentDir).filePath(QStringLiteral("media"));
        useRelative = true;
        break;
    }
    case DocxImportOptions::ImageLocation::CustomDir: {
        targetDir = opts.customImageDir;
        if (targetDir.isEmpty()) {
            res.error = "No image folder configured.";
            return res;
        }
        useRelative = !opts.documentDir.isEmpty();
        break;
    }
    case DocxImportOptions::ImageLocation::TempDir: {
        QTemporaryDir *temp = new QTemporaryDir();  // owned for process lifetime
        if (temp->isValid()) {
            temp->setAutoRemove(false);
            targetDir = temp->path();
        } else {
            targetDir = QDir::tempPath();
        }
        useRelative = false;
        res.warnings << "Imported images were written to " + targetDir
                     + "; save the document to move them next to it.";
        break;
    }
    case DocxImportOptions::ImageLocation::Ask: {
        targetDir = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("Choose Folder for Imported Images"),
            opts.documentDir);
        if (targetDir.isEmpty())
            res.warnings << "Skipped images (no folder chosen).";
        useRelative = !opts.documentDir.isEmpty() && !targetDir.isEmpty();
        break;
    }
    }

    if (targetDir.isEmpty()) {
        // No folder chosen (Ask cancelled, or unsaved doc without a configured
        // folder): drop the image elements rather than leak docximg:// refs.
        res.warnings << "Images were skipped.";
        html.remove(QRegularExpression(QStringLiteral(
            "<img[^>]*docximg://[^>]*>")));
    } else {
        QDir().mkpath(targetDir);

        QString html2 = html;
        if (!converted.images.isEmpty()) {
            int index = 0;
            for (const OoxmlImportedImage &img : converted.images) {
                const QString ext = extensionFor(img.contentType);
                const QString name = ext.isEmpty()
                    ? QStringLiteral("image%1").arg(++index)
                    : QStringLiteral("image%1.%2").arg(++index).arg(ext);
                const QString filePath = QDir(targetDir).filePath(name);

                QFile out(filePath);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(img.data);
                    out.close();
                    res.writtenImages << filePath;
                } else {
                    res.warnings << "Could not write image " + filePath;
                    continue;
                }

                QString ref;
                if (useRelative) {
                    ref = QDir(opts.documentDir).relativeFilePath(filePath);
                } else {
                    ref = QUrl::fromLocalFile(filePath).toString();
                }
                html2.replace(QStringLiteral("docximg://") + img.rId, ref);
            }
        }
        html = html2;
    }

    const QString markdown = HtmlToMarkdown::convert(html);
    if (markdown.trimmed().isEmpty()) {
        res.error = "No convertible content was found in the file.";
        return res;
    }

    res.markdown = markdown;
    res.ok = true;
    return res;
}

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
#include "DocxExporter.h"
#include "HtmlToOoxml.h"
#include "ZipReader.h"
#include <QDataStream>
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <QPair>
#include <QRegularExpression>
#include <QtMath>
#include <QXmlStreamWriter>
#include "miniz.h"

Q_LOGGING_CATEGORY(lcDocx, "scriba.docx")

static const char *kMimeType = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

// Curated default styling for DOCX export when no user template is chosen.
// buildStylesXml() regex-scrapes exactly these selectors/properties (hex only);
// everything else in the generated styles.xml (Normal font, SourceCode shading,
// Quote color) is hardcoded in buildStylesXml regardless of this CSS.
static const char *kDefaultTemplateCss = R"CSS(
h1 { color: #1F3864; }
h2 { color: #2B579A; }
h3 { color: #3B78B4; }
h4 { color: #4A8BC2; }
h5 { color: #5A9BD5; }
h6 { color: #6BA5D8; }
.admonition.note { border-left-color: #2B579A; background-color: #E8F4FD; }
.admonition.note .admonition-title { color: #2B579A; }
.admonition.tip { border-left-color: #1A7F37; background-color: #DAFBE1; }
.admonition.tip .admonition-title { color: #1A7F37; }
.admonition.important { border-left-color: #9A6700; background-color: #FFF8C5; }
.admonition.important .admonition-title { color: #9A6700; }
.admonition.warning { border-left-color: #CF222E; background-color: #FFEBE9; }
.admonition.warning .admonition-title { color: #CF222E; }
.admonition.caution { border-left-color: #CF222E; background-color: #FFEBE9; }
.admonition.caution .admonition-title { color: #CF222E; }
)CSS";

struct ZipEntry {
    QString name;
    QByteArray data;      // uncompressed payload
    QByteArray compData;  // DEFLATE-compressed payload (method 8); empty when STORE
    uint16_t method = 0;  // 0 = STORE, 8 = DEFLATE
    uint32_t crc32 = 0;
    uint32_t localOffset = 0;
};

// PKZip CRC-32 via miniz (linked for the DOCX importer); the exporter reuses
// the same code rather than duplicating a CRC table.
static uint32_t zipCrc32(const QByteArray &data)
{
    return mz_crc32(MZ_CRC32_INIT,
                    reinterpret_cast<const unsigned char *>(data.constData()),
                    size_t(data.size()));
}

// Compress with raw DEFLATE (no zlib header), as ZIP method 8 requires.
// Returns the compressed bytes, or a null QByteArray on failure. The caller
// falls back to STORE if compression fails or does not shrink the data.
static QByteArray deflateRaw(const QByteArray &data)
{
    size_t outLen = 0;
    void *buf = tdefl_compress_mem_to_heap(data.constData(), data.size(),
                                           &outLen, TDEFL_DEFAULT_MAX_PROBES);
    if (!buf)
        return {};
    QByteArray out(static_cast<const char *>(buf), int(outLen));
    mz_free(buf);
    return out;
}

static QByteArray buildContentTypes(int imageCount, bool hasFooter = false,
                                    bool hasSvg = false)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("Types");
    w.writeDefaultNamespace("http://schemas.openxmlformats.org/package/2006/content-types");
    w.writeStartElement("Default");
    w.writeAttribute("Extension", "rels");
    w.writeAttribute("ContentType",
        "application/vnd.openxmlformats-package.relationships+xml");
    w.writeEndElement();
    w.writeStartElement("Override");
    w.writeAttribute("PartName", "/word/document.xml");
    w.writeAttribute("ContentType",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
    w.writeEndElement();
    w.writeStartElement("Override");
    w.writeAttribute("PartName", "/word/styles.xml");
    w.writeAttribute("ContentType",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml");
    w.writeEndElement();
    w.writeStartElement("Override");
    w.writeAttribute("PartName", "/word/numbering.xml");
    w.writeAttribute("ContentType",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml");
    w.writeEndElement();
    w.writeStartElement("Override");
    w.writeAttribute("PartName", "/word/fontTable.xml");
    w.writeAttribute("ContentType",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.fontTable+xml");
    w.writeEndElement();
    if (imageCount > 0) {
        w.writeStartElement("Default");
        w.writeAttribute("Extension", "png");
        w.writeAttribute("ContentType", "image/png");
        w.writeEndElement();
    }
    if (hasSvg) {
        w.writeStartElement("Default");
        w.writeAttribute("Extension", "svg");
        w.writeAttribute("ContentType", "image/svg+xml");
        w.writeEndElement();
    }
    if (hasFooter) {
        w.writeStartElement("Override");
        w.writeAttribute("PartName", "/word/footer1.xml");
        w.writeAttribute("ContentType",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml");
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

static QByteArray buildDocumentXml(const OoxmlResult &ooxml,
                                   bool landscape,
                                   double marginTopCm,
                                   double marginBottomCm,
                                   double marginLeftCm,
                                   double marginRightCm,
                                   const QString &footerRelId = {})
{
    // A4 page size: 11906 TWIP wide, 16838 TWIP tall (21cm x 29.7cm)
    int pageW = landscape ? 16838 : 11906;
    int pageH = landscape ? 11906 : 16838;

    int marginT = qRound(marginTopCm * 567.0);
    int marginB = qRound(marginBottomCm * 567.0);
    int marginL = qRound(marginLeftCm * 567.0);
    int marginR = qRound(marginRightCm * 567.0);

    QByteArray doc(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
        " xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\""
        " xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\""
        " xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\""
        " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\""
        " xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\">\n"
        "<w:body>\n"
    );
    doc.append(ooxml.bodyXml.toUtf8());
    doc.append("<w:sectPr>");
    if (!footerRelId.isEmpty()) {
        doc.append("<w:footerReference w:type=\"default\" r:id=\"");
        doc.append(footerRelId.toUtf8());
        doc.append("\"/>");
    }
    doc.append("<w:pgSz w:w=\"");
    doc.append(QByteArray::number(pageW));
    doc.append("\" w:h=\"");
    doc.append(QByteArray::number(pageH));
    doc.append("\"/>");
    doc.append("<w:pgMar w:top=\"");
    doc.append(QByteArray::number(marginT));
    doc.append("\" w:right=\"");
    doc.append(QByteArray::number(marginR));
    doc.append("\" w:bottom=\"");
    doc.append(QByteArray::number(marginB));
    doc.append("\" w:left=\"");
    doc.append(QByteArray::number(marginL));
    doc.append("\" w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/>");
    doc.append("</w:sectPr>\n");
    doc.append("</w:body>\n</w:document>\n");
    return doc;
}

static QByteArray buildFooterXml()
{
    return QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:ftr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
        "  <w:p>\n"
        "    <w:pPr>\n"
        "      <w:jc w:val=\"center\"/>\n"
        "    </w:pPr>\n"
        "    <w:r>\n"
        "      <w:fldSimple w:instr=\" PAGE   \\* MERGEFORMAT \"/>\n"
        "    </w:r>\n"
        "  </w:p>\n"
        "</w:ftr>\n"
    );
}

static QByteArray buildFontTableXml(const QVector<QString> &fontFamilies)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("w:fonts");
    w.writeDefaultNamespace("http://schemas.openxmlformats.org/wordprocessingml/2006/main");

    struct FontDef { QString name; QString altName; QString family; bool fixed; };
    const FontDef defaults[] = {
        {"Calibri", "Arial", "swiss", false},
        {"Calibri Light", {}, "swiss", false},
        {"Arial", {}, "swiss", false},
        {"Times New Roman", {}, "roman", false},
        {"Courier New", {}, "modern", true},
        {"Symbol", {}, "roman", false},
    };

    for (const auto &fd : defaults) {
        w.writeStartElement("w:font");
        w.writeAttribute("w:name", fd.name);
        if (!fd.altName.isEmpty()) {
            w.writeStartElement("w:altName");
            w.writeAttribute("w:val", fd.altName);
            w.writeEndElement();
        }
        w.writeStartElement("w:family");
        w.writeAttribute("w:val", fd.family);
        w.writeEndElement();
        w.writeStartElement("w:pitch");
        w.writeAttribute("w:val", fd.fixed ? "fixed" : "variable");
        w.writeEndElement();
        w.writeEndElement();
    }

    for (const auto &f : fontFamilies) {
        QString lower = f.toLower();
        QString family = QStringLiteral("swiss");
        if (lower.contains("mono") || lower.contains("courier") || lower == "consolas" || lower == "menlo")
            family = QStringLiteral("modern");
        else if (lower.contains("serif") || lower.contains("times") || lower.contains("roman") || lower == "georgia")
            family = QStringLiteral("roman");
        w.writeStartElement("w:font");
        w.writeAttribute("w:name", f);
        w.writeStartElement("w:family");
        w.writeAttribute("w:val", family);
        w.writeEndElement();
        w.writeStartElement("w:pitch");
        w.writeAttribute("w:val", family == "modern" ? "fixed" : "variable");
        w.writeEndElement();
        w.writeEndElement();
    }

    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

static QByteArray buildRels(const QVector<OoxmlImage> &images,
                            const QVector<OoxmlHyperlink> &hyperlinks,
                            bool hasFooter = false)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("Relationships");
    w.writeDefaultNamespace("http://schemas.openxmlformats.org/package/2006/relationships");
    w.writeStartElement("Relationship");
    w.writeAttribute("Id", "rId1");
    w.writeAttribute("Type", ooxmlRelTypeUri(OoxmlRelType::Styles));
    w.writeAttribute("Target", "styles.xml");
    w.writeEndElement();
    w.writeStartElement("Relationship");
    w.writeAttribute("Id", "rId2");
    w.writeAttribute("Type", ooxmlRelTypeUri(OoxmlRelType::Numbering));
    w.writeAttribute("Target", "numbering.xml");
    w.writeEndElement();
    if (hasFooter) {
        w.writeStartElement("Relationship");
        w.writeAttribute("Id", "rId3");
        w.writeAttribute("Type",
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer");
        w.writeAttribute("Target", "footer1.xml");
        w.writeEndElement();
    }
    for (const auto &img : images) {
        w.writeStartElement("Relationship");
        w.writeAttribute("Id", img.relId);
        w.writeAttribute("Type", ooxmlRelTypeUri(OoxmlRelType::Image));
        w.writeAttribute("Target", img.fileName);
        w.writeEndElement();
        if (!img.svgRelId.isEmpty()) {
            w.writeStartElement("Relationship");
            w.writeAttribute("Id", img.svgRelId);
            w.writeAttribute("Type", ooxmlRelTypeUri(OoxmlRelType::Image));
            w.writeAttribute("Target", img.svgFileName);
            w.writeEndElement();
        }
    }
    for (const auto &hl : hyperlinks) {
        w.writeStartElement("Relationship");
        w.writeAttribute("Id", hl.relId);
        w.writeAttribute("Type", ooxmlRelTypeUri(OoxmlRelType::Hyperlink));
        w.writeAttribute("Target", hl.target);
        w.writeAttribute("TargetMode", "External");
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

static QByteArray buildPackageRels()
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("Relationships");
    w.writeDefaultNamespace("http://schemas.openxmlformats.org/package/2006/relationships");
    w.writeStartElement("Relationship");
    w.writeAttribute("Id", "rId1");
    w.writeAttribute("Type",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");
    w.writeAttribute("Target", "word/document.xml");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

static QByteArray buildCoreProps()
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("cp:coreProperties");
    w.writeDefaultNamespace(
        "http://schemas.openxmlformats.org/package/2006/metadata/core-properties");
    w.writeNamespace("cp",
        "http://schemas.openxmlformats.org/package/2006/metadata/core-properties");
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

// Write a unsigned 16-bit integer in little-endian
static void writeU16(QDataStream &ds, uint16_t v)
{
    ds.device()->write(reinterpret_cast<const char*>(&v), 2);
}

// Write a unsigned 32-bit integer in little-endian
static void writeU32(QDataStream &ds, uint32_t v)
{
    ds.device()->write(reinterpret_cast<const char*>(&v), 4);
}

static bool writeZip(QFile &out, const QVector<ZipEntry> &entries)
{
    QDataStream ds(&out);
    // Write local file headers + data
    uint32_t centralOffset = 0;
    QByteArray centralDir;

    for (const auto &entry : entries) {
        const QByteArray &stored = entry.method == 8 ? entry.compData : entry.data;
        // Local file header
        writeU32(ds, 0x04034b50);
        writeU16(ds, 20);      // version needed
        writeU16(ds, 0);       // flags
        writeU16(ds, entry.method);  // compression method (0 STORE / 8 DEFLATE)
        writeU16(ds, 0);       // mod time
        writeU16(ds, 0);       // mod date
        writeU32(ds, entry.crc32);
        writeU32(ds, stored.size());  // compressed size
        writeU32(ds, entry.data.size());  // uncompressed size
        QByteArray nameBytes = entry.name.toUtf8();
        writeU16(ds, nameBytes.size());   // filename length
        writeU16(ds, 0);       // extra field length
        ds.writeRawData(nameBytes.constData(), nameBytes.size());

        // File data
        ds.writeRawData(stored.constData(), stored.size());

        // Central directory entry
        QByteArray cdEntry;
        QDataStream cdDs(&cdEntry, QIODevice::WriteOnly);
        writeU32(cdDs, 0x02014b50);
        writeU16(cdDs, 20);      // version made by
        writeU16(cdDs, 20);      // version needed
        writeU16(cdDs, 0);       // flags
        writeU16(cdDs, entry.method);  // compression method (0 STORE / 8 DEFLATE)
        writeU16(cdDs, 0);       // mod time
        writeU16(cdDs, 0);       // mod date
        writeU32(cdDs, entry.crc32);
        writeU32(cdDs, stored.size());  // compressed size
        writeU32(cdDs, entry.data.size());  // uncompressed size
        writeU16(cdDs, nameBytes.size());
        writeU16(cdDs, 0);       // extra field length
        writeU16(cdDs, 0);       // file comment length
        writeU16(cdDs, 0);       // disk number start
        writeU16(cdDs, 0);       // internal attrs
        writeU32(cdDs, 0);       // external attrs
        writeU32(cdDs, entry.localOffset);
        cdDs.writeRawData(nameBytes.constData(), nameBytes.size());
        centralDir.append(cdEntry);
    }

    // Write central directory
    centralOffset = out.pos();
    ds.writeRawData(centralDir.constData(), centralDir.size());

    // End of central directory
    writeU32(ds, 0x06054b50);
    writeU16(ds, 0);            // disk number
    writeU16(ds, 0);            // disk with CD
    writeU16(ds, entries.size()); // entries on this disk
    writeU16(ds, entries.size()); // total entries
    writeU32(ds, centralDir.size());
    writeU32(ds, centralOffset);
    writeU16(ds, 0);            // comment length

    return true;
}

// Replace everything between <w:body> and the template's trailing <w:sectPr>
// (the last child of body) with the generated content, keeping the sectPr so
// the template's page setup and header/footer references survive.
static bool insertBody(QString &doc, const QString &bodyXml)
{
    int bodyStart = doc.indexOf(QLatin1String("<w:body"));
    if (bodyStart < 0) return false;
    bodyStart = doc.indexOf('>', bodyStart);
    if (bodyStart < 0) return false;
    ++bodyStart;
    const int bodyEnd = doc.lastIndexOf(QLatin1String("</w:body>"));
    if (bodyEnd < 0) return false;
    const int sectStart = doc.lastIndexOf(QLatin1String("<w:sectPr"), bodyEnd);
    const QString tail = sectStart > bodyStart
        ? doc.mid(sectStart, bodyEnd - sectStart) : QString();
    doc = doc.left(bodyStart) + bodyXml
        + (tail.isEmpty() ? QString() : QStringLiteral("\n") + tail)
        + doc.mid(bodyEnd);
    return true;
}

// The generated body uses wp:/a:/pic:/m:/r: on the root element; Word-written
// templates declare them, minimal ones may not. Inject any that are missing.
static void ensureRootNamespaces(QString &doc)
{
    const QPair<QString, QString> ns[] = {
        {"wp",  "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"},
        {"a",   "http://schemas.openxmlformats.org/drawingml/2006/main"},
        {"pic", "http://schemas.openxmlformats.org/drawingml/2006/picture"},
        {"m",   "http://schemas.openxmlformats.org/officeDocument/2006/math"},
        {"r",   "http://schemas.openxmlformats.org/officeDocument/2006/relationships"},
    };
    const int tagEnd = doc.indexOf('>');
    if (tagEnd < 0) return;
    for (const auto &n : ns) {
        if (doc.contains(QStringLiteral("xmlns:%1=").arg(n.first)))
            continue;
        doc.insert(tagEnd, QStringLiteral(" xmlns:%1=\"%2\"").arg(n.first, n.second));
    }
}

// Offset every rId<prefix><n> in text by `offset` (no-op when offset <= 0).
// Used when a template already declares scriba-style rel ids.
static QString shiftRelIds(QString text, const QString &prefix, int offset)
{
    if (offset <= 0) return text;
    QRegularExpression re(QStringLiteral("rId%1(\\d+)").arg(prefix));
    QString out;
    int last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        out += text.mid(last, m.capturedStart() - last);
        out += QStringLiteral("rId%1%2").arg(prefix).arg(m.captured(1).toInt() + offset);
        last = m.capturedEnd();
    }
    return out + text.mid(last);
}

static void ensureContentTypeDefault(QByteArray &ct, const char *ext, const char *mime)
{
    if (ct.contains(QByteArray("Extension=\"") + ext + "\""))
        return;
    const int close = ct.lastIndexOf("</Types>");
    if (close < 0) return;
    ct.insert(close, QByteArray("  <Default Extension=\"") + ext
              + "\" ContentType=\"" + mime + "\"/>\n");
}

static QByteArray relElement(const QString &id, const QString &type,
                             const QString &target, bool external = false)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartElement("Relationship");
    w.writeAttribute("Id", id);
    w.writeAttribute("Type", type);
    w.writeAttribute("Target", target);
    if (external)
        w.writeAttribute("TargetMode", "External");
    w.writeEndElement();
    return out;
}

// Compress each entry (raw DEFLATE), compute local-header offsets, write the
// archive. Shared by the from-scratch and template paths.
static bool writePackage(QVector<ZipEntry> &entries, const QString &outputPath);

// Export by injecting the converted body into an existing template package:
// every part is copied verbatim except word/document.xml (body content only —
// the template's sectPr survives) and word/_rels/document.xml.rels (scriba's
// image/hyperlink rels appended). Generated media files are remapped to names
// that don't collide with the template's; the body references images by rel id
// only, so no body rewrite is needed for that. The template owns all styling
// and page setup; options.landscape/margins/pageNumbers are ignored.
static bool exportIntoTemplate(const OoxmlResult &ooxml, const QString &outputPath,
                               const QString &templatePath)
{
    ZipReader zip(templatePath);
    QString error;
    if (!zip.open(&error)) {
        qCCritical(lcDocx) << "Cannot open DOCX template" << templatePath << error;
        return false;
    }
    QHash<QString, QByteArray> parts;
    for (const QString &name : zip.entryNames())
        parts.insert(name, zip.readEntry(name));

    if (!parts.contains(QStringLiteral("word/document.xml"))) {
        qCCritical(lcDocx) << "Template has no word/document.xml:" << templatePath;
        return false;
    }

    // 1. Body content: keep the template's sectPr.
    QString doc = QString::fromUtf8(parts.value(QStringLiteral("word/document.xml")));
    if (!insertBody(doc, ooxml.bodyXml)) {
        qCCritical(lcDocx) << "Template document.xml malformed:" << templatePath;
        return false;
    }
    ensureRootNamespaces(doc);
    parts.insert(QStringLiteral("word/document.xml"), doc.toUtf8());

    // 2. Media names: remap scriba images past any template media.
    QVector<OoxmlImage> images = ooxml.images;
    int nextMedia = 1;
    QRegularExpression mediaRe(QStringLiteral("^word/media/image(\\d+)\\."));
    for (const QString &name : parts.keys()) {
        auto m = mediaRe.match(name);
        if (m.hasMatch())
            nextMedia = qMax(nextMedia, m.captured(1).toInt() + 1);
    }
    for (auto &img : images) {
        while (!img.fileName.isEmpty() && parts.contains(QStringLiteral("word/") + img.fileName))
            img.fileName = QStringLiteral("media/image%1.png").arg(nextMedia++);
        while (!img.svgFileName.isEmpty()
               && parts.contains(QStringLiteral("word/") + img.svgFileName))
            img.svgFileName = QStringLiteral("media/image%1.svg").arg(nextMedia++);
    }

    // 3. Rels: rel-id offsets past any scriba-style ids the template declares.
    QByteArray rels = parts.value(QStringLiteral("word/_rels/document.xml.rels"));
    if (rels.isEmpty()) {
        rels = QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
            "</Relationships>\n");
    }
    const QString relsText = QString::fromUtf8(rels);
    auto maxPrefixedId = [&relsText](const QString &prefix) {
        QRegularExpression re(QStringLiteral("rId%1(\\d+)").arg(prefix));
        int max = 0;
        auto it = re.globalMatch(relsText);
        while (it.hasNext())
            max = qMax(max, it.next().captured(1).toInt());
        return max;
    };
    const int imgOff = maxPrefixedId(QStringLiteral("Img"));
    const int svgOff = maxPrefixedId(QStringLiteral("Svg"));
    const int lnkOff = maxPrefixedId(QStringLiteral("Link"));
    QString body = ooxml.bodyXml;
    body = shiftRelIds(body, QStringLiteral("Img"), imgOff);
    body = shiftRelIds(body, QStringLiteral("Svg"), svgOff);
    body = shiftRelIds(body, QStringLiteral("Link"), lnkOff);
    if (body != ooxml.bodyXml) {
        QString d = QString::fromUtf8(parts.value(QStringLiteral("word/document.xml")));
        const int p = d.indexOf(QLatin1String("<w:body"));
        if (p >= 0) {
            // bodyXml was inserted between <w:body> and the sectPr; splice in
            // the shifted variant over the same span.
            const int b0 = d.indexOf('>', p) + 1;
            const int b1 = d.lastIndexOf(QLatin1String("</w:body>"));
            const int s = d.lastIndexOf(QLatin1String("<w:sectPr"), b1);
            d = d.left(b0) + body + (s > b0 ? d.mid(s, b1 - s) : QString()) + d.mid(b1);
            parts.insert(QStringLiteral("word/document.xml"), d.toUtf8());
        }
    }

    // Append scriba's image/hyperlink relationships. The body's r:embed/r:id
    // values were already shifted by imgOff/svgOff/lnkOff, so each appended rel
    // must use the ACTUAL per-item relId (shifted to match), not a fresh counter.
    QString appends;
    for (const auto &img : images) {
        if (!img.pngData.isEmpty()) {
            appends += QString::fromUtf8(relElement(
                shiftRelIds(img.relId, QStringLiteral("Img"), imgOff),
                ooxmlRelTypeUri(OoxmlRelType::Image), img.fileName));
        }
        if (!img.svgData.isEmpty()) {
            appends += QString::fromUtf8(relElement(
                shiftRelIds(img.svgRelId, QStringLiteral("Svg"), svgOff),
                ooxmlRelTypeUri(OoxmlRelType::Image), img.svgFileName));
        }
    }
    for (const auto &hl : ooxml.hyperlinks) {
        appends += QString::fromUtf8(relElement(
            shiftRelIds(hl.relId, QStringLiteral("Link"), lnkOff),
            ooxmlRelTypeUri(OoxmlRelType::Hyperlink), hl.target, true));
    }
    const int relsClose = rels.lastIndexOf("</Relationships>");
    if (relsClose < 0) {
        qCCritical(lcDocx) << "Template rels malformed:" << templatePath;
        return false;
    }
    rels.insert(relsClose, appends.toUtf8());
    parts.insert(QStringLiteral("word/_rels/document.xml.rels"), rels);

    // 4. Content types: make sure png/svg defaults exist.
    QByteArray ct = parts.value(QStringLiteral("[Content_Types].xml"));
    bool hasSvg = false;
    for (const auto &img : images)
        if (!img.svgData.isEmpty()) { hasSvg = true; break; }
    if (!images.isEmpty())
        ensureContentTypeDefault(ct, "png", "image/png");
    if (hasSvg)
        ensureContentTypeDefault(ct, "svg", "image/svg+xml");
    if (!ct.isEmpty())
        parts.insert(QStringLiteral("[Content_Types].xml"), ct);

    // 5. Reassemble: mimetype first, template parts verbatim, images appended.
    QVector<ZipEntry> entries;
    {
        ZipEntry e;
        e.name = "mimetype";
        e.data = parts.take(QStringLiteral("mimetype"));
        if (e.data.isEmpty())
            e.data = QByteArray(kMimeType);
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    for (auto it = parts.constBegin(); it != parts.constEnd(); ++it) {
        ZipEntry e;
        e.name = it.key();
        e.data = it.value();
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    for (const auto &img : images) {
        if (!img.pngData.isEmpty()) {
            ZipEntry e;
            e.name = QStringLiteral("word/") + img.fileName;
            e.data = img.pngData;
            e.crc32 = zipCrc32(e.data);
            entries.append(e);
        }
        if (!img.svgData.isEmpty()) {
            ZipEntry s;
            s.name = QStringLiteral("word/") + img.svgFileName;
            s.data = img.svgData;
            s.crc32 = zipCrc32(s.data);
            entries.append(s);
        }
    }

    return writePackage(entries, outputPath);
}

// Compress each entry (raw DEFLATE), compute local-header offsets, write the
// archive. Shared by the from-scratch and template paths.
static bool writePackage(QVector<ZipEntry> &entries, const QString &outputPath)
{
    // Compress each entry (raw DEFLATE); fall back to STORE if it doesn't shrink.
    for (auto &entry : entries) {
        QByteArray comp = deflateRaw(entry.data);
        if (!comp.isNull() && comp.size() < entry.data.size()) {
            entry.compData = comp;
            entry.method = 8;
        }
    }

    // Compute local header offsets (mimetype = 0, rest accumulate)
    uint32_t offset = 0;
    for (auto &entry : entries) {
        entry.localOffset = offset;
        QByteArray nameBytes = entry.name.toUtf8();
        const QByteArray &stored = entry.method == 8 ? entry.compData : entry.data;
        // local file header: 30 bytes fixed + filename + extra(0)
        offset += 30 + nameBytes.size() + stored.size();
    }

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly)) {
        qCCritical(lcDocx) << "Cannot write to" << outputPath;
        return false;
    }

    return writeZip(out, entries);
}

// Build a complete package from scratch: fixed default styles (or the caller's
// stylesXml for template generation), dialog-controlled sectPr/footer.
static bool buildPackage(const OoxmlResult &ooxml, const QString &outputPath,
                         const QString &stylesXml, const DocxExportOptions &options)
{
    bool hasSvg = false;
    for (const auto &img : ooxml.images)
        if (!img.svgData.isEmpty()) { hasSvg = true; break; }

    bool hasFooter = options.pageNumbers;
    QString footerRelId;
    if (hasFooter)
        footerRelId = QStringLiteral("rId3");

    QVector<ZipEntry> entries;

    // Build all parts in memory
    {
        ZipEntry e;
        e.name = "mimetype";
        e.data = QByteArray(kMimeType);
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "[Content_Types].xml";
        e.data = buildContentTypes(ooxml.images.size(), hasFooter, hasSvg);
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "_rels/.rels";
        e.data = buildPackageRels();
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "word/document.xml";
        e.data = buildDocumentXml(ooxml, options.landscape,
                                  options.marginTopCm, options.marginBottomCm,
                                  options.marginLeftCm, options.marginRightCm,
                                  footerRelId);
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "word/_rels/document.xml.rels";
        e.data = buildRels(ooxml.images, ooxml.hyperlinks, hasFooter);
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "word/styles.xml";
        e.data = stylesXml.toUtf8();
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "word/numbering.xml";
        e.data = HtmlToOoxml::buildNumberingXml().toUtf8();
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "word/fontTable.xml";
        e.data = buildFontTableXml({});
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }
    {
        ZipEntry e;
        e.name = "docProps/core.xml";
        e.data = buildCoreProps();
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }

    // Add footer if needed
    if (hasFooter) {
        ZipEntry e;
        e.name = "word/footer1.xml";
        e.data = buildFooterXml();
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
    }

    // Add images
    for (const auto &img : ooxml.images) {
        ZipEntry e;
        e.name = "word/" + img.fileName;
        e.data = img.pngData;
        e.crc32 = zipCrc32(e.data);
        entries.append(e);
        if (!img.svgData.isEmpty()) {
            ZipEntry s;
            s.name = "word/" + img.svgFileName;
            s.data = img.svgData;
            s.crc32 = zipCrc32(s.data);
            entries.append(s);
        }
    }

    return writePackage(entries, outputPath);
}

bool DocxExporter::exportToDocx(const QString &html, const QString &outputPath,
                                const DocxExportOptions &options)
{
    OoxmlResult ooxml = HtmlToOoxml::convert(html);
    if (options.templatePath.isEmpty())
        return buildPackage(ooxml, outputPath,
                            HtmlToOoxml::buildStylesXml(QString::fromUtf8(kDefaultTemplateCss)),
                            options);
    return exportIntoTemplate(ooxml, outputPath, options.templatePath);
}

bool DocxExporter::saveAsTemplate(const QString &outputPath, const QString &themeCss)
{
    // An empty body plus the theme-derived styles: the user opens this in
    // Word, customizes (headers, backgrounds, fonts), and reuses it via
    // DocxExportOptions::templatePath.
    OoxmlResult empty;
    DocxExportOptions opts;   // defaults: portrait A4, 2.54 cm margins, no footer
    return buildPackage(empty, outputPath,
                        HtmlToOoxml::buildStylesXml(themeCss), opts);
}

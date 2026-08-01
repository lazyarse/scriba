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
#include <QDataStream>
#include <QFile>
#include <QLoggingCategory>
#include <QtMath>
#include <QXmlStreamWriter>

Q_LOGGING_CATEGORY(lcDocx, "scriba.docx")

static const char *kMimeType = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

struct ZipEntry {
    QString name;
    QByteArray data;
    uint32_t crc32 = 0;
    uint32_t localOffset = 0;
};

// Minimal CRC-32 (PKZip standard)
static uint32_t zipCrc32(const QByteArray &data)
{
    static const uint32_t t[256] = {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
        0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
        0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
        0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
        0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
        0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
        0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
        0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
        0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
        0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
        0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
        0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
        0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
        0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
        0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
        0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
        0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
        0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
        0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
        0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
        0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
        0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
        0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
        0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
        0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
    };
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); ++i)
        crc = t[(crc ^ (unsigned char)data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static QByteArray buildContentTypes(int imageCount, bool hasFooter = false)
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
        // Local file header
        writeU32(ds, 0x04034b50);
        writeU16(ds, 20);      // version needed
        writeU16(ds, 0);       // flags
        writeU16(ds, 0);       // compression method (STORE)
        writeU16(ds, 0);       // mod time
        writeU16(ds, 0);       // mod date
        writeU32(ds, entry.crc32);
        writeU32(ds, entry.data.size());  // compressed size
        writeU32(ds, entry.data.size());  // uncompressed size
        QByteArray nameBytes = entry.name.toUtf8();
        writeU16(ds, nameBytes.size());   // filename length
        writeU16(ds, 0);       // extra field length
        ds.writeRawData(nameBytes.constData(), nameBytes.size());

        // File data
        ds.writeRawData(entry.data.constData(), entry.data.size());

        // Central directory entry
        QByteArray cdEntry;
        QDataStream cdDs(&cdEntry, QIODevice::WriteOnly);
        writeU32(cdDs, 0x02014b50);
        writeU16(cdDs, 20);      // version made by
        writeU16(cdDs, 20);      // version needed
        writeU16(cdDs, 0);       // flags
        writeU16(cdDs, 0);       // compression method (STORE)
        writeU16(cdDs, 0);       // mod time
        writeU16(cdDs, 0);       // mod date
        writeU32(cdDs, entry.crc32);
        writeU32(cdDs, entry.data.size());  // compressed size
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

bool DocxExporter::exportToDocx(const QString &html, const QString &outputPath,
                                const QString &css, const DocxExportOptions &options)
{
    OoxmlResult ooxml = HtmlToOoxml::convert(html, css);

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
        e.data = buildContentTypes(ooxml.images.size(), hasFooter);
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
        e.data = HtmlToOoxml::buildStylesXml(css).toUtf8();
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
    }

    // Compute local header offsets (mimetype = 0, rest accumulate)
    uint32_t offset = 0;
    for (auto &entry : entries) {
        entry.localOffset = offset;
        QByteArray nameBytes = entry.name.toUtf8();
        // local file header: 30 bytes fixed + filename + extra(0)
        offset += 30 + nameBytes.size() + entry.data.size();
    }

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly)) {
        qCCritical(lcDocx) << "Cannot write to" << outputPath;
        return false;
    }

    return writeZip(out, entries);
}

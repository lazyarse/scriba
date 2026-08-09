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
#include "OoxmlToHtml.h"

#include <mathml2omml.h>

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

// ── tiny helpers ────────────────────────────────────────────────────────────

static QString esc(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return out;
}

static QString normStyleKey(const QString &s)
{
    QString n = s.toLower();
    n.remove(QLatin1Char(' '));
    return n;
}

// "Heading1" / "heading 1" / "1" → heading level 1..6, else 0.
static int headingLevelFor(const QString &styleId, const QString &styleName)
{
    const QString name = normStyleKey(styleName);
    if (name.startsWith(QStringLiteral("heading")) && name.size() > 7) {
        const QChar c = name.at(7);
        if (c >= QLatin1Char('1') && c <= QLatin1Char('6'))
            return c.unicode() - static_cast<int>(QLatin1Char('0').unicode());
    }
    const QString id = normStyleKey(styleId);
    if (id.startsWith(QStringLiteral("heading")) && id.size() > 7) {
        const QChar c = id.at(7);
        if (c >= QLatin1Char('1') && c <= QLatin1Char('6'))
            return c.unicode() - static_cast<int>(QLatin1Char('0').unicode());
    }
    // Word's built-in heading styles often use a bare numeric styleId ("1".."9").
    if (id.size() == 1) {
        const QChar c = id.at(0);
        if (c >= QLatin1Char('1') && c <= QLatin1Char('6'))
            return c.unicode() - static_cast<int>(QLatin1Char('0').unicode());
    }
    return 0;
}

static QString contentTypeFor(const QString &fileName)
{
    if (fileName.endsWith(QLatin1String(".png"))) return QStringLiteral("image/png");
    if (fileName.endsWith(QLatin1String(".jpg"))) return QStringLiteral("image/jpeg");
    if (fileName.endsWith(QLatin1String(".jpeg"))) return QStringLiteral("image/jpeg");
    if (fileName.endsWith(QLatin1String(".gif"))) return QStringLiteral("image/gif");
    if (fileName.endsWith(QLatin1String(".svg"))) return QStringLiteral("image/svg+xml");
    if (fileName.endsWith(QLatin1String(".bmp"))) return QStringLiteral("image/bmp");
    if (fileName.endsWith(QLatin1String(".tif"))) return QStringLiteral("image/tiff");
    if (fileName.endsWith(QLatin1String(".tiff"))) return QStringLiteral("image/tiff");
    if (fileName.endsWith(QLatin1String(".webp"))) return QStringLiteral("image/webp");
    return QStringLiteral("application/octet-stream");
}

// Serialize the element the reader currently points at (a StartElement) into a
// namespace-free XML string, consuming through its matching EndElement. Used
// to hand OMML sub-trees to the vendored mathml2omml (which strips prefixes).
static QString serializeCurrentElement(QXmlStreamReader &r)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);
    int depth = 0;
    bool first = true;
    while (!r.atEnd()) {
        if (first) {
            w.writeStartElement(r.name().toString());
            for (const QXmlStreamAttribute &a : r.attributes())
                w.writeAttribute(a.name().toString(), a.value().toString());
            depth = 1;
            first = false;
            r.readNext();
            continue;
        }
        if (r.isStartElement()) {
            w.writeStartElement(r.name().toString());
            for (const QXmlStreamAttribute &a : r.attributes())
                w.writeAttribute(a.name().toString(), a.value().toString());
            ++depth;
        } else if (r.isEndElement()) {
            w.writeEndElement();
            --depth;
            if (depth == 0)
                break;
        } else if (r.isCharacters()) {
            w.writeCharacters(r.text().toString());
        } else if (r.isCDATA()) {
            w.writeCDATA(r.text().toString());
        } else if (r.isComment()) {
            w.writeComment(r.text().toString());
        }
        r.readNext();
    }
    return QString::fromUtf8(out);
}

// Reads an attribute by its unprefixed name (attributes are namespaced in OOXML,
// e.g. w:val, so the qualified-name lookup would need the prefix). Returns empty
// if absent.
static QString attrOf(const QXmlStreamAttributes &attrs, const QString &localName)
{
    for (const QXmlStreamAttribute &a : attrs) {
        if (a.name() == localName)
            return a.value().toString();
    }
    return QString();
}

// OMML -> MathML -> LaTeX. Returns the empty string on failure so callers can
// fall back to the plain-text token stream.
static QString ommlToLatex(const QString &ommlXml)
{
    const std::string omml = ommlXml.toStdString();
    auto mathml = OmmlToMathml::convert(omml);
    if (!mathml)
        return QString();
    auto tex = MathmlToLatex::convert(*mathml);
    if (!tex)
        return QString();
    return QString::fromStdString(*tex);
}

// Fallback that brackets the raw equation text if conversion fails.
static QString ommlPlainText(const QString &ommlXml)
{
    QXmlStreamReader r(ommlXml.toUtf8());
    QString out;
    bool inT = false;
    while (!r.atEnd()) {
        r.readNext();
        const QString lm = r.name().toString();
        if (r.isStartElement() && lm == QLatin1String("t"))
            inT = true;
        else if (r.isEndElement() && lm == QLatin1String("t"))
            inT = false;
        else if (r.isCharacters() && inT)
            out += r.text().toString();
    }
    return out;
}

namespace {

struct Rel {
    QString target;
    QString type;      // last path segment of the relationship type URI
    bool external = false;
};

struct StyleInfo {
    int headingLevel = 0;
    bool bold = false;
    bool italic = false;
    bool mono = false;
};

struct ListLevel {
    bool ordered = false;
};

struct RunProps {
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool underline = false;
    bool superscript = false;
    bool subscript = false;
    bool mono = false;
};

struct Para {
    QString inner;
    int heading = 0;        // 1..6, or 0 for a plain paragraph
    int listLevel = -1;     // -1 = plain paragraph, else 0-based list level
    bool listOrdered = false;

    bool isListItem() const { return listLevel >= 0; }
};

struct Cell {
    QString html;
    int colspan = 0;        // 0 = "not specified"
};

struct ListState {
    QVector<bool> open;     // one flag per open <ol> (true) / <ul> (false)
};

// --------------------------------------------------------------- converter --
struct Converter {
    const QHash<QString, QByteArray> &parts;

    QHash<QString, Rel> rels;
    QHash<QString, StyleInfo> styles;
    QHash<QString, QString> numToAbstract;
    QHash<QString, QHash<int, ListLevel>> numLevels;   // abstractNumId -> level
    QHash<QString, QString> footnoteTexts;
    QHash<QString, int> imageIndex;
    QVector<OoxmlImportedImage> images;                        // first-use order

    QStringList warnings;
    QStringList headers;
    QStringList footers;

    QByteArray part(const QString &path) const
    {
        const auto it = parts.constFind(path);
        return it == parts.constEnd() ? QByteArray() : it.value();
    }

    // ── package level ───────────────────────────────────────────────────────
    void parseRelationships()
    {
        QXmlStreamReader r(part(QStringLiteral("word/_rels/document.xml.rels")));
        if (!r.readNextStartElement())   // <Relationships> wrapper
            return;
        while (r.readNextStartElement()) {
            if (r.name() != QLatin1String("Relationship")) {
                r.skipCurrentElement();
                continue;
            }
            Rel rel;
            rel.target = attrOf(r.attributes(), QStringLiteral("Target"));
            rel.external = attrOf(r.attributes(), QStringLiteral("TargetMode"))
                               == QLatin1String("External");
            const QString type = attrOf(r.attributes(), QStringLiteral("Type"));
            const int slash = type.lastIndexOf(QLatin1Char('/'));
            rel.type = slash < 0 ? type : type.mid(slash + 1);
            rels.insert(attrOf(r.attributes(), QStringLiteral("Id")), rel);
            r.skipCurrentElement();
        }
    }

    void parseStyles()
    {
        QXmlStreamReader r(part(QStringLiteral("word/styles.xml")));
        if (!r.readNextStartElement())   // <w:styles> wrapper
            return;
        while (r.readNextStartElement()) {
            if (r.name() != QLatin1String("style")) {
                r.skipCurrentElement();
                continue;
            }
            const QString styleId = attrOf(r.attributes(), QStringLiteral("styleId"));
            QString name;
            StyleInfo info;
            while (r.readNextStartElement()) {
                const QString lm = r.name().toString();
                if (lm == QLatin1String("name")) {
                    name = attrOf(r.attributes(), QStringLiteral("val"));
                    r.skipCurrentElement();
                } else if (lm == QLatin1String("rPr")) {
                    while (r.readNextStartElement()) {
                        const QString p = r.name().toString();
                        if (p == QLatin1String("b")) {
                            info.bold = toggleVal(r);
                        } else if (p == QLatin1String("i")) {
                            info.italic = toggleVal(r);
                        } else if (p == QLatin1String("rFonts")) {
                            const QString f = attrOf(r.attributes(), QStringLiteral("ascii"));
                            info.mono = f.contains(QLatin1String("Courier"), Qt::CaseInsensitive)
                                     || f.contains(QLatin1String("Consolas"), Qt::CaseInsensitive)
                                     || f.contains(QLatin1String("Mono"), Qt::CaseInsensitive);
                            r.skipCurrentElement();
                        } else {
                            r.skipCurrentElement();
                        }
                    }
                } else {
                    r.skipCurrentElement();
                }
            }
            info.headingLevel = headingLevelFor(styleId, name);
            styles.insert(styleId, info);
        }
    }

    // reads the boolean <w:b>-style element (w:val may negate).
    static bool toggleVal(QXmlStreamReader &r)
    {
        const QString v = attrOf(r.attributes(), QStringLiteral("val"));
        r.skipCurrentElement();
        if (v.isEmpty())
            return true;
        return v != QLatin1String("0") && v != QLatin1String("false");
    }

    void parseNumbering()
    {
        QXmlStreamReader r(part(QStringLiteral("word/numbering.xml")));
        if (!r.readNextStartElement())   // <w:numbering> wrapper
            return;
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("num")) {
                const QString numId = attrOf(r.attributes(), QStringLiteral("numId"));
                QString abstractId;
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("abstractNumId"))
                        abstractId = attrOf(r.attributes(), QStringLiteral("val"));
                    r.skipCurrentElement();
                }
                if (!numId.isEmpty() && !abstractId.isEmpty())
                    numToAbstract.insert(numId, abstractId);
            } else if (lm == QLatin1String("abstractNum")) {
                const QString abstractId =
                    attrOf(r.attributes(), QStringLiteral("abstractNumId"));
                QHash<int, ListLevel> levels;
                while (r.readNextStartElement()) {
                    if (r.name() != QLatin1String("lvl")) {
                        r.skipCurrentElement();
                        continue;
                    }
                    const int idx = attrOf(r.attributes(), QStringLiteral("ilvl")).toInt();
                    ListLevel lv;
                    while (r.readNextStartElement()) {
                        if (r.name() == QLatin1String("numFmt")) {
                            const QString fmt = attrOf(r.attributes(), QStringLiteral("val"));
                            lv.ordered = fmt != QLatin1String("bullet");
                            r.skipCurrentElement();
                        } else {
                            r.skipCurrentElement();
                        }
                    }
                    levels.insert(idx, lv);
                }
                if (!abstractId.isEmpty())
                    numLevels.insert(abstractId, levels);
            } else {
                r.skipCurrentElement();
            }
        }
    }

    bool listOrdered(const QString &numId, int level) const
    {
        const QString abs = numToAbstract.value(numId);
        if (abs.isEmpty())
            return false;
        const auto it = numLevels.constFind(abs);
        if (it == numLevels.constEnd())
            return false;
        const auto itl = it.value().constFind(level);
        return itl != it.value().constEnd() && itl.value().ordered;
    }

    void parseFootnotes()
    {
        QXmlStreamReader r(part(QStringLiteral("word/footnotes.xml")));
        if (!r.readNextStartElement())   // <w:footnotes> wrapper
            return;
        while (r.readNextStartElement()) {
            if (r.name() != QLatin1String("footnote")) {
                r.skipCurrentElement();
                continue;
            }
            const QString id = attrOf(r.attributes(), QStringLiteral("id"));
            if (id == QLatin1String("-1") || id == QLatin1String("0")) {   // separators
                r.skipCurrentElement();
                continue;
            }
            QStringList lines;
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("p"))
                    lines << plainParagraph(r);
                else
                    r.skipCurrentElement();
            }
            if (!id.isEmpty())
                footnoteTexts.insert(id, lines.join(QLatin1Char(' ')));
        }
    }

    QString plainParagraph(QXmlStreamReader &r)   // at <w:p>, consumes it
    {
        QString out;
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("r")) {
                while (r.readNextStartElement()) {
                    const QString rlm = r.name().toString();
                    if (rlm == QLatin1String("t"))
                        out += r.readElementText();
                    else if (rlm == QLatin1String("br") || rlm == QLatin1String("tab"))
                        out += QLatin1Char(' ');
                    else
                        r.skipCurrentElement();
                }
            } else if (lm == QLatin1String("t")) {
                out += r.readElementText();
            } else {
                r.skipCurrentElement();
            }
        }
        return out.trimmed();
    }

    QString plainPart(const QByteArray &bytes)
    {
        QXmlStreamReader r(bytes);
        QStringList lines;
        while (!r.atEnd()) {
            if (r.isStartElement() && r.name() == QLatin1String("p"))
                lines << plainParagraph(r);
            else
                r.readNext();
        }
        return lines.join(QLatin1Char(' '));
    }

    void gatherHeadersFooters()
    {
        QStringList headerTargets, footerTargets;
        for (const Rel &rel : rels) {
            if (rel.type == QLatin1String("header"))
                headerTargets << rel.target;
            else if (rel.type == QLatin1String("footer"))
                footerTargets << rel.target;
        }
        for (const QString &t : headerTargets)
            headers << plainPart(part(QStringLiteral("word/") + t));
        for (const QString &t : footerTargets)
            footers << plainPart(part(QStringLiteral("word/") + t));
    }

    // ── body ────────────────────────────────────────────────────────────────
    OoxmlToHtmlResult run(const QByteArray &doc)
    {
        OoxmlToHtmlResult res;
        if (doc.isEmpty()) {
            res.errors << QStringLiteral("The Word document contains no body");
            return res;
        }
        QXmlStreamReader r(doc);
        if (!r.readNextStartElement() || r.name() != QLatin1String("document")) {
            res.errors << QStringLiteral("Could not parse the Word document");
            return res;
        }
        QStringList frags;
        ListState st;
        while (r.readNextStartElement()) {
            if (r.name() == QLatin1String("body"))
                emitBlocks(r, frags, st);
            else
                r.skipCurrentElement();
        }
        closeLists(frags, st);
        return finalize(frags);
    }

    void emitBlocks(QXmlStreamReader &r, QStringList &frags, ListState &st)
    {
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("p")) {
                addPara(frags, st, emitParagraph(r));
            } else if (lm == QLatin1String("tbl")) {
                closeLists(frags, st);
                emitTable(r, frags);
            } else if (lm == QLatin1String("oMathPara")) {
                const QString tex = mathLatex(r);
                if (!tex.isEmpty()) {
                    closeLists(frags, st);
                    frags << QStringLiteral("\n$$\n%1\n$$\n").arg(tex);
                }
            } else {
                r.skipCurrentElement();
            }
        }
    }

    Para emitParagraph(QXmlStreamReader &r)
    {
        Para para;
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("pPr")) {
                parseParaProps(r, para);
            } else if (lm == QLatin1String("r")) {
                emitRun(r, para.inner);
            } else if (lm == QLatin1String("hyperlink")) {
                emitHyperlink(r, para.inner);
            } else if (lm == QLatin1String("oMath")) {
                const QString tex = mathLatex(r);
                if (!tex.isEmpty())
                    para.inner += QLatin1Char('$') + tex + QLatin1Char('$');
            } else {
                r.skipCurrentElement();
            }
        }
        return para;
    }

    void parseParaProps(QXmlStreamReader &r, Para &para)
    {
        QString numId;
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("numPr")) {
                int ilvl = 0;
                while (r.readNextStartElement()) {
                    const QString c = r.name().toString();
                    const QString v = attrOf(r.attributes(), QStringLiteral("val"));
                    if (c == QLatin1String("ilvl"))
                        ilvl = v.toInt();
                    else if (c == QLatin1String("numId"))
                        numId = v;
                    r.skipCurrentElement();
                }
                para.listLevel = ilvl;
                para.listOrdered = listOrdered(numId, ilvl);
            } else if (lm == QLatin1String("pStyle")) {
                const QString sid = attrOf(r.attributes(), QStringLiteral("val"));
                const auto it = styles.constFind(sid);
                if (it != styles.constEnd())
                    para.heading = it.value().headingLevel;
                r.skipCurrentElement();
            } else if (lm == QLatin1String("outlineLvl")) {
                const int lvl = attrOf(r.attributes(), QStringLiteral("val")).toInt();
                if (lvl >= 0 && lvl <= 5 && para.heading == 0)
                    para.heading = lvl + 1;
                r.skipCurrentElement();
            } else {
                r.skipCurrentElement();
            }
        }
    }

    void emitRun(QXmlStreamReader &r, QString &content)
    {
        RunProps props;
        QString frag;
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("rPr")) {
                parseRunProps(r, props);
            } else if (lm == QLatin1String("t")) {
                frag += esc(r.readElementText());
            } else if (lm == QLatin1String("br")) {
                r.skipCurrentElement();
                frag += QStringLiteral("<br/>");
            } else if (lm == QLatin1String("tab")) {
                r.skipCurrentElement();
                frag += QLatin1Char('\t');
            } else if (lm == QLatin1String("footnoteReference")) {
                const QString id = attrOf(r.attributes(), QStringLiteral("id"));
                if (footnoteTexts.contains(id))
                    frag += QStringLiteral("[^f%1]").arg(id);
                r.skipCurrentElement();
            } else if (lm == QLatin1String("noBreakHyphen")) {
                r.skipCurrentElement();
                frag += QLatin1Char('-');
            } else if (lm == QLatin1String("drawing") || lm == QLatin1String("pict")) {
                frag += image(r);
            } else if (lm == QLatin1String("oMath")) {
                const QString tex = mathLatex(r);
                if (!tex.isEmpty())
                    frag += QLatin1Char('$') + tex + QLatin1Char('$');
            } else {
                r.skipCurrentElement();
            }
        }
        content += applyFormatting(props, frag);
    }

    void parseRunProps(QXmlStreamReader &r, RunProps &props)
    {
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("b")) {
                props.bold = toggleVal(r);
            } else if (lm == QLatin1String("i")) {
                props.italic = toggleVal(r);
            } else if (lm == QLatin1String("strike")) {
                props.strike = toggleVal(r);
            } else if (lm == QLatin1String("u")) {
                props.underline = true;
                r.skipCurrentElement();
            } else if (lm == QLatin1String("vertAlign")) {
                const QString v = attrOf(r.attributes(), QStringLiteral("val"));
                props.superscript = v == QLatin1String("superscript");
                props.subscript = v == QLatin1String("subscript");
                r.skipCurrentElement();
            } else if (lm == QLatin1String("rFonts")) {
                const QString f = attrOf(r.attributes(), QStringLiteral("ascii"));
                props.mono = f.contains(QLatin1String("Courier"), Qt::CaseInsensitive)
                          || f.contains(QLatin1String("Consolas"), Qt::CaseInsensitive)
                          || f.contains(QLatin1String("Mono"), Qt::CaseInsensitive);
                r.skipCurrentElement();
            } else {
                r.skipCurrentElement();
            }
        }
    }

    QString applyFormatting(const RunProps &props, const QString &content) const
    {
        QString c = content;
        if (props.superscript || props.subscript) {
            c = props.superscript
                    ? QStringLiteral("<sup>%1</sup>").arg(c)
                    : QStringLiteral("<sub>%1</sub>").arg(c);
        }
        if (props.mono)
            c = QStringLiteral("<code>%1</code>").arg(c);
        if (props.underline)
            c = QStringLiteral("<u>%1</u>").arg(c);
        if (props.strike)
            c = QStringLiteral("<del>%1</del>").arg(c);
        if (props.italic)
            c = QStringLiteral("<em>%1</em>").arg(c);
        if (props.bold)
            c = QStringLiteral("<strong>%1</strong>").arg(c);
        return c;
    }

    void emitHyperlink(QXmlStreamReader &r, QString &content)
    {
        QString href;
        const QString anchor = attrOf(r.attributes(), QStringLiteral("anchor"));
        const QString rid = attrOf(r.attributes(), QStringLiteral("id"));
        if (!anchor.isEmpty()) {
            href = QLatin1Char('#') + anchor;
        } else {
            const auto it = rels.constFind(rid);
            if (it != rels.constEnd())
                href = it.value().target;
        }
        QString inner;
        while (r.readNextStartElement()) {
            const QString lm = r.name().toString();
            if (lm == QLatin1String("r"))
                emitRun(r, inner);
            else
                r.skipCurrentElement();
        }
        content += QStringLiteral("<a href=\"%1\">%2</a>").arg(esc(href), inner);
    }

    QString image(QXmlStreamReader &r)
    {
        QString rId;
        int cx = 0, cy = 0;
        int depth = 1;
        while (!r.atEnd() && depth > 0) {
            r.readNext();
            if (r.isStartElement()) {
                const QString lm = r.name().toString();
                if (lm == QLatin1String("blip"))
                    rId = attrOf(r.attributes(), QStringLiteral("embed"));
                else if (lm == QLatin1String("imagedata"))
                    rId = attrOf(r.attributes(), QStringLiteral("id"));
                else if (lm == QLatin1String("extent")) {
                    cx = attrOf(r.attributes(), QStringLiteral("cx")).toInt();
                    cy = attrOf(r.attributes(), QStringLiteral("cy")).toInt();
                }
                ++depth;
            } else if (r.isEndElement()) {
                --depth;
            }
        }
        if (rId.isEmpty())
            return QString();
        const auto relit = rels.constFind(rId);
        if (relit == rels.constEnd())
            return QString();
        const QString target = relit.value().target;
        const QByteArray data = part(QStringLiteral("word/") + target);
        if (data.isEmpty()) {
            warn(QStringLiteral("Missing image part ") + target);
            return QString();
        }
        const QString fileName = target.mid(target.lastIndexOf(QLatin1Char('/')) + 1);
        if (fileName.endsWith(QLatin1String(".emf")) || fileName.endsWith(QLatin1String(".wmf"))) {
            warn(QStringLiteral("Skipped %1 (EMF/WMF not supported)").arg(fileName));
            return QString();
        }
        const auto iti = imageIndex.constFind(rId);
        if (iti == imageIndex.constEnd()) {
            OoxmlImportedImage img;
            img.rId = rId;
            img.fileName = fileName;
            img.contentType = contentTypeFor(fileName);
            img.data = data;
            imageIndex.insert(rId, images.size());
            images << img;
        }
        QString size;
        if (cx > 0 && cy > 0)
            size = QStringLiteral(" width=\"%1\" height=\"%2\"")
                       .arg(qRound(cx / 9525.0)).arg(qRound(cy / 9525.0));
        return QStringLiteral("<img src=\"docximg://%1\"%2 alt=\"\">").arg(rId, size);
    }

    QString mathLatex(QXmlStreamReader &r)
    {
        const QString omml = serializeCurrentElement(r);
        QString tex = ommlToLatex(omml);
        if (tex.isEmpty()) {
            tex = ommlPlainText(omml).trimmed();
            if (!tex.isEmpty())
                warn(QStringLiteral("Math could not be converted to LaTeX; imported as plain text"));
        }
        return tex;
    }

    void warn(const QString &msg)
    {
        if (!warnings.contains(msg))
            warnings << msg;
    }

    // ── lists ───────────────────────────────────────────────────────────────
    void closeLists(QStringList &frags, ListState &st)
    {
        while (!st.open.isEmpty())
            frags << (st.open.takeLast() ? QStringLiteral("</ol>\n")
                                         : QStringLiteral("</ul>\n"));
    }

    void addPara(QStringList &frags, ListState &st, const Para &para)
    {
        if (para.isListItem()) {
            while (st.open.size() > para.listLevel + 1)
                frags << (st.open.takeLast() ? QStringLiteral("</ol>\n")
                                             : QStringLiteral("</ul>\n"));
            if (st.open.size() == para.listLevel + 1
                    && st.open.last() != para.listOrdered) {
                // Same depth but the numbering style flips (bullet↔number):
                // close the current list and reopen as the new type.
                frags << (st.open.takeLast() ? QStringLiteral("</ol>\n")
                                             : QStringLiteral("</ul>\n"));
                st.open << para.listOrdered;
                frags << (para.listOrdered ? QStringLiteral("<ol>\n")
                                           : QStringLiteral("<ul>\n"));
            }
            while (st.open.size() < para.listLevel + 1) {
                const bool ordered = para.listLevel == st.open.size() ? para.listOrdered : false;
                st.open << ordered;
                frags << (ordered ? QStringLiteral("<ol>\n") : QStringLiteral("<ul>\n"));
            }
            frags << QStringLiteral("<li>%1</li>\n").arg(para.inner);
        } else {
            closeLists(frags, st);
            if (para.heading > 0)
                frags << QStringLiteral("<h%1>%2</h%1>\n").arg(para.heading).arg(para.inner);
            else
                frags << QStringLiteral("<p>%1</p>\n").arg(para.inner);
        }
    }

    // ── tables ──────────────────────────────────────────────────────────────
    void emitTable(QXmlStreamReader &r, QStringList &frags)
    {
        struct Row {
            QVector<Cell> cells;
            bool header = false;
        };
        QVector<Row> rows;
        while (r.readNextStartElement()) {
            if (r.name() != QLatin1String("tr")) {
                r.skipCurrentElement();
                continue;
            }
            Row row;
            while (r.readNextStartElement()) {
                const QString lm = r.name().toString();
                if (lm == QLatin1String("trPr")) {
                    while (r.readNextStartElement()) {
                        if (r.name() == QLatin1String("tblHeader"))
                            row.header = toggleVal(r);
                        else
                            r.skipCurrentElement();
                    }
                } else if (lm == QLatin1String("tc")) {
                    row.cells << parseCell(r);
                } else {
                    r.skipCurrentElement();
                }
            }
            rows << row;
        }
        if (rows.isEmpty())
            return;
        QString out = QStringLiteral("<table>\n");
        QString thead, tbody;
        for (const Row &row : rows) {
            const QString tag = row.header ? QLatin1String("th") : QLatin1String("td");
            QString tr = QStringLiteral("<tr>");
            for (const Cell &cell : row.cells) {
                QString cellHtml = cell.html.isEmpty() ? QStringLiteral("&nbsp;") : cell.html;
                const QString attrs = cell.colspan > 1
                    ? QStringLiteral(" colspan=\"%1\"").arg(cell.colspan)
                    : QString();
                tr += QStringLiteral("<%1%2>%3</%1>").arg(tag, attrs, cellHtml);
            }
            tr += QStringLiteral("</tr>\n");
            if (row.header)
                thead += tr;
            else
                tbody += tr;
        }
        if (!thead.isEmpty())
            out += QStringLiteral("<thead>\n%1</thead>\n").arg(thead);
        out += QStringLiteral("<tbody>\n%1</tbody>\n").arg(tbody);
        out += QStringLiteral("</table>\n");
        frags << out;
    }

    Cell parseCell(QXmlStreamReader &r)
    {
        Cell c;
        QStringList cells;
        ListState st;
        while (r.readNext()) {
            if (r.isStartElement()) {
                const QString lm = r.name().toString();
                if (lm == QLatin1String("tcPr")) {
                    while (r.readNextStartElement()) {
                        if (r.name() == QLatin1String("gridSpan"))
                            c.colspan = attrOf(r.attributes(), QStringLiteral("val")).toInt();
                        r.skipCurrentElement();
                    }
                } else if (lm == QLatin1String("p")) {
                    addPara(cells, st, emitParagraph(r));
                } else if (lm == QLatin1String("tbl")) {
                    emitTable(r, cells);
                } else {
                    r.skipCurrentElement();
                }
            } else if (r.isEndElement() && r.name() == QLatin1String("tc")) {
                break;
            }
        }
        closeLists(cells, st);
        c.html = cells.join(QLatin1Char('\n'));
        return c;
    }

    // ── result assembly ─────────────────────────────────────────────────────
    OoxmlToHtmlResult finalize(const QStringList &frags)
    {
        QString html = frags.join(QString());

        // Header/footer text becomes Markdown footnotes: reference markers are
        // placed on the first (header) and last (footer) lines, definitions
        // are appended at the very end of the document.
        QString refs;
        for (int i = 0; i < headers.size(); ++i)
            refs += QStringLiteral("[^hdr%1] ").arg(i + 1);
        if (!refs.isEmpty())
            html = QStringLiteral("<p>%1</p>\n").arg(refs.trimmed()) + html;

        refs.clear();
        for (int i = 0; i < footers.size(); ++i)
            refs += QStringLiteral("[^ftr%1] ").arg(i + 1);
        if (!refs.isEmpty())
            html += QStringLiteral("\n<p>%1</p>\n").arg(refs.trimmed());

        QString defs;
        for (int i = 0; i < headers.size(); ++i)
            defs += QStringLiteral("<p>[^hdr%1]: %2</p>\n").arg(i + 1).arg(esc(headers[i]));
        for (int i = 0; i < footers.size(); ++i)
            defs += QStringLiteral("<p>[^ftr%1]: %2</p>\n").arg(i + 1).arg(esc(footers[i]));
        QList<int> ids;
        for (const QString &k : footnoteTexts.keys()) {
            bool ok = false;
            const int v = k.toInt(&ok);
            if (ok)
                ids << v;
        }
        std::sort(ids.begin(), ids.end());
        for (int id : ids)
            defs += QStringLiteral("<p>[^f%1]: %2</p>\n").arg(id).arg(esc(footnoteTexts[QString::number(id)]));
        if (!defs.isEmpty())
            html += QStringLiteral("\n%1").arg(defs);

        OoxmlToHtmlResult res;
        res.html = html;
        res.ok = true;
        res.errors = warnings;
        res.images = images;
        return res;
    }
};

} // namespace

OoxmlToHtmlResult OoxmlToHtml::convert(const QHash<QString, QByteArray> &parts)
{
    Converter conv{parts};
    conv.parseRelationships();
    conv.parseStyles();
    conv.parseNumbering();
    conv.parseFootnotes();
    conv.gatherHeadersFooters();
    return conv.run(conv.part(QStringLiteral("word/document.xml")));
}

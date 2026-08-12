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
#include "HtmlToOoxml.h"
#include "CssValueParser.h"
#include "UnitConverter.h"
#include <mathml2omml.h>
#include <string>
#include <string_view>
#include <QBuffer>
#include <QImage>
#include <QLoggingCategory>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSvgRenderer>
#include <QXmlStreamWriter>

Q_LOGGING_CATEGORY(lcH2O, "scriba.html2ooxml")

struct FormatState {
    bool bold = false;
    bool italic = false;
    bool code = false;
    bool strike = false;
    bool shadow = false;
    bool underline = false;
    QString underlineStyle = QStringLiteral("single");
    QString underlineColor;
    QString fontFamily;
};

static QXmlStreamWriter *bodyWriter = nullptr;
static QVector<OoxmlImage> *g_images = nullptr;
static int g_imageCounter = 0;
static QVector<OoxmlHyperlink> *g_hyperlinks = nullptr;
static int g_hyperlinkCounter = 0;
static QString g_admonitionType;

// ── Qt adapter for OmmlSink ───────────────────────────────────────────────────

struct QtOmmlSink : XmlSink {
    QXmlStreamWriter *w;
    explicit QtOmmlSink(QXmlStreamWriter *writer) : w(writer) {}

    void startElement(std::string_view name) override {
        w->writeStartElement(QString::fromUtf8(name.data(),
                                               static_cast<qsizetype>(name.size())));
    }

    void endElement() override {
        w->writeEndElement();
    }

    void attribute(std::string_view name, std::string_view value) override {
        w->writeAttribute(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())),
                          QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())));
    }

    void characters(std::string_view text) override {
        w->writeCharacters(QString::fromUtf8(text.data(),
                                             static_cast<qsizetype>(text.size())));
    }
};

// ── Simple HTML tag scanner ──────────────────────────────────────────────────

struct HtmlToken {
    enum Type { Text, TagOpen, TagClose, SelfClose, Done };
    Type type = Done;
    QString name;
    QMap<QString, QString> attrs;
    QString text;
};

class SimpleHtmlParser {
    const QString &html_;
    int pos_ = 0;
    int lastTagStart_ = 0;
    HtmlToken cur_;

    static QString decodeEntities(const QString &raw)
    {
        QString out;
        out.reserve(raw.size());
        for (int i = 0; i < raw.size(); ++i) {
            if (raw[i] == '&') {
                int semi = raw.indexOf(';', i);
                if (semi == -1) { out += raw[i]; continue; }
                QString ent = raw.mid(i + 1, semi - i - 1);
                if (ent == "amp") { out += '&'; }
                else if (ent == "lt") { out += '<'; }
                else if (ent == "gt") { out += '>'; }
                else if (ent == "quot") { out += '"'; }
                else if (ent == "apos") { out += '\''; }
                else if (ent == "nbsp") { out += QChar(0xA0); }
                else if (ent.startsWith('#') && ent.size() > 1) {
                    bool ok = false;
                    int cp = ent.mid(1).toInt(&ok);
                    if (ok) out += QChar(cp); else out += raw.mid(i, semi - i + 1);
                } else {
                    out += raw.mid(i, semi - i + 1);
                }
                i = semi;
            } else {
                out += raw[i];
            }
        }
        return out;
    }

    HtmlToken fetch()
    {
        HtmlToken t;
        if (pos_ >= html_.size()) { t.type = HtmlToken::Done; return t; }

        while (pos_ < html_.size() && html_[pos_] == '<') {
            lastTagStart_ = pos_;
            // Find closing >, skipping > inside quoted attribute values
            int close = pos_;
            bool dq = false, sq = false;
            while (close < html_.size()) {
                if (!dq && !sq && html_[close] == '>') break;
                if (html_[close] == '"' && !sq) dq = !dq;
                if (html_[close] == '\'' && !dq) sq = !sq;
                ++close;
            }
            if (close >= html_.size()) { t.type = HtmlToken::Done; return t; }
            int nameStart = pos_ + 1;
            bool isClose = (nameStart < html_.size() && html_[nameStart] == '/');
            if (isClose) ++nameStart;

            int nameEnd = nameStart;
            while (nameEnd < close && !html_[nameEnd].isSpace() && html_[nameEnd] != '>'
                   && html_[nameEnd] != '/')
                ++nameEnd;

            QString tagName = html_.mid(nameStart, nameEnd - nameStart).toLower();

            // Self-closing or void elements
            bool selfClose = (html_[close - 1] == '/');

            t.type = isClose ? HtmlToken::TagClose
                     : (selfClose ? HtmlToken::SelfClose : HtmlToken::TagOpen);
            t.name = tagName;

            if (!isClose) {
                int ap = nameEnd;
                while (ap < close) {
                    while (ap < close && html_[ap].isSpace()) ++ap;
                    if (ap >= close || html_[ap] == '/' || html_[ap] == '>') break;
                    int aNameStart = ap;
                    while (ap < close && !html_[ap].isSpace() && html_[ap] != '='
                           && html_[ap] != '>' && html_[ap] != '/')
                        ++ap;
                    QString aName = html_.mid(aNameStart, ap - aNameStart);
                    while (ap < close && html_[ap].isSpace()) ++ap;
                    QString aVal;
                    if (ap < close && html_[ap] == '=') {
                        ++ap;
                        while (ap < close && html_[ap].isSpace()) ++ap;
                        if (ap < close && (html_[ap] == '"' || html_[ap] == '\'')) {
                            QChar q = html_[ap];
                            ++ap;
                            int vs = ap;
                            while (ap < close && html_[ap] != q) ++ap;
                            aVal = decodeEntities(html_.mid(vs, ap - vs));
                            if (ap < close) ++ap;
                        } else {
                            int vs = ap;
                            while (ap < close && !html_[ap].isSpace() && html_[ap] != '>')
                                ++ap;
                            aVal = html_.mid(vs, ap - vs);
                        }
                    }
                    t.attrs.insert(aName.toLower(), aVal);
                }
            }

            pos_ = close + 1;
            return t;
        }

        // Text until next < or end
        int textStart = pos_;
        while (pos_ < html_.size() && html_[pos_] != '<')
            ++pos_;
        t.type = HtmlToken::Text;
        t.text = decodeEntities(html_.mid(textStart, pos_ - textStart));
        return t;
    }

public:
    bool started_ = false;

    explicit SimpleHtmlParser(const QString &html) : html_(html) {
        cur_.type = HtmlToken::Done;
    }

    const HtmlToken &current() const { return cur_; }
    void readNext() { cur_ = fetch(); started_ = true; }
    bool atEnd() const { return started_ && cur_.type == HtmlToken::Done; }
    int tagStart() const { return lastTagStart_; }
    int pos() const { return pos_; }
    const QString &rawHtml() const { return html_; }
    void seekTo(int p) { if (p > pos_) pos_ = p; }
};

// ── helpers ──────────────────────────────────────────────────────────────────

static bool isVoidElement(const QString &name)
{
    static const QSet<QString> voids = {
        QStringLiteral("area"),   QStringLiteral("base"),  QStringLiteral("col"),
        QStringLiteral("embed"),  QStringLiteral("hr"),    QStringLiteral("input"),
        QStringLiteral("keygen"), QStringLiteral("link"),  QStringLiteral("meta"),
        QStringLiteral("param"),  QStringLiteral("source"), QStringLiteral("track"),
        QStringLiteral("wbr"),
    };
    return voids.contains(name);
}

static void writeBreak(QXmlStreamWriter &w)
{
    w.writeStartElement("w:r");
    w.writeEmptyElement("w:br");
    w.writeEndElement();
}

// Extract plain text from a KaTeX HTML span tree, ignoring structure
static QString extractKatexText(SimpleHtmlParser &parser, const QString &endTag)
{
    QString result;
    int depth = 1;
    while (!parser.atEnd() && depth > 0) {
        parser.readNext();
        const auto &tok = parser.current();
        if (tok.type == HtmlToken::TagClose && tok.name == endTag) {
            --depth;
            continue;
        }
        if (tok.type == HtmlToken::TagOpen || tok.type == HtmlToken::SelfClose) {
            const QString &n = tok.name;
            if (isVoidElement(n)) continue;
            if (n == "annotation" && tok.attrs.value("encoding") == "application/x-tex") {
                // KaTeX stores the original TeX in <annotation encoding="application/x-tex">
                // This is the most faithful representation
                while (!parser.atEnd()) {
                    parser.readNext();
                    if (parser.current().type == HtmlToken::Text) {
                        return parser.current().text.trimmed();
                    }
                    if (parser.current().type == HtmlToken::TagClose && parser.current().name == "annotation")
                        break;
                }
            }
            ++depth;
        } else if (tok.type == HtmlToken::Text) {
            result += tok.text;
        }
    }
    return result.trimmed();
}

// Write KaTeX math as OMML <m:oMath> using MathML from data-mathml attribute,
// falling back to TeX text from data-tex attribute.
static void writeKatexAsOmml(SimpleHtmlParser &parser, const QString &endTag,
                             const QString &texSource, const QString &mathmlSource = {})
{
    // Consume everything until the matching closing tag
    int depth = 1;
    while (!parser.atEnd() && depth > 0) {
        parser.readNext();
        const auto &tok = parser.current();
        if (tok.type == HtmlToken::TagClose && tok.name == endTag) {
            --depth;
        } else if (tok.type == HtmlToken::TagOpen && tok.name == "span"
                   && tok.type != HtmlToken::SelfClose) {
            ++depth;
        }
    }

    QString mathml = mathmlSource.trimmed();
    if (!mathml.isEmpty()) {
        QtOmmlSink sink{bodyWriter};
        if (MathmlToOmml::convert(mathml.toStdString(), sink))
            return;
    }

    QString tex = texSource.trimmed();
    if (tex.isEmpty())
        tex = QStringLiteral("math");

    bodyWriter->writeStartElement("m:oMath");
    bodyWriter->writeStartElement("m:r");
    bodyWriter->writeStartElement("m:rPr");
    bodyWriter->writeStartElement("m:sty");
    bodyWriter->writeAttribute("m:val", "p");
    bodyWriter->writeEndElement();
    bodyWriter->writeEndElement();
    bodyWriter->writeStartElement("m:t");
    bodyWriter->writeAttribute("xml:space", "preserve");
    bodyWriter->writeCharacters(tex);
    bodyWriter->writeEndElement();
    bodyWriter->writeEndElement();
    bodyWriter->writeEndElement();
}

static void writeRunProperties(QXmlStreamWriter &w, const FormatState &fs)
{
    // Emit <w:rPr> children in ECMA-376 EG_RPrBase slot order so Word
    // does not silently drop out-of-sequence elements.
    if (!fs.bold && !fs.italic && !fs.code && !fs.strike && !fs.shadow
        && !fs.underline && fs.fontFamily.isEmpty())
        return;
    w.writeStartElement("w:rPr");
    // Slot 2: rFonts
    if (fs.code || !fs.fontFamily.isEmpty()) {
        w.writeStartElement("w:rFonts");
        w.writeAttribute("w:ascii", fs.code ? "Courier New" : fs.fontFamily);
        w.writeAttribute("w:hAnsi", fs.code ? "Courier New" : fs.fontFamily);
        w.writeEndElement();
    }
    // Slot 3: b
    if (fs.bold) w.writeEmptyElement("w:b");
    // Slot 5: i
    if (fs.italic) w.writeEmptyElement("w:i");
    // Slot 9: strike
    if (fs.strike) w.writeEmptyElement("w:strike");
    // Slot 12: shadow
    if (fs.shadow) w.writeEmptyElement("w:shadow");
    // Slot 24: sz (code font size)
    if (fs.code) {
        w.writeStartElement("w:sz");
        w.writeAttribute("w:val", "18");
        w.writeEndElement();
    }
    // Slot 27: u
    if (fs.underline) {
        w.writeStartElement("w:u");
        w.writeAttribute("w:val", fs.underlineStyle);
        if (!fs.underlineColor.isEmpty())
            w.writeAttribute("w:color", fs.underlineColor);
        w.writeEndElement();
    }
    w.writeEndElement();
}

static void writeRunWithBreaks(QXmlStreamWriter &w, const QString &text, const FormatState &fs)
{
    if (text.isEmpty()) return;

    // Split on newlines so OOXML preserves line breaks in code blocks
    QStringList parts = text.split(QLatin1Char('\n'));
    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            // Emit a line break between segments
            writeBreak(w);
        }
        if (parts[i].isEmpty()) continue;
        w.writeStartElement("w:r");
        writeRunProperties(w, fs);
        w.writeStartElement("w:t");
        if (!parts[i].isEmpty() && (parts[i][0] == ' ' || parts[i].back() == ' '))
            w.writeAttribute("xml:space", "preserve");
        w.writeCharacters(parts[i]);
        w.writeEndElement();
        w.writeEndElement();
    }
}

static void writeRun(QXmlStreamWriter &w, const QString &text, const FormatState &fs)
{
    writeRunWithBreaks(w, text, fs);
}


// ── forward declarations ────────────────────────────────────────────────────

static void processBlockChildren(SimpleHtmlParser &parser, const QString &endTag,
                                 FormatState state);

// ── SVG / image helpers ────────────────────────────────────────────────────

// Raster density of the PNG fallback used for viewers that cannot render the
// vector SVG part. Word 2016+ uses the SVG and ignores this PNG entirely.
// On-page extents come from the viewBox in inches and are independent of this
// DPI, so changing it must never change the displayed size.
static constexpr double kSvgFallbackDpi = 300.0;

// Drawing-extent cap: 5.5 inches, the widest printable figure we allow.
static constexpr int kMaxEmu = 5029200;

// Raster canvases are captured at 2× CSS pixels → 192 canvas-px/inch. Used for
// the no-explicit-dims raster path; explicit HTML width/height and vector
// SVGs are CSS px at 96 DPI instead (914400/96), so the 192-vs-96 distinction
// stays documented here.
static constexpr double kPxToEmu = 914400.0 / 192.0;

static QByteArray ensureSvgXmlns(const QByteArray &svg)
{
    int lt = svg.indexOf("<svg");
    int gt = svg.indexOf('>', lt);
    if (lt < 0 || gt <= lt)
        return svg;
    // Only bail if an xmlns appears inside the opening <svg> tag itself;
    // xmlns:xlink or an "xmlns" literal in an attribute value must not
    // suppress the default namespace insertion.
    int xmlnsPos = svg.indexOf("xmlns", lt);
    if (xmlnsPos >= lt && xmlnsPos < gt)
        return svg;
    QByteArray out = svg;
    out.insert(gt, " xmlns=\"http://www.w3.org/2000/svg\"");
    return out;
}

// Result of rasterizing an SVG: the PNG fallback, the raw vector bytes, and
// the on-page extent in EMUs. An empty pngData means rasterization failed.
struct SvgRaster {
    QByteArray pngData;
    QByteArray svgData;
    int cxEmu = 0;
    int cyEmu = 0;
};

static SvgRaster rasterizeSvg(const QString &svgXml)
{
    SvgRaster out;
    if (svgXml.isEmpty())
        return out;

    QSvgRenderer renderer(svgXml.toUtf8());
    if (!renderer.isValid())
        return out;

    QRectF vb = renderer.viewBox();
    if (vb.isEmpty())
        vb = QRectF(QPointF(0, 0), renderer.defaultSize());
    if (vb.isEmpty() || vb.width() <= 0 || vb.height() <= 0)
        return out;

    // 300-DPI fallback raster; SVGs are also embedded as-is for vector rendering.
    double scale = kSvgFallbackDpi / 72.0;
    int w = qMax(1, static_cast<int>(vb.width() * scale));
    int h = qMax(1, static_cast<int>(vb.height() * scale));

    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    painter.end();

    QBuffer buffer(&out.pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    // EMUs: 1 inch = 914400 EMUs. Cap width to 5.5 inches for printable fit.
    double inchesW = vb.width() / 72.0;
    double inchesH = vb.height() / 72.0;
    out.cxEmu = qMax(1, static_cast<int>(inchesW * 914400.0));
    out.cyEmu = qMax(1, static_cast<int>(inchesH * 914400.0));
    if (out.cxEmu > kMaxEmu) {
        out.cyEmu = static_cast<int>(static_cast<qint64>(out.cyEmu) * kMaxEmu / out.cxEmu);
        out.cxEmu = kMaxEmu;
    }

    out.svgData = ensureSvgXmlns(svgXml.toUtf8());
    return out;
}

static int registerImage(const QByteArray &pngData, int cxEmu, int cyEmu,
                         const QByteArray &svgData = {})
{
    if (!g_images || pngData.isEmpty())
        return -1;
    int id = ++g_imageCounter;
    OoxmlImage img;
    img.relId = QStringLiteral("rIdImg%1").arg(id);
    img.fileName = QStringLiteral("media/image%1.png").arg(id);
    img.pngData = pngData;
    img.svgData = svgData;
    img.cxEmu = cxEmu;
    img.cyEmu = cyEmu;
    if (!svgData.isEmpty()) {
        img.svgRelId = QStringLiteral("rIdSvg%1").arg(id);
        img.svgFileName = QStringLiteral("media/image%1.svg").arg(id);
    }
    g_images->push_back(img);
    return id - 1;
}

static void writeDrawingRun(QXmlStreamWriter &w, const QString &relId,
                            const QString &svgRelId, int cxEmu, int cyEmu,
                            int docPrId)
{
    w.writeStartElement("w:r");
    w.writeStartElement("w:drawing");
    w.writeStartElement("wp:inline");
    w.writeAttribute("distT", "0");
    w.writeAttribute("distB", "0");
    w.writeAttribute("distL", "0");
    w.writeAttribute("distR", "0");

    w.writeStartElement("wp:extent");
    w.writeAttribute("cx", QString::number(cxEmu));
    w.writeAttribute("cy", QString::number(cyEmu));
    w.writeEndElement();

    w.writeStartElement("wp:effectExtent");
    w.writeAttribute("l", "0");
    w.writeAttribute("t", "0");
    w.writeAttribute("r", "0");
    w.writeAttribute("b", "0");
    w.writeEndElement();

    w.writeStartElement("wp:docPr");
    w.writeAttribute("id", QString::number(docPrId));
    w.writeAttribute("name", "Picture");
    w.writeEndElement();

    w.writeEmptyElement("wp:cNvGraphicFramePr");

    w.writeStartElement("a:graphic");
    w.writeStartElement("a:graphicData");
    w.writeAttribute("uri", "http://schemas.openxmlformats.org/drawingml/2006/picture");

    w.writeStartElement("pic:pic");
    w.writeStartElement("pic:nvPicPr");
    w.writeStartElement("pic:cNvPr");
    w.writeAttribute("id", "0");
    w.writeAttribute("name", "Picture");
    w.writeEndElement();
    w.writeEmptyElement("pic:cNvPicPr");
    w.writeEndElement();

    w.writeStartElement("pic:blipFill");
    w.writeStartElement("a:blip");
    w.writeAttribute("r:embed", relId);
    if (!svgRelId.isEmpty()) {
        w.writeAttribute("cstate", "print");
        w.writeStartElement("a:extLst");
        w.writeStartElement("a:ext");
        w.writeAttribute("uri", "{96DAC541-7B7A-43D3-8B79-37D633B846F1}");
        // QXmlStreamWriter cannot declare the nested `asvg` prefix because the
        // writer's root element is stripped from bodyXml in convert(); emit a
        // marker and replace it with a self-declaring fragment after serialization.
        w.writeCharacters(QStringLiteral("@@SVGBLIP@") + svgRelId);
        w.writeEndElement(); // a:ext
        w.writeEndElement(); // a:extLst
    }
    w.writeEndElement(); // a:blip
    w.writeStartElement("a:stretch");
    w.writeEmptyElement("a:fillRect");
    w.writeEndElement();
    w.writeEndElement();

    w.writeStartElement("pic:spPr");
    w.writeStartElement("a:xfrm");
    w.writeStartElement("a:off");
    w.writeAttribute("x", "0");
    w.writeAttribute("y", "0");
    w.writeEndElement();
    w.writeStartElement("a:ext");
    w.writeAttribute("cx", QString::number(cxEmu));
    w.writeAttribute("cy", QString::number(cyEmu));
    w.writeEndElement();
    w.writeEndElement();
    w.writeStartElement("a:prstGeom");
    w.writeAttribute("prst", "rect");
    w.writeEndElement();
    w.writeEndElement();

    w.writeEndElement(); // pic:pic
    w.writeEndElement(); // a:graphicData
    w.writeEndElement(); // a:graphic

    w.writeEndElement(); // wp:inline
    w.writeEndElement(); // w:drawing
    w.writeEndElement(); // w:r
}

static QString expandSvgBlipMarkers(const QString &body)
{
    static const QString svgNs = QStringLiteral(
        "http://schemas.microsoft.com/office/drawing/2016/SVG/main");
    static const QRegularExpression marker(QStringLiteral("@@SVGBLIP@(rId\\w+)"));
    QString out = body;
    int offset = 0;
    auto it = marker.globalMatch(body);
    while (it.hasNext()) {
        auto m = it.next();
        const QString relId = m.captured(1);
        const QString frag = QStringLiteral("<asvg:svgBlip xmlns:asvg=\"%1\" r:embed=\"%2\"/>")
                                 .arg(svgNs, relId);
        out.replace(m.capturedStart(0) + offset, m.capturedLength(0), frag);
        offset += frag.size() - m.capturedLength(0);
    }
    return out;
}

static void handleSvgBlock(SimpleHtmlParser &parser)
{
    // Capture raw SVG XML by scanning the original HTML string
    int startPos = parser.tagStart();
    int depth = 1;
    int p = startPos;
    const QString &raw = parser.rawHtml();

    // Find the closing > of <svg ...>
    int openEnd = raw.indexOf('>', p);
    if (openEnd == -1) return;
    p = openEnd + 1;

    while (p < raw.size() && depth > 0) {
        int lt = raw.indexOf('<', p);
        if (lt == -1) break;
        int gt = raw.indexOf('>', lt);
        if (gt == -1) break;

        bool isClose = (lt + 1 < raw.size() && raw[lt + 1] == '/');
        bool isSelfClose = (gt > 0 && raw[gt - 1] == '/');

        int ns = lt + 1 + (isClose ? 1 : 0);
        int ne = ns;
        while (ne < gt && !raw[ne].isSpace() && raw[ne] != '/' && raw[ne] != '>')
            ++ne;
        QString tn = raw.mid(ns, ne - ns).toLower();

        if (tn == "svg") {
            if (isClose) --depth;
            else if (!isSelfClose) ++depth;
        }
        p = gt + 1;
    }

    QString svgXml = raw.mid(startPos, p - startPos);
    parser.seekTo(p);

    SvgRaster raster = rasterizeSvg(svgXml);
    if (raster.pngData.isEmpty()) return;

    int imgIdx = registerImage(raster.pngData, raster.cxEmu, raster.cyEmu, raster.svgData);
    if (imgIdx < 0) return;

    const OoxmlImage &img = g_images->at(imgIdx);
    bodyWriter->writeStartElement("w:p");
    writeDrawingRun(*bodyWriter, img.relId, img.svgRelId, raster.cxEmu, raster.cyEmu, imgIdx + 1);
    bodyWriter->writeEndElement();
}

static void handleSvgInline(SimpleHtmlParser &parser)
{
    int startPos = parser.tagStart();
    int depth = 1;
    int p = startPos;
    const QString &raw = parser.rawHtml();

    int openEnd = raw.indexOf('>', p);
    if (openEnd == -1) return;
    p = openEnd + 1;

    while (p < raw.size() && depth > 0) {
        int lt = raw.indexOf('<', p);
        if (lt == -1) break;
        int gt = raw.indexOf('>', lt);
        if (gt == -1) break;

        bool isClose = (lt + 1 < raw.size() && raw[lt + 1] == '/');
        bool isSelfClose = (gt > 0 && raw[gt - 1] == '/');

        int ns = lt + 1 + (isClose ? 1 : 0);
        int ne = ns;
        while (ne < gt && !raw[ne].isSpace() && raw[ne] != '/' && raw[ne] != '>')
            ++ne;
        QString tn = raw.mid(ns, ne - ns).toLower();

        if (tn == "svg") {
            if (isClose) --depth;
            else if (!isSelfClose) ++depth;
        }
        p = gt + 1;
    }

    QString svgXml = raw.mid(startPos, p - startPos);
    parser.seekTo(p);

    SvgRaster raster = rasterizeSvg(svgXml);
    if (raster.pngData.isEmpty()) return;

    int imgIdx = registerImage(raster.pngData, raster.cxEmu, raster.cyEmu, raster.svgData);
    if (imgIdx < 0) return;

    const OoxmlImage &img = g_images->at(imgIdx);
    writeDrawingRun(*bodyWriter, img.relId, img.svgRelId, raster.cxEmu, raster.cyEmu, imgIdx + 1);
}

static void handleImgTag(const HtmlToken &tok)
{
    QString src = tok.attrs.value(QStringLiteral("src"));
    if (!src.startsWith(QStringLiteral("data:")))
        return;

    // Parse "data:<mime>;base64,<data>"
    int b64Pos = src.indexOf(QStringLiteral(";base64,"));
    if (b64Pos < 0) return;
    QString mime = src.mid(5, b64Pos - 5);
    QByteArray imgData = QByteArray::fromBase64(src.mid(b64Pos + 8).toLatin1());
    if (imgData.isEmpty()) return;

    // Sizing from style attribute (set by the #WxH markdown suffix).
    // SVG uses exact `width`/`height` (target, may scale UP past natural size),
    // raster uses `max-width`/`max-height` (cap only, never blur-upscales).
    static const QRegularExpression dimRe(
        QStringLiteral("(max-)?(?:width|height)\\s*:\\s*(\\d+)px"),
        QRegularExpression::CaseInsensitiveOption);
    int targetW = -1, targetH = -1, maxW = -1, maxH = -1;
    QString style = tok.attrs.value(QStringLiteral("style"));
    auto styleIt = dimRe.globalMatch(style);
    while (styleIt.hasNext()) {
        auto m = styleIt.next();
        bool isMax = !m.captured(1).isEmpty();
        bool isWidth = m.captured(0).contains(QStringLiteral("width"),
                                              Qt::CaseInsensitive);
        int val = m.captured(2).toInt();
        if (isMax)
            (isWidth ? maxW : maxH) = val;
        else
            (isWidth ? targetW : targetH) = val;
    }

    int imgW = 0;
    int imgH = 0;
    qreal dispW = 0, dispH = 0;   // CSS-px display size (SVG branch only)
    QImage image;
    QByteArray pngData;

    const bool isSvg = mime == QStringLiteral("image/svg+xml")
        || imgData.startsWith("<svg") || imgData.startsWith("<?xml");

    if (isSvg) {
        // Rasterize vector image so DOCX can embed it. Target size lets a
        // #400x upscale a 200px-natural SVG crisply (vector).
        QSvgRenderer renderer(imgData);
        if (!renderer.isValid())
            return;
        QSizeF vb = renderer.defaultSize();
        if (vb.isEmpty())
            vb = (renderer.viewBox().isEmpty()
                ? QSizeF(300.0, 150.0)
                : renderer.viewBox().size());

        // Compute display size in pixels. Aspect ratio preserved when only one
        // dimension given. Natural size uses the SVG's intrinsic dimensions.
        qreal ar = vb.height() > 0 ? vb.width() / vb.height() : 1.0;
        dispW = targetW >= 0 ? targetW : vb.width();
        dispH = targetH >= 0 ? targetH : vb.height();
        if (targetW >= 0 && targetH < 0)
            dispH = dispW / ar;
        else if (targetW < 0 && targetH >= 0)
            dispW = dispH * ar;
        if (maxW > 0 && dispW > maxW) dispW = maxW;
        if (maxH > 0 && dispH > maxH) dispH = maxH;
        if (dispW <= 0 || dispH <= 0)
            return;

        // Rasterize fallback at 300 DPI for non-SVG-aware viewers (Word 2016+
        // renders the embedded vector and ignores this PNG).
        imgW = qMax(1, static_cast<int>(dispW * kSvgFallbackDpi / 96.0));
        imgH = qMax(1, static_cast<int>(dispH * kSvgFallbackDpi / 96.0));
        QImage svgImage(imgW, imgH, QImage::Format_ARGB32);
        svgImage.fill(Qt::white);
        QPainter painter(&svgImage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        renderer.render(&painter, QRectF(0, 0, imgW, imgH));
        painter.end();
        image = svgImage;
    } else {
        if (!image.loadFromData(imgData))
            return;
        imgW = image.width();
        imgH = image.height();
        if (imgW <= 0 || imgH <= 0) return;

        // Crop transparent margins (display math often has large transparent
        // padding from KaTeX blocks stretching to the full container width)
        {
            int firstX = imgW, lastX = 0, firstY = imgH, lastY = 0;
            for (int y = 0; y < imgH; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                for (int x = 0; x < imgW; ++x) {
                    if (qAlpha(line[x]) > 0) {
                        if (x < firstX) firstX = x;
                        if (x > lastX)  lastX = x;
                        if (y < firstY) firstY = y;
                        if (y > lastY)  lastY = y;
                    }
                }
            }
            if (firstX <= lastX && firstY <= lastY) {
                int cropW = lastX - firstX + 1;
                int cropH = lastY - firstY + 1;
                if (cropW < imgW || cropH < imgH) {
                    image = image.copy(firstX, firstY, cropW, cropH);
                    imgW = cropW;
                    imgH = cropH;
                }
            }
        }

        // Cap only (max-width/max-height) — never upscale raster pixels
        if (maxW > 0 && imgW > maxW)
            imgH = static_cast<int>(static_cast<qint64>(imgH) * maxW / imgW),
            imgW = maxW;
        if (maxH > 0 && imgH > maxH)
            imgW = static_cast<int>(static_cast<qint64>(imgW) * maxH / imgH),
            imgH = maxH;
    }

    // Convert to PNG for DOCX registration
    QBuffer buf(&pngData);
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, "PNG");

    // ── extents ────────────────────────────────────────────────────────────
    int cxEmu = 0, cyEmu = 0;
    QByteArray svgData;   // raw vector bytes, if any
    bool explicitHtmlDims = false;
    int htmlW = -1, htmlH = -1;

    if (isSvg) {
        // Vector: keep the original SVG for embedding; extents are CSS px @96 DPI.
        cxEmu = qMax(1, static_cast<int>(dispW * 914400.0 / 96.0));
        cyEmu = qMax(1, static_cast<int>(dispH * 914400.0 / 96.0));
        svgData = ensureSvgXmlns(imgData);
    } else {
        bool okW = false, okH = false;
        htmlW = tok.attrs.value(QStringLiteral("width")).toInt(&okW);
        htmlH = tok.attrs.value(QStringLiteral("height")).toInt(&okH);
        explicitHtmlDims = (okW && okH);
        if (explicitHtmlDims) {
            cxEmu = qMax(1, static_cast<int>(htmlW * 914400.0 / 96.0));
            cyEmu = qMax(1, static_cast<int>(htmlH * 914400.0 / 96.0));
        } else {
            cxEmu = qMax(1, static_cast<int>(imgW * kPxToEmu));
            cyEmu = qMax(1, static_cast<int>(imgH * kPxToEmu));
        }
    }
    if (cxEmu > kMaxEmu) {
        cyEmu = static_cast<int>(static_cast<qint64>(cyEmu) * kMaxEmu / cxEmu);
        cxEmu = kMaxEmu;
    }

    int imgIdx = registerImage(pngData, cxEmu, cyEmu, svgData);
    if (imgIdx < 0) return;

    const OoxmlImage &img = g_images->at(imgIdx);
    writeDrawingRun(*bodyWriter, img.relId, img.svgRelId, cxEmu, cyEmu, imgIdx + 1);
}

// ── style-attribute helper ────────────────────────────────────────────────────

static FormatState applyInlineStyle(const FormatState &base, const QMap<QString, QString> &attrs)
{
    FormatState s = base;
    QString style = attrs.value(QStringLiteral("style"));
    if (style.isEmpty()) return s;

    auto td = CssValueParser::parseTextDecoration(style);
    if (td.lineThrough) s.strike = true;
    if (td.underline) {
        s.underline = true;
        s.underlineStyle = td.style;
        s.underlineColor = td.color;
    }
    QString ff = CssValueParser::parseFontFamily(style);
    if (!ff.isEmpty()) s.fontFamily = ff;
    QString ts = CssValueParser::extractCssProperty(style, QStringLiteral("text-shadow"));
    if (!ts.isEmpty() && ts.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0)
        s.shadow = true;
    return s;
}

static void processInlineChildren(SimpleHtmlParser &parser, const QString &endTag,
                                  FormatState state)
{
    while (!parser.atEnd()) {
        parser.readNext();
        const auto &tok = parser.current();

        if (tok.type == HtmlToken::TagClose && tok.name == endTag)
            return;
        if (tok.type == HtmlToken::SelfClose && tok.name == endTag)
            return;

        if (tok.type == HtmlToken::TagOpen || tok.type == HtmlToken::SelfClose) {
            const QString &n = tok.name;
            if (n == "strong" || n == "b") {
                FormatState s = applyInlineStyle(state, tok.attrs);
                s.bold = true;
                processInlineChildren(parser, n, s);
            } else if (n == "em" || n == "i") {
                FormatState s = applyInlineStyle(state, tok.attrs);
                s.italic = true;
                processInlineChildren(parser, n, s);
            } else if (n == "code") {
                FormatState s = applyInlineStyle(state, tok.attrs);
                s.code = true;
                processInlineChildren(parser, n, s);
            } else if (n == "del" || n == "s" || n == "strike") {
                FormatState s = applyInlineStyle(state, tok.attrs);
                s.strike = true;
                processInlineChildren(parser, n, s);
            } else if (n == "u" || n == "ins") {
                FormatState s = applyInlineStyle(state, tok.attrs);
                s.underline = true;
                processInlineChildren(parser, n, s);
            } else if (n == "a") {
                QString href = tok.attrs.value(QStringLiteral("href"));
                QString relId;
                if (!href.isEmpty() && g_hyperlinks) {
                    int id = ++g_hyperlinkCounter;
                    relId = QStringLiteral("rIdLink%1").arg(id);
                    g_hyperlinks->push_back({relId, href});
                    bodyWriter->writeStartElement("w:hyperlink");
                    bodyWriter->writeAttribute(
                        QStringLiteral("r:id"), relId);
                }
                processInlineChildren(parser, QStringLiteral("a"), state);
                if (!relId.isEmpty())
                    bodyWriter->writeEndElement(); // w:hyperlink
            } else if (n == "br") {
                writeBreak(*bodyWriter);
            } else if (n == "img") {
                handleImgTag(tok);
            } else if (n == "svg") {
                handleSvgInline(parser);
            } else if (n == "span" && tok.attrs.value("class").split(' ').contains("katex")) {
                // KaTeX math — convert to OMML using data-mathml/data-tex
                writeKatexAsOmml(parser, n, tok.attrs.value(QStringLiteral("data-tex")),
                                 tok.attrs.value(QStringLiteral("data-mathml")));
            } else if (n == "span") {
                // Parse style for font-family, text-decoration, text-shadow
                processInlineChildren(parser, n, applyInlineStyle(state, tok.attrs));
            } else if (isVoidElement(n)) {
                // Void elements (input, hr, col, etc.) have no closing tag — skip
            } else {
                processInlineChildren(parser, n, applyInlineStyle(state, tok.attrs));
            }
        } else if (tok.type == HtmlToken::Text) {
            writeRun(*bodyWriter, tok.text, state);
        }
    }
}

// ── block-level handlers ────────────────────────────────────────────────────

static void writeParaStart(const QString &styleId, const Margins &margins = {})
{
    bodyWriter->writeStartElement("w:p");
    bool hasPPr = !styleId.isEmpty()
        || margins.top >= 0 || margins.bottom >= 0
        || margins.left >= 0 || margins.right >= 0;
    if (hasPPr) {
        bodyWriter->writeStartElement("w:pPr");
        if (!styleId.isEmpty()) {
            bodyWriter->writeStartElement("w:pStyle");
            bodyWriter->writeAttribute("w:val", styleId);
            bodyWriter->writeEndElement();
        }
        if (margins.top >= 0 || margins.bottom >= 0) {
            bodyWriter->writeStartElement("w:spacing");
            if (margins.top >= 0)
                bodyWriter->writeAttribute("w:before", QString::number(margins.top));
            if (margins.bottom >= 0)
                bodyWriter->writeAttribute("w:after", QString::number(margins.bottom));
            bodyWriter->writeEndElement();
        }
        if (margins.left >= 0 || margins.right >= 0) {
            bodyWriter->writeStartElement("w:ind");
            if (margins.left >= 0)
                bodyWriter->writeAttribute("w:left", QString::number(margins.left));
            if (margins.right >= 0)
                bodyWriter->writeAttribute("w:right", QString::number(margins.right));
            bodyWriter->writeEndElement();
        }
        bodyWriter->writeEndElement();
    }
}

static void writeParaEnd()
{
    bodyWriter->writeEndElement();
}

static void handleParagraph(const QString &styleId,
                            SimpleHtmlParser &parser, const QString &endTag,
                            FormatState state,
                            const QMap<QString, QString> &attrs = {})
{
    Margins margins;
    auto styleIt = attrs.find(QStringLiteral("style"));
    if (styleIt != attrs.end()) {
        margins = CssValueParser::parseMargins(styleIt.value());
        state = applyInlineStyle(state, attrs);
    }
    writeParaStart(styleId, margins);
    processInlineChildren(parser, endTag, state);
    writeParaEnd();
}

static void handlePre(SimpleHtmlParser &parser)
{
    bodyWriter->writeStartElement("w:p");
    bodyWriter->writeStartElement("w:pPr");
    bodyWriter->writeStartElement("w:pStyle");
    bodyWriter->writeAttribute("w:val", "SourceCode");
    bodyWriter->writeEndElement();
    bodyWriter->writeEndElement();

    FormatState state;
    state.code = true;
    processInlineChildren(parser, QStringLiteral("pre"), state);
    writeParaEnd();
}

static void handleDiv(SimpleHtmlParser &parser, const QString &endTag,
                      const QMap<QString, QString> &attrs = {})
{
    QString cls = attrs.value(QStringLiteral("class"));
    if (cls.startsWith(QStringLiteral("admonition "))) {
        QString prev = g_admonitionType;
        g_admonitionType = cls.section(' ', 1, 1);
        processBlockChildren(parser, endTag, FormatState{});
        g_admonitionType = prev;
        return;
    }
    processBlockChildren(parser, endTag, FormatState{});
}

// ── table handler ────────────────────────────────────────────────────────────

static void writeCellBorders(QXmlStreamWriter &w, const QString &styleAttr)
{
    if (styleAttr.isEmpty()) return;
    QString borderVal = CssValueParser::extractCssProperty(styleAttr, QStringLiteral("border"));
    if (borderVal.isEmpty() || borderVal == QStringLiteral("none") || borderVal == QStringLiteral("0"))
        return;
    auto bs = CssValueParser::parseCssBorder(borderVal);
    if (bs.stroke == QStringLiteral("nil")) return;
    w.writeStartElement("w:tcBorders");
    auto writeSide = [&](const QString &side) {
        w.writeStartElement("w:" + side);
        w.writeAttribute("w:val", bs.stroke);
        w.writeAttribute("w:sz", QString::number(bs.size));
        w.writeAttribute("w:space", "0");
        w.writeAttribute("w:color", bs.color);
        w.writeEndElement();
    };
    writeSide(QStringLiteral("top"));
    writeSide(QStringLiteral("left"));
    writeSide(QStringLiteral("bottom"));
    writeSide(QStringLiteral("right"));
    w.writeEndElement();
}

// Count the cells in the table's first row so <w:tblGrid> can give each column
// an equal share of the table width. The main parser is forward-only, so scan
// with a fresh parser positioned right after the <table> tag.
static int firstRowColumnCount(SimpleHtmlParser &parser)
{
    SimpleHtmlParser scan(parser.rawHtml());
    scan.seekTo(parser.pos());
    int cols = 0;
    bool inFirstRow = false;
    while (!scan.atEnd()) {
        scan.readNext();
        const auto &tok = scan.current();
        if (tok.type == HtmlToken::TagClose) {
            if (inFirstRow && tok.name == QStringLiteral("tr"))
                break;
            continue;
        }
        if (tok.type != HtmlToken::TagOpen && tok.type != HtmlToken::SelfClose)
            continue;
        const QString &n = tok.name;
        if (n == QStringLiteral("tr")) {
            inFirstRow = true;
        } else if (inFirstRow
                   && (n == QStringLiteral("th") || n == QStringLiteral("td"))) {
            ++cols;
        }
    }
    return qMax(1, cols);
}

static void handleTable(SimpleHtmlParser &parser,
                        const QMap<QString, QString> &attrs = {})
{
    bodyWriter->writeStartElement("w:tbl");

    bodyWriter->writeStartElement("w:tblPr");
    bodyWriter->writeStartElement("w:tblW");
    bodyWriter->writeAttribute("w:w", "9072");
    bodyWriter->writeAttribute("w:type", "dxa");
    bodyWriter->writeEndElement();

    // Table borders: always emitted — markdown tables carry no inline border
    // style (borders come from the preview CSS), so without this the grid is
    // invisible in Word. An inline border style (HTML-authored tables)
    // overrides the four outer sides; inside separators stay single 0.5 pt.
    bodyWriter->writeStartElement("w:tblBorders");
    auto writeBorderSide = [&](const QString &side, const QString &val,
                               const QString &sz, const QString &color) {
        bodyWriter->writeStartElement("w:" + side);
        bodyWriter->writeAttribute("w:val", val);
        bodyWriter->writeAttribute("w:sz", sz);
        bodyWriter->writeAttribute("w:space", "0");
        bodyWriter->writeAttribute("w:color", color);
        bodyWriter->writeEndElement();
    };
    QString tableStyle = attrs.value(QStringLiteral("style"));
    QString borderVal = CssValueParser::extractCssProperty(tableStyle, QStringLiteral("border"));
    auto bs = CssValueParser::parseCssBorder(borderVal);
    bool useInline = bs.stroke != QStringLiteral("nil");
    writeBorderSide(QStringLiteral("top"), useInline ? bs.stroke : QStringLiteral("single"),
                    useInline ? QString::number(bs.size) : QStringLiteral("4"),
                    useInline ? bs.color : QStringLiteral("auto"));
    writeBorderSide(QStringLiteral("left"), useInline ? bs.stroke : QStringLiteral("single"),
                    useInline ? QString::number(bs.size) : QStringLiteral("4"),
                    useInline ? bs.color : QStringLiteral("auto"));
    writeBorderSide(QStringLiteral("bottom"), useInline ? bs.stroke : QStringLiteral("single"),
                    useInline ? QString::number(bs.size) : QStringLiteral("4"),
                    useInline ? bs.color : QStringLiteral("auto"));
    writeBorderSide(QStringLiteral("right"), useInline ? bs.stroke : QStringLiteral("single"),
                    useInline ? QString::number(bs.size) : QStringLiteral("4"),
                    useInline ? bs.color : QStringLiteral("auto"));
    writeBorderSide(QStringLiteral("insideH"), QStringLiteral("single"),
                    QStringLiteral("4"), QStringLiteral("auto"));
    writeBorderSide(QStringLiteral("insideV"), QStringLiteral("single"),
                    QStringLiteral("4"), QStringLiteral("auto"));
    bodyWriter->writeEndElement(); // w:tblBorders

    bodyWriter->writeEndElement(); // w:tblPr

    // Column widths: equal split of the 9072 dxa table width into one gridCol
    // per first-row cell, emitted before the first row per the w:tbl schema.
    int cols = firstRowColumnCount(parser);
    bodyWriter->writeStartElement("w:tblGrid");
    for (int c = 0; c < cols; ++c) {
        bodyWriter->writeEmptyElement("w:gridCol");
        bodyWriter->writeAttribute("w:w", QString::number(9072 / cols));
    }
    bodyWriter->writeEndElement(); // w:tblGrid

    while (!parser.atEnd()) {
        parser.readNext();
        const auto &tok = parser.current();

        if (tok.type == HtmlToken::TagClose) {
            if (tok.name == QStringLiteral("table"))
                break;
            if (tok.name == QStringLiteral("tr"))
                bodyWriter->writeEndElement();
            continue;
        }
        if (tok.type != HtmlToken::TagOpen && tok.type != HtmlToken::SelfClose)
            continue;

        const QString &n = tok.name;
        if (n == "thead" || n == "tbody") {
            continue;
        } else if (n == "tr") {
            bodyWriter->writeStartElement("w:tr");
        } else if (n == "th") {
            bodyWriter->writeStartElement("w:tc");
            bodyWriter->writeStartElement("w:tcPr");
            bodyWriter->writeStartElement("w:shd");
            bodyWriter->writeAttribute("w:fill", "D9E2F3");
            bodyWriter->writeAttribute("w:val", "clear");
            bodyWriter->writeEndElement();
            writeCellBorders(*bodyWriter, tok.attrs.value(QStringLiteral("style")));
            bodyWriter->writeEndElement();
            handleParagraph(QString(), parser, QStringLiteral("th"), FormatState{}, tok.attrs);
            bodyWriter->writeEndElement();
        } else if (n == "td") {
            bodyWriter->writeStartElement("w:tc");
            bodyWriter->writeStartElement("w:tcPr");
            writeCellBorders(*bodyWriter, tok.attrs.value(QStringLiteral("style")));
            bodyWriter->writeEndElement();
            handleParagraph(QString(), parser, QStringLiteral("td"), FormatState{}, tok.attrs);
            bodyWriter->writeEndElement();
        }
    }

    bodyWriter->writeEndElement(); // w:tbl
}

// ── list handler ─────────────────────────────────────────────────────────────

static void handleList(SimpleHtmlParser &parser, bool ordered, int depth)
{
    int numId = ordered ? 2 : 1;

    while (!parser.atEnd()) {
        parser.readNext();
        const auto &tok = parser.current();

        if (tok.type == HtmlToken::TagClose
            && (tok.name == QStringLiteral("ul")
                || tok.name == QStringLiteral("ol")))
            return;

        if (tok.type != HtmlToken::TagOpen && tok.type != HtmlToken::SelfClose)
            continue;

        const QString &n = tok.name;
        if (n == "li") {
            bodyWriter->writeStartElement("w:p");
            bodyWriter->writeStartElement("w:pPr");
            bodyWriter->writeStartElement("w:numPr");
            bodyWriter->writeStartElement("w:ilvl");
            bodyWriter->writeAttribute("w:val", QString::number(depth));
            bodyWriter->writeEndElement();
            bodyWriter->writeStartElement("w:numId");
            bodyWriter->writeAttribute("w:val", QString::number(numId));
            bodyWriter->writeEndElement();
            bodyWriter->writeEndElement();
            bodyWriter->writeEndElement();
            processInlineChildren(parser, QStringLiteral("li"), FormatState{});
            bodyWriter->writeEndElement();
        } else if (n == "ul" || n == "ol") {
            bool nestedOrdered = (n == "ol");
            handleList(parser, nestedOrdered, depth + 1);
        }
    }
}

// ── block-level dispatcher ───────────────────────────────────────────────────

static void processBlockChildren(SimpleHtmlParser &parser, const QString &endTag,
                                 FormatState state)
{
    while (!parser.atEnd()) {
        parser.readNext();
        const auto &tok = parser.current();

        if (tok.type == HtmlToken::TagClose && tok.name == endTag)
            return;
        if (tok.type != HtmlToken::TagOpen)
            continue;

        const QString &tag = tok.name;

        if (tag.startsWith('h') && tag.length() == 2 && tag[1] >= '1' && tag[1] <= '6') {
            handleParagraph("Heading" + tag.mid(1), parser, tag, state, tok.attrs);
        } else if (tag == "p") {
            QString pClass = tok.attrs.value(QStringLiteral("class"));
            QString styleId = "Normal";
            if (pClass.contains("admonition-title")) {
                styleId = "AdmonitionTitle" + g_admonitionType;
            } else if (!g_admonitionType.isEmpty()) {
                styleId = "AdmonitionText" + g_admonitionType;
            }
            handleParagraph(styleId, parser, tag, state, tok.attrs);
        } else if (tag == "pre") {
            handlePre(parser);
        } else if (tag == "blockquote") {
            handleParagraph("Quote", parser, tag, state, tok.attrs);
        } else if (tag == "ul") {
            handleList(parser, false, 0);
        } else if (tag == "ol") {
            handleList(parser, true, 0);
        } else if (tag == "table") {
            handleTable(parser, tok.attrs);
        } else if (tag == "div") {
            handleDiv(parser, tag, tok.attrs);
        } else if (tag == "svg") {
            handleSvgBlock(parser);
        } else if (tag == "span" && tok.attrs.value("class").split(' ').contains("katex")) {
            // KaTeX math at block level (e.g. inside katex-display div)
            bodyWriter->writeStartElement("w:p");
            writeKatexAsOmml(parser, tag, tok.attrs.value(QStringLiteral("data-tex")),
                             tok.attrs.value(QStringLiteral("data-mathml")));
            bodyWriter->writeEndElement();
        } else if (tag == "img") {
            bodyWriter->writeStartElement("w:p");
            handleImgTag(tok);
            bodyWriter->writeEndElement();
        } else if (tag == "hr") {
            // Markdown --- separators must not vanish: render as a paragraph
            // with a bottom border instead of falling into the void-element skip.
            bodyWriter->writeStartElement("w:p");
            bodyWriter->writeStartElement("w:pPr");
            bodyWriter->writeStartElement("w:pBdr");
            bodyWriter->writeStartElement("w:bottom");
            bodyWriter->writeAttribute("w:val", "single");
            bodyWriter->writeAttribute("w:sz", "6");
            bodyWriter->writeAttribute("w:space", "1");
            bodyWriter->writeAttribute("w:color", "808080");
            bodyWriter->writeEndElement();
            bodyWriter->writeEndElement();
            bodyWriter->writeEndElement();
            bodyWriter->writeEndElement();
        } else if (isVoidElement(tag)) {
            // Void elements have no closing tag — skip
        } else {
            processBlockChildren(parser, tag, state);
        }
    }
}

// ── public API ──────────────────────────────────────────────────────────────

OoxmlResult HtmlToOoxml::convert(const QString &html, const QString &themeCss)
{
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    bodyWriter = &w;

    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
        QStringLiteral("w"));
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
        QStringLiteral("wp"));
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main"),
        QStringLiteral("a"));
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/picture"),
        QStringLiteral("pic"));
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"),
        QStringLiteral("r"));
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/math"),
        QStringLiteral("m"));

    OoxmlResult result;
    g_images = &result.images;
    g_imageCounter = 0;
    g_hyperlinks = &result.hyperlinks;
    g_hyperlinkCounter = 0;

    w.writeStartElement("x");

    SimpleHtmlParser parser(html);
    FormatState state;
    processBlockChildren(parser, QString(), state);

    w.writeEndElement(); // x

    QString raw = expandSvgBlipMarkers(QString::fromUtf8(buf));
    int start = raw.indexOf('>');
    int end = raw.lastIndexOf('<');
    if (start >= 0 && end > start)
        result.bodyXml = raw.mid(start + 1, end - start - 1);

    g_images = nullptr;
    g_hyperlinks = nullptr;
    return result;
}

// ── styles.xml builder ───────────────────────────────────────────────────────

QString styleColor(const QString &themeCss, const QString &tag, const QString &fallback)
{
    QRegularExpression re(tag + R"(\s*\{[^}]*\bcolor\s*:\s*([^;}]+))",
                          QRegularExpression::CaseInsensitiveOption);
    QString c;
    auto it = re.globalMatch(themeCss);
    while (it.hasNext()) {
        auto m = it.next();
        QString val = m.captured(1).trimmed();
        if (!val.isEmpty()) c = val;
    }
    if (c.isEmpty()) return fallback;
    if (c.startsWith('#')) {
        c = c.mid(1);
        if (c.length() == 3) {
            c = QString(c[0]) + c[0] + c[1] + c[1] + c[2] + c[2];
        }
        return c.toUpper();
    }
    return fallback;
}

static QString extractCssValue(const QString &themeCss, const QString &selector,
                                const QString &property, const QString &fallback)
{
    QRegularExpression re(QRegularExpression::escape(selector)
                          + R"(\s*\{[^}]*\b)" + property + R"(\s*:\s*([^;}]+))",
                          QRegularExpression::CaseInsensitiveOption);
    QString v;
    auto it = re.globalMatch(themeCss);
    while (it.hasNext()) {
        auto m = it.next();
        QString val = m.captured(1).trimmed();
        if (!val.isEmpty()) v = val;
    }
    if (v.isEmpty()) return fallback;
    if (v.startsWith('#')) {
        v = v.mid(1);
        if (v.length() == 3)
            v = QString(v[0]) + v[0] + v[1] + v[1] + v[2] + v[2];
        return v.toUpper();
    }
    return fallback;
}

QString HtmlToOoxml::buildStylesXml(const QString &themeCss)
{
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("w:styles");
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
        QStringLiteral("w"));

    // Normal
    w.writeStartElement("w:style");
    w.writeAttribute("w:type", "paragraph");
    w.writeAttribute("w:styleId", "Normal");
    w.writeStartElement("w:name");
    w.writeAttribute("w:val", "Normal");
    w.writeEndElement();
    auto rpr = [&]() {
        w.writeStartElement("w:rPr");
        w.writeStartElement("w:sz");
        w.writeAttribute("w:val", "22");
        w.writeEndElement();
        w.writeStartElement("w:szCs");
        w.writeAttribute("w:val", "22");
        w.writeEndElement();
        w.writeStartElement("w:rFonts");
        w.writeAttribute("w:ascii", "Calibri");
        w.writeAttribute("w:hAnsi", "Calibri");
        w.writeEndElement();
        w.writeEndElement();
    };
    rpr();
    w.writeEndElement();

    struct HeadingDef {
        QString tag;
        QString sid;
        int sz;
        QString fallbackColor;
    };
    HeadingDef hd[] = {
        {"h1", "Heading1", 48, "1F3864"},
        {"h2", "Heading2", 36, "2B579A"},
        {"h3", "Heading3", 28, "3B78B4"},
        {"h4", "Heading4", 24, "4A8BC2"},
        {"h5", "Heading5", 22, "5A9BD5"},
        {"h6", "Heading6", 20, "6BA5D8"},
    };
    for (auto &h : hd) {
        QString col = styleColor(themeCss, h.tag, h.fallbackColor);
        w.writeStartElement("w:style");
        w.writeAttribute("w:type", "paragraph");
        w.writeAttribute("w:styleId", h.sid);
        w.writeStartElement("w:name");
        w.writeAttribute("w:val", "heading " + QString(h.tag[1]));
        w.writeEndElement();
        w.writeStartElement("w:basedOn");
        w.writeAttribute("w:val", "Normal");
        w.writeEndElement();

        w.writeStartElement("w:pPr");
        w.writeStartElement("w:spacing");
        w.writeAttribute("w:before", "240");
        w.writeAttribute("w:after", "120");
        w.writeEndElement();
        w.writeEndElement();

        w.writeStartElement("w:rPr");
        w.writeStartElement("w:b");
        w.writeEndElement();
        w.writeStartElement("w:sz");
        w.writeAttribute("w:val", QString::number(h.sz));
        w.writeEndElement();
        w.writeStartElement("w:szCs");
        w.writeAttribute("w:val", QString::number(h.sz));
        w.writeEndElement();
        w.writeStartElement("w:color");
        w.writeAttribute("w:val", col);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
    }

    w.writeStartElement("w:style");
    w.writeAttribute("w:type", "paragraph");
    w.writeAttribute("w:styleId", "SourceCode");
    w.writeStartElement("w:name");
    w.writeAttribute("w:val", "Source Code");
    w.writeEndElement();
    w.writeStartElement("w:basedOn");
    w.writeAttribute("w:val", "Normal");
    w.writeEndElement();
    w.writeStartElement("w:pPr");
    w.writeStartElement("w:spacing");
    w.writeAttribute("w:before", "240");
    w.writeAttribute("w:after", "240");
    w.writeEndElement();
    w.writeStartElement("w:ind");
    w.writeAttribute("w:left", "480");
    w.writeAttribute("w:right", "480");
    w.writeEndElement();
    w.writeStartElement("w:shd");
    w.writeAttribute("w:val", "clear");
    w.writeAttribute("w:fill", "F2F2F2");
    w.writeEndElement();
    w.writeEndElement();
    w.writeStartElement("w:rPr");
    w.writeStartElement("w:rFonts");
    w.writeAttribute("w:ascii", "Consolas");
    w.writeAttribute("w:hAnsi", "Consolas");
    w.writeEndElement();
    w.writeStartElement("w:sz");
    w.writeAttribute("w:val", "20");
    w.writeEndElement();
    w.writeStartElement("w:szCs");
    w.writeAttribute("w:val", "20");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

    w.writeStartElement("w:style");
    w.writeAttribute("w:type", "paragraph");
    w.writeAttribute("w:styleId", "Quote");
    w.writeStartElement("w:name");
    w.writeAttribute("w:val", "Quote");
    w.writeEndElement();
    w.writeStartElement("w:basedOn");
    w.writeAttribute("w:val", "Normal");
    w.writeEndElement();
    w.writeStartElement("w:pPr");
    w.writeStartElement("w:ind");
    w.writeAttribute("w:left", "720");
    w.writeAttribute("w:right", "360");
    w.writeEndElement();
    w.writeStartElement("w:spacing");
    w.writeAttribute("w:before", "120");
    w.writeAttribute("w:after", "120");
    w.writeEndElement();
    w.writeEndElement();
    w.writeStartElement("w:rPr");
    w.writeStartElement("w:i");
    w.writeEndElement();
    w.writeStartElement("w:color");
    w.writeAttribute("w:val", "555555");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

    // Per-type admonition styles
    struct AdmonitionDef {
        const char *type;
        const char *titleColor;
        const char *accentColor;
        const char *bgColor;
    };
    AdmonitionDef ad[] = {
        {"note",      "2B579A", "2B579A", "E8F4FD"},
        {"tip",       "1A7F37", "1A7F37", "DAFBE1"},
        {"important", "9A6700", "9A6700", "FFF8C5"},
        {"warning",   "CF222E", "CF222E", "FFEBE9"},
        {"caution",   "CF222E", "CF222E", "FFEBE9"},
    };
    for (auto &a : ad) {
        QString type = QString::fromLatin1(a.type);
        QString tCol = extractCssValue(themeCss,
            ".admonition." + type + " .admonition-title", "color", a.titleColor);
        QString aCol = extractCssValue(themeCss,
            ".admonition." + type, "border-left-color", a.accentColor);
        QString bg = extractCssValue(themeCss,
            ".admonition." + type, "background-color", a.bgColor);

        // Title style
        w.writeStartElement("w:style");
        w.writeAttribute("w:type", "paragraph");
        w.writeAttribute("w:styleId", "AdmonitionTitle" + type);
        w.writeStartElement("w:name");
        w.writeAttribute("w:val", "Admonition " + type + " Title");
        w.writeEndElement();
        w.writeStartElement("w:basedOn");
        w.writeAttribute("w:val", "Normal");
        w.writeEndElement();
        w.writeStartElement("w:pPr");
        w.writeStartElement("w:spacing");
        w.writeAttribute("w:before", "120");
        w.writeAttribute("w:after", "60");
        w.writeEndElement();
        w.writeStartElement("w:shd");
        w.writeAttribute("w:val", "clear");
        w.writeAttribute("w:fill", bg);
        w.writeEndElement();
        w.writeStartElement("w:pBdr");
        w.writeStartElement("w:left");
        w.writeAttribute("w:val", "single");
        w.writeAttribute("w:sz", "24");
        w.writeAttribute("w:space", "8");
        w.writeAttribute("w:color", aCol);
        w.writeEndElement();
        w.writeStartElement("w:between");
        w.writeAttribute("w:val", "single");
        w.writeAttribute("w:sz", "24");
        w.writeAttribute("w:space", "8");
        w.writeAttribute("w:color", aCol);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
        w.writeStartElement("w:rPr");
        w.writeStartElement("w:b");
        w.writeEndElement();
        w.writeStartElement("w:color");
        w.writeAttribute("w:val", tCol);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();

        // Text/content style
        w.writeStartElement("w:style");
        w.writeAttribute("w:type", "paragraph");
        w.writeAttribute("w:styleId", "AdmonitionText" + type);
        w.writeStartElement("w:name");
        w.writeAttribute("w:val", "Admonition " + type + " Text");
        w.writeEndElement();
        w.writeStartElement("w:basedOn");
        w.writeAttribute("w:val", "Normal");
        w.writeEndElement();
        w.writeStartElement("w:pPr");
        w.writeStartElement("w:spacing");
        w.writeAttribute("w:before", "60");
        w.writeAttribute("w:after", "60");
        w.writeEndElement();
        w.writeStartElement("w:shd");
        w.writeAttribute("w:val", "clear");
        w.writeAttribute("w:fill", bg);
        w.writeEndElement();
        w.writeStartElement("w:pBdr");
        w.writeStartElement("w:left");
        w.writeAttribute("w:val", "single");
        w.writeAttribute("w:sz", "24");
        w.writeAttribute("w:space", "8");
        w.writeAttribute("w:color", aCol);
        w.writeEndElement();
        w.writeStartElement("w:between");
        w.writeAttribute("w:val", "single");
        w.writeAttribute("w:sz", "24");
        w.writeAttribute("w:space", "8");
        w.writeAttribute("w:color", aCol);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndElement();
    }

    w.writeEndElement();
    w.writeEndDocument();
    return QString::fromUtf8(buf);
}

// ── numbering.xml builder ────────────────────────────────────────────────────

static void writeLvl(QXmlStreamWriter &w, int ilvl, const QString &fmt,
                     const QString &text)
{
    w.writeStartElement("w:lvl");
    w.writeAttribute("w:ilvl", QString::number(ilvl));
    w.writeStartElement("w:start");
    w.writeAttribute("w:val", "1");
    w.writeEndElement();
    w.writeStartElement("w:numFmt");
    w.writeAttribute("w:val", fmt);
    w.writeEndElement();
    w.writeStartElement("w:lvlText");
    w.writeAttribute("w:val", text);
    w.writeEndElement();
    w.writeStartElement("w:pPr");
    w.writeStartElement("w:ind");
    w.writeAttribute("w:left", QString::number(720 + ilvl * 720));
    w.writeAttribute("w:hanging", "360");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();
}

QString HtmlToOoxml::buildNumberingXml()
{
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("w:numbering");
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
        QStringLiteral("w"));

    w.writeStartElement("w:abstractNum");
    w.writeAttribute("w:abstractNumId", "0");
    w.writeStartElement("w:multiLevelType");
    w.writeAttribute("w:val", "hybridMultilevel");
    w.writeEndElement();
    writeLvl(w, 0, "bullet", "\u2022");
    writeLvl(w, 1, "bullet", "\u25CB");
    writeLvl(w, 2, "bullet", "\u25A0");
    w.writeEndElement();

    w.writeStartElement("w:abstractNum");
    w.writeAttribute("w:abstractNumId", "1");
    w.writeStartElement("w:multiLevelType");
    w.writeAttribute("w:val", "hybridMultilevel");
    w.writeEndElement();
    writeLvl(w, 0, "decimal", "%1.");
    writeLvl(w, 1, "decimal", "%2.");
    writeLvl(w, 2, "decimal", "%3.");
    w.writeEndElement();

    w.writeStartElement("w:num");
    w.writeAttribute("w:numId", "1");
    w.writeStartElement("w:abstractNumId");
    w.writeAttribute("w:val", "0");
    w.writeEndElement();
    w.writeEndElement();

    w.writeStartElement("w:num");
    w.writeAttribute("w:numId", "2");
    w.writeStartElement("w:abstractNumId");
    w.writeAttribute("w:val", "1");
    w.writeEndElement();
    w.writeEndElement();

    w.writeEndElement();
    w.writeEndDocument();
    return QString::fromUtf8(buf);
}

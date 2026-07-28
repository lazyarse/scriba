#include "HtmlToOoxml.h"
#include "MathmlToOmml.h"
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
};

static QXmlStreamWriter *bodyWriter = nullptr;
static QVector<OoxmlImage> *g_images = nullptr;
static int g_imageCounter = 0;
static QVector<OoxmlHyperlink> *g_hyperlinks = nullptr;
static int g_hyperlinkCounter = 0;

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
        if (MathmlToOmml::convert(mathml, *bodyWriter))
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

static void writeRunWithBreaks(QXmlStreamWriter &w, const QString &text, const FormatState &fs)
{
    if (text.isEmpty()) return;

    auto writeRpr = [&]() {
        if (fs.bold || fs.italic || fs.code) {
            w.writeStartElement("w:rPr");
            if (fs.bold) w.writeEmptyElement("w:b");
            if (fs.italic) w.writeEmptyElement("w:i");
            if (fs.code) {
                w.writeStartElement("w:rFonts");
                w.writeAttribute("w:ascii", "Courier New");
                w.writeAttribute("w:hAnsi", "Courier New");
                w.writeEndElement();
                w.writeStartElement("w:sz");
                w.writeAttribute("w:val", "18");
                w.writeEndElement();
            }
            w.writeEndElement();
        }
    };

    // Split on newlines so OOXML preserves line breaks in code blocks
    QStringList parts = text.split(QLatin1Char('\n'));
    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            // Emit a line break between segments
            writeBreak(w);
        }
        if (parts[i].isEmpty()) continue;
        w.writeStartElement("w:r");
        writeRpr();
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

static std::tuple<QByteArray, int, int> rasterizeSvg(const QString &svgXml)
{
    if (svgXml.isEmpty())
        return {QByteArray(), 0, 0};

    QSvgRenderer renderer(svgXml.toUtf8());
    if (!renderer.isValid())
        return {QByteArray(), 0, 0};

    QRectF vb = renderer.viewBox();
    if (vb.isEmpty())
        vb = QRectF(QPointF(0, 0), renderer.defaultSize());
    if (vb.isEmpty() || vb.width() <= 0 || vb.height() <= 0)
        return {QByteArray(), 0, 0};

    // Rasterize at 150 DPI for good print quality (SVG default is ~72-96 DPI)
    double scale = 150.0 / 72.0;
    int w = qMax(1, static_cast<int>(vb.width() * scale));
    int h = qMax(1, static_cast<int>(vb.height() * scale));

    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    painter.end();

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    // EMUs: 1 inch = 914400 EMUs. Cap width to 5.5 inches for printable fit.
    static constexpr int kMaxEmu = 5029200; // 5.5 inches
    double inchesW = vb.width() / 72.0;
    double inchesH = vb.height() / 72.0;
    int cxEmu = qMax(1, static_cast<int>(inchesW * 914400.0));
    int cyEmu = qMax(1, static_cast<int>(inchesH * 914400.0));
    if (cxEmu > kMaxEmu) {
        cyEmu = static_cast<int>(static_cast<qint64>(cyEmu) * kMaxEmu / cxEmu);
        cxEmu = kMaxEmu;
    }

    return {pngData, cxEmu, cyEmu};
}

static QString registerImage(const QByteArray &pngData, int cxEmu, int cyEmu)
{
    if (!g_images || pngData.isEmpty())
        return {};
    int id = ++g_imageCounter;
    QString relId = QStringLiteral("rIdImg%1").arg(id);
    QString fileName = QStringLiteral("media/image%1.png").arg(id);
    g_images->push_back({relId, fileName, pngData, cxEmu, cyEmu});
    return relId;
}

static void writeDrawingRun(QXmlStreamWriter &w, const QString &relId,
                            int cxEmu, int cyEmu, int docPrId = 1)
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
    w.writeEndElement();
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

    auto [pngData, cxEmu, cyEmu] = rasterizeSvg(svgXml);
    if (pngData.isEmpty()) return;

    QString relId = registerImage(pngData, cxEmu, cyEmu);
    if (relId.isEmpty()) return;

    bodyWriter->writeStartElement("w:p");
    writeDrawingRun(*bodyWriter, relId, cxEmu, cyEmu, g_imageCounter);
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

    auto [pngData, cxEmu, cyEmu] = rasterizeSvg(svgXml);
    if (pngData.isEmpty()) return;

    QString relId = registerImage(pngData, cxEmu, cyEmu);
    if (relId.isEmpty()) return;

    writeDrawingRun(*bodyWriter, relId, cxEmu, cyEmu, g_imageCounter);
}

static void handleImgTag(const HtmlToken &tok)
{
    QString src = tok.attrs.value(QStringLiteral("src"));
    if (!src.startsWith(QStringLiteral("data:")))
        return;

    // Parse "data:<mime>;base64,<data>"
    int b64Pos = src.indexOf(QStringLiteral(";base64,"));
    if (b64Pos < 0) return;
    QByteArray imgData = QByteArray::fromBase64(src.mid(b64Pos + 8).toLatin1());
    if (imgData.isEmpty()) return;

    QImage image;
    if (!image.loadFromData(imgData))
        return;

    int imgW = image.width();
    int imgH = image.height();
    if (imgW <= 0 || imgH <= 0) return;

    // Crop transparent margins (display math often has large transparent padding
    // from KaTeX block elements stretching to the full container width)
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

    // Apply max-width / max-height from style attribute (set by #WxH markdown suffix)
    static const QRegularExpression maxRe(
        QStringLiteral("max-(?:width|height)\\s*:\\s*(\\d+)px"));
    int maxW = -1, maxH = -1;
    QString style = tok.attrs.value(QStringLiteral("style"));
    auto styleIt = maxRe.globalMatch(style);
    while (styleIt.hasNext()) {
        auto m = styleIt.next();
        if (m.captured(0).contains(QStringLiteral("width")))
            maxW = m.captured(1).toInt();
        else
            maxH = m.captured(1).toInt();
    }
    if (maxW > 0 && imgW > maxW) {
        imgH = static_cast<int>(static_cast<qint64>(imgH) * maxW / imgW);
        imgW = maxW;
    }
    if (maxH > 0 && imgH > maxH) {
        imgW = static_cast<int>(static_cast<qint64>(imgW) * maxH / imgH);
        imgH = maxH;
    }

    // Convert to PNG for DOCX registration
    QByteArray pngData;
    QBuffer buf(&pngData);
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, "PNG");

    // EMUs: 1 inch = 914400 EMUs. Cap width to 5.5 inches.
    // Canvas captured at 2× CSS pixels (HiDPI in convertKatexToImages).
    // CSS pixels are at 96 DPI → 192 canvas-pixels per inch.
    static constexpr int kMaxEmu = 5029200;
    static constexpr double kPxToEmu = 914400.0 / 192.0;
    int cxEmu = qMax(1, static_cast<int>(imgW * kPxToEmu));
    int cyEmu = qMax(1, static_cast<int>(imgH * kPxToEmu));
    if (cxEmu > kMaxEmu) {
        cyEmu = static_cast<int>(static_cast<qint64>(cyEmu) * kMaxEmu / cxEmu);
        cxEmu = kMaxEmu;
    }

    QString relId = registerImage(pngData, cxEmu, cyEmu);
    if (relId.isEmpty()) return;

    writeDrawingRun(*bodyWriter, relId, cxEmu, cyEmu, g_imageCounter);
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
                FormatState s = state;
                s.bold = true;
                processInlineChildren(parser, n, s);
            } else if (n == "em" || n == "i") {
                FormatState s = state;
                s.italic = true;
                processInlineChildren(parser, n, s);
            } else if (n == "code") {
                FormatState s = state;
                s.code = true;
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
            } else if (isVoidElement(n)) {
                // Void elements (input, hr, col, etc.) have no closing tag — skip
            } else {
                processInlineChildren(parser, n, state);
            }
        } else if (tok.type == HtmlToken::Text) {
            writeRun(*bodyWriter, tok.text, state);
        }
    }
}

// ── block-level handlers ────────────────────────────────────────────────────

static void writeParaStart(const QString &styleId)
{
    bodyWriter->writeStartElement("w:p");
    if (!styleId.isEmpty()) {
        bodyWriter->writeStartElement("w:pPr");
        bodyWriter->writeStartElement("w:pStyle");
        bodyWriter->writeAttribute("w:val", styleId);
        bodyWriter->writeEndElement();
        bodyWriter->writeEndElement();
    }
}

static void writeParaEnd()
{
    bodyWriter->writeEndElement();
}

static void handleParagraph(const QString &styleId,
                            SimpleHtmlParser &parser, const QString &endTag,
                            FormatState state)
{
    writeParaStart(styleId);
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

static void handleDiv(SimpleHtmlParser &parser, const QString &endTag)
{
    processBlockChildren(parser, endTag, FormatState{});
}

// ── table handler ────────────────────────────────────────────────────────────

static void handleTable(SimpleHtmlParser &parser)
{
    bodyWriter->writeStartElement("w:tbl");

    bodyWriter->writeStartElement("w:tblPr");
    bodyWriter->writeStartElement("w:tblW");
    bodyWriter->writeAttribute("w:w", "9072");
    bodyWriter->writeAttribute("w:type", "dxa");
    bodyWriter->writeEndElement();
    bodyWriter->writeEndElement();

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
            bodyWriter->writeEndElement();
            handleParagraph(QString(), parser, QStringLiteral("th"), FormatState{});
            bodyWriter->writeEndElement();
        } else if (n == "td") {
            bodyWriter->writeStartElement("w:tc");
            handleParagraph(QString(), parser, QStringLiteral("td"), FormatState{});
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
            handleParagraph("Heading" + tag.mid(1), parser, tag, state);
        } else if (tag == "p") {
            // Check if this is an admonition title paragraph
            QString pClass = tok.attrs.value(QStringLiteral("class"));
            QString styleId = "Normal";
            if (pClass.contains("admonition-title")) {
                styleId = "AdmonitionTitle";
            }
            handleParagraph(styleId, parser, tag, state);
        } else if (tag == "pre") {
            handlePre(parser);
        } else if (tag == "blockquote") {
            handleParagraph("Quote", parser, tag, state);
        } else if (tag == "ul") {
            handleList(parser, false, 0);
        } else if (tag == "ol") {
            handleList(parser, true, 0);
        } else if (tag == "table") {
            handleTable(parser);
        } else if (tag == "div") {
            handleDiv(parser, tag);
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

    QString raw = QString::fromUtf8(buf);
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
    auto m = re.match(themeCss);
    if (!m.hasMatch()) return fallback;

    QString c = m.captured(1).trimmed();
    if (c.startsWith('#')) {
        c = c.mid(1);
        if (c.length() == 3) {
            c = QString(c[0]) + c[0] + c[1] + c[1] + c[2] + c[2];
        }
        return c.toUpper();
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
    w.writeTextElement("w:name", "Normal");
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
        w.writeTextElement("w:basedOn", "Normal");

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
    w.writeTextElement("w:name", "Source Code");
    w.writeTextElement("w:basedOn", "Normal");
    w.writeStartElement("w:pPr");
    w.writeStartElement("w:spacing");
    w.writeAttribute("w:before", "120");
    w.writeAttribute("w:after", "120");
    w.writeEndElement();
    w.writeStartElement("w:ind");
    w.writeAttribute("w:left", "240");
    w.writeAttribute("w:right", "240");
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
    w.writeTextElement("w:name", "Quote");
    w.writeTextElement("w:basedOn", "Normal");
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

    // AdmonitionTitle style: bold text with light blue background
    w.writeStartElement("w:style");
    w.writeAttribute("w:type", "paragraph");
    w.writeAttribute("w:styleId", "AdmonitionTitle");
    w.writeTextElement("w:name", "Admonition Title");
    w.writeTextElement("w:basedOn", "Normal");
    w.writeStartElement("w:pPr");
    w.writeStartElement("w:spacing");
    w.writeAttribute("w:before", "120");
    w.writeAttribute("w:after", "60");
    w.writeEndElement();
    w.writeStartElement("w:shd");
    w.writeAttribute("w:val", "clear");
    w.writeAttribute("w:fill", "E8F4FD");
    w.writeEndElement();
    w.writeStartElement("w:pBdr");
    w.writeStartElement("w:left");
    w.writeAttribute("w:val", "single");
    w.writeAttribute("w:sz", "24");
    w.writeAttribute("w:space", "8");
    w.writeAttribute("w:color", "2B579A");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();
    w.writeStartElement("w:rPr");
    w.writeStartElement("w:b");
    w.writeEndElement();
    w.writeStartElement("w:color");
    w.writeAttribute("w:val", "2B579A");
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndElement();

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

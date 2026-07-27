#include "HtmlToOoxml.h"
#include <QLoggingCategory>
#include <QMap>
#include <QRegularExpression>
#include <QXmlStreamWriter>

Q_LOGGING_CATEGORY(lcH2O, "scriba.html2ooxml")

struct FormatState {
    bool bold = false;
    bool italic = false;
    bool code = false;
};

static QXmlStreamWriter *bodyWriter = nullptr;

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
            int close = html_.indexOf('>', pos_);
            if (close == -1) { t.type = HtmlToken::Done; return t; }
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
};

// ── helpers ──────────────────────────────────────────────────────────────────

static void writeRun(QXmlStreamWriter &w, const QString &text, const FormatState &fs)
{
    if (text.isEmpty()) return;
    w.writeStartElement("w:r");
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
    w.writeStartElement("w:t");
    if (!text.isEmpty() && (text[0] == ' ' || text.back() == ' '))
        w.writeAttribute("xml:space", "preserve");
    w.writeCharacters(text);
    w.writeEndElement();
    w.writeEndElement();
}

static void writeBreak(QXmlStreamWriter &w)
{
    w.writeStartElement("w:r");
    w.writeEmptyElement("w:br");
    w.writeEndElement();
}

// ── forward declarations ────────────────────────────────────────────────────

static void processBlockChildren(SimpleHtmlParser &parser, const QString &endTag,
                                 FormatState state);

// ── inline-content processor ─────────────────────────────────────────────────

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
                processInlineChildren(parser, QStringLiteral("a"), state);
            } else if (n == "br") {
                writeBreak(*bodyWriter);
            } else if (n == "img") {
                // skip images for now
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
            handleParagraph("Normal", parser, tag, state);
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
        } else {
            processBlockChildren(parser, tag, state);
        }
    }
}

// ── public API ──────────────────────────────────────────────────────────────

QString HtmlToOoxml::convert(const QString &html, const QString &themeCss)
{
    QByteArray buf;
    QXmlStreamWriter w(&buf);
    bodyWriter = &w;

    w.writeStartElement("x");
    w.writeNamespace(
        QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
        QStringLiteral("w"));

    SimpleHtmlParser parser(html);
    FormatState state;
    processBlockChildren(parser, QString(), state);

    w.writeEndElement(); // x

    QString result = QString::fromUtf8(buf);
    int start = result.indexOf('>');
    int end = result.lastIndexOf('<');
    if (start >= 0 && end > start)
        result = result.mid(start + 1, end - start - 1);
    else
        result.clear();

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
    w.writeStartElement("w:rPr");
    w.writeStartElement("w:rFonts");
    w.writeAttribute("w:ascii", "Courier New");
    w.writeAttribute("w:hAnsi", "Courier New");
    w.writeEndElement();
    w.writeStartElement("w:sz");
    w.writeAttribute("w:val", "18");
    w.writeEndElement();
    w.writeStartElement("w:szCs");
    w.writeAttribute("w:val", "18");
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

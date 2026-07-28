#include "MathmlToOmml.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QMap>

namespace {

struct MmlNode {
    QString tag;
    QMap<QString, QString> attrs;
    QString text;
    QList<MmlNode> children;
};

static MmlNode parseMmlElement(QXmlStreamReader &r)
{
    MmlNode node;
    node.tag = r.name().toString();
    for (const auto &a : r.attributes())
        node.attrs[a.name().toString()] = a.value().toString();

    r.readNext();
    while (!r.atEnd()) {
        if (r.isCharacters()) {
            if (!r.isWhitespace() || !node.text.isEmpty())
                node.text += r.text().toString();
            r.readNext();
        } else if (r.isStartElement()) {
            node.children.append(parseMmlElement(r));
        } else if (r.isEndElement()) {
            r.readNext();
            return node;
        } else {
            r.readNext();
        }
    }
    return node;
}

static QString defaultStyleForTag(const QString &tag)
{
    if (tag == QStringLiteral("mi")) return QStringLiteral("i");
    return QStringLiteral("p");
}

static QString mathvariantToStyle(const QString &variant)
{
    if (variant == QStringLiteral("normal")) return QStringLiteral("p");
    if (variant == QStringLiteral("italic")) return QStringLiteral("i");
    if (variant == QStringLiteral("bold")) return QStringLiteral("b");
    if (variant == QStringLiteral("bold-italic")) return QStringLiteral("bi");
    if (variant == QStringLiteral("double-struck")) return QStringLiteral("d");
    if (variant == QStringLiteral("fraktur")) return QStringLiteral("f");
    if (variant == QStringLiteral("script")) return QStringLiteral("s");
    return {};
}

static void emitTextRun(const MmlNode &node, QXmlStreamWriter &w)
{
    w.writeStartElement(QStringLiteral("m:r"));

    QString style = defaultStyleForTag(node.tag);
    if (node.attrs.contains(QStringLiteral("mathvariant"))) {
        QString s = mathvariantToStyle(node.attrs[QStringLiteral("mathvariant")]);
        if (!s.isEmpty()) style = s;
    }
    if (style != QStringLiteral("p")) {
        w.writeStartElement(QStringLiteral("m:rPr"));
        w.writeTextElement(QStringLiteral("m:sty"), style);
        w.writeEndElement();
    }

    w.writeStartElement(QStringLiteral("m:t"));
    w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
    w.writeCharacters(node.text);
    w.writeEndElement();
    w.writeEndElement();
}

static void emitOmml(const MmlNode &node, QXmlStreamWriter &w)
{
    const QString &t = node.tag;

    if (t == QStringLiteral("math") || t == QStringLiteral("semantics")) {
        for (const auto &c : node.children) emitOmml(c, w);
        return;
    }
    if (t == QStringLiteral("annotation") || t == QStringLiteral("annotation-xml"))
        return;

    if (t == QStringLiteral("mi") || t == QStringLiteral("mn")
        || t == QStringLiteral("mo") || t == QStringLiteral("mtext")
        || t == QStringLiteral("ms")) {
        emitTextRun(node, w);
        return;
    }
    if (t == QStringLiteral("mspace")) return;

    if (t == QStringLiteral("mrow")) {
        for (const auto &c : node.children) emitOmml(c, w);
        return;
    }

    if (t == QStringLiteral("msup")) {
        w.writeStartElement(QStringLiteral("m:sSup"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:sup")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("msub")) {
        w.writeStartElement(QStringLiteral("m:sSub"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:sub")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("msubsup")) {
        w.writeStartElement(QStringLiteral("m:sSubSup"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:sub")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        if (node.children.size() >= 3) {
            w.writeStartElement(QStringLiteral("m:sup")); emitOmml(node.children[2], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mfrac")) {
        w.writeStartElement(QStringLiteral("m:f"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:num")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:den")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("msqrt")) {
        w.writeStartElement(QStringLiteral("m:rad"));
        w.writeStartElement(QStringLiteral("m:deg")); w.writeEndElement();
        w.writeStartElement(QStringLiteral("m:e"));
        for (const auto &c : node.children) emitOmml(c, w);
        w.writeEndElement();
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mroot")) {
        w.writeStartElement(QStringLiteral("m:rad"));
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:deg")); emitOmml(node.children[1], w); w.writeEndElement();
        } else {
            w.writeStartElement(QStringLiteral("m:deg")); w.writeEndElement();
        }
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mover")) {
        w.writeStartElement(QStringLiteral("m:limUpp"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:lim")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("munder")) {
        w.writeStartElement(QStringLiteral("m:limLow"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
        }
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:lim")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("munderover")) {
        w.writeStartElement(QStringLiteral("m:limLow"));
        w.writeStartElement(QStringLiteral("m:e"));
        if (node.children.size() >= 1) {
            w.writeStartElement(QStringLiteral("m:limUpp"));
            w.writeStartElement(QStringLiteral("m:e")); emitOmml(node.children[0], w); w.writeEndElement();
            if (node.children.size() >= 3) {
                w.writeStartElement(QStringLiteral("m:lim")); emitOmml(node.children[2], w); w.writeEndElement();
            }
            w.writeEndElement();
        }
        w.writeEndElement();
        if (node.children.size() >= 2) {
            w.writeStartElement(QStringLiteral("m:lim")); emitOmml(node.children[1], w); w.writeEndElement();
        }
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mtable")) {
        w.writeStartElement(QStringLiteral("m:m"));
        for (const auto &c : node.children) emitOmml(c, w);
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mtr")) {
        w.writeStartElement(QStringLiteral("m:mr"));
        for (const auto &c : node.children) emitOmml(c, w);
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mtd")) {
        w.writeStartElement(QStringLiteral("m:mc"));
        for (const auto &c : node.children) emitOmml(c, w);
        w.writeEndElement();
        return;
    }

    if (t == QStringLiteral("mstyle") || t == QStringLiteral("mpadded")
        || t == QStringLiteral("merror") || t == QStringLiteral("menclose")) {
        for (const auto &c : node.children) emitOmml(c, w);
        return;
    }

    for (const auto &c : node.children) emitOmml(c, w);
}

} // anonymous namespace

bool MathmlToOmml::convert(const QString &mathmlXml, QXmlStreamWriter &w)
{
    if (mathmlXml.isEmpty()) return false;

    QXmlStreamReader r(mathmlXml);
    if (!r.readNextStartElement()) return false;
    if (r.atEnd()) return false;

    MmlNode root = parseMmlElement(r);

    w.writeStartElement(QStringLiteral("m:oMath"));
    emitOmml(root, w);
    w.writeEndElement();

    return true;
}

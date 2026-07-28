#include "CssValueParser.h"
#include "UnitConverter.h"
#include <QColor>
#include <QRegularExpression>

namespace CssValueParser {

static const QRegularExpression rgbRe(R"(rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\))",
                                      QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression hslRe(R"(hsl\s*\(\s*(\d+)\s*,\s*([\d.]+)%\s*,\s*([\d.]+)%\s*\))",
                                      QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression hexRe(R"(#([0-9A-Fa-f]{6}))");
static const QRegularExpression hex3Re(R"(#([0-9A-Fa-f])([0-9A-Fa-f])([0-9A-Fa-f]))");

static double hslHueToRgb(double p, double q, double t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0/6) return p + (q - p) * 6 * t;
    if (t < 1.0/2) return q;
    if (t < 2.0/3) return p + (q - p) * (2.0/3 - t) * 6;
    return p;
}

QString fixupColorCode(const QString &value)
{
    if (value.isEmpty())
        return QStringLiteral("000000");

    QString v = value.trimmed();

    // Named color via QColor (handles "red", "blue", etc.)
    {
        QColor named(v);
        if (named.isValid() && v.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) != 0
            && v.compare(QStringLiteral("currentcolor"), Qt::CaseInsensitive) != 0
            && v.compare(QStringLiteral("inherit"), Qt::CaseInsensitive) != 0
            && v.compare(QStringLiteral("initial"), Qt::CaseInsensitive) != 0
            && v.compare(QStringLiteral("unset"), Qt::CaseInsensitive) != 0) {
            // QColor knows all SVG named colors and hex codes
            return named.name().mid(1).toUpper(); // strip #
        }
    }

    // rgb(r, g, b)
    {
        auto m = rgbRe.match(v);
        if (m.hasMatch()) {
            return QColor(m.captured(1).toInt(), m.captured(2).toInt(),
                          m.captured(3).toInt()).name().mid(1).toUpper();
        }
    }

    // hsl(h, s, l)
    {
        auto m = hslRe.match(v);
        if (m.hasMatch()) {
            double h = m.captured(1).toDouble() / 360.0;
            double s = m.captured(2).toDouble() / 100.0;
            double l = m.captured(3).toDouble() / 100.0;
            double r, g, b;
            if (s == 0) {
                r = g = b = l;
            } else {
                double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
                double p = 2 * l - q;
                r = hslHueToRgb(p, q, h + 1.0/3);
                g = hslHueToRgb(p, q, h);
                b = hslHueToRgb(p, q, h - 1.0/3);
            }
            return QColor::fromRgbF(r, g, b).name().mid(1).toUpper();
        }
    }

    // #RRGGBB
    {
        auto m = hexRe.match(v);
        if (m.hasMatch())
            return m.captured(1).toUpper();
    }

    // #RGB → RRGGBB
    {
        auto m = hex3Re.match(v);
        if (m.hasMatch()) {
            QString r = m.captured(1), g = m.captured(2), b = m.captured(3);
            return (r + r + g + g + b + b).toUpper();
        }
    }

    return QStringLiteral("000000");
}

QString extractCssProperty(const QString &styleAttr, const QString &propertyName)
{
    if (styleAttr.isEmpty())
        return {};
    // Split on ';' to get individual declarations
    QStringList decls = styleAttr.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &decl : decls) {
        int colon = decl.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        QString prop = decl.left(colon).trimmed().toLower();
        if (prop == propertyName.toLower())
            return decl.mid(colon + 1).trimmed();
    }
    return {};
}

TextDecoration parseTextDecoration(const QString &styleAttr)
{
    TextDecoration result;

    // First try the shorthand "text-decoration"
    QString shorthand = extractCssProperty(styleAttr, QStringLiteral("text-decoration"));
    if (!shorthand.isEmpty() && shorthand.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0) {
        QStringList tokens = shorthand.split(' ', Qt::SkipEmptyParts);
        for (const QString &token : tokens) {
            QString t = token.toLower();
            // Is it a line value?
            if (t == QStringLiteral("underline")) {
                result.underline = true;
            } else if (t == QStringLiteral("line-through")) {
                result.lineThrough = true;
            } else if (t == QStringLiteral("overline") || t == QStringLiteral("blink")) {
                // Not supported in OOXML — skip
            } else if (t == QStringLiteral("solid") || t == QStringLiteral("dotted")
                       || t == QStringLiteral("dashed") || t == QStringLiteral("double")
                       || t == QStringLiteral("wavy")) {
                result.style = textDecorationStyleToOoxml(t);
            } else {
                // Try as color
                QString c = fixupColorCode(t);
                if (c != QStringLiteral("000000") || t == QStringLiteral("black"))
                    result.color = c;
            }
        }
        return result;
    }

    // Fall back to individual properties
    QString line = extractCssProperty(styleAttr, QStringLiteral("text-decoration-line"));
    if (line.isEmpty())
        return result;

    QStringList lineParts = line.split(' ', Qt::SkipEmptyParts);
    for (const QString &lp : lineParts) {
        QString mapped = textDecorationLineToOoxml(lp.toLower());
        if (mapped == QStringLiteral("underline"))
            result.underline = true;
        else if (mapped == QStringLiteral("strike"))
            result.lineThrough = true;
    }

    QString style = extractCssProperty(styleAttr, QStringLiteral("text-decoration-style"));
    if (!style.isEmpty())
        result.style = textDecorationStyleToOoxml(style.toLower());

    QString color = extractCssProperty(styleAttr, QStringLiteral("text-decoration-color"));
    if (!color.isEmpty())
        result.color = fixupColorCode(color);

    return result;
}

QString parseFontFamily(const QString &styleAttr)
{
    QString ff = extractCssProperty(styleAttr, QStringLiteral("font-family"));
    if (ff.isEmpty()) return {};

    // Split on comma, take first, strip quotes
    QStringList families = ff.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (families.isEmpty()) return {};
    QString first = families.first().trimmed();
    if (first.isEmpty()) return {};
    // Strip surrounding quotes
    if ((first.startsWith(QLatin1Char('"')) && first.endsWith(QLatin1Char('"')))
        || (first.startsWith(QLatin1Char('\'')) && first.endsWith(QLatin1Char('\''))))
        first = first.mid(1, first.size() - 2);
    return first.trimmed();
}

BorderSpec parseCssBorder(const QString &value)
{
    BorderSpec result;
    if (value.isEmpty() || value == QStringLiteral("none")
        || value == QStringLiteral("0")) {
        result.stroke = QStringLiteral("nil");
        result.size = 0;
        return result;
    }

    QStringList tokens = value.split(' ', Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        QString t = token.trimmed();
        // Check if it's a border-style keyword
        QString lower = t.toLower();
        if (lower == QStringLiteral("solid") || lower == QStringLiteral("dashed")
            || lower == QStringLiteral("dotted") || lower == QStringLiteral("double")
            || lower == QStringLiteral("inset") || lower == QStringLiteral("outset")
            || lower == QStringLiteral("groove") || lower == QStringLiteral("ridge")
            || lower == QStringLiteral("hidden") || lower == QStringLiteral("none")
            || lower == QStringLiteral("windowtext")) {
            result.stroke = borderStyleToOoxml(lower);
            continue;
        }
        // Check if it has dimension units (px, pt, em, cm, in)
        if (t.contains(QLatin1Char('p')) || t.contains(QLatin1Char('P'))
            || t.contains(QLatin1Char('c')) || t.contains(QLatin1Char('C'))
            || t.contains(QLatin1Char('i')) || t.contains(QLatin1Char('I'))) {
            int eip = UnitConverter::cssToEip(t);
            if (eip >= 0) {
                result.size = eip;
                continue;
            }
        }
        // Check if it's an em value (special handling: treat as pt via default font size)
        if (lower.endsWith(QLatin1Char('m')) && !lower.endsWith(QStringLiteral("cm"))
            && !lower.endsWith(QStringLiteral("mm"))) {
            // em → assume 11pt base → 1em = 11pt → 11 * 8 = 88 EIP
            bool ok = false;
            double emVal = t.left(t.size() - 2).toDouble(&ok);
            if (ok && emVal > 0) {
                result.size = qRound(emVal * 88.0);
                continue;
            }
        }
        // Otherwise treat as color
        QString c = fixupColorCode(t);
        if (c != QStringLiteral("000000") || lower == QStringLiteral("black")
            || t.startsWith(QLatin1Char('#')) || t.startsWith(QStringLiteral("rgb"))
            || t.startsWith(QStringLiteral("hsl"))) {
            result.color = c;
        }
    }
    return result;
}

Margins parseMargins(const QString &styleAttr)
{
    Margins result;

    // Try "margin" shorthand
    QString margin = extractCssProperty(styleAttr, QStringLiteral("margin"));
    if (!margin.isEmpty()) {
        QStringList parts = margin.split(' ', Qt::SkipEmptyParts);
        int values[4] = {-1, -1, -1, -1};
        for (int i = 0; i < parts.size() && i < 4; ++i)
            values[i] = UnitConverter::cssToTwip(parts[i]);

        switch (parts.size()) {
        case 1: // all sides
            result.top = result.right = result.bottom = result.left = values[0];
            break;
        case 2: // top/bottom, left/right
            result.top = result.bottom = values[0];
            result.left = result.right = values[1];
            break;
        case 3: // top, left/right, bottom
            result.top = values[0];
            result.left = result.right = values[1];
            result.bottom = values[2];
            break;
        case 4: // top, right, bottom, left
            result.top = values[0];
            result.right = values[1];
            result.bottom = values[2];
            result.left = values[3];
            break;
        default:
            break;
        }
        return result;
    }

    // Individual margin properties
    QString mt = extractCssProperty(styleAttr, QStringLiteral("margin-top"));
    if (!mt.isEmpty()) result.top = UnitConverter::cssToTwip(mt);

    QString mr = extractCssProperty(styleAttr, QStringLiteral("margin-right"));
    if (!mr.isEmpty()) result.right = UnitConverter::cssToTwip(mr);

    QString mb = extractCssProperty(styleAttr, QStringLiteral("margin-bottom"));
    if (!mb.isEmpty()) result.bottom = UnitConverter::cssToTwip(mb);

    QString ml = extractCssProperty(styleAttr, QStringLiteral("margin-left"));
    if (!ml.isEmpty()) result.left = UnitConverter::cssToTwip(ml);

    return result;
}

QString borderStyleToOoxml(const QString &cssStyle)
{
    QString lower = cssStyle.toLower();
    if (lower == QStringLiteral("solid"))    return QStringLiteral("single");
    if (lower == QStringLiteral("dashed"))   return QStringLiteral("dashed");
    if (lower == QStringLiteral("dotted"))   return QStringLiteral("dotted");
    if (lower == QStringLiteral("double"))   return QStringLiteral("double");
    if (lower == QStringLiteral("inset"))    return QStringLiteral("inset");
    if (lower == QStringLiteral("outset"))   return QStringLiteral("outset");
    if (lower == QStringLiteral("hidden"))   return QStringLiteral("nil");
    if (lower == QStringLiteral("none"))     return QStringLiteral("nil");
    if (lower == QStringLiteral("groove"))   return QStringLiteral("single");
    if (lower == QStringLiteral("ridge"))    return QStringLiteral("single");
    if (lower == QStringLiteral("windowtext")) return QStringLiteral("single");
    return QStringLiteral("single");
}

QString textDecorationStyleToOoxml(const QString &cssStyle)
{
    QString lower = cssStyle.toLower();
    if (lower == QStringLiteral("solid"))    return QStringLiteral("single");
    if (lower == QStringLiteral("dotted"))   return QStringLiteral("dotted");
    if (lower == QStringLiteral("dashed"))   return QStringLiteral("dash");
    if (lower == QStringLiteral("double"))   return QStringLiteral("double");
    if (lower == QStringLiteral("wavy"))     return QStringLiteral("wave");
    return QStringLiteral("single");
}

QString textDecorationLineToOoxml(const QString &cssLine)
{
    QString lower = cssLine.toLower();
    if (lower == QStringLiteral("underline"))    return QStringLiteral("underline");
    if (lower == QStringLiteral("line-through")) return QStringLiteral("strike");
    if (lower == QStringLiteral("overline"))     return {}; // not supported
    if (lower == QStringLiteral("blink"))        return {}; // not supported
    return {};
}

} // namespace CssValueParser

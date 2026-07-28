#pragma once

#include <QString>
#include <QMap>

struct TextDecoration {
    bool underline = false;
    bool lineThrough = false;
    QString style = "single"; // OOXML style name
    QString color; // RRGGBB, empty = use default
};

struct BorderSpec {
    int size = 0;       // eighths of a point
    QString stroke = "nil"; // OOXML border style
    QString color = "000000";
};

struct Margins {
    int top = -1;    // TWIP, -1 = not set
    int right = -1;
    int bottom = -1;
    int left = -1;
};

namespace CssValueParser {

// Parse CSS color value → RRGGBB hex. Handles: #RGB, #RRGGBB,
// rgb(r,g,b), hsl(h,s,l), named colors.
QString fixupColorCode(const QString &value);

// Parse text-decoration shorthand → TextDecoration.
// Also handles individual text-decoration-line/style/color properties.
TextDecoration parseTextDecoration(const QString &styleAttr);

// Parse font-family → primary font name (first in list, quotes stripped).
QString parseFontFamily(const QString &styleAttr);

// Parse a border shorthand → BorderSpec.
BorderSpec parseCssBorder(const QString &value);

// Parse margin:T R B L (1-4 values) from style attribute → Margins.
// Returns Margins with -1 for unset sides.
Margins parseMargins(const QString &styleAttr);

// Parse a single CSS property from a full style attribute string.
// Returns the value of the first matching declaration, or empty string.
QString extractCssProperty(const QString &styleAttr, const QString &propertyName);

// Border style mapping: CSS → OOXML
QString borderStyleToOoxml(const QString &cssStyle);

// Text decoration style mapping: CSS → OOXML
QString textDecorationStyleToOoxml(const QString &cssStyle);

// Text decoration line mapping: CSS → OOXML (returns empty for unsupported)
QString textDecorationLineToOoxml(const QString &cssLine);

} // namespace CssValueParser

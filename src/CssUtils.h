#pragma once

#include <QString>
#include <QColor>

namespace CssUtils {

struct ThemeColors { QColor background; QColor text; };

QString deriveChromeCss(const QString &themeCss);
QColor chromeTextColor(const QString &themeCss);
ThemeColors themeColors(const QString &themeCss);
bool isDarkTheme(const QString &themeCss);
QString splitViewMaxWidthCss(int maxWidth);

} // namespace CssUtils


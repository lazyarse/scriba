#pragma once

#include <QString>
#include <QColor>

namespace CssUtils {

inline constexpr int kUiFontSizePt = 10;

struct ThemeColors { QColor background; QColor text; };

QString deriveChromeCss(const QString &themeCss);
QString scribaConfigDir();
QColor chromeTextColor(const QString &themeCss);
ThemeColors themeColors(const QString &themeCss);
bool isDarkTheme(const QString &themeCss);
QString splitViewMaxWidthCss(int maxWidth);

} // namespace CssUtils


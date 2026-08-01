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


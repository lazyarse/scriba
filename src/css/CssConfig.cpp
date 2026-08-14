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
#include "CssConfig.h"
#include "prefs/Preferences.h"
#include <QFile>
#include <QSettings>

CssConfig::CssConfig()
{
    QSettings settings;
    m_stylesheets = settings.value(Preferences::CssFiles, QStringList()).toStringList();
    m_activeStylesheet = settings.value(Preferences::ActiveCssFile, "").toString();
    if (!m_activeStylesheet.isEmpty() && !isUsableTheme(m_activeStylesheet)) {
        m_activeStylesheet.clear();
        settings.setValue(Preferences::ActiveCssFile, QString());
    }
}

bool CssConfig::isUsableTheme(const QString &path) const
{
    if (path.startsWith(":/") || path.startsWith("qrc:"))
        return bundledThemes().contains(path);
    return QFile::exists(path);
}

QStringList CssConfig::bundledThemes()
{
    return {
        ":/themes/catppuccin-latte.css",
        ":/themes/catppuccin-mocha.css",
        ":/themes/dracula.css",
        ":/themes/github-dark.css",
        ":/themes/github-light.css",
        ":/themes/gruvbox-dark.css",
        ":/themes/gruvbox-light.css",
        ":/themes/nord.css",
        ":/themes/one-dark.css",
        ":/themes/rose-pine.css",
        ":/themes/rose-pine-dawn.css",
        ":/themes/solarized-dark.css",
        ":/themes/solarized-light.css",
        ":/themes/tokyo-night-dark.css",
        ":/themes/tokyo-night-light.css",
    };
}

QStringList CssConfig::stylesheets() const
{
    return m_stylesheets;
}

void CssConfig::setStylesheets(const QStringList &paths)
{
    m_stylesheets = paths;
    QSettings settings;
    settings.setValue(Preferences::CssFiles, paths);
}

QString CssConfig::activeStylesheet() const
{
    return m_activeStylesheet;
}

void CssConfig::setActiveStylesheet(const QString &path)
{
    m_activeStylesheet = path;
    QSettings settings;
    settings.setValue(Preferences::ActiveCssFile, path);
}

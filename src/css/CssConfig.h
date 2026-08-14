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
#include <QStringList>

class CssConfig
{
public:
    CssConfig();

    static QStringList bundledThemes();

    QStringList stylesheets() const;
    void setStylesheets(const QStringList &paths);

    QString activeStylesheet() const;
    void setActiveStylesheet(const QString &path);

private:
    bool isUsableTheme(const QString &path) const;

    QStringList m_stylesheets;
    QString m_activeStylesheet;
};


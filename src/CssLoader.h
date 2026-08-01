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

#include <QHash>
#include <QString>
#include <QStringList>

class CssConfig;

class CssLoader
{
public:
    explicit CssLoader(const CssConfig *config);

    QString themeCss();
    QString previewBaseCss();
    QString printBaseCss();
    QString printCss();

    void setPreviewBaseCss(const QString &css);
    void setPrintBaseCss(const QString &css);

    void invalidateCache();

    // Config-dir base-CSS copies that were superseded because they carried an
    // outdated version marker (saved by an older Scriba). Paths are removed by
    // clearStaleBaseCssFlags() after the user has been informed.
    QStringList staleBaseCssFiles() const { return m_staleFiles; }
    void clearStaleBaseCssFlags() { m_staleFiles.clear(); }

    QString themesDir() const;

private:
    QString configDir() const;
    QString loadCssFile(const QString &filePath) const;
    QString loadBaseCss(const QString &configFile, const QString &resourcePath);
    QString bundledHash(const QString &resourcePath);
    QString extractMarker(const QString &css) const;
    static QString stripMarker(QString css);
    static QString normalizeCss(const QString &css, const QString &hash);
    static void supersedeStaleFile(const QString &path);

    const CssConfig *m_config;
    QString m_themeCache;
    QString m_previewBaseCache;
    QString m_printBaseCache;
    QStringList m_staleFiles;
    QHash<QString, QString> m_hashCache;
    bool m_cacheDirty = true;
};


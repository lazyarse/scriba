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
#include "CssLoader.h"
#include "CssConfig.h"
#include "CssUtils.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

CssLoader::CssLoader(const CssConfig *config)
    : m_config(config)
{
}

QString CssLoader::configDir() const
{
    return CssUtils::scribaConfigDir();
}

QString CssLoader::themesDir() const
{
    return configDir() + "/themes";
}

QString CssLoader::previewBaseCss()
{
    if (!m_cacheDirty && !m_previewBaseCache.isNull())
        return m_previewBaseCache;
    m_previewBaseCache = loadBaseCss(configDir() + "/preview-base.css", ":/preview-base.css");
    return m_previewBaseCache;
}

void CssLoader::setPreviewBaseCss(const QString &css)
{
    QString normalized = normalizeCss(css, bundledHash(":/preview-base.css"));
    QDir().mkpath(configDir());
    QFile f(configDir() + "/preview-base.css");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(normalized.toUtf8());
    }
    m_previewBaseCache = normalized;
    invalidateCache();
}

QString CssLoader::printBaseCss()
{
    if (!m_cacheDirty && !m_printBaseCache.isNull())
        return m_printBaseCache;
    m_printBaseCache = loadBaseCss(configDir() + "/print-base.css", ":/print-base.css");
    return m_printBaseCache;
}

void CssLoader::setPrintBaseCss(const QString &css)
{
    QString normalized = normalizeCss(css, bundledHash(":/print-base.css"));
    QDir().mkpath(configDir());
    QFile f(configDir() + "/print-base.css");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(normalized.toUtf8());
    }
    m_printBaseCache = normalized;
    invalidateCache();
}

QString CssLoader::themeCss()
{
    if (m_cacheDirty) {
        QString active = m_config->activeStylesheet();
        if (!active.isEmpty())
            m_themeCache = loadCssFile(active);
        else
            m_themeCache.clear();
    }
    return m_themeCache;
}

void CssLoader::invalidateCache()
{
    m_cacheDirty = true;
    m_previewBaseCache.clear();
    m_printBaseCache.clear();
}

QString CssLoader::printCss()
{
    return printBaseCss();
}

QString CssLoader::loadCssFile(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(file.readAll());
    return {};
}

// Load a base stylesheet, honouring a config-dir copy only while it carries a
// version marker that matches the current bundled resource. Saved copies from
// older Scriba releases (whose base CSS changed) are moved to a .bak file and
// the bundled CSS is used instead.
QString CssLoader::loadBaseCss(const QString &configFile, const QString &resourcePath)
{
    QString bundled = loadCssFile(resourcePath);
    QString expected = bundledHash(resourcePath);

    QFile f(configFile);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString saved = QString::fromUtf8(f.readAll());
        if (extractMarker(saved) == expected)
            return stripMarker(saved);
        supersedeStaleFile(configFile);
        if (!m_staleFiles.contains(configFile))
            m_staleFiles << configFile;
    }
    return bundled;
}

QString CssLoader::bundledHash(const QString &resourcePath)
{
    if (!m_hashCache.contains(resourcePath))
        m_hashCache.insert(resourcePath, QString::fromLatin1(
            QCryptographicHash::hash(loadCssFile(resourcePath).toUtf8(),
                                     QCryptographicHash::Sha256).toHex()));
    return m_hashCache.value(resourcePath);
}

QString CssLoader::extractMarker(const QString &css) const
{
    QRegularExpression re(QStringLiteral("^\\s*/\\* scriba-base-css-version: ([0-9a-f]{64}) \\*/"));
    QRegularExpressionMatch match = re.match(css);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString CssLoader::stripMarker(QString css)
{
    QRegularExpression re(QStringLiteral("^\\s*/\\* scriba-base-css-version: [0-9a-f]{64} \\*/[ \\t]*\\r?\\n?"));
    return css.remove(re);
}

QString CssLoader::normalizeCss(const QString &css, const QString &hash)
{
    return QStringLiteral("/* scriba-base-css-version: %1 */\n").arg(hash)
        + stripMarker(css);
}

void CssLoader::supersedeStaleFile(const QString &path)
{
    QFile::remove(path + ".bak");
    if (!QFile::rename(path, path + ".bak"))
        QFile::remove(path);
}

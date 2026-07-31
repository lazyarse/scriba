#include "CssLoader.h"
#include "CssConfig.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>

CssLoader::CssLoader(const CssConfig *config)
    : m_config(config)
{
}

QString CssLoader::configDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/scriba";
}

QString CssLoader::themesDir() const
{
    return configDir() + "/themes";
}

QString CssLoader::loadOrFallback(const QString &path, const QString &fallback) const
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    QFile res(fallback);
    if (res.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(res.readAll());
    return {};
}

QString CssLoader::previewBaseCss()
{
    if (!m_cacheDirty && !m_previewBaseCache.isNull())
        return m_previewBaseCache;
    m_previewBaseCache = loadOrFallback(configDir() + "/preview-base.css", ":/preview-base.css");
    return m_previewBaseCache;
}

void CssLoader::setPreviewBaseCss(const QString &css)
{
    QDir().mkpath(configDir());
    QFile f(configDir() + "/preview-base.css");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(css.toUtf8());
    }
    m_previewBaseCache = css;
    invalidateCache();
}

QString CssLoader::printBaseCss()
{
    if (!m_cacheDirty && !m_printBaseCache.isNull())
        return m_printBaseCache;
    m_printBaseCache = loadOrFallback(configDir() + "/print-base.css", ":/print-base.css");
    return m_printBaseCache;
}

void CssLoader::setPrintBaseCss(const QString &css)
{
    QDir().mkpath(configDir());
    QFile f(configDir() + "/print-base.css");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(css.toUtf8());
    }
    m_printBaseCache = css;
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

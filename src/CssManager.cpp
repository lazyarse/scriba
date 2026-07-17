#include "CssManager.h"
#include "Preferences.h"
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

CssManager::CssManager()
{
    QSettings settings;
    m_stylesheets = settings.value(Preferences::CssFiles, QStringList()).toStringList();
    m_activeStylesheet = settings.value(Preferences::ActiveCssFile, "").toString();
    m_printStylesheets = settings.value(Preferences::PrintCssFiles, QStringList()).toStringList();
    m_activePrintStylesheet = settings.value(Preferences::ActivePrintCssFile, "").toString();
}

QString CssManager::configDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString CssManager::loadOrFallback(const QString &path, const QString &fallback) const
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    QFile res(fallback);
    if (res.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(res.readAll());
    return {};
}

QString CssManager::editorBaseCss() const
{
    if (!m_cacheDirty && !m_editorBaseCache.isNull())
        return m_editorBaseCache;
    m_editorBaseCache = loadOrFallback(configDir() + "/editor-base.css", ":/editor-base.css");
    return m_editorBaseCache;
}

QString CssManager::previewBaseCss() const
{
    if (!m_cacheDirty && !m_previewBaseCache.isNull())
        return m_previewBaseCache;
    m_previewBaseCache = loadOrFallback(configDir() + "/preview-base.css", ":/preview-base.css");
    return m_previewBaseCache;
}

void CssManager::setEditorBaseCss(const QString &css)
{
    QDir().mkpath(configDir());
    QFile f(configDir() + "/editor-base.css");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(css.toUtf8());
    }
    m_editorBaseCache = css;
    invalidateCache();
}

void CssManager::setPreviewBaseCss(const QString &css)
{
    QDir().mkpath(configDir());
    QFile f(configDir() + "/preview-base.css");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(css.toUtf8());
    }
    m_previewBaseCache = css;
    invalidateCache();
}

QString CssManager::themeCss() const
{
    if (m_cacheDirty) {
        if (!m_activeStylesheet.isEmpty())
            m_themeCache = loadCssFile(m_activeStylesheet);
        else
            m_themeCache.clear();
    }
    return m_themeCache;
}

void CssManager::invalidateCache()
{
    m_cacheDirty = true;
    m_editorBaseCache.clear();
    m_previewBaseCache.clear();
}

void CssManager::setStylesheets(const QStringList &paths)
{
    m_stylesheets = paths;
    invalidateCache();
    QSettings settings;
    settings.setValue(Preferences::CssFiles, paths);
}

QStringList CssManager::stylesheets() const
{
    return m_stylesheets;
}

void CssManager::setActiveStylesheet(const QString &path)
{
    m_activeStylesheet = path;
    invalidateCache();
    QSettings settings;
    settings.setValue(Preferences::ActiveCssFile, path);
}

QString CssManager::activeStylesheet() const
{
    return m_activeStylesheet;
}

QString CssManager::printCss() const
{
    if (!m_activePrintStylesheet.isEmpty()) {
        QString css = loadCssFile(m_activePrintStylesheet);
        if (!css.isEmpty())
            return css;
    }
    QFile f(":/print-base.css");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return {};
}

void CssManager::setPrintStylesheets(const QStringList &paths)
{
    m_printStylesheets = paths;
    QSettings settings;
    settings.setValue(Preferences::PrintCssFiles, paths);
}

QStringList CssManager::printStylesheets() const
{
    return m_printStylesheets;
}

void CssManager::setActivePrintStylesheet(const QString &path)
{
    m_activePrintStylesheet = path;
    QSettings settings;
    settings.setValue(Preferences::ActivePrintCssFile, path);
}

QString CssManager::activePrintStylesheet() const
{
    return m_activePrintStylesheet;
}

QString CssManager::loadCssFile(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(file.readAll());
    return {};
}

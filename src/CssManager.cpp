#include "CssManager.h"
#include "Preferences.h"
#include <QFile>
#include <QSettings>

CssManager::CssManager()
{
    QSettings settings;
    m_stylesheets = settings.value(Preferences::CssFiles, QStringList()).toStringList();
    m_activeStylesheet = settings.value(Preferences::ActiveCssFile, "").toString();
}

QString CssManager::combinedCss() const
{
    if (!m_cacheDirty)
        return m_combinedCache;

    QString css;

    QFile defaultFile(":/default.css");
    if (defaultFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        css += QString::fromUtf8(defaultFile.readAll());
        css += "\n";
    }

    if (!m_activeStylesheet.isEmpty()) {
        css += loadCssFile(m_activeStylesheet);
        css += "\n";
    }

    m_combinedCache = css;
    m_cacheDirty = false;
    return css;
}

void CssManager::invalidateCache()
{
    m_cacheDirty = true;
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

QString CssManager::loadCssFile(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(file.readAll());
    return "";
}

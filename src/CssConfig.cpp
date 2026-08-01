#include "CssConfig.h"
#include "Preferences.h"
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

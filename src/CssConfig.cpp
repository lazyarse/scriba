#include "CssConfig.h"
#include "Preferences.h"
#include <QSettings>

CssConfig::CssConfig()
{
    QSettings settings;
    m_stylesheets = settings.value(Preferences::CssFiles, QStringList()).toStringList();
    m_activeStylesheet = settings.value(Preferences::ActiveCssFile, "").toString();
    m_printStylesheets = settings.value(Preferences::PrintCssFiles, QStringList()).toStringList();
    m_activePrintStylesheet = settings.value(Preferences::ActivePrintCssFile, "").toString();
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

QStringList CssConfig::printStylesheets() const
{
    return m_printStylesheets;
}

void CssConfig::setPrintStylesheets(const QStringList &paths)
{
    m_printStylesheets = paths;
    QSettings settings;
    settings.setValue(Preferences::PrintCssFiles, paths);
}

QString CssConfig::activePrintStylesheet() const
{
    return m_activePrintStylesheet;
}

void CssConfig::setActivePrintStylesheet(const QString &path)
{
    m_activePrintStylesheet = path;
    QSettings settings;
    settings.setValue(Preferences::ActivePrintCssFile, path);
}

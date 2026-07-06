#include "CssManager.h"
#include "Preferences.h"
#include <QFile>
#include <QDir>
#include <QSettings>

CssManager::CssManager()
{
    QSettings settings;
    m_cssDirectory = settings.value(Preferences::CssDirectory, "").toString();
    m_enabledFiles = settings.value(Preferences::EnabledCssFiles, QStringList{"default.css"}).toStringList();
}

QString CssManager::combinedCss() const
{
    QString css;

    QFile defaultFile(":/default.css");
    if (defaultFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        css += QString::fromUtf8(defaultFile.readAll());
        css += "\n";
    }

    QDir dir(m_cssDirectory);
    for (const QString &file : m_enabledFiles) {
        if (file == "default.css") continue;
        QString path = dir.filePath(file);
        css += loadCssFile(path);
        css += "\n";
    }

    return css;
}

void CssManager::setCssDirectory(const QString &directory)
{
    m_cssDirectory = directory;
    QSettings settings;
    settings.setValue(Preferences::CssDirectory, directory);
}

QString CssManager::cssDirectory() const
{
    return m_cssDirectory;
}

void CssManager::setEnabledFiles(const QStringList &files)
{
    m_enabledFiles = files;
    QSettings settings;
    settings.setValue(Preferences::EnabledCssFiles, files);
}

QStringList CssManager::enabledFiles() const
{
    return m_enabledFiles;
}

QStringList CssManager::availableFiles() const
{
    QDir dir(m_cssDirectory);
    return dir.entryList(QStringList() << "*.css", QDir::Files, QDir::Name);
}

QString CssManager::loadCssFile(const QString &filePath) const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.readAll());
    }
    return "";
}

#ifndef CSSMANAGER_H
#define CSSMANAGER_H

#include <QString>
#include <QStringList>

class CssManager
{
public:
    CssManager();

    QString combinedCss() const;
    void setCssDirectory(const QString &directory);
    QString cssDirectory() const;
    void setEnabledFiles(const QStringList &files);
    QStringList enabledFiles() const;
    QStringList availableFiles() const;

private:
    QString loadCssFile(const QString &filePath) const;
    QString m_cssDirectory;
    QStringList m_enabledFiles;
};

#endif

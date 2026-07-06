#ifndef CSSMANAGER_H
#define CSSMANAGER_H

#include <QString>
#include <QStringList>

class CssManager
{
public:
    CssManager();

    QString combinedCss() const;
    void invalidateCache();
    void setCssDirectory(const QString &directory);
    QString cssDirectory() const;
    void setEnabledFiles(const QStringList &files);
    QStringList enabledFiles() const;
    QStringList availableFiles() const;

private:
    QString loadCssFile(const QString &filePath) const;
    QString m_cssDirectory;
    QStringList m_enabledFiles;
    mutable QString m_combinedCache;
    mutable bool m_cacheDirty = true;
};

#endif

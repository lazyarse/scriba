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

    void setStylesheets(const QStringList &paths);
    QStringList stylesheets() const;

    void setActiveStylesheet(const QString &path);
    QString activeStylesheet() const;

private:
    QString loadCssFile(const QString &filePath) const;
    QStringList m_stylesheets;
    QString m_activeStylesheet;
    mutable QString m_combinedCache;
    mutable bool m_cacheDirty = true;
};

#endif

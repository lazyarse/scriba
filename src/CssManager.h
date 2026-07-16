#ifndef CSSMANAGER_H
#define CSSMANAGER_H

#include <QString>
#include <QStringList>

class CssManager
{
public:
    CssManager();

    QString combinedCss() const;
    QString editorBaseCss() const;
    QString previewBaseCss() const;
    QString themeCss() const;
    void invalidateCache();

    void setEditorBaseCss(const QString &css);
    void setPreviewBaseCss(const QString &css);

    void setStylesheets(const QStringList &paths);
    QStringList stylesheets() const;

    void setActiveStylesheet(const QString &path);
    QString activeStylesheet() const;

    QString printCss() const;
    void setPrintStylesheets(const QStringList &paths);
    QStringList printStylesheets() const;
    void setActivePrintStylesheet(const QString &path);
    QString activePrintStylesheet() const;

private:
    QString loadCssFile(const QString &filePath) const;
    QString loadOrFallback(const QString &path, const QString &fallback) const;
    QString configDir() const;

    QStringList m_stylesheets;
    QString m_activeStylesheet;
    QStringList m_printStylesheets;
    QString m_activePrintStylesheet;
    mutable QString m_combinedCache;
    mutable QString m_themeCache;
    mutable QString m_editorBaseCache;
    mutable QString m_previewBaseCache;
    mutable bool m_cacheDirty = true;
};

#endif

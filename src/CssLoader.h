#pragma once

#include <QString>

class CssConfig;

class CssLoader
{
public:
    explicit CssLoader(const CssConfig *config);

    QString themeCss();
    QString previewBaseCss();
    QString printBaseCss();
    QString printCss();

    void setPreviewBaseCss(const QString &css);
    void setPrintBaseCss(const QString &css);

    void invalidateCache();

private:
    QString configDir() const;
    QString loadCssFile(const QString &filePath) const;
    QString loadOrFallback(const QString &path, const QString &fallback) const;

    const CssConfig *m_config;
    QString m_themeCache;
    QString m_previewBaseCache;
    QString m_printBaseCache;
    bool m_cacheDirty = true;
};


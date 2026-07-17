#ifndef CSSLOADER_H
#define CSSLOADER_H

#include <QString>

class CssConfig;

class CssLoader
{
public:
    explicit CssLoader(const CssConfig *config);

    QString themeCss();
    QString editorBaseCss();
    QString previewBaseCss();
    QString printCss() const;

    void setEditorBaseCss(const QString &css);
    void setPreviewBaseCss(const QString &css);

    void invalidateCache();

private:
    QString configDir() const;
    QString loadCssFile(const QString &filePath) const;
    QString loadOrFallback(const QString &path, const QString &fallback) const;

    const CssConfig *m_config;
    QString m_themeCache;
    QString m_editorBaseCache;
    QString m_previewBaseCache;
    bool m_cacheDirty = true;
};

#endif

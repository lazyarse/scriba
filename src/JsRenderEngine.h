#pragma once

#include <QString>

class JsRenderEngine
{
public:
    static QString buildFullHtml(const QString &bodyHtml, const QString &css,
                                 const QString &emojiMode, const QString &mermaidTheme);

    static QString replaceQrcUrls(const QString &html);

    static QString renderSync(const QString &fullHtml, const QString &baseUrl,
                              int timeoutMs = 30000);
};

#pragma once

#include <QString>
#include <QUrl>

enum class ScriptHandling { Strip, EmbedExternal };

class JsRenderEngine
{
public:
    static QString buildFullHtml(const QString &bodyHtml, const QString &css,
                                 const QString &emojiMode, const QString &mermaidTheme);

    static QString replaceQrcUrls(const QString &html);

    static QString renderSync(const QString &fullHtml, const QString &baseUrl,
                              int timeoutMs = 30000);

    static QString katexCss();

    static QString embedImages(const QString &html, const QUrl &baseUrl);

    static QString embedResources(const QString &html, ScriptHandling handling);

    static QString stripScriptTags(const QString &html);
};

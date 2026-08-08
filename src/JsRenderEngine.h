// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

#include <QString>
#include <QUrl>
#include "StaticHelpers.h"

#define DEFAULT_EMOJI_FONT "@font-face{font-family:'Symbola';src:url('qrc:///fonts/Symbola.ttf')format('truetype')}"

enum class ScriptHandling { Strip, EmbedExternal };

class JsRenderEngine
{
public:
    static QString buildFullHtml(const QString &bodyHtml, const QString &css,
                                 const QString &emojiMode, const QString &mermaidTheme);

    static QString buildFullHtmlForDocx(const QString &bodyHtml, const QString &css,
                                        const QString &emojiMode, const QString &mermaidTheme);

    static QString buildFullHtmlForDocxOmml(const QString &bodyHtml, const QString &css,
                                            const QString &emojiMode, const QString &mermaidTheme);

    static QString replaceQrcUrls(const QString &html);

    static QString renderSync(const QString &fullHtml, const QString &baseUrl,
                              int timeoutMs = Timeout::RenderTimeoutMs);

    static QString katexCss();

    static QString embedImages(const QString &html, const QUrl &baseUrl);

    static QString embedResources(const QString &html, ScriptHandling handling);

    static QString stripScriptTags(const QString &html);
};

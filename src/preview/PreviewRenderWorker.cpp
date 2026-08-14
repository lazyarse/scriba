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
#include "PreviewRenderWorker.h"
#include "JsRenderEngine.h"
#include "MarkdownParser.h"

void PreviewRenderWorker::render(quint64 generation, const QString &markdown,
                                 bool blockRawHtml, bool stripScripts)
{
    QString html = MarkdownParser::toHtml(markdown, blockRawHtml);
    if (stripScripts)
        html = JsRenderEngine::stripScriptTags(html);
    emit finished(generation, html);
}

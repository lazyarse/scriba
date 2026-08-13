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

#include <QObject>
#include <QString>

// Background markdown→HTML renderer for the live preview. Rendering a large
// document with md4c takes hundreds of milliseconds, which — done inline on
// the GUI thread — is exactly the "opening a big file locks the UI until it's
// rendered" freeze. MainWindow dispatches large-document renders here (via a
// queued functor on this worker's thread) and the result comes back as a
// queued signal tagged with a generation counter, so results superseded by a
// newer render (or a tab switch) can be dropped.
//
// Both MarkdownParser::toHtml and JsRenderEngine::stripScriptTags are pure
// static string functions, so this is safe to run off the GUI thread.
class PreviewRenderWorker : public QObject
{
    Q_OBJECT

public:
    explicit PreviewRenderWorker() = default;

public slots:
    void render(quint64 generation, const QString &markdown,
                bool blockRawHtml, bool stripScripts);

signals:
    void finished(quint64 generation, const QString &html);
};

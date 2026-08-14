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
#include "PdfRenderer.h"

#include <QEventLoop>
#include <QFile>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QTimer>
#include <QWebEngineView>

bool PdfRenderer::render(const QString &fullHtml, const QString &baseUrl,
                         const QString &outPath)
{
    QWebEngineView view;
    view.resize(1024, 1400);
    view.setHtml(fullHtml, QUrl(baseUrl));
    view.show(); // some printToPdf paths need a shown view under Xvfb/CI

    QEventLoop loop;
    bool ok = false;
    bool loadDone = false;
    QObject::connect(&view, &QWebEngineView::loadFinished, &loop, [&](bool loadOk) {
        loadDone = true;
        ok = loadOk;
        if (loadOk) {
            // Wait for async ECharts/Mermaid rendering (mirrors the dialog).
            // When there are no charts the promises resolve immediately.
            view.page()->runJavaScript(
                QStringLiteral("Promise.all("
                               "[window.echartsReady||Promise.resolve(),"
                               " window.mermaidReady||Promise.resolve()])"
                               ".then(function(){return true;})"),
                [&](const QVariant &) {
                    QPageLayout layout(QPageSize(QPageSize::A4),
                                       QPageLayout::Portrait,
                                       QMarginsF(42.52, 42.52, 42.52, 42.52), // 15 mm in pt
                                       QPageLayout::Point);
                    view.page()->printToPdf([&](const QByteArray &data) {
                        QFile f(outPath);
                        if (f.open(QIODevice::WriteOnly)) {
                            f.write(data);
                            f.close();
                            ok = true;
                        } else {
                            ok = false;
                        }
                        loop.quit();
                    }, layout);
                });
        } else {
            loop.quit();
        }
    });
    QTimer::singleShot(15000, &loop, &QEventLoop::quit); // safety timeout
    loop.exec();
    return ok && loadDone;
}

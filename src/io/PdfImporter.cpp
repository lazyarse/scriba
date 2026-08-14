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
#include "PdfImporter.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWebEnginePage>

// Runs bundled pdf.js + the pdf2md heuristics in a hidden QWebEnginePage.
// pdf.js needs a worker script and a PDF bytes buffer; both are inlined via
// markers so nothing is fetched from the network or the qrc URL scheme.
PdfImportResult PdfImporter::convert(const QString &filePath, int timeoutMs)
{
    PdfImportResult result;

    QFile pdfFile(filePath);
    if (!pdfFile.open(QIODevice::ReadOnly))
        return { .ok = false, .error = QStringLiteral("Cannot open PDF: ") + filePath };
    const QByteArray pdfBytes = pdfFile.readAll();
    if (pdfBytes.isEmpty())
        return { .ok = false, .error = QStringLiteral("PDF is empty") };

    QFile pdfjsFile(QStringLiteral(":/pdf.min.mjs"));
    if (!pdfjsFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return { .ok = false, .error = QStringLiteral("pdf.js library missing") };
    QFile workerFile(QStringLiteral(":/pdf.worker.min.mjs"));
    if (!workerFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return { .ok = false, .error = QStringLiteral("pdf.js worker missing") };
    QFile pdf2mdFile(QStringLiteral(":/pdf2md.js"));
    if (!pdf2mdFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return { .ok = false, .error = QStringLiteral("pdf2md.js missing") };
    const QByteArray workerBytes = workerFile.readAll();

    struct Ctx {
        QEventLoop loop;
        QString json;
        bool timeout = false;
        std::shared_ptr<QTimer> poll;
    };
    auto ctx = std::make_shared<Ctx>();
    ctx->poll = std::make_shared<QTimer>();
    ctx->poll->setInterval(100);

    auto page = new QWebEnginePage();

    QObject::connect(page, &QWebEnginePage::loadFinished, page,
            [ctx, page, pdfBytes, workerBytes](bool ok) {
        if (ctx->timeout)
            return;
        if (!ok) {
            ctx->loop.quit();
            return;
        }

        const QString pdfB64 = QString::fromLatin1(pdfBytes.toBase64());
        const QString workerB64 = QString::fromLatin1(workerBytes.toBase64());

        // QtWebEngine's runJavaScript does not await a returned promise: the
        // callback fires immediately with no resolved value. The async
        // converter therefore stashes its JSON in a global and we poll for it
        // from C++ until it is set or the deadline passes.
        page->runJavaScript(
            QStringLiteral("(async function(){"
                "try{"
                "pdfjsLib.GlobalWorkerOptions.workerSrc = URL.createObjectURL("
                "  new Blob([atob('%1')], {type:'text/javascript'}));"
                // Pre-register the worker with the library so PDFWorker skips
                // spawning a real module worker (which this WebEngine refuses
                // for blob URLs); the built-in fake worker path drives the
                // bundled worker via dynamic import() instead.
                "globalThis.pdfjsWorker = await import(pdfjsLib.GlobalWorkerOptions.workerSrc);"
                "var bytes = Uint8Array.from(atob('%2'), function(c){return c.charCodeAt(0);});"
                "var doc = await pdfjsLib.getDocument({"
                "  data: bytes,"
                "  isEvalSupported: false, disableFontFace: true,"
                "  enableScripting: false, fontExtraProperties: true"
                "}).promise;"
                "var pages = [];"
                "for (var p = 1; p <= doc.numPages; p++) {"
                "  var page = await doc.getPage(p);"
                "  await page.getOperatorList();"
                "  var tc = await page.getTextContent();"
                "  var items = [];"
                "  var fontStyles = {};"
                "  for (var i = 0; i < tc.items.length; i++) {"
                "    var it = tc.items[i];"
                "    if (!it || typeof it.str !== 'string') continue;"
                "    var fn = it.fontName || '';"
                "    var st = fontStyles[fn];"
                "    if (fn && !st && page.commonObjs && page.commonObjs.has(fn)) {"
                "      try {"
                "        var f = page.commonObjs.get(fn);"
                "        st = fontStyles[fn] = {"
                "          name: f ? (f.name || f.fallbackName || '') : '',"
                "          isMonospace: !!(f && f.isMonospace),"
                "          black: !!(f && f.black),"
                "          italic: !!(f && f.italic)"
                "        };"
                "      } catch (e) {}"
                "    }"
                "    items.push({"
                "      str: it.str,"
                "      tx: it.transform ? it.transform[4] : 0,"
                "      ty: it.transform ? it.transform[5] : 0,"
                "      width: it.width || 0,"
                "      height: it.height || 0,"
                "      fontName: fn,"
                "      hasEOL: it.hasEOL === true"
                "    });"
                "  }"
                "  var vp = page.getViewport({scale:1});"
                "  pages.push({ items: items, height: vp.height, fontStyles: fontStyles });"
                "}"
                "if (!pages.length) { window.__scribaPdfResult = "
                "JSON.stringify({ok:false,error:'No text layer'}); return; }"
                "var md = window.scribaPdf2Md({pages: pages});"
                "window.__scribaPdfResult = JSON.stringify({ok:true, markdown: md, pages: doc.numPages});"
                "}catch(e){"
                "window.__scribaPdfResult = JSON.stringify({ok:false,error:String(e && e.message || e)});"
                "}"
                "})()").arg(workerB64, pdfB64),
            [](const QVariant &) {});

        auto poll = ctx->poll;
        QObject::connect(poll.get(), &QTimer::timeout, [ctx, page, poll]() {
            if (ctx->timeout)
                return;
            page->runJavaScript(QStringLiteral("window.__scribaPdfResult || ''"),
                [ctx, poll](const QVariant &value) {
                    const QString v = value.toString();
                    if (v.isEmpty())
                        return; // keep polling
                    poll->stop();
                    ctx->json = v;
                    ctx->loop.quit();
                });
        });
        poll->start();
    });

    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, [ctx]() {
            if (ctx->poll)
                ctx->poll->stop();
            ctx->timeout = true;
            ctx->loop.quit();
        });
    }

    QString wrapper = QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<script type=\"module\">@@PDFJS@@</script>"
        "<script>@@PDF2MD@@</script>"
        "</head><body></body></html>");
    wrapper.replace(QStringLiteral("@@PDFJS@@"),
                    QString::fromUtf8(pdfjsFile.readAll()));
    wrapper.replace(QStringLiteral("@@PDF2MD@@"),
                    QString::fromUtf8(pdf2mdFile.readAll()));

    page->setHtml(wrapper);
    ctx->loop.exec(QEventLoop::ExcludeUserInputEvents);
    page->deleteLater();

    if (ctx->timeout)
        return { .ok = false, .error = QStringLiteral("PDF conversion timed out") };
    if (ctx->json.isEmpty())
        return { .ok = false, .error = QStringLiteral("PDF conversion returned no result") };

    const QJsonObject obj = QJsonDocument::fromJson(ctx->json.toUtf8()).object();
    if (obj.value(QStringLiteral("ok")).toBool()) {
        result.ok = true;
        result.markdown = obj.value(QStringLiteral("markdown")).toString();
        result.pages = obj.value(QStringLiteral("pages")).toInt();
    } else {
        result.ok = false;
        result.error = obj.value(QStringLiteral("error")).toString();
    }
    return result;
}
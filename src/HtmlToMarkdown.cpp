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
#include "HtmlToMarkdown.h"
#include <QEventLoop>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>
#include <QWebEnginePage>

// Produces a double-quoted JS string literal from arbitrary text, escaping
// backslashes, quotes and control characters so input can never escape the
// wrapper page or break the injected script.
static QString jsStringLiteral(const QString &s)
{
    QString out = QStringLiteral("\"");
    for (const QChar &c : s) {
        const ushort u = c.unicode();
        switch (u) {
        case '"': out += QStringLiteral("\\\""); break;
        case '\\': out += QStringLiteral("\\\\"); break;
        case '\n': out += QStringLiteral("\\n"); break;
        case '\r': out += QStringLiteral("\\r"); break;
        case '\t': out += QStringLiteral("\\t"); break;
        case '\b': out += QStringLiteral("\\b"); break;
        case '\f': out += QStringLiteral("\\f"); break;
        default:
            if (u < 0x20)
                out += QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    out += QLatin1Char('"');
    return out;
}

// The DOM fragment parser drops <head>/<title> start tags when a full HTML
// document is injected into a <div>, but their text content survives as
// plain text. Remove those sections up front so document imports are clean.
static QString stripHeadAndTitle(const QString &html)
{
    const QRegularExpression headRe(QStringLiteral("<head[^>]*>[\\s\\S]*?</head>"),
                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression titleRe(QStringLiteral("<title[^>]*>[\\s\\S]*?</title>"),
                                     QRegularExpression::CaseInsensitiveOption);
    QString out = html;
    out.remove(headRe);
    out.remove(titleRe);
    return out;
}

// Runs turndown.js (bundled in qrc, MIT licensed) plus its GFM plugin
// (tables, strikethrough, task lists) in a hidden QWebEnginePage and extracts
// the resulting Markdown. The scripts are inlined into a minimal wrapper page
// so the conversion never depends on loading resources over the network or
// the qrc URL scheme.
QString HtmlToMarkdown::convert(const QString &html, const QUrl &baseUrl, int timeoutMs)
{
    QFile turndownFile(QStringLiteral(":/turndown.min.js"));
    if (!turndownFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QFile gfmFile(QStringLiteral(":/turndown-plugin-gfm.js"));
    if (!gfmFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    struct Ctx {
        QEventLoop loop;
        QString result;
        bool timeout = false;
    };
    auto ctx = std::make_shared<Ctx>();

    auto page = new QWebEnginePage();

    QObject::connect(page, &QWebEnginePage::loadFinished, page,
            [ctx, page, html, baseUrl](bool ok) {
        if (ctx->timeout)
            return;
        if (!ok) {
            ctx->loop.quit();
            return;
        }

        // Escaped as JS string literals so the input HTML can never break
        // out of the wrapper page or the injected script.
        const QString htmlLiteral = jsStringLiteral(stripHeadAndTitle(html));
        const QString baseLiteral = jsStringLiteral(baseUrl.toString());

        page->runJavaScript(
            QStringLiteral("document.getElementById('import-root').innerHTML = %1; true")
                .arg(htmlLiteral),
            [ctx, page, baseLiteral](const QVariant &) {
                page->runJavaScript(
                    QStringLiteral(
                        "(function(){"
                        "var base = %1;"
                        "if (base) {"
                        "  function absolutize(sel, attr) {"
                        "    document.querySelectorAll(sel).forEach(function(x){"
                        "      if (!x.hasAttribute(attr)) return;"
                        "      var v = x.getAttribute(attr);"
                        "      if (!v || v.charAt(0) === '#' || /^(javascript|mailto|tel|data):/i.test(v)) return;"
                        "      try { x.setAttribute(attr, new URL(v, base).href); } catch (e) {}"
                        "    });"
                        "  }"
                        "  absolutize('a[href]', 'href');"
                        "  absolutize('img[src]', 'src');"
                        "}"
                        "var root = document.getElementById('import-root');"
                        "root.querySelectorAll('head, script, style, noscript, iframe').forEach(function(n){ n.parentNode.removeChild(n); });"
                        "var td = new TurndownService({"
                        "  headingStyle: 'atx',"
                        "  bulletListMarker: '-',"
                        "  codeBlockStyle: 'fenced',"
                        "  fence: '```',"
                        "  emDelimiter: '*',"
                        "  strongDelimiter: '**',"
                        "  linkStyle: 'inlined',"
                        "  hr: '---'"
                        "});"
                        "td.use(turndownPluginGfm.gfm);"
                        "td.addRule('strikethrough', {"
                        "  filter: ['del', 's', 'strike'],"
                        "  replacement: function (content) { return '~~' + content + '~~'; }"
                        "});"
                        "var md = td.turndown(root);"
                        "if (!md || !md.replace(/\\s+/g, '')) md = root.innerText || '';"
                        "return md;"
                        "})()"
                    ).arg(baseLiteral),
                    [ctx](const QVariant &result) {
                        ctx->result = result.toString();
                        ctx->loop.quit();
                    }
                );
            }
        );
    });

    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, [ctx]() {
            ctx->timeout = true;
            ctx->loop.quit();
        });
    }

    // The markers are replaced instead of using QString::arg so that no
    // placeholder inside the minified scripts could be substituted.
    QString wrapper = QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<script>@@TURNDOWN@@</script>"
        "<script>@@TURNDOWN_GFM@@</script>"
        "</head><body><div id=\"import-root\"></div></body></html>");
    wrapper.replace(QStringLiteral("@@TURNDOWN@@"),
                    QString::fromUtf8(turndownFile.readAll()));
    wrapper.replace(QStringLiteral("@@TURNDOWN_GFM@@"),
                    QString::fromUtf8(gfmFile.readAll()));

    page->setHtml(wrapper, baseUrl);
    ctx->loop.exec(QEventLoop::ExcludeUserInputEvents);

    page->deleteLater();
    return ctx->result;
}

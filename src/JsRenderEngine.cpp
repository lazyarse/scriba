#include "JsRenderEngine.h"
#include "JsSnippets.h"
#include <QEventLoop>
#include <QFile>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>

QString JsRenderEngine::buildFullHtml(const QString &bodyHtml, const QString &css,
                                      const QString &emojiMode, const QString &mermaidTheme)
{
    return QString(
        "<!DOCTYPE html><html><head>"
        "<style>%1</style>"
        "<style>#preview .emoji-char{font-family:'Symbola',monospace}.emoji{height:1em;width:1em;vertical-align:-0.1em;display:inline-block}</style>"
        "<script src=\"qrc:///highlight.min.js\"></script>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<script src=\"qrc:///contrib/auto-render.min.js\"></script>"
        "<script src=\"qrc:///vega.min.js\"></script>"
        "<script src=\"qrc:///vega-lite.min.js\"></script>"
        "<script src=\"qrc:///vega-embed.min.js\"></script>"
        "<script src=\"qrc:///twemoji.min.js\"></script>"
        "<script src=\"qrc:///emoji.js\"></script>"
        "<script>%2%3%4%5"
        "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}"
        "document.addEventListener('DOMContentLoaded',function(){"
        "mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});"
        "window.mermaidReady=initMermaid();hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();generateHeadingIds();initKaTeX();window.vegaLiteReady=initVegaLite();"
        "replaceEmoji(document.body);twemojiParse('%6');"
        "});</script>"
        "</head><body id=\"preview\">%7</body></html>"
    ).arg(css, mermaidInitJs, headingIdJs, katexInitJs, vegaLiteInitJs, emojiMode, bodyHtml);
}

QString JsRenderEngine::replaceQrcUrls(const QString &html)
{
    QMimeDatabase mimeDb;
    static const QRegularExpression re(QStringLiteral("qrc:///[^\"' )]+"),
                                       QRegularExpression::CaseInsensitiveOption);
    QString result = html;
    int offset = 0;
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        auto match = it.next();
        QString qrcUrl = match.captured(0);
        QString resPath = QStringLiteral(":") + qrcUrl.mid(6);
        QFile f(resPath);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QByteArray data = f.readAll();
        QString mime = mimeDb.mimeTypeForFileNameAndData(qrcUrl, data).name();
        QString dataUri = QStringLiteral("data:%1;base64,%2")
                              .arg(mime, QString::fromLatin1(data.toBase64()));
        result.replace(match.capturedStart(0) + offset, match.capturedLength(0), dataUri);
        offset += dataUri.size() - match.capturedLength(0);
    }
    return result;
}

QString JsRenderEngine::renderSync(const QString &fullHtml, const QString &baseUrl,
                                   int timeoutMs)
{
    struct Ctx {
        QEventLoop loop;
        QString result;
        bool timeout = false;
    };
    auto ctx = std::make_shared<Ctx>();

    auto page = new QWebEnginePage();

    QObject::connect(page, &QWebEnginePage::loadFinished, page,
            [ctx, page](bool ok) {
        if (ctx->timeout) return;
        if (!ok) {
            ctx->loop.quit();
            return;
        }
        page->runJavaScript(
            QStringLiteral("Promise.all([window.vegaLiteReady||Promise.resolve(),window.mermaidReady||Promise.resolve()]).then(function(){return true;})"),
            [ctx, page](const QVariant &) {
                page->runJavaScript(
                    QStringLiteral("document.body.innerHTML"),
                    [ctx](const QVariant &html) {
                        ctx->result = html.toString();
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

    page->setHtml(fullHtml, QUrl(baseUrl));
    ctx->loop.exec(QEventLoop::ExcludeUserInputEvents);

    page->deleteLater();
    return ctx->result;
}

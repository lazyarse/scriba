#include "JsRenderEngine.h"
#include "JsSnippets.h"
#include <QEventLoop>
#include <QFile>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
        "<style>#preview{width:100%;min-width:800px}</style>"
        "<script src=\"qrc:///highlight.min.js\"></script>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<script src=\"qrc:///contrib/mhchem.min.js\"></script>"
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
        "window.mermaidReady=initMermaid();hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();generateHeadingIds();if(typeof renderMathInElement==='function')renderMathInElement(document.body,{delimiters:[{left:'$$',right:'$$',display:true},{left:'$',right:'$',display:false}]});document.querySelectorAll('.katex-mathml').forEach(function(el){el.remove()});window.vegaLiteReady=initVegaLite();"
        "replaceEmoji(document.body);twemojiParse('%6');"
        "});</script>"
        "</head><body id=\"preview\">%7</body></html>"
    ).arg(css, mermaidInitJs, headingIdJs, katexInitJs, vegaLiteInitJs, emojiMode, bodyHtml);
}

QString JsRenderEngine::buildFullHtmlForDocx(const QString &bodyHtml, const QString &css,
                                              const QString &emojiMode, const QString &mermaidTheme)
{
    return QString(
        "<!DOCTYPE html><html><head>"
        "<style>%1</style>"
        "<style>#preview .emoji-char{font-family:'Symbola',monospace}.emoji{height:1em;width:1em;vertical-align:-0.1em;display:inline-block}</style>"
        "<style>#preview{width:100%;min-width:800px}</style>"
        "<script src=\"qrc:///highlight.min.js\"></script>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<script src=\"qrc:///contrib/mhchem.min.js\"></script>"
        "<script src=\"qrc:///contrib/auto-render.min.js\"></script>"
        "<script src=\"qrc:///vega.min.js\"></script>"
        "<script src=\"qrc:///vega-lite.min.js\"></script>"
        "<script src=\"qrc:///vega-embed.min.js\"></script>"
        "<script src=\"qrc:///twemoji.min.js\"></script>"
        "<script src=\"qrc:///emoji.js\"></script>"
        "<script>%2%3%4%5%8"
        "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}"
        "document.addEventListener('DOMContentLoaded',function(){"
        "mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});"
        "window.mermaidReady=initMermaid();hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();generateHeadingIds();if(typeof renderMathInElement==='function')renderMathInElement(document.body,{delimiters:[{left:'$$',right:'$$',display:true},{left:'$',right:'$',display:false}]});document.querySelectorAll('.katex-mathml').forEach(function(el){el.remove()});window.katexReady=convertKatexToImages();window.vegaLiteReady=initVegaLite();"
        "replaceEmoji(document.body);twemojiParse('%6');"
        "});</script>"
        "</head><body id=\"preview\">%7</body></html>"
    ).arg(css, mermaidInitJs, headingIdJs, katexInitJs, vegaLiteInitJs, emojiMode, bodyHtml, katexToImageJs);
}

QString JsRenderEngine::buildFullHtmlForDocxOmml(const QString &bodyHtml, const QString &css,
                                                  const QString &emojiMode, const QString &mermaidTheme)
{
    return QString(
        "<!DOCTYPE html><html><head>"
        "<style>%1</style>"
        "<style>#preview .emoji-char{font-family:'Symbola',monospace}.emoji{height:1em;width:1em;vertical-align:-0.1em;display:inline-block}</style>"
        "<style>#preview{width:100%;min-width:800px}</style>"
        "<script src=\"qrc:///highlight.min.js\"></script>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<script src=\"qrc:///contrib/mhchem.min.js\"></script>"
        "<script src=\"qrc:///contrib/auto-render.min.js\"></script>"
        "<script src=\"qrc:///vega.min.js\"></script>"
        "<script src=\"qrc:///vega-lite.min.js\"></script>"
        "<script src=\"qrc:///vega-embed.min.js\"></script>"
        "<script src=\"qrc:///twemoji.min.js\"></script>"
        "<script src=\"qrc:///emoji.js\"></script>"
        "<script>%2%3%4%5"
        "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}"
        "document.addEventListener('DOMContentLoaded',function(){try{"
        "mermaid.initialize({startOnLoad:false,theme:'" + mermaidTheme + "'});"
        "window.mermaidReady=initMermaid();hljs.registerAliases('vl',{languageName:'json'});hljs.highlightAll();generateHeadingIds();if(typeof renderMathInElement==='function')renderMathInElement(document.body,{delimiters:[{left:'$$',right:'$$',display:true},{left:'$',right:'$',display:false}]});"
        "var MN='http://www.w3.org/1998/Math/MathML';"
        "document.querySelectorAll('.katex').forEach(function(el){"
        "var a=null;try{"
        "var anns=el.getElementsByTagNameNS(MN,'annotation');"
        "for(var i=0;i<anns.length;i++){if(anns[i].getAttribute('encoding')==='application/x-tex'){a=anns[i];break;}}"
        "}catch(e){}"
        "if(a)el.setAttribute('data-tex',a.textContent);"
        "var mml=el.querySelector('.katex-mathml');"
        "if(mml&&mml.innerHTML)el.setAttribute('data-mathml',mml.innerHTML);"
        "});"
        "document.querySelectorAll('.katex-mathml').forEach(function(el){el.remove()});"
        "document.querySelectorAll('.katex-html').forEach(function(el){el.remove()});"
        "window.vegaLiteReady=initVegaLite();window.katexReady=Promise.resolve();"
"replaceEmoji(document.body);twemojiParse('%6');"
"}catch(e){}});</script>"
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
            QStringLiteral("Promise.all([window.vegaLiteReady||Promise.resolve(),window.mermaidReady||Promise.resolve(),window.katexReady||Promise.resolve()]).then(function(){return true;})"),
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

QString JsRenderEngine::katexCss()
{
    QFile f(QStringLiteral(":/katex.min.css"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QString JsRenderEngine::embedImages(const QString &html, const QUrl &baseUrl)
{
    QMimeDatabase mimeDb;
    static const QRegularExpression imgRe(
        QStringLiteral("<img\\s[^>]*src=[\"']([^\"']+)[\"']"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);

    QString result = html;
    int offset = 0;
    auto it = imgRe.globalMatch(html);
    while (it.hasNext()) {
        auto match = it.next();
        QString src = match.captured(1);

        // Skip already-embedded data URIs and qrc URLs
        if (src.startsWith(QStringLiteral("data:")) ||
            src.startsWith(QStringLiteral("qrc:"))) {
            continue;
        }

        QByteArray imgData;
        QString mime;

        QUrl srcUrl(src, QUrl::TolerantMode);
        if (srcUrl.isRelative()) {
            // Local file — resolve against baseUrl
            QUrl resolved = baseUrl.resolved(srcUrl);
            QFile localFile(resolved.toLocalFile());
            if (localFile.open(QIODevice::ReadOnly)) {
                imgData = localFile.readAll();
                mime = mimeDb.mimeTypeForFileNameAndData(resolved.toLocalFile(), imgData).name();
            }
        } else if (srcUrl.scheme() == QStringLiteral("http") ||
                   srcUrl.scheme() == QStringLiteral("https")) {
            // External URL — fetch synchronously
            QNetworkAccessManager nam;
            QNetworkRequest req(srcUrl);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            QNetworkReply *reply = nam.get(req);

            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() == QNetworkReply::NoError) {
                imgData = reply->readAll();
                mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
                if (mime.isEmpty())
                    mime = mimeDb.mimeTypeForFileNameAndData(src, imgData).name();
            }
            reply->deleteLater();
        }

        // Validate: must be an image type and non-empty
        if (!imgData.isEmpty() && mime.startsWith(QStringLiteral("image/"))) {
            QString dataUri = QStringLiteral("data:%1;base64,%2")
                                  .arg(mime, QString::fromLatin1(imgData.toBase64()));
            result.replace(match.capturedStart(1) + offset, match.capturedLength(1), dataUri);
            offset += dataUri.size() - match.capturedLength(1);
        }
        // On failure: leave original src unchanged (graceful fallback)
    }
    return result;
}

// ── fetch helper for embedResources ──────────────────────────────────────────

static QByteArray fetchUrl(const QString &url)
{
    QUrl srcUrl(url, QUrl::TolerantMode);
    if (srcUrl.scheme() != QStringLiteral("http") &&
        srcUrl.scheme() != QStringLiteral("https"))
        return {};

    QNetworkAccessManager nam;
    QNetworkRequest req(srcUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError)
        data = reply->readAll();
    reply->deleteLater();
    return data;
}

// Extract href from a raw <link> tag string
static QString extractLinkHref(const QString &tag)
{
    static const QRegularExpression hrefRe(
        QStringLiteral("href=[\"']([^\"']+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    auto m = hrefRe.match(tag);
    return m.hasMatch() ? m.captured(1) : QString();
}

// Check if a raw <link> tag is rel="stylesheet"
static bool isStylesheetLink(const QString &tag)
{
    return tag.contains(QStringLiteral("rel=\"stylesheet\"")) ||
           tag.contains(QStringLiteral("rel='stylesheet'"));
}

// Extract src from a raw <script> tag string
static QString extractScriptSrc(const QString &tag)
{
    static const QRegularExpression srcRe(
        QStringLiteral("src=[\"']([^\"']+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    auto m = srcRe.match(tag);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString JsRenderEngine::embedResources(const QString &html, ScriptHandling handling)
{
    QString result = html;

    // ── Phase 1: <link rel="stylesheet"> → inline <style> ──────────────────
    static const QRegularExpression linkRe(
        QStringLiteral("<link\\s[^>]*>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);

    {
        int offset = 0;
        auto it = linkRe.globalMatch(result);
        while (it.hasNext()) {
            auto match = it.next();
            if (!isStylesheetLink(match.captured(0)))
                continue;
            QString href = extractLinkHref(match.captured(0));
            if (href.isEmpty())
                continue;

            QByteArray cssData;
            QUrl hrefUrl(href, QUrl::TolerantMode);
            if (hrefUrl.isRelative()) {
                QFile f(href);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                    cssData = f.readAll();
            } else {
                cssData = fetchUrl(href);
            }

            if (!cssData.isEmpty()) {
                QString styleTag = QStringLiteral("<style>%1</style>")
                                       .arg(QString::fromUtf8(cssData));
                int start = match.capturedStart(0) + offset;
                int len = match.capturedLength(0);
                result.replace(start, len, styleTag);
                offset += styleTag.size() - len;
            }
        }
    }

    // ── Phase 2: <script> tags ─────────────────────────────────────────────
    // Match full <script>...</script> blocks (with optional src attribute)
    static const QRegularExpression scriptRe(
        QStringLiteral("<script\\b[^>]*>.*?</script>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    {
        int offset = 0;
        auto it = scriptRe.globalMatch(result);
        while (it.hasNext()) {
            auto match = it.next();
            QString fullTag = match.captured(0);
            QString src = extractScriptSrc(fullTag);

            if (handling == ScriptHandling::Strip) {
                // Remove all script blocks
                int start = match.capturedStart(0) + offset;
                int len = match.capturedLength(0);
                result.remove(start, len);
                offset -= len;
            } else {
                // EmbedExternal: fetch external scripts, inline them
                if (src.isEmpty()) {
                    // Inline script — keep as-is
                    continue;
                }

                QByteArray jsData;
                QUrl srcUrl(src, QUrl::TolerantMode);
                if (srcUrl.isRelative()) {
                    QFile f(src);
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                        jsData = f.readAll();
                } else {
                    jsData = fetchUrl(src);
                }

                int start = match.capturedStart(0) + offset;
                int len = match.capturedLength(0);
                if (!jsData.isEmpty()) {
                    QString inlineScript = QStringLiteral("<script>%1</script>")
                                               .arg(QString::fromUtf8(jsData));
                    result.replace(start, len, inlineScript);
                    offset += inlineScript.size() - len;
                } else {
                    // Fetch failed — remove the block
                    result.remove(start, len);
                    offset -= len;
                }
            }
        }
    }

    // ── Phase 3: strip <script src="..."> that have no closing </script> ───
    // Some HTML may have self-closing or unclosed script tags — catch standalone ones
    if (handling == ScriptHandling::Strip) {
        static const QRegularExpression looseScriptRe(
            QStringLiteral("<script\\b[^>]*src=[\"'][^\"']+[\"'][^>]*/?>"),
            QRegularExpression::CaseInsensitiveOption);
        int offset = 0;
        auto it = looseScriptRe.globalMatch(result);
        while (it.hasNext()) {
            auto match = it.next();
            int start = match.capturedStart(0) + offset;
            int len = match.capturedLength(0);
            result.remove(start, len);
            offset -= len;
        }
    }

    return result;
}

QString JsRenderEngine::stripScriptTags(const QString &html)
{
    static const QRegularExpression scriptBlockRe(
        QStringLiteral("<script\\b[^>]*>.*?</script>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

    static const QRegularExpression looseScriptRe(
        QStringLiteral("<script\\b[^>]*/?>"),
        QRegularExpression::CaseInsensitiveOption);

    QString result = html;
    result.remove(scriptBlockRe);
    result.remove(looseScriptRe);
    return result;
}

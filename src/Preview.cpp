#include "Preview.h"
#include <QFileInfo>
#include <QLoggingCategory>
#include <QUrl>
#include <QWebEngineSettings>

Q_LOGGING_CATEGORY(lcPreview, "scriba.preview")

PreviewPage::PreviewPage(QObject *parent)
    : QWebEnginePage(parent)
{
}

void PreviewPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                           const QString &message,
                                           int lineNumber,
                                           const QString &sourceID)
{
    Q_UNUSED(sourceID);
    switch (level) {
    case InfoMessageLevel:
        qCDebug(lcPreview) << message;
        break;
    case WarningMessageLevel:
        qCDebug(lcPreview) << "line" << lineNumber << message;
        break;
    case ErrorMessageLevel:
        qCCritical(lcPreview) << "line" << lineNumber << message;
        break;
    }
}

Preview::Preview(QWidget *parent)
    : QWebEngineView(parent)
{
    setPage(new PreviewPage(this));
    page()->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    page()->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
}

void Preview::setHtmlContent(const QString &html)
{
    setHtml(html);
}

void Preview::setDocumentPath(const QString &path)
{
    m_documentPath = path;
}

QString Preview::documentPath() const
{
    return m_documentPath;
}

void Preview::scrollToLine(int line)
{
    QString js = QString(
        "(function() {"
        "  var els = document.querySelectorAll('[data-line]');"
        "  var best = null;"
        "  var bestLine = 0;"
        "  for (var i = 0; i < els.length; i++) {"
        "    var elLine = parseInt(els[i].getAttribute('data-line'));"
        "    if (elLine <= %1 && elLine > bestLine) {"
        "      best = els[i];"
        "      bestLine = elLine;"
        "    }"
        "  }"
        "  if (best) best.scrollIntoView({block: 'start', behavior: 'auto'});"
        "})();"
    ).arg(line);
    page()->runJavaScript(js);
}

void Preview::scrollToPercent(double pct)
{
    QString js = QString(
        "if(document.body)window.scrollTo(0, %1 * Math.max(1, document.body.scrollHeight - window.innerHeight));"
    ).arg(pct, 0, 'f', 6);
    page()->runJavaScript(js);
}

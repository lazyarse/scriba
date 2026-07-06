#include "Preview.h"
#include <QFileInfo>
#include <QUrl>

Preview::Preview(QWidget *parent)
    : QWebEngineView(parent)
{
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

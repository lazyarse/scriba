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

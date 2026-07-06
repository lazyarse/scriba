#include "Preview.h"
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QImage>

Preview::Preview(QWidget *parent)
    : QTextBrowser(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    setOpenExternalLinks(true);

    connect(m_networkManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return;
        }

        QUrl url = reply->url();
        QByteArray data = reply->readAll();
        reply->deleteLater();

        QImage image;
        image.loadFromData(data);
        if (image.isNull()) {
            return;
        }

        document()->addResource(QTextDocument::ImageResource, url, image);
        document()->markContentsDirty(0, document()->characterCount());
        update();
    });
}

void Preview::setHtmlContent(const QString &html)
{
    setHtml(html);
    preloadRemoteImages(html);
}

void Preview::setDocumentPath(const QString &path)
{
    if (!path.isEmpty()) {
        setSearchPaths(QStringList() << QFileInfo(path).absolutePath());
    }
}

void Preview::preloadRemoteImages(const QString &html)
{
    QRegularExpression re("<img\\s+[^>]*src=\"(https?://[^\"]+)\"[^>]*>");
    QRegularExpressionMatchIterator it = re.globalMatch(html);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QUrl url(match.captured(1));
        if (!document()->resource(QTextDocument::ImageResource, url).isNull()) {
            continue;
        }
        document()->addResource(QTextDocument::ImageResource, url, QPixmap(1, 1));
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        m_networkManager->get(request);
    }
}

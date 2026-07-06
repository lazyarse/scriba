#ifndef PREVIEW_H
#define PREVIEW_H

#include <QTextBrowser>
#include <QNetworkAccessManager>
#include <QRegularExpression>

class Preview : public QTextBrowser
{
    Q_OBJECT

public:
    explicit Preview(QWidget *parent = nullptr);
    void setHtmlContent(const QString &html);
    void setDocumentPath(const QString &path);

private:
    void preloadRemoteImages(const QString &html);
    QNetworkAccessManager *m_networkManager;
};

#endif

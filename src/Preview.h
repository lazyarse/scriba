#ifndef PREVIEW_H
#define PREVIEW_H

#include <QWebEngineView>

class Preview : public QWebEngineView
{
    Q_OBJECT

public:
    explicit Preview(QWidget *parent = nullptr);
    void setHtmlContent(const QString &html);
    void setDocumentPath(const QString &path);
    QString documentPath() const;

private:
    QString m_documentPath;
};

#endif

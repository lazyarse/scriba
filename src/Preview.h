#ifndef PREVIEW_H
#define PREVIEW_H

#include <QWebEnginePage>
#include <QWebEngineView>

class PreviewPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit PreviewPage(QObject *parent = nullptr);

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;
};

class Preview : public QWebEngineView
{
    Q_OBJECT

public:
    explicit Preview(QWidget *parent = nullptr);
    void setHtmlContent(const QString &html);
    void setDocumentPath(const QString &path);
    QString documentPath() const;
    void scrollToLine(int line);
    void scrollToPercent(double pct);

private:
    QString m_documentPath;
};

#endif

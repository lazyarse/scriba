#ifndef PREVIEW_H
#define PREVIEW_H

#include <QWebEnginePage>
#include <QWebEngineView>

class QContextMenuEvent;

class PreviewPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit PreviewPage(QObject *parent = nullptr);

signals:
    void openLinkRequested(const QUrl &url);

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
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

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QString m_documentPath;
};

#endif

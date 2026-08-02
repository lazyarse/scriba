// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

#include <QColor>
#include <QString>
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

QWebEngineView *createPreviewView(QWidget *parent, const QString &themeCss = QString());

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
    void setHtmlWithOverlay(const QString &html, const QUrl &baseUrl);
    void hideRenderOverlay();
    void setThemeBackgroundColor(const QColor &color);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QString m_documentPath;
    QColor m_backgroundColor;
};


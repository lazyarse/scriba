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
#include "Preview.h"
#include "CssUtils.h"
#include "StaticHelpers.h"
#include <QAbstractButton>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineContextMenuRequest>
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
        qCWarning(lcPreview) << "line" << lineNumber << message;
        break;
    case ErrorMessageLevel:
        qCCritical(lcPreview) << "line" << lineNumber << message;
        break;
    }
}

bool PreviewPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (type == NavigationTypeLinkClicked)
        return false;
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

Preview::Preview(QWidget *parent)
    : QWebEngineView(parent)
{
    setPage(new PreviewPage(this));
    page()->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    page()->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
}

void Preview::setThemeBackgroundColor(const QColor &color)
{
    if (!color.isValid() || color == m_backgroundColor)
        return;
    m_backgroundColor = color;
    // Paint the page's canvas with the theme background so the preview is never
    // white before the document's content commits its first frame.
    page()->setBackgroundColor(color);
    setStyleSheet(QStringLiteral("QWebEngineView{background-color:%1}").arg(color.name()));
}

void Preview::setHtmlContent(const QString &html)
{
    setHtml(html);
}

void Preview::setHtmlWithOverlay(const QString &html, const QUrl &baseUrl)
{
    // Blank the current page and show the "Rendering…" overlay BEFORE the new
    // document starts loading; the overlay then lives in the new document too
    // (visible by default, hidden after rendering completes).
    // Deliberately no result callback: a callback still pending when the page
    // is destroyed is invoked by WebContentsAdapter::clearJavaScriptCallbacks,
    // and calling setHtml() from there re-entrantly crashes. The deferred
    // setHtml() still commits after the blank (same renderer message queue).
    page()->runJavaScript(QStringLiteral("typeof scribaBeginRender!=='undefined'&&scribaBeginRender()"));
    QTimer::singleShot(0, this, [this, html, baseUrl]() {
        setHtml(html, baseUrl);
    });
}

void Preview::hideRenderOverlay()
{
    page()->runJavaScript(QStringLiteral("scribaEndRender()"));
}

void Preview::showRenderError(const QString &message)
{
    // Replaces the "white pane" with a styled error panel when a render or
    // page-load failure occurs (a failure scribaUpdate's own try/catch could
    // not surface). Falls back to body text if the report's script isn't
    // present in the current document.
    const QString msg = message.isEmpty() ? QStringLiteral("Unknown error") : message;
    page()->runJavaScript(
        QStringLiteral("typeof scribaShowRenderError!=='undefined'"
                       "?scribaShowRenderError(%1):void(document.body.textContent=%1)")
            .arg(escapeJsString(msg)));
    hideRenderOverlay();
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

void Preview::contextMenuEvent(QContextMenuEvent *event)
{
    QWebEngineContextMenuRequest *request = lastContextMenuRequest();
    if (!request) {
        QWebEngineView::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);

    if (!request->selectedText().isEmpty()) {
        QAction *copyAction = menu.addAction("Copy");
        connect(copyAction, &QAction::triggered, this, [this]() {
            page()->triggerAction(QWebEnginePage::Copy);
        });
    }

    if (!request->linkUrl().isEmpty()) {
        if (!menu.isEmpty()) menu.addSeparator();

        QAction *copyLink = menu.addAction("Copy Link Address");
        connect(copyLink, &QAction::triggered, this, [url = request->linkUrl()]() {
            QGuiApplication::clipboard()->setText(url.toString());
        });

        QAction *openExternal = menu.addAction("Open in External Browser");
        connect(openExternal, &QAction::triggered, this, [url = request->linkUrl()]() {
            QDesktopServices::openUrl(url);
        });
    }

    if (request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage) {
        if (!menu.isEmpty()) menu.addSeparator();

        QAction *copyImage = menu.addAction("Copy Image");
        connect(copyImage, &QAction::triggered, this, [this]() {
            page()->triggerAction(QWebEnginePage::CopyImageToClipboard);
        });

        QAction *copyImageUrl = menu.addAction("Copy Image URL");
        connect(copyImageUrl, &QAction::triggered, this, [url = request->mediaUrl()]() {
            QGuiApplication::clipboard()->setText(url.toString());
        });

        QAction *saveImage = menu.addAction("Save Image As...");
        connect(saveImage, &QAction::triggered, this, [this]() {
            page()->triggerAction(QWebEnginePage::DownloadImageToDisk);
        });
    }

    if (!menu.isEmpty()) menu.addSeparator();

    QAction *zoomIn = menu.addAction("Zoom In");
    connect(zoomIn, &QAction::triggered, this, [this]() {
        double factor = zoomFactor() * 1.1;
        setZoomFactor(qMin(factor, 5.0));
    });

    QAction *zoomOut = menu.addAction("Zoom Out");
    connect(zoomOut, &QAction::triggered, this, [this]() {
        double factor = zoomFactor() / 1.1;
        setZoomFactor(qMax(factor, 0.2));
    });

    QAction *resetZoom = menu.addAction("Reset Zoom");
    connect(resetZoom, &QAction::triggered, this, [this]() {
        setZoomFactor(1.0);
    });

    menu.addSeparator();

    QAction *viewSource = menu.addAction("View Page Source");
    connect(viewSource, &QAction::triggered, this, [this]() {
        page()->toHtml([this](const QString &html) {
            QDialog dlg(this);
            dlg.setWindowTitle("Page Source");
            dlg.resize(700, 500);
            auto *layout = new QVBoxLayout(&dlg);
            auto *textEdit = new QPlainTextEdit(html);
            textEdit->setReadOnly(true);
            textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            layout->addWidget(textEdit);
            auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
            btnBox->button(QDialogButtonBox::Close)->setText(tr("&Close"));
            stripButtonIcons(btnBox);
            connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
            layout->addWidget(btnBox);
            dlg.exec();
        });
    });

    menu.exec(event->globalPos());
}

QWebEngineView *createPreviewView(QWidget *parent, const QString &themeCss)
{
    auto *view = new QWebEngineView(parent);
    auto *page = new PreviewPage(view);
    view->setPage(page);
if (!themeCss.isEmpty()) {
        // Keep dialog-level previews on their theme background before first paint
        // instead of flashing Chromium's default white.
        QColor bg = CssUtils::themeColors(themeCss).background;
        if (bg.isValid()) {
            page->setBackgroundColor(bg);
            view->setStyleSheet(QStringLiteral("QWebEngineView{background-color:%1}").arg(bg.name()));
        }
    }
    return view;
}

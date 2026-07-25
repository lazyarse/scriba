#include "Preview.h"
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
#include <QRegularExpression>
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
            for (auto *btn : btnBox->buttons()) btn->setIcon(QIcon());
            connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
            layout->addWidget(btnBox);
            dlg.exec();
        });
    });

    menu.exec(event->globalPos());
}

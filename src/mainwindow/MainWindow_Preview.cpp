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


#include "MainWindow.h"
#include "editor/Editor.h"
#include "preview/Preview.h"
#include "css/CssConfig.h"
#include "css/CssLoader.h"
#include "css/CssUtils.h"
#include "preview/JsRenderEngine.h"
#include "preview/JsSnippets.h"
#include "preview/MarkdownParser.h"
#include "prefs/Preferences.h"
#include "preview/PreviewPagination.h"
#include "preview/PreviewRenderWorker.h"
#include "preview/PrintOptions.h"
#include "StaticHelpers.h"
#include "validation/ValidationReport.h"
#include <QApplication>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QMetaObject>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

void MainWindow::refreshPreviewCss()
{
    QSettings settings;
    QString rawThemeCss = m_cssLoader->themeCss();
    if (m_printLayoutMode) {
        // Print layout strips the theme and swaps in the print base CSS; the
        // page is rebuilt from scratch when it is toggled off, so the classic
        // CSS never needs live-patching here.
        m_preview->setThemeBackgroundColor(CssUtils::themeColors(rawThemeCss).background);
        return;
    }
    int uiFontSize = settings.value(Preferences::UiFontSize, Preferences::DefaultUiFontSize).toInt();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss, uiFontSize);
    QString previewCss = chromeCss + rawThemeCss;
    QString previewBaseCss = m_cssLoader->previewBaseCss();

    bool needPreviewUpdate = (previewCss != m_cachedPreviewCss);
    bool needChromeUpdate = (chromeCss != m_cachedFullCss);
    bool needBaseUpdate = (previewBaseCss != m_cachedPreviewBaseCss);
    QString overlayCss = CssUtils::renderOverlayCss(rawThemeCss);
    bool needOverlayUpdate = (overlayCss != m_cachedOverlayCss);

    m_preview->setThemeBackgroundColor(CssUtils::themeColors(rawThemeCss).background);

    if (!needPreviewUpdate && !needChromeUpdate && !needBaseUpdate && !needOverlayUpdate)
        return;

    if (needPreviewUpdate) {
        m_cachedPreviewCss = previewCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('theme-css').textContent = '%1';")
                .arg(escapeJsString(previewCss));
            m_preview->page()->runJavaScript(js);
        }
    }

    if (needOverlayUpdate) {
        m_cachedOverlayCss = overlayCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('render-css').textContent = '%1';")
                .arg(escapeJsString(overlayCss));
            m_preview->page()->runJavaScript(js);
        }
    }

    if (needBaseUpdate) {
        m_cachedPreviewBaseCss = previewBaseCss;
        if (m_previewInitialized) {
            QString js = QString("document.getElementById('base-css').textContent = '%1';")
                .arg(escapeJsString(previewBaseCss));
            m_preview->page()->runJavaScript(js);
        }
    }

    if (needChromeUpdate) {
        m_cachedFullCss = chromeCss;
        applyStyleSheetToAllEditors();
        QSettings s;
        applyEditorLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt());
        if (!m_chromeUpdateScheduled) {
            m_chromeUpdateScheduled = true;
            QTimer::singleShot(0, this, [this, chromeCss]() {
                m_chromeUpdateScheduled = false;
                qApp->setStyleSheet(chromeCss);
            });
        }
        QColor iconColor = CssUtils::chromeTextColor(rawThemeCss);
        m_fullscreenBtn->setIcon(themedIcon(":/icons/fullscreen.svg", iconColor));
        m_previewBtn->setIcon(themedIcon(":/icons/preview.svg", iconColor));
    }
}

void MainWindow::updatePreview()
{
    updatePreview(false);
}

void MainWindow::updatePreview(bool tabSwitch)
{
    Editor *ed = currentEditor();
    if (!ed) return;

    QSettings prefs;
    updateStats();
    bool blockRawHtml = prefs.value(Preferences::BlockRawHtmlPreview, true).toBool();
    bool stripScripts = prefs.value(Preferences::StripPreviewScripts, true).toBool();
    TabInfo *info = activeTabInfo();
    QString html;
    if (info && info->previewHtmlValid
        && info->previewBlockRaw == blockRawHtml
        && info->previewStripScripts == stripScripts) {
        html = info->previewHtml;
        commitPreviewHtml(html, tabSwitch, info);
        return;
    }

    // Large documents render md→HTML on a background worker (the md4c pass
    // alone takes hundreds of ms for a multi-thousand-block file). Small
    // documents keep the inline path so the preview stays exactly as
    // synchronous as before.
    if (ed->document()->blockCount() > kLargeDocBlocks) {
        requestPreviewRender(ed, tabSwitch);
        return;
    }

    html = m_parser->toHtml(ed->toPlainText(), blockRawHtml);
    if (stripScripts)
        html = JsRenderEngine::stripScriptTags(html);
    if (info) {
        info->previewHtml = html;
        info->previewBlockRaw = blockRawHtml;
        info->previewStripScripts = stripScripts;
        info->previewHtmlValid = true;
    }
    commitPreviewHtml(html, tabSwitch, info);
}

void MainWindow::requestPreviewRender(Editor *ed, bool tabSwitch)
{
    QSettings prefs;
    const bool blockRawHtml = prefs.value(Preferences::BlockRawHtmlPreview, true).toBool();
    const bool stripScripts = prefs.value(Preferences::StripPreviewScripts, true).toBool();

    // Bump the generation FIRST: results of any in-flight render (including
    // one dispatched a moment ago for this same document) are now stale.
    const quint64 generation = ++m_renderGeneration;
    m_pendingRender.gen = generation;
    m_pendingRender.editor = ed;
    m_pendingRender.blockRawHtml = blockRawHtml;
    m_pendingRender.stripScripts = stripScripts;
    m_pendingRender.tabSwitch = tabSwitch;

    if (!m_renderThread) {
        // Create the worker lazily on first use; stopped in closeEvent().
        m_renderThread = new QThread(this);
        m_renderThread->setObjectName(QStringLiteral("preview-render"));
        m_renderWorker = new PreviewRenderWorker;
        m_renderWorker->moveToThread(m_renderThread);
        connect(m_renderWorker, &PreviewRenderWorker::finished,
                this, &MainWindow::onPreviewRenderReady, Qt::QueuedConnection);
        m_renderThread->start();
    }

    const QString text = ed->toPlainText();
    PreviewRenderWorker *worker = m_renderWorker;
    // Queued functor invocation: renders on the worker thread; the snapshot is
    // taken here on the GUI thread so a later edit cannot tear it mid-render.
    QMetaObject::invokeMethod(worker, [generation, text, blockRawHtml, stripScripts, worker]() {
        worker->render(generation, text, blockRawHtml, stripScripts);
    }, Qt::QueuedConnection);
}

void MainWindow::onPreviewRenderReady(quint64 generation, const QString &html)
{
    if (generation != m_renderGeneration)
        return; // superseded by a newer edit/render or a tab switch
    Editor *ed = currentEditor();
    if (!ed || ed != m_pendingRender.editor)
        return; // the requesting tab is no longer current; a newer render is pending
    const bool tabSwitch = m_pendingRender.tabSwitch;

    // Cache the render on the requesting tab so a later re-visit is served
    // without re-parsing.
    for (TabInfo &tab : m_tabs) {
        if (tab.editor == m_pendingRender.editor) {
            tab.previewHtml = html;
            tab.previewBlockRaw = m_pendingRender.blockRawHtml;
            tab.previewStripScripts = m_pendingRender.stripScripts;
            tab.previewHtmlValid = true;
            break;
        }
    }

    commitPreviewHtml(html, tabSwitch, activeTabInfo());
}

// The tail shared by the synchronous and background preview paths: computes
// the CSS environment and base URL for the current tab and pushes `html` to
// the page — a full shell load when the preview isn't initialized yet, a
// scribaUpdate call otherwise.
void MainWindow::commitPreviewHtml(const QString &html, bool tabSwitch, const TabInfo *info)
{
    const PreviewEnviron env = computePreviewCssAndEnviron();

    const QUrl baseUrl = computePreviewBaseUrl(info);

    QSettings prefs;
    QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();
    bool cspEnabled = prefs.value(Preferences::EnableCspPreview, true).toBool();
    if (!m_previewInitialized) {
        m_cachedPreviewBaseCss = env.baseCss;
        int heavyRenderDelay = prefs.value(Preferences::HeavyRenderDelay,
            Preferences::DefaultHeavyRenderDelay).toInt();
        QString renderCss = CssUtils::renderOverlayCss(env.rawThemeCss);
        m_cachedOverlayCss = renderCss;
        bool striping = prefs.value(Preferences::TableStriping, true).toBool();
        QString stripeInit = striping ? QString()
            : QLatin1String(Preferences::TableStripeCss);
        bool showCodeLang = prefs.value(Preferences::ShowCodeLangPreview, true).toBool();
        QString codeLangInit = showCodeLang ? QString()
            : QLatin1String(Preferences::HideCodeLangCss);
        QString centerCss = env.printLayoutCss;
        if (!m_printLayoutMode && m_previewState == 3) {
            bool centre = prefs.value(Preferences::CentreSingleViewContent, true).toBool();
            int centreWidth = prefs.value(Preferences::CentreSingleViewWidth, 800).toInt();
            if (centre)
                centerCss = QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth);
        }
        QString splitCss;
        if (!m_printLayoutMode && (m_previewState == 1 || m_previewState == 2)) {
            int splitWidth = prefs.value(Preferences::SplitViewPreviewMaxWidth, 0).toInt();
            splitCss = CssUtils::splitViewMaxWidthCss(splitWidth);
        }
        QString fullHtml = buildPreviewShellHtml(heavyRenderDelay, env.mermaidTheme,
            emojiMode, env.baseCss, env.previewCss, html, stripeInit, centerCss,
            splitCss, codeLangInit, renderCss);
        if (m_printLayoutMode) {
            // The paginator runs from the preview script's own render tails
            // (see PreviewPagination), so the page only needs the paginator
            // script + the print option overrides for this geometry.
            const QString printOptionsCss = PrintOptions::buildCss(env.printOpts);
            int headEnd = fullHtml.indexOf("</head>");
            if (headEnd >= 0)
                fullHtml.insert(headEnd,
                    QStringLiteral("<style id=\"print-options-css\">%1</style>").arg(printOptionsCss));
            int bodyEnd = fullHtml.indexOf("</body>");
            if (bodyEnd >= 0)
                fullHtml.insert(bodyEnd,
                    PreviewPagination::paginatorScript(env.printOpts, env.printContentHpx));
        }
        if (cspEnabled) {
            int headEnd = fullHtml.indexOf("</head>");
            if (headEnd >= 0)
                fullHtml.insert(headEnd, QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">").arg(Security::CspHeader));
        }
        m_preview->setHtmlWithOverlay(fullHtml, baseUrl);
    } else {
        QString js = buildUpdateCallJavascript(html, env.cssChanged, env.previewCss,
            env.mermaidTheme, emojiMode, baseUrl, tabSwitch);
        m_preview->page()->runJavaScript(js, [this, tabSwitch](const QVariant &result) {
            if (!result.toBool())
                return;
            // No immediate re-sync here: the page's own anchored restoreScroll
            // re-docks the preview when the user hasn't scrolled it manually,
            // and a C++ re-assert would yank a user preview scroll back to the
            // editor's line (pinned by AsyncContentUpdateDoesNotYankPreviewScroll).
            m_lastSyncLine = -1.0;
            if (tabSwitch) {
                QTimer::singleShot(450, this, [this] {  // index builds only after the heavy pass; JS restore skips tabSwitch
                    if (!m_previewInitialized) return;
                    m_lastSyncLine = -1.0;
                    syncPreviewScroll();
                });
            }
        });
    }
}

void MainWindow::stopPreviewRenderWorker()
{
    if (!m_renderThread)
        return;
    QThread *thread = m_renderThread;
    PreviewRenderWorker *worker = m_renderWorker;
    m_renderThread = nullptr;
    m_renderWorker = nullptr;
    ++m_renderGeneration; // invalidate any queued result delivery
    // Quit and join: any render already in flight (a bounded md→HTML pass)
    // completes, then the worker and thread are reaped. Queued functor events
    // that never ran are discarded when their worker is deleted.
    thread->quit();
    thread->wait();
    delete worker;
    delete thread;
}

MainWindow::PreviewEnviron MainWindow::computePreviewCssAndEnviron()
{
    QSettings prefs;
    QString rawThemeCss = m_cssLoader->themeCss();
    QString baseCss = m_cssLoader->previewBaseCss();
    int uiFontSize = prefs.value(Preferences::UiFontSize, Preferences::DefaultUiFontSize).toInt();
    QString chromeCss = CssUtils::deriveChromeCss(rawThemeCss, uiFontSize);
    QString previewCss = chromeCss + rawThemeCss;
    QString mermaidTheme = CssUtils::isDarkTheme(rawThemeCss)
        ? QStringLiteral("dark") : QStringLiteral("default");

    // Print layout: rebuild the preview as an exact page-by-page pagination of
    // the PDF export. Geometry comes from the same merged CSS + print options
    // the export path uses, so the on-screen page boxes match the printout.
    const PrintOptions::Options printOpts = m_printLayoutMode
        ? PrintOptions::fromSettings() : PrintOptions::Options();
    QString printLayoutCss;
    int printContentHpx = 0;
    if (m_printLayoutMode) {
        QString merged = m_cssLoader->printCss()
            + QStringLiteral("\n") + PrintOptions::buildCss(printOpts)
            + QStringLiteral("\n") + PrintOptions::buildPageOverrideCss(printOpts);
        const QSizeF pagePt = PrintOptions::parsePageSize(merged);
        const QMarginsF marginsPt = PrintOptions::parsePageMargins(merged);
        const double px = 96.0 / 72.0;
        const int contentW = qMax(160, qRound((pagePt.width() - marginsPt.left() - marginsPt.right()) * px));
        printContentHpx = qMax(160, qRound((pagePt.height() - marginsPt.top() - marginsPt.bottom()) * px));
        printLayoutCss = PreviewPagination::layoutCss(
            contentW, printContentHpx,
            qRound(marginsPt.top() * px), qRound(marginsPt.left() * px), qRound(marginsPt.bottom() * px));
        if (merged != m_printLayoutFp) {
            // Page geometry/options changed: force a full rebuild so the new
            // page box and paginator take effect.
            m_printLayoutFp = merged;
            m_previewInitialized = false;
        }
        baseCss = m_cssLoader->printCss();
        previewCss = QString();
        mermaidTheme = QStringLiteral("default");
    }

    bool cssChanged = (previewCss != m_cachedPreviewCss);
    if (cssChanged) {
        m_cachedPreviewCss = previewCss;
    }

    PreviewEnviron env;
    env.rawThemeCss = rawThemeCss;
    env.baseCss = baseCss;
    env.previewCss = previewCss;
    env.mermaidTheme = mermaidTheme;
    env.printOpts = printOpts;
    env.printLayoutCss = printLayoutCss;
    env.printContentHpx = printContentHpx;
    env.cssChanged = cssChanged;
    return env;
}

QUrl MainWindow::computePreviewBaseUrl(const TabInfo *info) const
{
    QUrl baseUrl;
    QString docPath = info ? info->filePath : QString();
    if (!docPath.isEmpty()) {
        baseUrl = QUrl::fromLocalFile(QFileInfo(docPath).absolutePath() + "/");
    }
    // Untitled documents in an open corpus default to the corpus root as their
    // base dir (relative images resolve against the corpus). Saving the document
    // moves the base to the saved file's directory (see saveFile's re-render).
    if (baseUrl.isEmpty() && !m_corpus.filePath.isEmpty())
        baseUrl = QUrl::fromLocalFile(m_corpus.rootDir() + "/");
    return baseUrl;
}

QString MainWindow::buildPreviewShellHtml(int heavyRenderDelay, const QString &mermaidTheme,
                                          const QString &emojiMode, const QString &baseCss,
                                          const QString &previewCss, const QString &html,
                                          const QString &stripeInit, const QString &centerCss,
                                          const QString &splitCss, const QString &codeLangInit,
                                          const QString &renderCss) const
{
    QString script = readResourceFile(":/preview-script.js");
    script.replace("{{MERMAID_THEME}}", mermaidTheme);
    script.replace("{{EMOJI_MODE}}", emojiMode);

    QString headScript = QStringLiteral("window._scribaHeavyDelay=%1;").arg(heavyRenderDelay)
        + mermaidInitJs + headingIdJs + anchorNavJs + katexInitJs + echartsInitJs
        + setImgTitlesJs + setFootnoteTitlesJs + imageOverlayJs + chartEditJs + script;

    QString shell = readResourceFile(":/preview-shell.html");
    shell.replace("/* SCRIBA_SCRIPT */", headScript);
    shell.replace("{{BASE_CSS}}", baseCss);
    shell.replace("{{THEME_CSS}}", previewCss);
    shell.replace("{{STRIPE_CSS}}", stripeInit);
    shell.replace("{{CENTER_CSS}}", centerCss);
    shell.replace("{{SPLIT_CSS}}", splitCss);
    shell.replace("{{CODE_LANG_CSS}}", codeLangInit);
    shell.replace("{{RENDER_CSS}}", renderCss);
    shell.replace("{{CONTENT}}", html);
    return shell;
}

QString MainWindow::buildUpdateCallJavascript(const QString &html, bool cssChanged,
                                              const QString &previewCss,
                                              const QString &mermaidTheme,
                                              const QString &emojiMode,
                                              const QUrl &baseUrl, bool tabSwitch) const
{
    QSettings prefs;
    QString escapedHtml = escapeJsString(html);
    QString escapedCss = cssChanged ? escapeJsString(previewCss) : QString();
    int delay = -1;
    if (tabSwitch) {
        int heavyDelay = prefs.value(Preferences::HeavyRenderDelay,
            Preferences::DefaultHeavyRenderDelay).toInt();
        delay = std::min(heavyDelay, Debounce::TabSwitchRender);
    }
    // Re-assert the active document's base URL on EVERY render, not just
    // tab switches. Relative image paths resolve against the shared page's
    // <base> element; with an empty baseUrl scribaUpdate removes it, making
    // images fall back to the stale page URL from the last full load.
    QString escapedBaseUrl;
    if (!baseUrl.isEmpty())
        escapedBaseUrl = escapeJsString(baseUrl.toString());
    QString js = QString("scribaUpdate('%1','%2','%3','%4',%5,'%6',%7)")
        .arg(escapedHtml, escapedCss, mermaidTheme, emojiMode,
             QString::number(delay), escapedBaseUrl,
             tabSwitch ? QStringLiteral("true") : QStringLiteral("false"));
    return js;
}

void MainWindow::syncPreviewScroll()
{
    if (!m_previewInitialized)
        return;
    QSettings settings;
    if (!settings.value(Preferences::SyncScroll, true).toBool())
        return;
    Editor *ed = currentEditor();
    if (!ed) return;
    const double line = currentEditorTopSourceLine();
    if (qAbs(line - m_lastSyncLine) < 1e-6)
        return;
    m_lastSyncLine = line;
    m_preview->scrollToSourceLine(line);
}

double MainWindow::currentEditorTopSourceLine()
{
    Editor *ed = currentEditor();
    if (!ed) return 1.0;
    QTextCursor c = ed->cursorForPosition(QPoint(ed->viewport()->width() / 2, 1));
    const int blockLen = c.block().length();
    return c.blockNumber() + 1
        + (blockLen > 0 ? static_cast<double>(c.positionInBlock()) / blockLen : 0.0);
}

void MainWindow::scrollPreviewToAnchor(const QString &anchor)
{
    m_pendingAnchor = anchor;
    m_anchorTries = 0;
    m_anchorTimer->start();
}

void MainWindow::tryScrollPreviewToAnchor()
{
    if (m_pendingAnchor.isEmpty())
        return;
    const QString js = QStringLiteral("scribaScrollToSlug('%1')")
        .arg(escapeJsString(m_pendingAnchor));
    m_preview->page()->runJavaScript(js, [this](const QVariant &result) {
        if (result.toBool()) {
            m_anchorTimer->stop();
            m_pendingAnchor.clear();
        }
    });
    // Keep ticking even if the runJavaScript callback never fires: a
    // navigation that replaces the page mid-flight can drop the pending
    // callback, and a chain that only re-arms from the callback would die
    // on the first lost tick. Cap the budget at ~18s (60 x 300ms ticks) so
    // a genuinely-missing anchor still gives up.
    if (++m_anchorTries > 60) {
        m_pendingAnchor.clear();
        m_anchorTimer->stop();
        return;
    }
    m_anchorTimer->start();
}

void MainWindow::syncCssWatcher()
{
    QStringList watched = m_cssWatcher->files();
    if (!watched.isEmpty())
        m_cssWatcher->removePaths(watched);

    QString active = m_cssConfig->activeStylesheet();
    if (!active.isEmpty() && QFile::exists(active))
        m_cssWatcher->addPath(active);
}

void MainWindow::onCssFileChanged()
{
    m_cssLoader->invalidateCache();
    refreshPreviewCss();
    syncCssWatcher();
}

void MainWindow::onEditorScroll()
{
    syncPreviewScroll();
}

void MainWindow::setPreviewState(int state)
{
    if (state < 0 || state > 3)
        return;
    m_previewState = state;
    QSettings().setValue(Preferences::PreviewState, state);
    syncPreviewLayout();

    QSettings settings;
    bool centre = settings.value(Preferences::CentreSingleViewContent, true).toBool();
    int centreWidth = settings.value(Preferences::CentreSingleViewWidth, 800).toInt();

    Editor *ed = currentEditor();

    if (m_previewState == 0) {
        m_preview->setVisible(false);
        m_editorStack->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
    } else if (m_previewState == 3) {
        m_editorStack->setVisible(false);
        m_preview->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
        if (m_previewInitialized && !m_printLayoutMode) {
            QString css = centre
                ? QString("body{margin:0 auto!important;max-width:%1px!important}").arg(centreWidth)
                : QString();
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent='%1'").arg(css));
        }
    } else if (m_previewState == 1) {
        m_splitter->insertWidget(0, m_editorStack);
        m_splitter->insertWidget(1, m_preview);
        m_editorStack->setVisible(true);
        m_preview->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
        if (m_previewInitialized && !m_printLayoutMode)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
    } else {
        m_splitter->insertWidget(0, m_preview);
        m_splitter->insertWidget(1, m_editorStack);
        m_preview->setVisible(true);
        m_editorStack->setVisible(true);
        if (ed) applyEditorContentWidth(ed);
        applyPreviewSplitWidth();
        if (m_previewInitialized && !m_printLayoutMode)
            m_preview->page()->runJavaScript(
                QStringLiteral("document.getElementById('center-css').textContent=''"));
    }

    int w = m_splitter->width();
    if (w <= 0) w = 1200;
    m_splitter->setSizes({w / 2, w / 2});
}

void MainWindow::togglePreview()
{
    setPreviewState((m_previewState + 1) % 4);
}

void MainWindow::syncPreviewLayout()
{
    if (!m_layoutActions)
        return;
    for (QAction *a : m_layoutActions->actions())
        a->setChecked(a->data().toInt() == m_previewState);
}

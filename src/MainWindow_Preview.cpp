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
#include "Editor.h"
#include "Preview.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "CssUtils.h"
#include "JsRenderEngine.h"
#include "JsSnippets.h"
#include "MarkdownParser.h"
#include "Preferences.h"
#include "PreviewPagination.h"
#include "PrintOptions.h"
#include "StaticHelpers.h"
#include "ValidationReport.h"
#include <QApplication>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
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
    } else {
        html = m_parser->toHtml(ed->toPlainText(), blockRawHtml);
        if (stripScripts)
            html = JsRenderEngine::stripScriptTags(html);
        if (info) {
            info->previewHtml = html;
            info->previewBlockRaw = blockRawHtml;
            info->previewStripScripts = stripScripts;
            info->previewHtmlValid = true;
        }
    }

    const PreviewEnviron env = computePreviewCssAndEnviron();

    const QUrl baseUrl = computePreviewBaseUrl(info);

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
            // Re-paginate after each render pass (see PreviewPagination) and
            // embed the print option overrides + paginator for this geometry.
            fullHtml = PreviewPagination::patchIncrementalPaginate(fullHtml);
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
        m_preview->page()->runJavaScript(js, [this](const QVariant &result) {
            if (result.toBool())
                syncPreviewScroll();
        });
    }
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
    // A Table-of-Contents tab is unbacked, but its relative links resolve
    // against the corpus root; mirror the per-file base for it.
    const int curIdx = m_tabBar->currentIndex();
    if (m_tocTabs.contains(curIdx))
        baseUrl = QUrl::fromLocalFile(m_corpus.rootDir() + "/");
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
    QString js = QString("scribaUpdate('%1','%2','%3','%4',%5,'%6')")
        .arg(escapedHtml, escapedCss, mermaidTheme, emojiMode,
             QString::number(delay), escapedBaseUrl);
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
    auto *sb = ed->verticalScrollBar();
    double range = sb->maximum() - sb->minimum();
    double pct = range > 0 ? static_cast<double>(sb->value() - sb->minimum()) / range : 0.0;
    m_preview->scrollToPercent(pct);
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
        // Give up after ~6s; the ids appear after the heavy render pass, so
        // retries normally succeed on the second or third tick.
        if (result.toBool() || ++m_anchorTries > 20) {
            m_anchorTimer->stop();
            m_pendingAnchor.clear();
        } else {
            m_anchorTimer->start();
        }
    });
}

void MainWindow::refreshPreviewForTocTab(int index, const QString &rootDir)
{
    Q_UNUSED(rootDir);
    if (index < 0 || index >= m_tabs.size())
        return;
    // Force a re-parse so the (unchanged) cached previewHtml can't serve
    // stale link resolution; updatePreview derives the <base> from m_tocTabs.
    m_tabs[index].previewHtmlValid = false;
    if (m_tabBar->currentIndex() == index)
        updatePreview(true);
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

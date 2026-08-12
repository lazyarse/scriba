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
#include "Corpus.h"
#include "CorpusWatcher.h"
#include "CssLoader.h"
#include "CssUtils.h"
#include "DocxExporter.h"
#include "DocxImporter.h"
#include "ExportDocxDialog.h"
#include "ExportHtmlDialog.h"
#include "ExportPdfDialog.h"
#include "HtmlToMarkdown.h"
#include "LinkFixer.h"
#include "MarkdownParser.h"
#include "PdfImporter.h"
#include "PdfRenderer.h"
#include "Preferences.h"
#include "PrintOptions.h"
#include "StaticHelpers.h"
#include "ValidationReport.h"
#include <QClipboard>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QStatusBar>
#include <QTextDocument>
#include <QTimer>

static constexpr const char *kMdFilter = "Markdown Files (*.md);;All Files (*)";
static constexpr const char *kOpenMdFilter = "Markdown Files (*.md *.markdown *.txt);;All Files (*)";
static constexpr int kMsPerMinute = 60000;

// Appends a default suffix to a save-dialog result if the chosen path has no
// extension at all; any existing suffix (e.g. ".txt") is respected as typed.
QString MainWindow::ensureDefaultSuffix(const QString &path, const char *suffix)
{
    if (path.isEmpty() || !QFileInfo(path).suffix().isEmpty())
        return path;
    return path + QLatin1Char('.') + QLatin1String(suffix);
}


void MainWindow::loadFile(const QString &filePath, bool forceReload)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filePath);
        return;
    }

    int existing = findTabByPath(filePath);
    if (!forceReload && existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
        file.close();
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    TabInfo *info = nullptr;
    int idx;
    bool replacedUntitled = false;

    if (forceReload && existing >= 0) {
        idx = existing;
        m_tabBar->setCurrentIndex(idx);
        m_tabs[idx].previewHtmlValid = false;
        QSignalBlocker blocker(m_tabs[idx].editor);
        m_tabs[idx].editor->setPlainText(content);
        {
            QSettings s;
            QTextBlockFormat fmt;
            fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                              QTextBlockFormat::ProportionalHeight);
            QTextCursor cursor(m_tabs[idx].editor->document());
            cursor.select(QTextCursor::Document);
            cursor.mergeBlockFormat(fmt);
        }
        m_tabs[idx].dirty = false;
        updateTabLabel(idx);
        info = &m_tabs[idx];
    } else {
        idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size() && m_tabs[idx].filePath.isEmpty() && !m_tabs[idx].dirty && m_tabs[idx].editor->toPlainText().isEmpty()) {
            m_tabs[idx].filePath = filePath;
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(content);
            m_tabs[idx].previewHtmlValid = false;
            {
                QSettings s;
                QTextBlockFormat fmt;
                fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                                  QTextBlockFormat::ProportionalHeight);
                QTextCursor cursor(m_tabs[idx].editor->document());
                cursor.select(QTextCursor::Document);
                cursor.mergeBlockFormat(fmt);
            }
            m_tabs[idx].editor->setCurrentFile(filePath);
            m_tabs[idx].dirty = false;
            info = &m_tabs[idx];
            updateTabLabel(idx);
            m_tabBar->setTabToolTip(idx, filePath);
            replacedUntitled = true;
        } else {
            idx = addTab(filePath);
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(content);
            m_tabs[idx].previewHtmlValid = false;
            {
                QSettings s;
                QTextBlockFormat fmt;
                fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                                  QTextBlockFormat::ProportionalHeight);
                QTextCursor cursor(m_tabs[idx].editor->document());
                cursor.select(QTextCursor::Document);
                cursor.mergeBlockFormat(fmt);
            }
            m_tabs[idx].dirty = false;
            updateTabLabel(idx);
            info = &m_tabs[idx];
        }
    }

    updateWindowTitle();
    m_preview->setDocumentPath(filePath);
    m_previewInitialized = false;
    updatePreview();
}

void MainWindow::saveFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file: " + filePath);
        return;
    }

    Editor *ed = currentEditor();
    if (!ed) return;

    file.write(ed->toPlainText().toUtf8());
    file.close();

    TabInfo *info = activeTabInfo();
    if (!info) return;

    const bool wasUntitled = info->filePath.isEmpty();
    bool pathChanged = (info->filePath != filePath);
    // A new directory changes how relative image paths resolve (they hang off
    // the shared page's <base> element), so re-render to move the base over.
    QString oldDir = QFileInfo(info->filePath).absolutePath();
    info->filePath = filePath;
    ed->setCurrentFile(filePath);
    info->dirty = false;

    int idx = m_tabBar->currentIndex();
    updateTabLabel(idx);
    m_tabBar->setTabToolTip(idx, filePath);

    updateWindowTitle();
    m_preview->setDocumentPath(filePath);
    statusBar()->showMessage("Saved", 2000);

    if (pathChanged) {
        // Saving an untitled tab always moves the base (corpus root or none →
        // the saved file's directory), even when it lands in the same directory
        // that the empty-path sentinel happens to resolve to.
        if (wasUntitled
            || (QFileInfo(filePath).absolutePath() != oldDir && m_previewInitialized))
            updatePreview();
    }
}

void MainWindow::renameCurrentFile()
{
    TabInfo *info = activeTabInfo();
    if (!info)
        return;

    if (info->filePath.isEmpty()) {
        QMessageBox::information(this, "Rename",
            "Save the file first; unsaved tabs cannot be renamed.");
        return;
    }

    const QFileInfo finfo(info->filePath);
    // Use a file dialog so the user can pick a new name/location like a save.
    // DontConfirmOverwrite defers the existing-file check to our own warning
    // (a rename must never silently replace another file).
    QString newPath = QFileDialog::getSaveFileName(
        this, "Rename File", info->filePath, QString::fromLatin1(kMdFilter),
        nullptr, QFileDialog::DontConfirmOverwrite);
    if (newPath.isEmpty())
        return;

    if (newPath == info->filePath)
        return;

    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, "Rename",
            "Cannot rename: a file with that name already exists.");
        return;
    }

    if (!QFile::rename(info->filePath, newPath)) {
        QMessageBox::warning(this, "Rename", "Could not rename file: " + info->filePath);
        return;
    }

    Editor *ed = currentEditor();
    if (!ed)
        return;
    QString oldDir = QFileInfo(info->filePath).absolutePath();
    const QString oldAbs = QFileInfo(info->filePath).absoluteFilePath();
    info->filePath = newPath;
    ed->setCurrentFile(newPath);

    int idx = m_tabBar->currentIndex();
    updateTabLabel(idx);
    m_tabBar->setTabToolTip(idx, newPath);

    updateWindowTitle();
    m_preview->setDocumentPath(newPath);
    statusBar()->showMessage("Renamed to " + newPath, 2000);

    // The base directory changed: re-render so relative images (which resolve
    // against the shared page's <base> element) follow the new location.
    if (QFileInfo(newPath).absolutePath() != oldDir && m_previewInitialized)
        updatePreview();

    // Update any corpus links that pointed at the old path.
    rewriteLinksForFile(oldAbs, newPath);
}

void MainWindow::autoSave()
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        TabInfo &info = m_tabs[i];
        if (info.filePath.isEmpty())
            continue;
        QFile file(info.filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(info.editor->toPlainText().toUtf8());
            info.dirty = false;
            updateTabLabel(i);
        }
    }
}

void MainWindow::importHtmlFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import HTML File", QString(), "HTML Files (*.html *.htm);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import HTML", "Could not open file: " + path);
        return;
    }

    const QString markdown = HtmlToMarkdown::convert(
        QString::fromUtf8(file.readAll()), QUrl::fromLocalFile(path));
    file.close();

    if (markdown.isEmpty()) {
        QMessageBox::warning(this, "Import HTML",
            "No convertible content was found in the file.");
        return;
    }

    openImportedTab(markdown);
    statusBar()->showMessage("Imported " + QFileInfo(path).fileName(), 3000);
}

// Opens a Word (.docx) package, converts the body OOXML to Markdown and loads
// it into a new tab. Embedded images are written according to the Import
// preference (next to the document, a configured folder, system temp, or ask).
void MainWindow::importDocxFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Word Document", QString(), "Word Documents (*.docx);;All Files (*)");
    if (path.isEmpty())
        return;

    QSettings s;
    DocxImportOptions opts;
    const QString loc = s.value(Preferences::ImportImageLocation,
        QStringLiteral("currentDir")).toString();
    if (loc == QLatin1String("customDir"))
        opts.imageLocation = DocxImportOptions::ImageLocation::CustomDir;
    else if (loc == QLatin1String("tempDir"))
        opts.imageLocation = DocxImportOptions::ImageLocation::TempDir;
    else if (loc == QLatin1String("ask"))
        opts.imageLocation = DocxImportOptions::ImageLocation::Ask;
    else
        opts.imageLocation = DocxImportOptions::ImageLocation::CurrentDir;
    opts.customImageDir = s.value(Preferences::ImportImageDir).toString();
    opts.documentDir = QFileInfo(path).absolutePath();

    DocxImportResult result = DocxImporter::import(path, opts);
    if (!result.ok) {
        QMessageBox::warning(this, "Import Word Document", result.error);
        return;
    }

    openImportedTab(result.markdown);

    if (!result.warnings.isEmpty()) {
        statusBar()->showMessage("Imported " + QFileInfo(path).fileName()
                                 + " (" + QString::number(result.warnings.size())
                                 + " warnings)", 5000);
        QMessageBox::information(this, "Import Word Document",
                                 result.warnings.join('\n'));
    } else {
        statusBar()->showMessage("Imported " + QFileInfo(path).fileName(), 3000);
    }
}

// Opens a PDF file, extracts its text via pdf.js and heuristically converts
// it to Markdown in a new tab. Ghost images (pure scans) yield a warning.
void MainWindow::importPdfFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Import PDF Document", QString(), "PDF Files (*.pdf);;All Files (*)");
    if (path.isEmpty())
        return;

    PdfImportResult result = PdfImporter::convert(path);
    if (!result.ok) {
        QMessageBox::warning(this, "Import PDF Document", result.error);
        return;
    }

    openImportedTab(result.markdown);

    statusBar()->showMessage("Imported " + QFileInfo(path).fileName()
                             + " (" + QString::number(result.pages) + " pages)", 5000);
}

void MainWindow::openImportedTab(const QString &markdown)
{
    QSettings s;
    int idx = addTab();
    QSignalBlocker blocker(m_tabs[idx].editor);
    m_tabs[idx].editor->setPlainText(markdown);
    m_tabs[idx].previewHtmlValid = false;
    QTextBlockFormat fmt;
    fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                      QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(m_tabs[idx].editor->document());
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
    m_tabs[idx].dirty = true;
    updateTabLabel(idx);
    m_previewInitialized = false;
    updatePreview();
}

void MainWindow::pasteAsMarkdown()
{
    Editor *ed = currentEditor();
    if (!ed) return;

    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    QString text;
    if (mime && mime->hasHtml()) {
        text = HtmlToMarkdown::convert(mime->html());
        if (text.isEmpty())
            return;
    } else if (mime && mime->hasText()) {
        text = mime->text();
    } else {
        return;
    }

    ed->textCursor().insertText(text);
    // The converted text is inserted directly (bypassing the editor's
    // insertFromMimeData), so a table in it never marks the paste-time dirty
    // flag; align it right away. No-op for non-table content.
    ed->formatTableAt(ed->textCursor().position());
}

ExportPreamble MainWindow::currentExportHtml()
{
    ExportPreamble pre;
    pre.ed = currentEditor();
    if (!pre.ed) return pre;
    pre.info = activeTabInfo();
    if (!pre.info) return pre;
    QSettings s;
    pre.html = m_parser->toHtml(pre.ed->toPlainText(),
        s.value(Preferences::BlockRawHtmlExport, true).toBool());
    pre.ok = true;
    return pre;
}

void MainWindow::exportPdf()
{
    ExportPreamble pre = currentExportHtml();
    if (!pre.ok) return;

    QSettings prefs;
    QString html = pre.html;
    if (prefs.value(Preferences::StripExportScripts, true).toBool())
        html = JsRenderEngine::stripScriptTags(html);

    ExportPdfDialog dlg(html, pre.info->filePath, m_cssLoader, this);
    dlg.exec();
}

void MainWindow::exportDocx()
{
    ExportPreamble pre = currentExportHtml();
    if (!pre.ok) return;
    TabInfo *info = pre.info;

    // Show export dialog to get math mode preference
    ExportDocxDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    DocxMathMode mathMode = dlg.selectedMathMode();
    DocxExportOptions opts;
    opts.mathMode = mathMode;
    opts.landscape = dlg.isLandscape();
    opts.marginTopCm = dlg.marginTop();
    opts.marginBottomCm = dlg.marginBottom();
    opts.marginLeftCm = dlg.marginLeft();
    opts.marginRightCm = dlg.marginRight();
    opts.pageNumbers = dlg.hasPageNumbers();

    QSettings prefs;
    QString html = pre.html;
    if (prefs.value(Preferences::StripExportScripts, true).toBool())
        html = JsRenderEngine::stripScriptTags(html);
    QString css = m_cssLoader->previewBaseCss() + "\n" + m_cssLoader->themeCss();
    if (!prefs.value(Preferences::ShowCodeLangExport, true).toBool())
        css += QStringLiteral("\n") + QLatin1String(Preferences::HideCodeLangCss);

    QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    QString mermaidTheme = CssUtils::isDarkTheme(m_cssLoader->themeCss())
        ? QStringLiteral("dark") : QStringLiteral("default");

    QUrl baseUrl;
    if (!info->filePath.isEmpty())
        baseUrl = QUrl::fromLocalFile(QFileInfo(info->filePath).absolutePath() + "/");

    // Use image mode: render KaTeX as PNG images in WebEngine
    // Use OMML mode: keep KaTeX HTML for conversion in HtmlToOoxml
    QString fullHtml;
    if (mathMode == DocxMathMode::Images) {
        fullHtml = JsRenderEngine::buildFullHtmlForDocx(html, css, emojiMode, mermaidTheme);
    } else {
        fullHtml = JsRenderEngine::buildFullHtmlForDocxOmml(html, css, emojiMode, mermaidTheme);
    }
    if (prefs.value(Preferences::EnableCspExport, true).toBool()) {
        int headEnd = fullHtml.indexOf("</head>");
        if (headEnd >= 0)
            fullHtml.insert(headEnd, QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">").arg(Security::CspHeader));
    }
    QString renderedHtml = JsRenderEngine::renderSync(fullHtml, baseUrl.toString());

    if (renderedHtml.isEmpty()) {
        showCenteredWarning("Export Failed",
            "Could not render the document for DOCX export.",
            "The JavaScript rendering step timed out or failed.");
        return;
    }

    renderedHtml = JsRenderEngine::replaceQrcUrls(renderedHtml);
    renderedHtml = JsRenderEngine::embedImages(renderedHtml, baseUrl);
    renderedHtml = JsRenderEngine::embedResources(renderedHtml, ScriptHandling::Strip);

    // Include KaTeX CSS so HtmlToOoxml can resolve font metrics for math spans
    QString katexCss = JsRenderEngine::katexCss();
    QString docxCss = css;
    if (!katexCss.isEmpty())
        docxCss += QStringLiteral("\n") + katexCss;

    QString defaultName = info->filePath.isEmpty()
        ? "document.docx"
        : QFileInfo(info->filePath).completeBaseName() + ".docx";

    QString path = QFileDialog::getSaveFileName(
        this, "Export as Word (DOCX)", defaultName, "Word Documents (*.docx)");
    if (path.isEmpty()) return;

    if (!DocxExporter::exportToDocx(renderedHtml, path, docxCss, opts)) {
        showCenteredWarning("Export Failed",
            "Could not export the document as DOCX.",
            "Check that the file is not open in another application and that the path is writable.");
    }
}

void MainWindow::exportHtml()
{
    ExportPreamble pre = currentExportHtml();
    if (!pre.ok) return;
    TabInfo *info = pre.info;

    ExportHtmlDialog dlg(m_cssConfig, m_cssLoader, info->filePath, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString themePath = dlg.selectedThemePath();
    if (themePath.isEmpty())
        return;

    // Load the selected theme CSS
    QFile themeFile(themePath);
    QString themeCss;
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text))
        themeCss = QString::fromUtf8(themeFile.readAll());

    QString baseCss = m_cssLoader->previewBaseCss();
    QString combinedCss = baseCss + "\n" + themeCss;

    QSettings prefs;
    QString bodyHtml = pre.html;

    QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    QString mermaidTheme = CssUtils::isDarkTheme(themeCss)
        ? QStringLiteral("dark") : QStringLiteral("default");

    QUrl baseUrl;
    if (!info->filePath.isEmpty())
        baseUrl = QUrl::fromLocalFile(QFileInfo(info->filePath).absolutePath() + "/");

    QString fullHtml = JsRenderEngine::buildFullHtml(bodyHtml, combinedCss, emojiMode, mermaidTheme);
    QString renderedBody = JsRenderEngine::renderSync(fullHtml, baseUrl.toString());

    if (renderedBody.isEmpty()) {
        showCenteredWarning("Export Failed",
            "Could not render the document for HTML export.",
            "The JavaScript rendering step timed out or failed.");
        return;
    }

    renderedBody = JsRenderEngine::replaceQrcUrls(renderedBody);
    renderedBody = JsRenderEngine::embedImages(renderedBody, baseUrl);
    renderedBody = JsRenderEngine::embedResources(renderedBody, dlg.selectedScriptHandling());

    // Strip #editor rule from theme CSS for the export (editor-only styling)
    QString exportCss = themeCss;
    exportCss.remove(QRegularExpression(R"(#editor\s*\{[^}]*\})"));
    exportCss = baseCss + "\n" + exportCss;

    // Responsive SVG rule for ECharts charts (baked-in SVG width needs to scale)
    exportCss += QStringLiteral(
        "\n.echarts-chart svg{max-width:100%;height:auto;width:auto!important}");

    if (!prefs.value(Preferences::ShowCodeLangExport, true).toBool())
        exportCss += QStringLiteral("\n") + QLatin1String(Preferences::HideCodeLangCss);

    // Include KaTeX CSS so math renders correctly without external dependencies
    QString katexCss = JsRenderEngine::katexCss();

    QString cspMeta;
    if (prefs.value(Preferences::EnableCspExport, true).toBool())
        cspMeta = QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">\n").arg(Security::CspHeader);

    QString output = QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"utf-8\">\n"
        "%4"
        "<style>%1</style>\n"
        "<style>%3</style>\n"
        "</head>\n"
        "<body>%2</body>\n"
        "</html>\n"
    ).arg(exportCss, renderedBody, katexCss, cspMeta);

    QString defaultName = info->filePath.isEmpty()
        ? "document.html"
        : QFileInfo(info->filePath).completeBaseName() + ".html";

    QString path = QFileDialog::getSaveFileName(
        this, "Export as HTML", defaultName, "HTML Files (*.html);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showCenteredWarning("Export Failed",
            "Could not write the HTML file.",
            "Check that the path is writable.");
        return;
    }

    f.write(output.toUtf8());
    f.close();
    statusBar()->showMessage("Exported as HTML", 2000);
}

void MainWindow::renderDocumentHtml(const QString &markdown, const QString &baseDir,
                                    bool omml, QString *body)
{
    QSettings prefs;
    const QString html = m_parser->toHtml(
        markdown, prefs.value(Preferences::BlockRawHtmlExport, true).toBool());
    const QString css = m_cssLoader->previewBaseCss() + "\n" + m_cssLoader->themeCss();
    const QString emojiMode = prefs.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();
    const QString mermaidTheme = CssUtils::isDarkTheme(m_cssLoader->themeCss())
        ? QStringLiteral("dark") : QStringLiteral("default");
    const QString baseUrl = QUrl::fromLocalFile(baseDir + "/").toString();

    QString fullHtml;
    if (omml)
        fullHtml = JsRenderEngine::buildFullHtmlForDocxOmml(html, css, emojiMode, mermaidTheme);
    else
        fullHtml = JsRenderEngine::buildFullHtml(html, css, emojiMode, mermaidTheme);
    if (prefs.value(Preferences::EnableCspExport, true).toBool()) {
        const int headEnd = fullHtml.indexOf("</head>");
        if (headEnd >= 0)
            fullHtml.insert(headEnd, QStringLiteral(
                "<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">")
                .arg(Security::CspHeader));
    }

    QString rendered = JsRenderEngine::renderSync(fullHtml, baseUrl);
    rendered = JsRenderEngine::replaceQrcUrls(rendered);
    rendered = JsRenderEngine::embedImages(rendered, QUrl(baseUrl));
    rendered = JsRenderEngine::embedResources(rendered, ScriptHandling::Strip);
    *body = rendered;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // A still-running validation grammar worker would keep a QThread alive
    // past the MainWindow it signals back to: stop and reap it first.
    stopValidationReport();

    QSettings s;

    bool autoSave = s.value(Preferences::AutoSaveOnExit, false).toBool();
    bool anyDirty = false;
    bool hasUntitledDirty = false;

    for (const TabInfo &info : m_tabs) {
        if (info.dirty) {
            anyDirty = true;
            if (info.filePath.isEmpty()) {
                hasUntitledDirty = true;
            }
        }
    }

    auto saveAllDirtyTabs = [this](QCloseEvent *event) {
        for (TabInfo &info : m_tabs) {
            if (!info.dirty) continue;
            if (info.filePath.isEmpty()) {
                QString file = saveAsDialogPath();
                if (file.isEmpty()) {
                    event->ignore();
                    return false;
                }
                info.filePath = file;
            }
            QFile file(info.filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox msgBox(this);
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.setWindowTitle("Save Failed");
                msgBox.setText(QString("Could not save \"%1\".\n%2")
                    .arg(info.filePath, file.errorString()));
                msgBox.setStandardButtons(QMessageBox::Ok);
                for (auto *btn : msgBox.buttons())
                    stripButtonIcon(btn);
                msgBox.exec();
                event->ignore();
                return false;
            }
            file.write(info.editor->toPlainText().toUtf8());
            info.dirty = false;
        }
        return true;
    };

    if (anyDirty && (!autoSave || hasUntitledDirty)) {
        ClosePromptResult ret = promptUnsavedChanges(hasUntitledDirty);
        if (ret == ClosePromptResult::Cancel) {
            event->ignore();
            return;
        }
        if (ret == ClosePromptResult::Save && !saveAllDirtyTabs(event)) {
            return;
        }
    } else if (autoSave) {
        for (TabInfo &info : m_tabs) {
            if (info.dirty && !info.filePath.isEmpty()) {
                QFile file(info.filePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.write(info.editor->toPlainText().toUtf8());
                    info.dirty = false;
                }
            }
        }
    }

    QJsonObject corpus = serializeCorpus();

    if (corpus["files"].toArray().isEmpty()) {
        s.remove(Preferences::OnExitCorpusData);
    } else {
        s.setValue(Preferences::OnExitCorpusData, QString::fromUtf8(QJsonDocument(corpus).toJson(QJsonDocument::Compact)));
    }

    QMainWindow::closeEvent(event);
}

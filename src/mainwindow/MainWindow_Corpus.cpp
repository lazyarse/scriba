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
#include "corpus/Corpus.h"
#include "corpus/CorpusFilesPanel.h"
#include "corpus/CorpusIndex.h"
#include "corpus/CorpusWatcher.h"
#include "css/CssLoader.h"
#include "io/DocxExporter.h"
#include "corpus/ExportCorpusDialog.h"
#include "preview/JsRenderEngine.h"
#include "corpus/LinkFixer.h"
#include "io/PdfRenderer.h"
#include "prefs/Preferences.h"
#include "preview/Preview.h"
#include "StaticHelpers.h"

#include <QDir>
#include <QDockWidget>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QStatusBar>
#include <QTextDocument>
#include <QTimer>

static QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s;
}

void MainWindow::exportCorpus()
{
    if (m_corpus.filePath.isEmpty()) {
        showCenteredWarning(tr("Export Corpus"), tr("No corpus is open."), QString());
        return;
    }
    ExportCorpusDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted || dlg.outputDir().isEmpty())
        return;

    const ExportCorpusDialog::Format format = dlg.format();
    const QString ext = format == ExportCorpusDialog::Format::Docx ? QStringLiteral("docx")
                      : format == ExportCorpusDialog::Format::Pdf  ? QStringLiteral("pdf")
                                                                   : QStringLiteral("html");

    QDir outDir(dlg.outputDir());
    if (!outDir.exists())
        QDir().mkpath(outDir.absolutePath());
    const QString root = m_corpus.rootDir();
    const QString themeCss = m_cssLoader->themeCss();
    QSettings prefs;

    // 1. Gather export items: source (abs path or embedded content) + output path.
    struct Item {
        QString absPath;      // source document absolute path ("" for embedded)
        bool embedded = false;
        QString content;      // embedded content
        QString outRel;       // output path relative to outDir ("" => skipped)
        bool inRoot = true;
    };
    QVector<Item> items;
    QHash<QString, QString> untitledByLabel;   // live text of open untitled tabs
    for (int i = 0; i < m_tabs.size(); ++i) {
        const TabInfo &ti = m_tabs[i];
        if (ti.filePath.isEmpty())
            untitledByLabel.insert(tabTitleForEmbedded(i), ti.editor->toPlainText());
    }
    int untitled = 0;
    for (const CorpusDocument &d : m_corpus.documents) {
        Item it;
        if (d.path.isEmpty()) {
            it.embedded = true;
            it.content = untitledByLabel.value(d.name, d.content);
            it.outRel = QStringLiteral("Untitled-%1.%2").arg(++untitled).arg(ext);
            items.append(it);
            continue;
        }
        const bool isAbs = QFileInfo(d.path).isAbsolute();
        const QString abs = Corpus::absolutePath(root, d.path);
        it.absPath = abs;
        it.inRoot = !isAbs || !QDir(root).relativeFilePath(abs).startsWith(QLatin1String(".."));
        items.append(it);
    }

    // 2. External (out-of-root) documents: mirror from their common ancestor
    //    into the named subfolder; skip when disabled.
    QStringList externalAbs;
    for (const Item &it : items) {
        if (!it.inRoot && !it.absPath.isEmpty())
            externalAbs.append(it.absPath);
    }
    const QString extName = dlg.exportExternal() ? dlg.externalDirName() : QString();
    QString commonExt;
    if (!externalAbs.isEmpty()) {
        commonExt = QFileInfo(externalAbs.first()).absolutePath();
        for (const QString &p : externalAbs) {
            QString dir = QFileInfo(p).absolutePath();
            while (!commonExt.isEmpty() && !dir.startsWith(commonExt)) {
                const int slash = commonExt.lastIndexOf(QLatin1Char('/'));
                if (slash <= 0) { commonExt.clear(); break; }
                commonExt = commonExt.left(slash);
            }
        }
    }
    int skippedExternal = 0;
    for (Item &it : items) {
        if (it.embedded)
            continue;                          // outRel already set
        if (it.inRoot) {
            it.outRel = QDir(root).relativeFilePath(it.absPath) + "." + ext;
        } else if (!extName.isEmpty() && !commonExt.isEmpty()) {
            it.outRel = QDir(extName).filePath(
                QDir(commonExt).relativeFilePath(it.absPath) + "." + ext);
        } else {
            it.outRel.clear();
            ++skippedExternal;
        }
    }

    // 3. Render each page.
    auto buildHtmlPage = [&](const QString &body) {
        QString exportCss = themeCss;
        exportCss.remove(QRegularExpression(R"(#editor\s*\{[^}]*\})"));
        exportCss = m_cssLoader->previewBaseCss() + "\n" + exportCss;
        exportCss += QStringLiteral(
            "\n.echarts-chart svg{max-width:100%;height:auto;width:auto!important}");
        if (!prefs.value(Preferences::ShowCodeLangExport, true).toBool())
            exportCss += QStringLiteral("\n") + QLatin1String(Preferences::HideCodeLangCss);
        const QString katexCss = JsRenderEngine::katexCss();
        QString cspMeta;
        if (prefs.value(Preferences::EnableCspExport, true).toBool())
            cspMeta = QStringLiteral(
                "<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">\n")
                .arg(Security::CspHeader);
        return QStringLiteral(
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "<meta charset=\"utf-8\">\n"
            "%4"
            "<style>%1</style>\n"
            "<style>%3</style>\n"
            "</head>\n"
            "<body>%2</body>\n"
            "</html>\n").arg(exportCss, body, katexCss, cspMeta);
    };

    QHash<QString, QString> pageLinks;         // abs path -> exported rel path
    QStringList embeddedOutRels;               // exported Untitled-N files
    int exported = 0;
    int missing = 0;
    for (const Item &it : items) {
        if (it.outRel.isEmpty())
            continue;
        QString markdown;
        QString baseDir;
        if (it.embedded) {
            markdown = it.content;
            baseDir = root;
        } else {
            QFile f(it.absPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                ++missing;
                continue;
            }
            markdown = QString::fromUtf8(f.readAll());
            f.close();
            baseDir = QFileInfo(it.absPath).absolutePath();
        }
        if (markdown.isEmpty())
            continue;

        QString body;
        renderDocumentHtml(markdown, baseDir, format == ExportCorpusDialog::Format::Docx, &body);
        if (body.isEmpty())
            continue;

        const QString finalPath = outDir.filePath(it.outRel);
        QDir().mkpath(QFileInfo(finalPath).absolutePath());
        bool ok = false;
        switch (format) {
        case ExportCorpusDialog::Format::Html: {
            QFile out(finalPath);
            if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                out.write(buildHtmlPage(body).toUtf8());
                out.close();
                ok = true;
            }
            break;
        }
        case ExportCorpusDialog::Format::Docx: {
            DocxExportOptions opts;
            opts.mathMode = DocxMathMode::Omml;
            const QString docxCss = m_cssLoader->previewBaseCss() + "\n" + themeCss
                + "\n" + JsRenderEngine::katexCss();
            ok = DocxExporter::exportToDocx(body, finalPath, docxCss, opts);
            break;
        }
        case ExportCorpusDialog::Format::Pdf: {
            ok = PdfRenderer::render(buildHtmlPage(body),
                                     QUrl::fromLocalFile(baseDir + "/").toString(),
                                     finalPath);
            break;
        }
        }
        if (ok) {
            ++exported;
            pageLinks.insert(it.absPath, it.outRel);
            if (it.embedded)
                embeddedOutRels.append(it.outRel);
        }
    }

    // 4. TOC/index page (all formats), linking the exported pages.
    QHash<QString, QString> indexLinks;
    const bool indexInFiles = format != ExportCorpusDialog::Format::Html;
    for (auto it = pageLinks.constBegin(); it != pageLinks.constEnd(); ++it)
        indexLinks.insert(it.key(), indexInFiles ? "../" + it.value() : it.value());
    QString indexMd = CorpusIndex::renderToc(m_corpus, indexLinks);
    // renderToc deliberately skips embedded (untitled) documents; they are still
    // exported as Untitled-N files, so append them so every exported page is
    // reachable from the index.
    if (!embeddedOutRels.isEmpty()) {
        indexMd += QStringLiteral("\n## Untitled documents\n");
        for (const QString &rel : embeddedOutRels)
            indexMd += QStringLiteral("- [%1](%2%1)\n").arg(rel, indexInFiles ? QStringLiteral("../") : QString());
    }

    QString indexPath;
    if (format == ExportCorpusDialog::Format::Html) {
        indexPath = outDir.filePath(QStringLiteral("index.html"));
        QDir().mkpath(QFileInfo(indexPath).absolutePath());
        QString body;
        renderDocumentHtml(indexMd, dlg.outputDir(), false, &body);
        if (!body.isEmpty()) {
            QFile out(indexPath);
            if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                out.write(buildHtmlPage(body).toUtf8());
                out.close();
            }
        }
    } else {
        const QString baseName = m_corpus.name.isEmpty()
            ? QFileInfo(m_corpus.filePath).completeBaseName() : m_corpus.name;
        const QString dir = outDir.filePath(QStringLiteral("files"));
        QDir().mkpath(dir);
        indexPath = dir + "/" + baseName + "-index." + ext;
        QString body;
        renderDocumentHtml(indexMd, dir, format == ExportCorpusDialog::Format::Docx, &body);
        if (!body.isEmpty()) {
            if (format == ExportCorpusDialog::Format::Docx) {
                DocxExportOptions opts;
                opts.mathMode = DocxMathMode::Omml;
                const QString docxCss = m_cssLoader->previewBaseCss() + "\n" + themeCss
                    + "\n" + JsRenderEngine::katexCss();
                DocxExporter::exportToDocx(body, indexPath, docxCss, opts);
            } else {
                PdfRenderer::render(buildHtmlPage(body),
                                    QUrl::fromLocalFile(dir + "/").toString(),
                                    indexPath);
            }
        }
    }

    // 5. Zip (optional).
    QString zipPath;
    if (dlg.compressToZip()) {
        QString baseName = m_corpus.name.isEmpty()
            ? QFileInfo(m_corpus.filePath).completeBaseName() : m_corpus.name;
        baseName.replace(QRegularExpression(R"([^A-Za-z0-9 _.-])"), QStringLiteral("_"));
        zipPath = outDir.filePath(baseName + ".zip");
        if (!createZipArchive(dlg.outputDir(), zipPath))
            showCenteredWarning(tr("Export Corpus"),
                tr("Could not create the zip archive."), QString());
    }

    QString msg = tr("Corpus exported to %1").arg(dlg.outputDir());
    if (missing > 0)
        msg += tr(" (%1 document(s) missing)").arg(missing);
    if (skippedExternal > 0)
        msg += tr(" (%1 document(s) outside the corpus root skipped)").arg(skippedExternal);
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::refreshCorpusFromTabs()
{
    m_corpus.documents.clear();
    const int rawActive = m_tabBar->currentIndex();
    int fileActive = 0;
    int activeIndex = -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        const TabInfo &info = m_tabs[i];
        if (!info.filePath.isEmpty() && isCorpusTocPath(info.filePath))
            continue;   // the corpus TOC sidecar is never a corpus document

        CorpusDocument d;
        if (!info.filePath.isEmpty())
            d.path = Corpus::storedPath(m_corpus.rootDir(), info.filePath);
        else {
            d.content = info.editor->toPlainText();
            d.name = tabTitleForEmbedded(i);
        }
        d.cursorBlock = info.editor->textCursor().blockNumber();
        d.cursorCol = info.editor->textCursor().positionInBlock();
        d.scroll = info.editor->verticalScrollBar()->value();
        d.folds = info.editor->foldedBlockNumbers();
        m_corpus.documents.append(d);

        if (i == rawActive)
            activeIndex = fileActive;
        ++fileActive;
    }
    m_corpus.active = activeIndex;
}

bool MainWindow::promptSaveUnsavedCorpusDocs()
{
    if (QSettings().value(Preferences::CorpusUnsavedDocs, QStringLiteral("embed")).toString()
            != QLatin1String("prompt"))
        return true;

    for (int i = 0; i < m_tabs.size(); ++i) {
        TabInfo &info = m_tabs[i];
        if (!info.filePath.isEmpty())
            continue;
        if (info.editor->toPlainText().isEmpty() && !info.dirty)
            continue;                            // empty placeholder: nothing to save
        const QString file = saveAsDialogPath();
        if (file.isEmpty())
            return false;                        // cancel: abort the corpus save
        info.filePath = file;
        QFile out(file);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showCenteredWarning(tr("Save Failed"),
                tr("Could not save \"%1\".\n%2").arg(file, out.errorString()), QString());
            return false;
        }
        out.write(info.editor->toPlainText().toUtf8());
        out.close();
        info.editor->setCurrentFile(file);
        setTabSaved(i);
        updateTabLabel(i);
        m_tabBar->setTabToolTip(i, file);
        if (i == m_tabBar->currentIndex()) {
            updateWindowTitle();
            m_preview->setDocumentPath(file);
            if (m_previewInitialized)
                updatePreview();                 // untitled save moves the base dir
        }
    }
    return true;
}

QJsonObject MainWindow::serializeCorpus()
{
    refreshCorpusFromTabs();
    return m_corpus.toJson();
}

QString MainWindow::tabTitleForEmbedded(int index) const
{
    if (index < 0 || index >= m_tabs.size())
        return QString();
    return m_tabs[index].filePath.isEmpty() ? QStringLiteral("Untitled")
                                            : QFileInfo(m_tabs[index].filePath).fileName();
}

void MainWindow::restoreTabState(int idx, const CorpusDocument &d)
{
    if (idx < 0 || idx >= m_tabs.size())
        return;
    Editor *ed = m_tabs[idx].editor;
    if (!ed)
        return;

    if (!d.folds.isEmpty())
        ed->restoreFolds(d.folds);

    const bool restorePositions = QSettings().value(Preferences::RestorePositions, true).toBool();
    const int block = restorePositions ? d.cursorBlock : 0;
    const int col = restorePositions ? d.cursorCol : 0;
    const int scroll = restorePositions ? d.scroll : 0;

    QTextCursor tc = restoreCursorPosition(ed->document(), block, col);
    ed->setTextCursor(tc);
    QTimer::singleShot(0, [this, idx, scroll]() {
        if (idx < m_tabs.size() && m_tabs[idx].editor)
            m_tabs[idx].editor->verticalScrollBar()->setValue(scroll);
    });
}

void MainWindow::restoreCorpus(const QJsonObject &corpus)
{
    if (corpus["version"].toInt() != 1) return;
    QJsonArray docs = corpus["documents"].toArray();
    if (docs.isEmpty()) return;

    QVector<CorpusDocument> parsed;
    for (const QJsonValue &v : docs) {
        const QJsonObject o = v.toObject();
        CorpusDocument d;
        d.path = o["path"].toString();
        d.content = o["content"].toString();
        d.name = o["name"].toString();
        const QJsonObject c = o["cursor"].toObject();
        d.cursorBlock = c["block"].toInt();
        d.cursorCol = c["col"].toInt();
        d.scroll = o["scroll"].toInt();
        for (const QJsonValue &f : o["folds"].toArray())
            d.folds.append(f.toInt());
        parsed.append(d);
    }

    removeEmptyUntitledTab();

    for (const CorpusDocument &d : parsed) {
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        QString content;
        if (!d.path.isEmpty()) {
            QFile f(abs);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;   // missing document: don't resurrect a blank tab
            content = QString::fromUtf8(f.readAll());
            f.close();
        } else {
            content = d.content;
        }

        int idx = addTab(abs);
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
        m_tabs[idx].editor->document()->clearUndoRedoStacks();
        setTabSaved(idx);
        restoreTabState(idx, d);
    }

    if (corpus["active"].toInt(0) >= 0 && corpus["active"].toInt(0) < m_tabBar->count())
        m_tabBar->setCurrentIndex(corpus["active"].toInt(0));

    updateTabBarVisibility();
    if (auto *ed = currentEditor())
        ed->setFocus();
}

void MainWindow::saveCorpusAction()
{
    if (m_corpus.filePath.isEmpty()) {
        saveCorpusAsAction();
        return;
    }
    if (!promptSaveUnsavedCorpusDocs())
        return;                       // user cancelled a save dialog: abort
    refreshCorpusFromTabs();
    startCorpusWatcher();          // tab set may have changed without loadFile
    QString error;
    if (!m_corpus.save(&error)) {
        showCenteredWarning(tr("Save Corpus Failed"),
            tr("Could not save the corpus file."),
            tr("Check that the file is not open in another application and that the path is writable."));
        return;
    }
    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Corpus saved: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 2000);
}

void MainWindow::saveCorpusAsAction()
{
    QString startDir;
    if (TabInfo *info = activeTabInfo(); info && !info->filePath.isEmpty())
        startDir = QFileInfo(info->filePath).absolutePath();
    const QString path = ensureDefaultSuffix(
        QFileDialog::getSaveFileName(
            this, tr("Save Corpus As"), startDir, tr("Scriba Corpus (*.scriba)")),
        "scriba");
    if (path.isEmpty())
        return;
    m_corpus.filePath = QFileInfo(path).absoluteFilePath();
    if (!promptSaveUnsavedCorpusDocs())
        return;                       // user cancelled a save dialog: abort
    refreshCorpusFromTabs();
    startCorpusWatcher();          // tab set may have changed without loadFile
    QString error;
    if (!m_corpus.save(&error)) {
        showCenteredWarning(tr("Save Corpus Failed"),
            tr("Could not save the corpus file."),
            tr("Check that the file is not open in another application and that the path is writable."));
        return;
    }
    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    statusBar()->showMessage(tr("Corpus saved: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 2000);
}

void MainWindow::newCorpusAction()
{
    const QString path = ensureDefaultSuffix(
        QFileDialog::getSaveFileName(this, tr("New Corpus"),
                                     QString(), tr("Scriba Corpus (*.scriba)")),
        "scriba");
    if (path.isEmpty())
        return;                        // cancel: nothing touched

    if (!maybeDiscardCurrentTabs())
        return;                        // cancel: current corpus stays open

    closeAllTabs();
    stopCorpusWatcher();
    m_corpus = Corpus{};               // reset everything a .scriba can store
    applyCorpusDictionary();           // clears old corpus words/override off editors
    updateWindowTitle();
    updateTabBarVisibility();

    m_corpus.filePath = QFileInfo(path).absoluteFilePath();
    if (!promptSaveUnsavedCorpusDocs())
        return;                       // user cancelled a save dialog: abort
    refreshCorpusFromTabs();          // records the blank tab as the first embedded doc
    QString error;
    if (!m_corpus.save(&error)) {
        showCenteredWarning(tr("New Corpus Failed"),
            tr("Could not save the corpus file."), error);
        return;
    }
    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    updateTabBarVisibility();
    updateRecentCorporaMenu();
    startCorpusWatcher();
    applyUntitledLinkBaseDir();
    statusBar()->showMessage(tr("New corpus created: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 3000);
}

void MainWindow::openCorpusAction()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Corpus"), QString(), tr("Scriba Corpus (*.scriba)"));
    if (path.isEmpty())
        return;
    openCorpusFile(path, /*skipPrompt=*/false);
}

void MainWindow::openCorpusFile(const QString &path, bool skipPrompt)
{
    Corpus loaded;
    QString error;
    if (!Corpus::loadFile(path, &loaded, &error)) {
        showCenteredWarning(tr("Open Corpus Failed"),
            tr("Could not open the corpus file."), error);
        return;
    }
    if (!skipPrompt && !maybeDiscardCurrentTabs())
        return;

    closeAllTabs();
    removeEmptyUntitledTab();

    m_corpus = loaded;

    int missing = 0;
    for (int i = 0; i < m_corpus.documents.size(); ++i) {
        const CorpusDocument &d = m_corpus.documents.at(i);
        if (!d.path.isEmpty()) {
            const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
            QFile f(abs);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { ++missing; continue; }
            const QString content = QString::fromUtf8(f.readAll());
            f.close();
            const int idx = addTab(abs);
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(content);
            m_tabs[idx].editor->document()->clearUndoRedoStacks();
            setTabSaved(idx);
            m_tabs[idx].previewHtmlValid = false;
            restoreTabState(idx, d);
        } else {
            const int idx = addTab(QString());
            QSignalBlocker blocker(m_tabs[idx].editor);
            m_tabs[idx].editor->setPlainText(d.content);
            m_tabs[idx].editor->document()->clearUndoRedoStacks();
            setTabSaved(idx);
            restoreTabState(idx, d);
        }
    }
    if (m_tabs.isEmpty())
        addTab();
    if (m_corpus.active >= 0 && m_corpus.active < m_tabBar->count())
        m_tabBar->setCurrentIndex(m_corpus.active);

    applyCorpusDictionary();
    startCorpusWatcher();
    applyUntitledLinkBaseDir();

    addRecentCorpus(m_corpus.filePath);
    QSettings().setValue(Preferences::LastCorpusPath, m_corpus.filePath);
    updateWindowTitle();
    updateTabBarVisibility();
    updateRecentCorporaMenu();
    if (missing > 0)
        statusBar()->showMessage(tr("Corpus opened; %1 document(s) missing").arg(missing), 4000);
    else
        statusBar()->showMessage(tr("Corpus opened: %1").arg(QFileInfo(m_corpus.filePath).fileName()), 2000);
}

bool MainWindow::maybeDiscardCurrentTabs()
{
    bool anyDirty = false;
    bool hasUntitledDirty = false;
    for (const TabInfo &info : m_tabs) {
        if (info.dirty) {
            anyDirty = true;
            if (info.filePath.isEmpty())
                hasUntitledDirty = true;
        }
    }
    if (!anyDirty)
        return true;
    const ClosePromptResult ret = promptUnsavedChanges(hasUntitledDirty);
    if (ret == ClosePromptResult::Cancel)
        return false;
    if (ret == ClosePromptResult::Save)
        return saveAllDirtyTabs();
    return true;   // Discard
}

bool MainWindow::saveAllDirtyTabs()
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        TabInfo &info = m_tabs[i];
        if (!info.dirty)
            continue;
        if (info.filePath.isEmpty()) {
            const QString file = saveAsDialogPath();
            if (file.isEmpty())
                return false;
            info.filePath = file;
        }
        QFile file(info.filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showCenteredWarning(tr("Save Failed"),
                tr("Could not save \"%1\".\n%2").arg(info.filePath, file.errorString()),
                QString());
            return false;
        }
        file.write(info.editor->toPlainText().toUtf8());
        setTabSaved(i);
    }
    return true;
}

void MainWindow::addRecentCorpus(const QString &path)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    QSettings s;
    QStringList recents = s.value(Preferences::RecentCorpora).toStringList();
    recents.removeAll(abs);
    recents.prepend(abs);
    while (recents.size() > Preferences::MaxRecentCorpora)
        recents.removeLast();
    s.setValue(Preferences::RecentCorpora, recents);
    updateRecentCorporaMenu();
}

void MainWindow::updateRecentCorporaMenu()
{
    if (!m_recentCorpusMenu)
        return;
    QSettings s;
    QStringList recents = s.value(Preferences::RecentCorpora).toStringList();
    recents.erase(std::remove_if(recents.begin(), recents.end(),
        [](const QString &p) { return !QFileInfo::exists(p); }), recents.end());
    s.setValue(Preferences::RecentCorpora, recents);

    m_recentCorpusMenu->clear();
    if (recents.isEmpty()) {
        QAction *emptyAction = m_recentCorpusMenu->addAction(tr("(empty)"));
        emptyAction->setEnabled(false);
        return;
    }
    for (const QString &p : recents) {
        QAction *a = m_recentCorpusMenu->addAction(QFileInfo(p).fileName());
        a->setToolTip(p);
        connect(a, &QAction::triggered, this, [this, p]() { openCorpusFile(p); });
    }
}

void MainWindow::applyCorpusDictionary()
{
    if (m_corpus.filePath.isEmpty())
        return;
    const QString mode = QSettings().value(
        Preferences::CorpusDictionaryMode, QStringLiteral("override")).toString();
    const bool merge = (mode == QLatin1String("merge"));
    for (const TabInfo &tab : m_tabs) {
        if (tab.editor)
            tab.editor->applyCorpusDictionary(m_corpus.dictionary, merge);
    }
}

void MainWindow::startCorpusWatcher()
{
    stopCorpusWatcher();
    if (!m_corpus.monitor || m_corpus.filePath.isEmpty()) {
        updateCorpusFilesPanel();
        return;
    }
    QSet<QString> files;
    for (const CorpusDocument &d : m_corpus.documents) {
        QString content;
        QString docDir = m_corpus.rootDir();
        if (!d.path.isEmpty()) {
            const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
            if (QFileInfo::exists(abs))
                files.insert(abs);
            const int tabIdx = findTabByPath(abs);
            content = tabIdx >= 0 ? m_tabs[tabIdx].editor->toPlainText()
                                  : readTextFile(abs);
            docDir = QFileInfo(abs).absolutePath();
        } else {
            content = d.content;               // embedded doc: stored snapshot
        }
        for (const QString &target : LinkFixer::resolvedLinkTargets(content, docDir))
            files.insert(target);
    }
    m_corpusWatcher->setMonitoredFiles(files.values());
    updateCorpusFilesPanel();
}

void MainWindow::stopCorpusWatcher()
{
    m_corpusWatcher->clear();
    updateCorpusFilesPanel();
}

void MainWindow::updateCorpusFilesPanel()
{
    if (m_corpus.filePath.isEmpty()) {
        m_corpusFilesPanel->clear();
        m_corpusFilesDock->setVisible(false);
        // Re-sync the View menu action: the dock was hidden explicitly, so
        // the action must not stay checked (the persisted pref still governs
        // what happens when a corpus opens). isHidden() tracks the explicit
        // hide state even while the main window itself is not yet shown.
        m_showCorpusFilesAction->setChecked(!m_corpusFilesDock->isHidden());
        return;
    }
    m_corpusFilesPanel->setRootDir(m_corpus.rootDir());
    m_corpusFilesPanel->setExcludedPath(m_corpus.filePath);
    m_corpusFilesDock->setVisible(
        QSettings().value(Preferences::ShowCorpusFilesPanel, true).toBool());
    m_showCorpusFilesAction->setChecked(!m_corpusFilesDock->isHidden());
}

void MainWindow::applyUntitledLinkBaseDir()
{
    const QString dir = m_corpus.filePath.isEmpty() ? QString()
                                                    : m_corpus.rootDir();
    for (TabInfo &info : m_tabs) {
        if (info.filePath.isEmpty() && info.editor)
            info.editor->setFallbackLinkBaseDir(dir);
    }
}

QString MainWindow::corpusTocPath() const
{
    if (m_corpus.filePath.isEmpty())
        return QString();
    const QString fileName = QSettings().value(
        Preferences::CorpusTocFileName, QStringLiteral("toc.md")).toString();
    return QDir(m_corpus.rootDir()).filePath(fileName);
}

bool MainWindow::isCorpusTocPath(const QString &absPath) const
{
    const QString toc = corpusTocPath();
    return !toc.isEmpty()
        && QFileInfo(toc).absoluteFilePath() == QFileInfo(absPath).absoluteFilePath();
}

void MainWindow::onCorpusFileActivated(const QString &absPath)
{
    // Mirrors the preview link-click dispatch (MainWindow.cpp:140-165).
    if (QFileInfo(absPath).suffix().compare(QLatin1String("md"), Qt::CaseInsensitive) == 0)
        loadFile(absPath);          // focuses the existing tab when already open
    else
        insertCorpusResourceLink(absPath);
}

void MainWindow::insertCorpusResourceLink(const QString &absPath)
{
    Editor *ed = currentEditor();
    if (!ed)
        return;
    const QString rel = Corpus::storedPath(m_corpus.rootDir(), absPath);
    const QString label = QFileInfo(absPath).completeBaseName();
    const QString text = isSafePreviewImage(absPath)
        ? QStringLiteral("![%1](%2)").arg(label, rel)
        : QStringLiteral("[%1](%2)").arg(label, rel);
    ed->insertPlainText(text);
    ed->setFocus();
}

void MainWindow::handleExternalEdit(const QString &path)
{
    const int idx = findTabByPath(path);
    if (idx < 0)
        return;

    // Scriba's own saves (auto-save, save, corpus save) write the tab's text
    // back to disk, which the watcher reports as an external edit. If the disk
    // content already equals the tab, there is nothing to reload and no reason
    // to prompt (this is what makes auto-saving a dirty tab not nag).
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray diskHash = QCryptographicHash::hash(
                f.readAll(), QCryptographicHash::Md5);
            const QByteArray tabHash = QCryptographicHash::hash(
                m_tabs[idx].editor->toPlainText().toUtf8(), QCryptographicHash::Md5);
            if (diskHash == tabHash)
                return;
        }
    }

    const QString policy = QSettings().value(
        Preferences::CorpusExternalEditPolicy, QStringLiteral("autoReload")).toString();
    if (policy == QLatin1String("prompt")) {
        if (QMessageBox::question(this, tr("File changed on disk"),
                 tr("%1 was modified outside Scriba. Reload it?")
                     .arg(QFileInfo(path).fileName())) != QMessageBox::Yes)
            return;
        loadFile(path, /*forceReload=*/true);
    } else if (policy == QLatin1String("autoReload")) {
        if (m_tabs[idx].dirty) {
            if (QMessageBox::question(this, tr("File changed on disk"),
                     tr("%1 was modified outside Scriba. Reload it and discard your changes?")
                         .arg(QFileInfo(path).fileName())) != QMessageBox::Yes)
                return;
        }
        loadFile(path, /*forceReload=*/true);
    } else {                                     // autoReloadDirty
        loadFile(path, /*forceReload=*/true);
    }
}

void MainWindow::handleExternalRename(const QString &from, const QString &to)
{
    const int idx = findTabByPath(from);
    if (idx >= 0) {
        m_tabs[idx].filePath = to;
        m_tabs[idx].editor->setCurrentFile(to);
        updateTabLabel(idx);
        m_tabBar->setTabToolTip(idx, to);
        m_preview->setDocumentPath(to);
    }
    for (CorpusDocument &d : m_corpus.documents) {
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(abs).absoluteFilePath() == QFileInfo(from).absoluteFilePath()) {
            d.path = Corpus::storedPath(m_corpus.rootDir(), to);
            break;
        }
    }
    m_corpus.save();
    statusBar()->showMessage(tr("Corpus updated: %1 → %2")
                                 .arg(QFileInfo(from).fileName(), QFileInfo(to).fileName()),
                             4000);
    rewriteLinksForFile(from, to);
    // Re-extract the monitored set from the corpus docs + their link targets so
    // the renamed path (and any links the rewrite just changed) are tracked.
    startCorpusWatcher();
    refreshCorpusToc();
}

void MainWindow::handleExternalDelete(const QString &path)
{
    const int idx = findTabByPath(path);
    if (idx < 0)
        return;
    statusBar()->showMessage(tr("%1 was deleted on disk").arg(QFileInfo(path).fileName()), 4000);
    refreshCorpusToc();
}

void MainWindow::rewriteLinksForFile(const QString &oldAbs, const QString &newAbs)
{
    const QSettings prefs;
    const QString policy = prefs.value(Preferences::CorpusLinkRewritePolicy,
                                       QStringLiteral("prompt")).toString();
    if (policy == QLatin1String("ignore"))
        return;

    struct TabEdit { Editor *editor = nullptr; int tabIndex = -1; QString replaced; };
    QStringList affectedDocs;
    QList<TabEdit> tabEdits;
    QList<QPair<QString, QString>> diskEdits;   // abs path -> rewritten content
    const bool scopeAll = prefs.value(Preferences::CorpusLinkRewriteScope,
                                      QStringLiteral("open")).toString() == QLatin1String("all");

    auto consider = [&](const QString &absPath, Editor *ed, int tabIndex, bool onDisk) {
        const QString source = ed ? ed->toPlainText() : readTextFile(absPath);
        const QString docDir = QFileInfo(absPath).absolutePath();
        const QString rewritten = LinkFixer::rewrite(source, docDir, oldAbs, newAbs);
        if (rewritten == source)
            return;
        affectedDocs.append(absPath);
        if (ed)
            tabEdits.append({ed, tabIndex, rewritten});
        else if (onDisk) {
            diskEdits.append({absPath, rewritten});
        }
    };

    for (const CorpusDocument &d : m_corpus.documents) {
        if (d.path.isEmpty())
            continue;
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(abs).absoluteFilePath() == QFileInfo(oldAbs).absoluteFilePath())
            continue;                                  // the renamed doc itself
        const int tabIdx = findTabByPath(abs);
        if (tabIdx >= 0)
            consider(abs, m_tabs[tabIdx].editor, tabIdx, false);
        else if (scopeAll)
            consider(abs, nullptr, -1, true);
    }

    if (affectedDocs.isEmpty())
        return;
    if (policy == QLatin1String("prompt")) {
        if (QMessageBox::question(this, tr("Update links"),
                 tr("Update links to %1 in %2 document(s)?")
                     .arg(QFileInfo(oldAbs).fileName()).arg(affectedDocs.size())) != QMessageBox::Yes)
            return;
    }
    // Deferred past the prompt: declining must not have rewritten closed docs.
    for (const auto &de : diskEdits) {
        QFile f(de.first);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(de.second.toUtf8());
            f.close();
        }
    }
    for (const TabEdit &te : tabEdits) {
        te.editor->setPlainText(te.replaced);
        if (te.tabIndex >= 0) {
            setTabDirty(te.tabIndex, true);
            updateTabLabel(te.tabIndex);
        }
    }
    statusBar()->showMessage(tr("Updated links to %1 in %2 document(s)")
                                 .arg(QFileInfo(oldAbs).fileName()).arg(affectedDocs.size()), 4000);
}

void MainWindow::openCorpusToc()
{
    if (m_corpus.documents.isEmpty()) {
        showCenteredWarning(tr("No Corpus"), tr("No corpus is open."), QString());
        return;
    }
    if (m_corpus.filePath.isEmpty()) {
        showCenteredWarning(tr("Unsaved Corpus"),
            tr("Save the corpus first — its Table of Contents is a file in the corpus folder."),
            QString());
        return;
    }

    const QString tocPath = corpusTocPath();
    if (!QFileInfo::exists(tocPath)) {
        QString links = CorpusIndex::renderTocLinks(m_corpus, tocLinks());
        QString templateText = QSettings().value(Preferences::CorpusTocTemplate).toString();
        if (templateText.trimmed().isEmpty())
            templateText = CorpusIndex::defaultTocTemplate();
        QString content = CorpusIndex::replaceTocBlock(templateText, links);
        QFile out(tocPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showCenteredWarning(tr("Table of Contents Failed"),
                tr("Could not create %1").arg(tocPath), QString());
            return;
        }
        out.write(content.toUtf8());
        out.close();
    }

    const int existing = findTabByPath(tocPath);
    if (existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
    } else {
        loadFile(tocPath);   // opens a normal, editable tab
    }
    refreshCorpusToc();
}

QHash<QString, QString> MainWindow::tocLinks() const
{
    QHash<QString, QString> links;
    for (const CorpusDocument &d : m_corpus.documents) {
        if (d.path.isEmpty())
            continue;
        const QString abs = Corpus::absolutePath(m_corpus.rootDir(), d.path);
        if (QFileInfo(d.path).isAbsolute())
            links.insert(abs, QUrl::fromLocalFile(abs).toString());
        else
            links.insert(abs, d.path);
    }
    return links;
}

void MainWindow::refreshCorpusToc()
{
    const QString tocPath = corpusTocPath();
    if (tocPath.isEmpty() || !QFileInfo::exists(tocPath))
        return;

    QFile in(tocPath);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString before = QString::fromUtf8(in.readAll());
    in.close();
    if (before.isEmpty() || !before.contains(CorpusIndex::tocStartMarker()))
        return; // user removed the markers: leave the file alone

    const QString after = CorpusIndex::replaceTocBlock(before, CorpusIndex::renderTocLinks(m_corpus, tocLinks()));
    if (after == before)
        return;

    QFile out(tocPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    out.write(after.toUtf8());
    out.close();

    // Sync any open tab: replace only the marker region, preserving user text
    // above/below and the dirty flag.
    const int idx = findTabByPath(tocPath);
    if (idx < 0 || !m_tabs[idx].editor)
        return;
    QTextCursor c(m_tabs[idx].editor->document());
    c.beginEditBlock();
    QTextCursor startCursor = c.document()->find(CorpusIndex::tocStartMarker(), c);
    if (!startCursor.isNull()) {
        // Select from the start-marker to just past the end-marker line.
        QTextCursor region = startCursor;
        region.setPosition(startCursor.selectionEnd(), QTextCursor::MoveAnchor);
        QTextCursor endCursor = region.document()->find(CorpusIndex::tocEndMarker(), region);
        if (!endCursor.isNull()) {
            region.setPosition(endCursor.selectionEnd(), QTextCursor::MoveAnchor);
            region.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            const QString newBlock = CorpusIndex::tocStartMarker() + QLatin1Char('\n')
                + CorpusIndex::renderTocLinks(m_corpus, tocLinks())
                + QLatin1Char('\n') + CorpusIndex::tocEndMarker();
            region.insertText(newBlock);
        }
    }
    c.endEditBlock();
    if (!m_tabs[idx].dirty)
        setTabSaved(idx);
}

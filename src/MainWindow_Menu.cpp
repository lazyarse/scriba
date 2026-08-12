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
#include "AdvancedChartDialog.h"
#include "AboutDialog.h"
#include "ChartDialog.h"
#include "ChartSource.h"
#include "CssLoader.h"
#include "CssUtils.h"
#include "EmojiDialog.h"
#include "ExportDocxDialog.h"
#include "ExportHtmlDialog.h"
#include "ExportPdfDialog.h"
#include "FindDialog.h"
#include "GrammarChecker.h"
#include "KatexHelperDialog.h"
#include "LogWindow.h"
#include "MchemHelperDialog.h"
#include "MermaidDialog.h"
#include "Preferences.h"
#include "SpellCheckDialog.h"
#include "SpellChecker.h"
#include "StaticHelpers.h"
#include "StockChartDialog.h"
#include "TableDialog.h"
#include "ValidationReport.h"
#include <QAction>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocument>
#include <QTextCursor>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QScrollBar>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QStatusBar>

static constexpr const char *kMdFilter = "Markdown Files (*.md);;All Files (*)";

static constexpr const char *kOpenMdFilter = "Markdown Files (*.md *.markdown *.txt);;All Files (*)";



void MainWindow::setupMenuBar()
{
    buildFileMenu(menuBar());
    buildEditMenu(menuBar());
    buildViewMenu(menuBar());
    buildToolsMenu(menuBar());
    buildHelpMenu(menuBar());
}

void MainWindow::buildFileMenu(QMenuBar *bar)
{
    QMenu *fileMenu = bar->addMenu("&File");

    QAction *newAction = fileMenu->addAction("&New Tab");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        addTab();
    });

    QMenu *corpusMenu = fileMenu->addMenu(tr("C&orpus"));
    QAction *saveCorpusAct = corpusMenu->addAction(tr("&Save Corpus"));
    connect(saveCorpusAct, &QAction::triggered, this, &MainWindow::saveCorpusAction);
    QAction *saveCorpusAsAct = corpusMenu->addAction(tr("Save Corpus &As…"));
    connect(saveCorpusAsAct, &QAction::triggered, this, &MainWindow::saveCorpusAsAction);
    QAction *openCorpusAct = corpusMenu->addAction(tr("&Open Corpus…"));
    connect(openCorpusAct, &QAction::triggered, this, &MainWindow::openCorpusAction);
    QAction *tocAction = corpusMenu->addAction(tr("&View Table of Contents"));
    tocAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(tocAction, &QAction::triggered, this, &MainWindow::viewTableOfContents);
    corpusMenu->addSeparator();
    m_recentCorpusMenu = corpusMenu->addMenu(tr("Recent C&orpus"));
    updateRecentCorporaMenu();

    fileMenu->addSeparator();

    QAction *openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Open Markdown File(s)", QString(), kOpenMdFilter);
        for (const QString &file : files)
            loadFile(file);
    });

    QAction *reloadAction = fileMenu->addAction("&Reload");
    reloadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(reloadAction, &QAction::triggered, this, [this]() {
        TabInfo *info = activeTabInfo();
        if (info && !info->filePath.isEmpty())
            loadFile(info->filePath, true);
    });

    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        TabInfo *info = activeTabInfo();
        if (!info) return;
        if (info->filePath.isEmpty()) {
            QString file = ensureDefaultSuffix(
                QFileDialog::getSaveFileName(this, "Save Markdown File", QString(), kMdFilter), "md");
            if (!file.isEmpty()) saveFile(file);
        } else {
            saveFile(info->filePath);
        }
    });

    QAction *saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        QString file = ensureDefaultSuffix(
            QFileDialog::getSaveFileName(this, "Save Markdown File As", QString(), kMdFilter), "md");
        if (!file.isEmpty()) saveFile(file);
    });

    QAction *renameAction = fileMenu->addAction("Re&name...");
    connect(renameAction, &QAction::triggered, this, &MainWindow::renameCurrentFile);

    fileMenu->addSeparator();

    QAction *closeTabAction = fileMenu->addAction("&Close Tab");
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    fileMenu->addSeparator();

    QMenu *importMenu = fileMenu->addMenu("&Import");

    QAction *importPdfAction = importMenu->addAction("Import &PDF...");
    importPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    connect(importPdfAction, &QAction::triggered, this, &MainWindow::importPdfFromFile);

    QAction *importDocxAction = importMenu->addAction("Import &Word (DOCX)...");
    importDocxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(importDocxAction, &QAction::triggered, this, &MainWindow::importDocxFromFile);

    QAction *importHtmlAction = importMenu->addAction("Import &HTML...");
    importHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(importHtmlAction, &QAction::triggered, this, &MainWindow::importHtmlFromFile);

    QMenu *exportMenu = fileMenu->addMenu("&Export");

    QAction *exportPdfAction = exportMenu->addAction("&Print / Export PDF...");
    exportPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::exportPdf);

    QAction *exportDocxAction = exportMenu->addAction("Export as &Word (DOCX)...");
    exportDocxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(exportDocxAction, &QAction::triggered, this, &MainWindow::exportDocx);

    QAction *exportHtmlAction = exportMenu->addAction("Export as &HTML...");
    exportHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    connect(exportHtmlAction, &QAction::triggered, this, &MainWindow::exportHtml);

    QAction *exportCorpusAction = exportMenu->addAction(tr("&Export Corpus…"));
    exportCorpusAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(exportCorpusAction, &QAction::triggered, this, &MainWindow::exportCorpus);

    fileMenu->addSeparator();

    QAction *prefsAction = fileMenu->addAction("&Preferences...");
    prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P));
    connect(prefsAction, &QAction::triggered, this, &MainWindow::showPreferences);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::buildEditMenu(QMenuBar *bar)
{
    QMenu *editMenu = bar->addMenu("&Edit");

    QAction *findAction = editMenu->addAction("Find / &Replace...");
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::toggleFindDialog);

    QAction *findNextAction = editMenu->addAction("Find &Next");
    findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(findNextAction, &QAction::triggered, this, &MainWindow::onFindNext);

    QAction *findPrevAction = editMenu->addAction("Find &Previous");
    findPrevAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(findPrevAction, &QAction::triggered, this, &MainWindow::onFindPrev);

    editMenu->addSeparator();

    QAction *pasteMarkdownAction = editMenu->addAction("Paste as &Markdown");
    pasteMarkdownAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(pasteMarkdownAction, &QAction::triggered, this, &MainWindow::pasteAsMarkdown);

    QAction *fullscreenAction = new QAction("Toggle &Fullscreen", this);
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::toggleFullscreen);
    addAction(fullscreenAction);

    QAction *previewAction = new QAction("Toggle &Preview", this);
    previewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(previewAction, &QAction::triggered, this, &MainWindow::togglePreview);
    addAction(previewAction);
}

void MainWindow::buildViewMenu(QMenuBar *bar)
{
    QMenu *viewMenu = bar->addMenu("&View");

    QAction *nextTabAction = viewMenu->addAction("&Next Tab");
    nextTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(nextTabAction, &QAction::triggered, this, [this]() {
        int count = m_tabBar->count();
        if (count > 1)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % count);
    });

    QAction *prevTabAction = viewMenu->addAction("&Previous Tab");
    prevTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTabAction, &QAction::triggered, this, [this]() {
        int count = m_tabBar->count();
        if (count > 1)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() - 1 + count) % count);
    });

    viewMenu->addSeparator();

    QAction *lineNumbersAction = viewMenu->addAction("Show &Line Numbers");
    lineNumbersAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    lineNumbersAction->setCheckable(true);
    lineNumbersAction->setChecked(QSettings().value(Preferences::ShowLineNumbers, true).toBool());
    connect(lineNumbersAction, &QAction::toggled, this, [this](bool checked) {
        QSettings s;
        s.setValue(Preferences::ShowLineNumbers, checked);
        s.sync();
        for (const auto &tab : m_tabs)
            if (tab.editor)
                tab.editor->updateGutterSettings();
    });

    m_wrapTextAction = viewMenu->addAction("Wrap &Text");
    m_wrapTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W));
    m_wrapTextAction->setCheckable(true);
    m_wrapTextAction->setChecked(QSettings().value(Preferences::EditorWrapEnabled, true).toBool());
    connect(m_wrapTextAction, &QAction::toggled, this, [this](bool checked) {
        QSettings s;
        s.setValue(Preferences::EditorWrapEnabled, checked);
        s.sync();
        for (const auto &tab : m_tabs)
            if (tab.editor)
                tab.editor->applyLineWrap();
    });

    viewMenu->addSeparator();

    m_layoutActions = new QActionGroup(this);
    m_layoutActions->setExclusive(true);

    const auto addLayoutAction = [this, viewMenu](const QString &text, int state) {
        QAction *a = viewMenu->addAction(text);
        a->setCheckable(true);
        a->setData(state);
        m_layoutActions->addAction(a);
        connect(a, &QAction::triggered, this, [this, state]() {
            setPreviewState(state);
        });
    };

    addLayoutAction("Editor | Pre&view", 1);
    addLayoutAction("Preview | Ed&itor", 2);
    addLayoutAction("&Editor", 0);
    addLayoutAction("&Preview", 3);
    syncPreviewLayout();

    viewMenu->addSeparator();

    QAction *fullScreenAction = viewMenu->addAction("&Full Screen");
    fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, this, [this](bool checked) {
        if (checked) {
            showFullScreen();
        } else {
            showNormal();
        }
    });

    m_showPageBreaksAction = viewMenu->addAction("Show Page &Breaks");
    m_showPageBreaksAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    m_showPageBreaksAction->setCheckable(true);
    m_showPageBreaksAction->setChecked(m_printLayoutMode);
    connect(m_showPageBreaksAction, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue(Preferences::PreviewShowPageBreaks, on);
        m_printLayoutMode = on;
        if (!on)
            m_printLayoutFp.clear();
        // Force the full rebuild path so the page picks up the print/classic
        // base CSS, theme, page box and paginator (or the absence of them).
        m_previewInitialized = false;
        updatePreview();
    });

    for (int i = 1; i <= 10; ++i) {
        Qt::Key key = (i == 10) ? Qt::Key_0
                                : static_cast<Qt::Key>(Qt::Key_0 + i);
        QAction *tabAction = new QAction(QString("Switch to Tab %1").arg(i), this);
        tabAction->setShortcut(QKeySequence(Qt::ALT | key));
        connect(tabAction, &QAction::triggered, this, [this, i]() {
            if (i - 1 < m_tabBar->count())
                m_tabBar->setCurrentIndex(i - 1);
        });
        addAction(tabAction);
    }
}

void MainWindow::buildToolsMenu(QMenuBar *bar)
{
    QMenu *toolsMenu = bar->addMenu("&Tools");

    QAction *tableAction = toolsMenu->addAction("&Table Insert...");
    tableAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(tableAction, &QAction::triggered, this, &MainWindow::showTableInsert);

    QAction *emojiAction = toolsMenu->addAction("&Emoji Picker...");
    emojiAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(emojiAction, &QAction::triggered, this, [this]() {
        EmojiDialog dlg(this);
        Editor *ed = currentEditor();
        if (!ed) return;
        connect(&dlg, &EmojiDialog::emojiChosen, this, [this, ed](const QString &sc) {
            ed->insertPlainText(":" + sc + ":");
        });
        dlg.exec();
    });

    toolsMenu->addSeparator();

    QAction *katexAction = toolsMenu->addAction("&KaTeX Equation...");
    katexAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(katexAction, &QAction::triggered, this, &MainWindow::showKatexHelper);

    QAction *mchemAction = toolsMenu->addAction("Chemistry &Notation...");
    mchemAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    connect(mchemAction, &QAction::triggered, this, &MainWindow::showMchemHelper);

    toolsMenu->addSeparator();

    QAction *chartAction = toolsMenu->addAction("&Chart Builder...");
    chartAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(chartAction, &QAction::triggered, this, &MainWindow::showChartBuilder);

    QAction *stockAction = toolsMenu->addAction("&Stock Chart...");
    stockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(stockAction, &QAction::triggered, this, &MainWindow::showStockChartBuilder);

    QAction *advancedAction = toolsMenu->addAction("&Advanced Charts...");
    connect(advancedAction, &QAction::triggered, this, &MainWindow::showAdvancedChartBuilder);

    QAction *mermaidAction = toolsMenu->addAction("&Mermaid Diagrams...");
    mermaidAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(mermaidAction, &QAction::triggered, this, [this]() {
        MermaidDialog dlg(m_cssLoader->themeCss(), this);
        if (dlg.exec() == QDialog::Accepted) {
            QString block = dlg.mermaidBlock();
            Editor *ed = editor();
            if (!block.isEmpty() && ed)
                ed->insertPlainText(block);
        }
    });

    toolsMenu->addSeparator();

    QAction *spellCheckAction = toolsMenu->addAction("&Check Spelling...");
    spellCheckAction->setShortcut(QKeySequence(Qt::Key_F7));
    connect(spellCheckAction, &QAction::triggered, this, &MainWindow::showSpellCheckDialog);

    QAction *reportAction = toolsMenu->addAction("&Validation Report...");
    reportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F7));
    connect(reportAction, &QAction::triggered, this, &MainWindow::generateValidationReport);

    toolsMenu->addSeparator();

    QAction *logAction = toolsMenu->addAction("&Debug Log");
    logAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(logAction, &QAction::triggered, this, &MainWindow::showLogWindow);

    m_insertActions = {tableAction, emojiAction, katexAction, mchemAction, chartAction};
    m_mermaidAction = mermaidAction;

    for (TabInfo &info : m_tabs) {
        if (info.editor) {
            info.editor->setInsertActions(m_insertActions);
            info.editor->setMermaidAction(m_mermaidAction);
        }
    }
}

void MainWindow::buildHelpMenu(QMenuBar *bar)
{
    QMenu *helpMenu = bar->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About Scriba...");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    QAction *shortcutsAction = helpMenu->addAction("&Keyboard Shortcuts...");
    connect(shortcutsAction, &QAction::triggered, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Keyboard Shortcuts");
        dlg.resize(520, 420);
        auto *layout = new QVBoxLayout(&dlg);
        auto *browser = new QTextBrowser();
        browser->setReadOnly(true);
        QFile f(":/shortcuts.html");
        if (f.open(QIODevice::ReadOnly))
            browser->setHtml(f.readAll());
        layout->addWidget(browser);
        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
        btnBox->button(QDialogButtonBox::Close)->setText(tr("&Close"));
        stripButtonIcons(btnBox);
        connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::close);
        layout->addWidget(btnBox);
        dlg.exec();
    });
}

void MainWindow::showChartBuilder()
{
    insertFromDialog([this] {
        ChartDialog dlg(this);
        return dlg.exec() == QDialog::Accepted ? dlg.generatedSpec() : QString();
    });
}

void MainWindow::showStockChartBuilder()
{
    insertFromDialog([this] {
        StockChartDialog dlg(this);
        return dlg.exec() == QDialog::Accepted ? dlg.generatedSpec() : QString();
    });
}

void MainWindow::showAdvancedChartBuilder()
{
    insertFromDialog([this] {
        AdvancedChartDialog dlg(this);
        return dlg.exec() == QDialog::Accepted ? dlg.generatedSpec() : QString();
    });
}

void MainWindow::showKatexHelper()
{
    insertFromDialog([this] {
        KatexHelperDialog dlg(m_cssLoader->themeCss(), QString(), this);
        return dlg.exec() == QDialog::Accepted ? dlg.generatedLatex() : QString();
    });
}

void MainWindow::showMchemHelper()
{
    insertFromDialog([this] {
        MchemHelperDialog dlg(m_cssLoader->themeCss(), QString(), this);
        return dlg.exec() == QDialog::Accepted ? dlg.generatedNotation() : QString();
    });
}

void MainWindow::insertFromDialog(const std::function<QString()> &runDialog)
{
    const QString text = runDialog();
    Editor *ed = currentEditor();
    if (!text.isEmpty() && ed)
        ed->insertPlainText(text);
}

void MainWindow::editChartBlock(Editor *ed, int blockNumber)
{
    if (!ed || blockNumber < 0)
        return;
    const QPair<int, int> range = ed->fencedCodeBlockRange(blockNumber);
    if (range.first < 0)
        return;
    const QString body = ed->fenceBody(blockNumber);
    const QString lang = ed->fenceLanguage(blockNumber);

    if (lang == QLatin1String("mermaid")) {
        MermaidDialog dlg(body, m_cssLoader->themeCss(), this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString replacement = dlg.mermaidBlock().trimmed();
        if (!replacement.isEmpty())
            ed->replaceBlockRange(range.first, range.second, replacement);
        return;
    }
    if (lang == QLatin1String("ec")) {
        const QByteArray json = body.toUtf8();
        switch (ChartSource::detectEcType(json)) {
        case ChartSource::EcType::Stock: {
            StockChartDialog dlg(body, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QString replacement = dlg.generatedSpec().trimmed();
            if (!replacement.isEmpty())
                ed->replaceBlockRange(range.first, range.second, replacement);
            break;
        }
        case ChartSource::EcType::Chart: {
            ChartDialog dlg(body, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QString replacement = dlg.generatedSpec().trimmed();
            if (!replacement.isEmpty())
                ed->replaceBlockRange(range.first, range.second, replacement);
            break;
        }
        case ChartSource::EcType::Sankey:
        case ChartSource::EcType::Boxplot:
        case ChartSource::EcType::Parallel:
        case ChartSource::EcType::ThemeRiver:
        case ChartSource::EcType::Graph:
        case ChartSource::EcType::Treemap:
        case ChartSource::EcType::Sunburst: {
            AdvancedChartDialog dlg(body, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QString replacement = dlg.generatedSpec().trimmed();
            if (!replacement.isEmpty())
                ed->replaceBlockRange(range.first, range.second, replacement);
            break;
        }
        default:
            QMessageBox::warning(this, tr("Chart"),
                tr("This chart was not created by Scriba's chart helpers and "
                   "cannot be reopened for editing."));
        }
        return;
    }

    QMessageBox::warning(this, tr("Chart"),
        tr("This chart was not created by Scriba's chart helpers and "
           "cannot be reopened for editing."));
}

void MainWindow::handleChartEdit(const QString &kind, int line, int index, const QString &tex)
{
    Editor *ed = currentEditor();
    if (!ed)
        return;

    if (kind == QLatin1String("mermaid") || kind == QLatin1String("ec")) {
        // The preview's line numbers are only approximate; the anchor carries
        // the fenced block's body, so match on that (the line only breaks
        // ties between identical charts).
        int blockNumber = ed->findFenceByBody(tex, line);
        if (blockNumber < 0) {
            QMessageBox::warning(this, tr("Edit"),
                tr("Could not locate this diagram in the document."));
            return;
        }
        editChartBlock(ed, blockNumber);
        return;
    }

    // Math: katex or mchem. The preview's line/index are only approximate, so
    // resolve the equation by its inner text (line/index break ties).
    bool mchem = tex.contains(QLatin1String("\\ce{"));
    int matchIndex = -1;
    int blockNumber = ed->findMathByContent(tex, line, index, &matchIndex);
    if (blockNumber < 0) {
        QMessageBox::warning(this, tr("Edit"),
            tr("Could not locate this equation in the document."));
        return;
    }
    if (mchem) {
        MchemHelperDialog dlg(m_cssLoader->themeCss(), tex, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString replacement = dlg.generatedNotation().trimmed();
        if (!replacement.isEmpty())
            ed->replaceInlineMath(blockNumber, matchIndex, replacement);
    } else {
        KatexHelperDialog dlg(m_cssLoader->themeCss(), tex, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString replacement = dlg.generatedLatex().trimmed();
        if (!replacement.isEmpty())
            ed->replaceInlineMath(blockNumber, matchIndex, replacement);
    }
}

void MainWindow::showSpellCheckDialog()
{
    Editor *ed = currentEditor();
    if (!ed || !ed->spellChecker() || !ed->spellChecker()->isLoaded()) {
        showCenteredWarning(
            tr("Spell checking is disabled"),
            tr("The spelling checker is not loaded, so a document-wide check "
               "cannot run."),
            tr("Enable \u201cCheck spelling as you type\u201d on the Spelling "
               "page of Preferences."));
        return;
    }
    if (!m_spellCheckDlg) {
        m_spellCheckDlg = new SpellCheckDialog(ed, this);
        m_spellCheckDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    // A modeless panel that follows the active tab: re-point at the current
    // editor each time it is summoned (or after a tab switch) and bring it
    // to the front.
    m_spellCheckDlg->retarget(ed);
    m_spellCheckDlg->show();
    m_spellCheckDlg->raise();
    m_spellCheckDlg->activateWindow();
}

void MainWindow::showTableInsert()
{
    TableDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Editor *ed = currentEditor();
        if (!ed) return;
        QString table = dlg.generateTable();
        QTextCursor cursor = ed->textCursor();
        int insertPos = cursor.position();
        cursor.insertText(table);
        int offset = dlg.isHtml() ? 16 : 2;
        cursor.setPosition(insertPos + offset, QTextCursor::MoveAnchor);
        ed->setTextCursor(cursor);
        if (!dlg.isHtml())
            ed->formatTableAt(insertPos);
        ed->centerCursor();
    }
}

void MainWindow::showLogWindow()
{
    if (!m_logWindow) {
        m_logWindow = new LogWindow(this);
        m_logWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_logWindow->show();
    m_logWindow->raise();
    m_logWindow->activateWindow();
}

void MainWindow::toggleFindDialog()
{
    if (!m_findDialog) {
        m_findDialog = new FindDialog(this);
        connect(m_findDialog, &FindDialog::findNextRequested, this, [this](const QString &text, bool useRegex, bool caseSensitive) {
            findText(text, false, useRegex, caseSensitive);
        });
        connect(m_findDialog, &FindDialog::findPrevRequested, this, [this](const QString &text, bool useRegex, bool caseSensitive) {
            findText(text, true, useRegex, caseSensitive);
        });
        connect(m_findDialog, &FindDialog::replaceRequested, this, &MainWindow::onReplace);
        connect(m_findDialog, &FindDialog::replaceAllRequested, this, &MainWindow::onReplaceAll);
        connect(m_findDialog, &FindDialog::searchTextChanged, this, [this](const QString &text, bool useRegex, bool caseSensitive) {
            int count = countMatches(text, useRegex, caseSensitive);
            m_findDialog->setMatchCount(count);
        });
    }

    if (!m_findDialog->searchTerm().isEmpty())
        m_findDialog->setMatchCount(countMatches(m_findDialog->searchTerm(), m_findDialog->regexEnabled(), m_findDialog->caseSensitive()));
    else
        m_findDialog->setMatchCount(0);

    m_findDialog->show();
    m_findDialog->raise();
    m_findDialog->activateWindow();
    m_findDialog->focusSearchInput();
}

void MainWindow::onFindNext()
{
    if (!m_findDialog) {
        toggleFindDialog();
        return;
    }
    QString text = m_findDialog->searchTerm();
    if (text.isEmpty()) {
        toggleFindDialog();
        return;
    }
    findText(text, false, m_findDialog->regexEnabled(), m_findDialog->caseSensitive());
}

void MainWindow::onFindPrev()
{
    if (!m_findDialog) {
        toggleFindDialog();
        return;
    }
    QString text = m_findDialog->searchTerm();
    if (text.isEmpty()) {
        toggleFindDialog();
        return;
    }
    findText(text, true, m_findDialog->regexEnabled(), m_findDialog->caseSensitive());
}

bool MainWindow::findText(const QString &text, bool backward, bool useRegex, bool caseSensitive)
{
    if (text.isEmpty()) return false;

    Editor *ed = currentEditor();
    if (!ed) return false;

    QScrollBar *scrollBar = ed->verticalScrollBar();
    const int scrollBefore = scrollBar->value();
    const QTextCursor caretBefore = ed->textCursor();

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    if (backward)
        flags |= QTextDocument::FindBackward;

    bool found;
    if (useRegex)
        found = ed->find(QRegularExpression(text), flags);
    else
        found = ed->find(text, flags);

    if (!found) {
        QTextCursor c = ed->textCursor();
        c.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        ed->setTextCursor(c);

        if (useRegex)
            found = ed->find(QRegularExpression(text), flags);
        else
            found = ed->find(text, flags);

        if (found)
            statusBar()->showMessage("Search wrapped around", 3000);
        else {
            ed->setTextCursor(caretBefore);
            scrollBar->setValue(scrollBefore);
        }
    }

    if (!found)
        statusBar()->showMessage("No matches found", 3000);
    else if (scrollBar->value() != scrollBefore)
        ed->centerCursor();

    return found;
}

int MainWindow::countMatches(const QString &text, bool useRegex, bool caseSensitive) const
{
    if (text.isEmpty()) return 0;

    Editor *ed = currentEditor();
    if (!ed) return 0;

    QTextDocument *doc = ed->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;

    int count = 0;

    if (useRegex) {
        auto opts = caseSensitive ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(text, opts);
        while (true) {
            QTextCursor match = doc->find(regex, cursor, flags);
            if (match.isNull()) break;
            count++;
            cursor = match;
        }
    } else {
        while (true) {
            QTextCursor match = doc->find(text, cursor, flags);
            if (match.isNull()) break;
            count++;
            cursor = match;
        }
    }

    return count;
}

void MainWindow::onReplace(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive)
{
    if (search.isEmpty()) return;
    Editor *ed = currentEditor();
    if (!ed) return;

    QTextCursor cursor = ed->textCursor();
    if (cursor.hasSelection()) {
        QString sel = cursor.selectedText();
        if (useRegex) {
            auto opts = caseSensitive ? QRegularExpression::NoPatternOption
                                       : QRegularExpression::CaseInsensitiveOption;
            QRegularExpression regex(search, opts);
            QString replaced = sel;
            if (replaced.contains(regex)) {
                replaced.replace(regex, replacement);
                cursor.insertText(replaced);
                findText(search, false, useRegex, caseSensitive);
                return;
            }
        } else {
            if (sel == search) {
                cursor.insertText(replacement);
                findText(search, false, useRegex, caseSensitive);
                return;
            }
        }
    }
    findText(search, false, useRegex, caseSensitive);
}

void MainWindow::onReplaceAll(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive)
{
    if (search.isEmpty()) return;
    Editor *ed = currentEditor();
    if (!ed) return;

    QTextDocument *doc = ed->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;

    int count = 0;
    cursor.beginEditBlock();

    if (useRegex) {
        auto opts = caseSensitive ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(search, opts);
        while (true) {
            QTextCursor match = doc->find(regex, cursor, flags);
            if (match.isNull()) break;
            QString replaced = match.selectedText();
            replaced.replace(regex, replacement);
            match.insertText(replaced);
            count++;
            cursor = match;
        }
    } else {
        while (true) {
            QTextCursor match = doc->find(search, cursor, flags);
            if (match.isNull()) break;
            match.insertText(replacement);
            count++;
            cursor = match;
        }
    }

    cursor.endEditBlock();

    if (count > 0)
        statusBar()->showMessage(QString("Replaced %1 occurrence%2").arg(count).arg(count == 1 ? "" : "s"), 5000);
    else
        statusBar()->showMessage("No matches found", 3000);
}

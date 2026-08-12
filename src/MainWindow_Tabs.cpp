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
#include "Preferences.h"
#include "SpellCheckDialog.h"
#include "ValidationReport.h"
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTabBar>
#include <QTextDocument>
#include <QTimer>

static constexpr const char *kMdFilter = "Markdown Files (*.md);;All Files (*)";



int MainWindow::addTab(const QString &filePath)
{
    int existing = findTabByPath(filePath);
    if (existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
        return existing;
    }

    auto *editor = new Editor();
    editor->setInsertActions(m_insertActions);
    editor->setMermaidAction(m_mermaidAction);
    editor->setCurrentFile(filePath);

    QString label = filePath.isEmpty() ? QStringLiteral("Untitled")
                                       : QFileInfo(filePath).fileName();

    // Populate m_tabs BEFORE adding the tab so currentChanged handlers
    // (which fire synchronously for the first tab) can find the TabInfo.
    int idx = m_tabs.size();
    m_tabs.append({editor, filePath, false});
    m_editorStack->addWidget(editor);
    m_tabBar->addTab(label);
    m_tabBar->setCurrentIndex(idx);
    m_tabBar->setTabToolTip(idx, filePath.isEmpty() ? QString() : filePath);
    // Stable per-tab identity so onTabMoved() can rebuild the parallel
    // containers (m_tabs, m_editorStack, m_reportTitles) to match the tab
    // bar's order after the user drags a tab. The Editor is deleted with its
    // tab (removeTab), so we never look this pointer up after removal.
    m_tabBar->setTabData(idx, QVariant::fromValue(reinterpret_cast<qulonglong>(editor)));

    connect(editor->document(), &QTextDocument::contentsChange, this,
        [this, editor](int, int charsRemoved, int charsAdded) {
            if (charsRemoved == 0 && charsAdded == 0)
                return; // format-only change (e.g. spell/syntax highlighting)
            for (int i = 0; i < m_tabs.size(); ++i) {
                if (m_tabs[i].editor == editor && !m_tabs[i].dirty) {
                    setTabDirty(i, true);
                    break;
                }
            }
        });

    // The gutter pencil opens the chart's helper dialog directly (the editor
    // knows the exact fence block, so no preview body-matching is needed).
    connect(editor, &Editor::chartEditRequested, this,
        [this, editor](int blockNumber) { editChartBlock(editor, blockNumber); });

    if (!m_cachedFullCss.isEmpty()) {
        editor->setStyleSheet(m_cachedFullCss + applyEditorSettings());
        editor->update();
    }
    editor->setCursorWidth(QSettings().value(Preferences::EditorCaretWidth,
                                              Preferences::DefaultEditorCaretWidth).toInt());
    {
        QSettings s;
        QTextBlockFormat fmt;
        fmt.setLineHeight(s.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt(),
                          QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(editor->document());
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(fmt);
    }
    m_tabs[idx].dirty = false;
    updateTabLabel(idx);

    editor->updateGutterSettings();

    updateTabBarVisibility();
    editor->setFocus();
    return idx;
}

void MainWindow::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size() || m_tabs.size() <= 1)
        return;

    disconnectTabEditor(index);
    m_connectedTabIndex = -1;

    Editor *editor = m_tabs[index].editor;
    m_tabs.removeAt(index);
    m_editorStack->removeWidget(editor);
    m_tabBar->removeTab(index);

    // The spelling panel must never point at a deleted editor. Removing the
    // current tab re-targets it via currentChanged → connectTabEditor; any
    // other removal that deletes the panel's editor needs an explicit rebind.
    // This runs after removeTab(), so currentEditor() is valid.
    if (m_spellCheckDlg && m_spellCheckDlg->targetEditor() == editor) {
        if (Editor *next = currentEditor())
            m_spellCheckDlg->retarget(next);
    }
    delete editor;

    // Removing a tab renumbers every following tab: shift report-tab titles
    // down so updateTabLabel() still finds them.
    m_reportTitles.remove(index);
    QHash<int, QString> shifted;
    for (auto it = m_reportTitles.constBegin(); it != m_reportTitles.constEnd(); ++it) {
        const int key = it.key();
        shifted.insert(key > index ? key - 1 : key, it.value());
    }
    m_reportTitles = shifted;
    m_tocTabs.remove(index);
    QHash<int, QString> tocShifted;
    for (auto it = m_tocTabs.constBegin(); it != m_tocTabs.constEnd(); ++it) {
        const int key = it.key();
        tocShifted.insert(key > index ? key - 1 : key, it.value());
    }
    m_tocTabs = tocShifted;

    updateTabBarVisibility();
}

void MainWindow::onTabMoved(int from, int to)
{
    Q_UNUSED(from);
    Q_UNUSED(to);
    if (m_tabs.isEmpty())
        return;

    // The tab bar reorders itself when the user drags a tab, but the parallel
    // containers (m_tabs, m_editorStack, m_reportTitles) stay in their old
    // order. Rebuild them all from the tab bar's authoritative order using the
    // Editor* identity stamped in each tab's tabData. Without this, index-keyed
    // lookups (activeTabInfo()/currentEditor() -> m_tabs[tabBar->currentIndex()])
    // return the wrong tab after a drag, so the preview would show another
    // file's cached render. Called on every tabMoved during a drag; idempotent
    // because identity is the stable Editor*.

    QHash<Editor *, QString> oldReportTitles;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_reportTitles.contains(i))
            oldReportTitles.insert(m_tabs[i].editor, m_reportTitles.value(i));
    }
    QHash<Editor *, QString> oldTocTitles;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tocTabs.contains(i))
            oldTocTitles.insert(m_tabs[i].editor, m_tocTabs.value(i));
    }

    QVector<TabInfo> reordered;
    reordered.reserve(m_tabs.size());
    QVector<QWidget *> stackOrder;
    stackOrder.reserve(m_tabs.size());
    for (int i = 0; i < m_tabBar->count(); ++i) {
        auto *ed = reinterpret_cast<Editor *>(
            m_tabBar->tabData(i).toULongLong());
        if (!ed)
            continue;
        for (int j = 0; j < m_tabs.size(); ++j) {
            if (m_tabs[j].editor == ed) {
                reordered.append(m_tabs[j]);
                stackOrder.append(ed);
                break;
            }
        }
    }
    if (reordered.size() != m_tabs.size())
        return;

    m_tabs = reordered;

    const int active = m_tabBar->currentIndex();
    while (m_editorStack->count() > 0)
        m_editorStack->removeWidget(m_editorStack->widget(0));
    for (QWidget *w : stackOrder)
        m_editorStack->addWidget(w);
    if (active >= 0 && active < m_editorStack->count())
        m_editorStack->setCurrentIndex(active);

    m_reportTitles.clear();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (auto it = oldReportTitles.constFind(m_tabs[i].editor);
            it != oldReportTitles.constEnd()) {
            m_reportTitles.insert(i, it.value());
        }
    }

    m_tocTabs.clear();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (auto it = oldTocTitles.constFind(m_tabs[i].editor);
            it != oldTocTitles.constEnd()) {
            m_tocTabs.insert(i, it.value());
        }
    }

    m_connectedTabIndex = -1;
    connectActiveEditor();
    for (int i = 0; i < m_tabs.size(); ++i)
        updateTabLabel(i);
}

int MainWindow::findTabByPath(const QString &filePath) const
{
    if (filePath.isEmpty())
        return -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == filePath)
            return i;
    }
    return -1;
}

void MainWindow::connectTabEditor(int index)
{
    if (m_connectedTabIndex == index)
        return;

    disconnectActiveEditor();
    m_connectedTabIndex = index;

    if (index < 0 || index >= m_tabs.size())
        return;

    Editor *editor = m_tabs[index].editor;
    if (!editor)
        return;

    if (m_updateTimer) {
        connect(editor, &QTextEdit::textChanged, m_updateTimer, qOverload<>(&QTimer::start));
        connect(editor, &QTextEdit::textChanged, this, [this, index]() {
            // The cached md->html render is stale as soon as the editor changes
            if (index >= 0 && index < m_tabs.size())
                m_tabs[index].previewHtmlValid = false;
        });
    }

    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::onEditorScroll);

    // The modeless spelling panel follows the active tab.
    if (m_spellCheckDlg && m_spellCheckDlg->isVisible())
        m_spellCheckDlg->retarget(editor);
}

void MainWindow::disconnectTabEditor(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    Editor *editor = m_tabs[index].editor;
    if (!editor)
        return;

    if (m_updateTimer)
        disconnect(editor, &QTextEdit::textChanged, m_updateTimer, nullptr);

    disconnect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
               this, &MainWindow::onEditorScroll);
}

void MainWindow::disconnectActiveEditor()
{
    disconnectTabEditor(m_connectedTabIndex);
    m_connectedTabIndex = -1;
}

void MainWindow::connectActiveEditor()
{
    int idx = m_tabBar->currentIndex();
    connectTabEditor(idx);
}

void MainWindow::updateTabLabel(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    const TabInfo &info = m_tabs[index];
    QString name;
    if (m_reportTitles.contains(index))
        name = m_reportTitles.value(index);
    else if (m_tocTabs.contains(index))
        name = m_tocTabs.value(index);
    else
        name = info.filePath.isEmpty() ? QStringLiteral("Untitled")
                                       : QFileInfo(info.filePath).fileName();
    if (info.dirty)
        name += QStringLiteral(" *");
    if (index == m_tabBar->currentIndex())
        updateWindowTitle();
    m_tabBar->setTabText(index, name);
}

void MainWindow::updateWindowTitle()
{
    if (!m_corpus.filePath.isEmpty()) {
        setWindowTitle(QFileInfo(m_corpus.filePath).completeBaseName()
                       + QStringLiteral(" — Scriba"));
        return;
    }
    TabInfo *info = activeTabInfo();
    if (!info) {
        setWindowTitle(QStringLiteral("Scriba"));
        return;
    }
    QString title = info->filePath.isEmpty()
        ? QStringLiteral("Scriba - Untitled")
        : QStringLiteral("Scriba - ") + info->filePath;
    if (info->dirty)
        title += QStringLiteral(" *");
    setWindowTitle(title);
}

void MainWindow::setTabDirty(int index, bool dirty)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    if (m_tabs[index].dirty == dirty)
        return;
    m_tabs[index].dirty = dirty;
    updateTabLabel(index);
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    connectActiveEditor();
    if (auto *ed = currentEditor())
        ed->setFocus();

    TabInfo *info = activeTabInfo();
    if (info) {
        updateWindowTitle();
        m_preview->setDocumentPath(info->filePath);
        if (m_previewInitialized) {
            // The preview page stays alive across tab switches: push the new
            // tab's cached render through the incremental scribaUpdate() path
            // instead of reloading the whole page. The old content is still
            // on screen here, so a pre-scroll to the new editor's top line is
            // cosmetic only; the real anchor lands via the post-settle re-assert
            // scheduled in updatePreview() (the JS restore skips tab switches).
            QSettings settings;
            if (settings.value(Preferences::SyncScroll, true).toBool()) {
                m_lastSyncLine = -1.0;
                m_preview->scrollToSourceLine(currentEditorTopSourceLine());
            }
            updatePreview(true);
        } else {
            updatePreview();
        }

        applyEditorContentWidth(info->editor);
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    if (m_tabs.size() <= 1) {
        showSaveDiscardDialog(index);
        if (m_tabs.size() == 1 && !m_tabs[0].dirty) {
            m_tabs[0].filePath.clear();
            m_tabs[0].editor->clear();
            m_tabs[0].previewHtmlValid = false;
            m_tabs[0].dirty = false;
            updateTabLabel(0);
            updateWindowTitle();
            m_preview->setDocumentPath(QString());
            m_previewInitialized = false;
            updatePreview();
        }
        return;
    }

    showSaveDiscardDialog(index);
}

void MainWindow::showSaveDiscardDialog(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;

    TabInfo &info = m_tabs[index];
    if (!info.dirty) {
        removeTab(index);
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("Unsaved Changes");
    msgBox.setText(QString("Do you want to save changes to \"%1\"?")
        .arg(info.filePath.isEmpty() ? QStringLiteral("Untitled")
                                     : QFileInfo(info.filePath).fileName()));
    msgBox.setInformativeText("Your changes will be lost if you don't save them.");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
    auto *discardBtn = msgBox.addButton(tr("&Discard"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);

    int ret = msgBox.exec();
    if (ret == QMessageBox::Save) {
        if (info.filePath.isEmpty()) {
                        QString file = ensureDefaultSuffix(
                QFileDialog::getSaveFileName(this, "Save File", QString(), kMdFilter), "md");
            if (file.isEmpty())
                return;
            info.filePath = file;
        }
        saveFile(info.filePath);
        removeTab(index);
    } else if (msgBox.clickedButton() == discardBtn) {
        removeTab(index);
    }
}

void MainWindow::closeCurrentTab()
{
    int idx = m_tabBar->currentIndex();
    onTabCloseRequested(idx);
}

void MainWindow::closeAllTabs()
{
    while (m_tabs.size() > 1) {
        int idx = m_tabBar->currentIndex();
        removeTab(idx);
    }
    if (m_tabs.size() == 1) {
        int idx = 0;
        disconnectTabEditor(idx);
        m_tabs[0].editor->clear();
        m_tabs[0].previewHtmlValid = false;
        m_tabs[0].filePath.clear();
        m_tabs[0].dirty = false;
        // A virtual TOC/report tab left as the sole tab is blanked into an
        // Untitled placeholder; drop its mapping so it can't haunt the next corpus.
        m_tocTabs.remove(idx);
        m_reportTitles.remove(idx);
    }
}

QString MainWindow::saveAsDialogPath()
{
    return ensureDefaultSuffix(
        QFileDialog::getSaveFileName(this, "Save File", QString(), kMdFilter), "md");
}

MainWindow::ClosePromptResult MainWindow::promptUnsavedChanges(bool hasUntitledDirty)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setWindowTitle("Unsaved Changes");
    if (hasUntitledDirty) {
        msgBox.setText("There are unsaved changes in untitled tabs.\n"
            "Save all before closing?");
    } else {
        msgBox.setText("There are unsaved changes.\n"
            "Save all before closing?");
    }
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
    auto *discardBtn = msgBox.addButton(tr("&Discard"), QMessageBox::DestructiveRole);
    msgBox.setDefaultButton(QMessageBox::Save);
    msgBox.setEscapeButton(QMessageBox::Cancel);
    auto ret = msgBox.exec();

    if (ret == QMessageBox::Cancel)
        return ClosePromptResult::Cancel;
    if (ret == QMessageBox::Save)
        return ClosePromptResult::Save;
    return ClosePromptResult::Discard;
}

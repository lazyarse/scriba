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
#include "PreferencesDialog.h"
#include "Preferences.h"
#include "StaticHelpers.h"
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

void PreferencesDialog::setupCorpusPage()
{
    QSettings settings;

    /* --- Page 7: Corpus --- */
    {
        QWidget *page = addPage(tr("Corpus"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        const bool corpusOpen = m_corpus && !m_corpus->filePath.isEmpty();

        QGroupBox *startupGroup = new QGroupBox("Startup");
        QVBoxLayout *startupLayout = new QVBoxLayout(startupGroup);
        startupLayout->addSpacing(8);

        m_reopenCheck = new QCheckBox("Open last corpus on startup");
        m_reopenCheck->setObjectName("corpus-reopen-startup");
        m_reopenCheck->setChecked(settings.value(Preferences::ReopenLastCorpus, true).toBool());
        startupLayout->addWidget(m_reopenCheck);

        layout->addWidget(startupGroup);

        QGroupBox *recentGroup = new QGroupBox("Recent Corpora");
        QVBoxLayout *recentLayout = new QVBoxLayout(recentGroup);
        recentLayout->addSpacing(8);

        m_recentCorpusList = new QListWidget;
        m_recentCorpusList->setObjectName("corpus-recent-list");
        m_recentCorpusList->setSelectionMode(QAbstractItemView::SingleSelection);
        m_recentCorpusList->setMinimumHeight(60);
        m_recentCorpusList->addItems(
            settings.value(Preferences::RecentCorpora).toStringList().mid(0, Preferences::MaxRecentCorpora));
        recentLayout->addWidget(m_recentCorpusList);

        QHBoxLayout *recentButtons = new QHBoxLayout();
        auto *clearRecentsBtn = new QPushButton(tr("&Clear List"));
        auto *removeRecentBtn = new QPushButton(tr("&Remove Selected"));
        removeRecentBtn->setEnabled(false);
        recentButtons->addWidget(clearRecentsBtn);
        recentButtons->addWidget(removeRecentBtn);
        recentButtons->addStretch();
        stripButtonIcons({clearRecentsBtn, removeRecentBtn});
        recentLayout->addLayout(recentButtons);

        connect(clearRecentsBtn, &QPushButton::clicked, this, [this]() {
            m_recentCorpusList->clear();
        });
        connect(removeRecentBtn, &QPushButton::clicked, this, [this]() {
            delete m_recentCorpusList->currentItem();
        });
        connect(m_recentCorpusList, &QListWidget::itemSelectionChanged, this,
                [this, removeRecentBtn]() {
                    removeRecentBtn->setEnabled(m_recentCorpusList->currentItem() != nullptr);
                });

        layout->addWidget(recentGroup);

        QGroupBox *monitorGroup = new QGroupBox("Monitoring");
        QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
        monitorLayout->addSpacing(8);

        m_corpusMonitorCheck = new QCheckBox("Monitor corpus directory for external changes");
        m_corpusMonitorCheck->setObjectName("corpus-monitor");
        m_corpusMonitorCheck->setChecked(m_corpus ? m_corpus->monitor : true);
        m_corpusMonitorCheck->setEnabled(corpusOpen);
        monitorLayout->addWidget(m_corpusMonitorCheck);

        QHBoxLayout *editPolicyRow = new QHBoxLayout();
        auto *editPolicyLabel = new QLabel(tr("When a document changes on disk:"));
        editPolicyRow->addWidget(editPolicyLabel);
        m_corpusEditPolicyCombo = new QComboBox;
        m_corpusEditPolicyCombo->setObjectName("corpus-edit-policy");
        m_corpusEditPolicyCombo->addItem(tr("Reload clean tabs; prompt when dirty"), "autoReload");
        m_corpusEditPolicyCombo->addItem(tr("Always prompt"), "prompt");
        m_corpusEditPolicyCombo->addItem(tr("Auto-reload always"), "autoReloadDirty");
        const QString editPolicy = settings.value(
            Preferences::CorpusExternalEditPolicy, QStringLiteral("autoReload")).toString();
        const int epIdx = m_corpusEditPolicyCombo->findData(editPolicy);
        m_corpusEditPolicyCombo->setCurrentIndex(epIdx < 0 ? 0 : epIdx);
        editPolicyRow->addWidget(m_corpusEditPolicyCombo, 1);
        monitorLayout->addLayout(editPolicyRow);

        layout->addWidget(monitorGroup);

        QGroupBox *linksGroup = new QGroupBox("Links");
        QVBoxLayout *linksLayout = new QVBoxLayout(linksGroup);
        linksLayout->addSpacing(8);

        QHBoxLayout *rewritePolicyRow = new QHBoxLayout();
        auto *rewritePolicyLabel = new QLabel(tr("When a corpus document is renamed/moved:"));
        rewritePolicyRow->addWidget(rewritePolicyLabel);
        m_linkRewritePolicyCombo = new QComboBox;
        m_linkRewritePolicyCombo->setObjectName("corpus-link-rewrite-policy");
        m_linkRewritePolicyCombo->addItem(tr("Ask me first"), "prompt");
        m_linkRewritePolicyCombo->addItem(tr("Rewrite links automatically"), "silent");
        m_linkRewritePolicyCombo->addItem(tr("Do nothing"), "ignore");
        const QString rewritePolicy = settings.value(
            Preferences::CorpusLinkRewritePolicy, QStringLiteral("prompt")).toString();
        const int rwIdx = m_linkRewritePolicyCombo->findData(rewritePolicy);
        m_linkRewritePolicyCombo->setCurrentIndex(rwIdx < 0 ? 0 : rwIdx);
        rewritePolicyRow->addWidget(m_linkRewritePolicyCombo, 1);
        linksLayout->addLayout(rewritePolicyRow);

        QHBoxLayout *scopeRow = new QHBoxLayout();
        auto *scopeLabel = new QLabel(tr("Links to update:"));
        scopeRow->addWidget(scopeLabel);
        m_linkRewriteScopeCombo = new QComboBox;
        m_linkRewriteScopeCombo->setObjectName("corpus-link-rewrite-scope");
        m_linkRewriteScopeCombo->addItem(tr("Only open documents"), "open");
        m_linkRewriteScopeCombo->addItem(tr("All corpus documents"), "all");
        const QString scope = settings.value(
            Preferences::CorpusLinkRewriteScope, QStringLiteral("open")).toString();
        const int scIdx = m_linkRewriteScopeCombo->findData(scope);
        m_linkRewriteScopeCombo->setCurrentIndex(scIdx < 0 ? 0 : scIdx);
        scopeRow->addWidget(m_linkRewriteScopeCombo, 1);
        linksLayout->addLayout(scopeRow);

        layout->addWidget(linksGroup);

        QGroupBox *externalGroup = new QGroupBox("External Documents");
        QVBoxLayout *externalLayout = new QVBoxLayout(externalGroup);
        externalLayout->addSpacing(8);

        QHBoxLayout *exportRow = new QHBoxLayout();
        auto *exportLabel = new QLabel(tr("Documents outside the corpus root:"));
        exportRow->addWidget(exportLabel);
        m_externalExportCombo = new QComboBox;
        m_externalExportCombo->setObjectName("corpus-external-export");
        m_externalExportCombo->addItem(tr("Don't export"), QString());
        m_externalExportCombo->addItem(tr("Export to subfolder"), "subfolder");
        exportRow->addWidget(m_externalExportCombo, 1);
        externalLayout->addLayout(exportRow);

        QHBoxLayout *folderRow = new QHBoxLayout();
        auto *folderLabel = new QLabel(tr("Export subfolder:"));
        folderRow->addWidget(folderLabel);
        m_externalExportDirEdit = new QLineEdit;
        m_externalExportDirEdit->setObjectName("corpus-external-export-dir");
        m_externalExportDirEdit->setPlaceholderText("external");
        folderRow->addWidget(m_externalExportDirEdit, 1);
        externalLayout->addLayout(folderRow);

        const QString exportDir = settings.value(Preferences::CorpusExternalExportDirName).toString();
        const bool exportOn = !exportDir.isEmpty();
        m_externalExportCombo->setCurrentIndex(exportOn ? 1 : 0);
        m_externalExportDirEdit->setText(exportOn ? exportDir : QString());
        m_externalExportDirEdit->setEnabled(exportOn);
        connect(m_externalExportCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            m_externalExportDirEdit->setEnabled(index == 1);
        });

        layout->addWidget(externalGroup);

        layout->addStretch();

    }
}

void PreferencesDialog::setupSecurityPage()
{
    QSettings settings;

    /* --- Page 6: Security --- */
    {
        QWidget *page = addPage(tr("Security"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *previewGroup = new QGroupBox("Preview");
        QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
        previewLayout->addSpacing(8);

        m_stripPreviewScriptsCheck = new QCheckBox("Strip <script> tags from markdown content");
        m_stripPreviewScriptsCheck->setChecked(settings.value(Preferences::StripPreviewScripts, true).toBool());
        previewLayout->addWidget(m_stripPreviewScriptsCheck);

        m_blockRawHtmlPreviewCheck = new QCheckBox("Block raw HTML at parser level");
        m_blockRawHtmlPreviewCheck->setChecked(settings.value(Preferences::BlockRawHtmlPreview, true).toBool());
        previewLayout->addWidget(m_blockRawHtmlPreviewCheck);

        m_enableCspPreviewCheck = new QCheckBox("Enable Content Security Policy (blocks inline event handlers, javascript: URLs, external resources)");
        m_enableCspPreviewCheck->setChecked(settings.value(Preferences::EnableCspPreview, true).toBool());
        previewLayout->addWidget(m_enableCspPreviewCheck);

        m_showPageBreaksCheck = new QCheckBox("Show page breaks in preview (print layout)");
        m_showPageBreaksCheck->setChecked(settings.value(Preferences::PreviewShowPageBreaks, false).toBool());
        previewLayout->addWidget(m_showPageBreaksCheck);

        layout->addWidget(previewGroup);

        QGroupBox *exportGroup = new QGroupBox("Export (PDF, DOCX, HTML)");
        QVBoxLayout *exportLayout = new QVBoxLayout(exportGroup);
        exportLayout->addSpacing(8);

        m_stripExportScriptsCheck = new QCheckBox("Strip <script> tags from markdown content");
        m_stripExportScriptsCheck->setChecked(settings.value(Preferences::StripExportScripts, true).toBool());
        exportLayout->addWidget(m_stripExportScriptsCheck);

        m_blockRawHtmlExportCheck = new QCheckBox("Block raw HTML at parser level");
        m_blockRawHtmlExportCheck->setChecked(settings.value(Preferences::BlockRawHtmlExport, true).toBool());
        exportLayout->addWidget(m_blockRawHtmlExportCheck);

        m_enableCspExportCheck = new QCheckBox("Enable Content Security Policy (blocks inline event handlers, javascript: URLs, external resources)");
        m_enableCspExportCheck->setChecked(settings.value(Preferences::EnableCspExport, true).toBool());
        exportLayout->addWidget(m_enableCspExportCheck);

        layout->addWidget(exportGroup);

        auto *cspNote = new QLabel(
            "Content Security Policy restricts what resources can execute in the preview or exported HTML. "
            "The app requires 'unsafe-inline' for both script and style because bundled JS libraries "
            "(KaTeX, Mermaid, highlight.js, ECharts) and the app's own initialization code use inline "
            "scripts and styles. A stricter CSP would break rendering. "
            "The current policy blocks inline event handlers (onclick, onerror), javascript: URLs, "
            "and external network requests.");
        cspNote->setWordWrap(true);
        cspNote->setStyleSheet("color: gray; padding: 8px;");
        layout->addWidget(cspNote);

        layout->addStretch();

    }
}

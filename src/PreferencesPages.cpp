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
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>

void PreferencesDialog::setupGeneralPage()
{
    QSettings settings;

    /* --- Page 0: General --- */
    {
        QWidget *page = addPage(tr("General"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        m_syncCheck = new QCheckBox("Sync editor and preview scrolling");
        m_syncCheck->setChecked(settings.value(Preferences::SyncScroll, true).toBool());
        layout->addWidget(m_syncCheck);

        QGroupBox *autoCompleteGroup = new QGroupBox("Autocomplete");
        QVBoxLayout *autoCompleteLayout = new QVBoxLayout(autoCompleteGroup);
        autoCompleteLayout->addSpacing(8);

        m_filenameAutoCompleteCheck = new QCheckBox("Enable filename autocomplete");
        m_filenameAutoCompleteCheck->setChecked(settings.value(Preferences::FileAutoComplete, true).toBool());
        autoCompleteLayout->addWidget(m_filenameAutoCompleteCheck);

        QHBoxLayout *compRow = new QHBoxLayout();
        compRow->addWidget(new QLabel("Filename autocomplete limit:"));
        m_fileCompletionSpin = new QSpinBox();
        m_fileCompletionSpin->setRange(2, 100);
        m_fileCompletionSpin->setValue(settings.value(Preferences::FileCompletionLimit, 20).toInt());
        compRow->addWidget(m_fileCompletionSpin);
        compRow->addStretch();
        autoCompleteLayout->addLayout(compRow);

        m_emojiAutoCompleteCheck = new QCheckBox("Use emoji auto-complete");
        m_emojiAutoCompleteCheck->setChecked(settings.value(Preferences::EmojiAutoComplete, true).toBool());
        autoCompleteLayout->addWidget(m_emojiAutoCompleteCheck);

        QHBoxLayout *emojiCompRow = new QHBoxLayout();
        emojiCompRow->addWidget(new QLabel("Emoji autocomplete limit:"));
        m_emojiCompletionSpin = new QSpinBox();
        m_emojiCompletionSpin->setRange(5, 500);
        m_emojiCompletionSpin->setValue(settings.value(Preferences::EmojiCompletionLimit, 100).toInt());
        emojiCompRow->addWidget(m_emojiCompletionSpin);
        emojiCompRow->addStretch();
        autoCompleteLayout->addLayout(emojiCompRow);

        m_languageAutoCompleteCheck = new QCheckBox("Enable code language autocomplete");
        m_languageAutoCompleteCheck->setChecked(settings.value(Preferences::LanguageAutoComplete, true).toBool());
        autoCompleteLayout->addWidget(m_languageAutoCompleteCheck);

        layout->addWidget(autoCompleteGroup);

        QGroupBox *singleViewGroup = new QGroupBox("Single Pane View (Editor/Preview-Only View)");
        QVBoxLayout *singleViewLayout = new QVBoxLayout(singleViewGroup);
        singleViewLayout->addSpacing(8);

        m_centreSingleViewCheck = new QCheckBox("Centre editor/preview content on single view");
        m_centreSingleViewCheck->setChecked(settings.value(Preferences::CentreSingleViewContent, true).toBool());
        singleViewLayout->addWidget(m_centreSingleViewCheck);

        QHBoxLayout *widthRow = new QHBoxLayout();
        widthRow->addWidget(new QLabel("Content width:"));
        m_centreSingleViewWidthSpin = new QSpinBox();
        m_centreSingleViewWidthSpin->setRange(400, 2000);
        m_centreSingleViewWidthSpin->setSuffix(" px");
        m_centreSingleViewWidthSpin->setValue(settings.value(Preferences::CentreSingleViewWidth, 800).toInt());
        connect(m_centreSingleViewCheck, &QCheckBox::toggled,
                this, &PreferencesDialog::updateContentWidthEnable);
        widthRow->addWidget(m_centreSingleViewWidthSpin);
        widthRow->addStretch();
        singleViewLayout->addLayout(widthRow);

        layout->addWidget(singleViewGroup);

        QGroupBox *splitViewGroup = new QGroupBox("Split View Content Width");
        QVBoxLayout *splitViewLayout = new QVBoxLayout(splitViewGroup);
        splitViewLayout->addSpacing(8);

        auto makeWidthRow = [this, splitViewLayout](const QString &label, QSpinBox *&spin, QCheckBox *&autoCheck) {
            QHBoxLayout *row = new QHBoxLayout();
            auto *lbl = new QLabel(label);
            spin = new QSpinBox();
            spin->setRange(300, 2000);
            spin->setSuffix(" px");
            lbl->setBuddy(spin);
            autoCheck = new QCheckBox("Auto (fill pane)");
            connect(autoCheck, &QCheckBox::toggled,
                    this, &PreferencesDialog::updateContentWidthEnable);
            row->addWidget(lbl);
            row->addWidget(spin);
            row->addWidget(autoCheck);
            row->addStretch();
            splitViewLayout->addLayout(row);
        };
        int editorWidth = settings.value(Preferences::SplitViewEditorMaxWidth, 0).toInt();
        int previewWidth = settings.value(Preferences::SplitViewPreviewMaxWidth, 0).toInt();
        makeWidthRow("Editor max width:", m_splitEditorWidthSpin, m_splitEditorAutoCheck);
        makeWidthRow("Preview max width:", m_splitPreviewWidthSpin, m_splitPreviewAutoCheck);
        m_splitEditorAutoCheck->setChecked(editorWidth <= 0);
        m_splitEditorWidthSpin->setValue(editorWidth > 0 ? editorWidth : 800);
        m_splitPreviewAutoCheck->setChecked(previewWidth <= 0);
        m_splitPreviewWidthSpin->setValue(previewWidth > 0 ? previewWidth : 800);

        layout->addWidget(splitViewGroup);

        QGroupBox *wrapGroup = new QGroupBox("Editor Line Wrap");
        QVBoxLayout *wrapLayout = new QVBoxLayout(wrapGroup);
        wrapLayout->addSpacing(8);

        QHBoxLayout *wrapModeRow = new QHBoxLayout();
        wrapModeRow->addWidget(new QLabel("Wrap text:"));
        m_wrapModeCombo = new QComboBox();
        m_wrapModeCombo->addItem("Off", "no-wrap");
        m_wrapModeCombo->addItem("At window width", "window");
        m_wrapModeCombo->addItem("At column", "column");
        {
            const bool wrapEnabled = settings.value(Preferences::EditorWrapEnabled, true).toBool();
            const QString wrapMode = settings.value(Preferences::EditorWrapMode,
                                                     QStringLiteral("window")).toString();
            int idx = wrapMode == QLatin1String("column") ? 2 : 1;
            if (!wrapEnabled)
                idx = 0;
            m_wrapModeCombo->setCurrentIndex(idx);
        }
        wrapModeRow->addWidget(m_wrapModeCombo);
        wrapModeRow->addStretch();
        wrapLayout->addLayout(wrapModeRow);

        QHBoxLayout *wrapColRow = new QHBoxLayout();
        wrapColRow->addWidget(new QLabel("Wrap column:"));
        m_wrapColumnSpin = new QSpinBox();
        m_wrapColumnSpin->setRange(40, 400);
        m_wrapColumnSpin->setSuffix(" chars");
        m_wrapColumnSpin->setValue(settings.value(Preferences::EditorWrapColumn,
                                                   Preferences::DefaultEditorWrapColumn).toInt());
        wrapColRow->addWidget(m_wrapColumnSpin);
        wrapColRow->addStretch();
        wrapLayout->addLayout(wrapColRow);

        QLabel *wrapHint = new QLabel(
            "When wrapping at a column, that column becomes the editor's max width.");
        wrapHint->setWordWrap(true);
        wrapHint->setStyleSheet("color:#888;");
        wrapLayout->addWidget(wrapHint);

        connect(m_wrapModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &PreferencesDialog::updateContentWidthEnable);
        layout->addWidget(wrapGroup);

        updateContentWidthEnable();

        QGroupBox *autoSaveGroup = new QGroupBox("Auto-Save");
        QVBoxLayout *autoSaveLayout = new QVBoxLayout(autoSaveGroup);
        autoSaveLayout->addSpacing(8);

        m_autoSaveExitCheck = new QCheckBox("Auto-save on exit");
        m_autoSaveExitCheck->setChecked(settings.value(Preferences::AutoSaveOnExit, false).toBool());
        autoSaveLayout->addWidget(m_autoSaveExitCheck);

        QHBoxLayout *intervalRow = new QHBoxLayout();
        m_autoSaveCheck = new QCheckBox("Auto-save every");
        m_autoSaveCheck->setChecked(settings.value(Preferences::AutoSaveInterval, 0).toInt() > 0);
        m_autoSaveSpin = new QSpinBox();
        m_autoSaveSpin->setRange(1, 60);
        m_autoSaveSpin->setValue(settings.value(Preferences::AutoSaveInterval, 5).toInt());
        m_autoSaveSpin->setSuffix(" minutes");
        m_autoSaveSpin->setEnabled(m_autoSaveCheck->isChecked());
        connect(m_autoSaveCheck, &QCheckBox::toggled, m_autoSaveSpin, &QSpinBox::setEnabled);
        intervalRow->addWidget(m_autoSaveCheck);
        intervalRow->addWidget(m_autoSaveSpin);
        intervalRow->addStretch();
        autoSaveLayout->addLayout(intervalRow);

        layout->addWidget(autoSaveGroup);

        QGroupBox *importGroup = new QGroupBox("Imported Documents");
        QVBoxLayout *importLayout = new QVBoxLayout(importGroup);
        importLayout->addSpacing(8);

        const QString imgLocation = settings.value(Preferences::ImportImageLocation,
            QStringLiteral("currentDir")).toString();

        m_imgCurrentDir = new QRadioButton("Save imported images next to the document");
        m_imgCustomDir = new QRadioButton("Save in a specific folder");
        m_imgTempDir = new QRadioButton("Save in the system temp folder until saved");
        m_imgAsk = new QRadioButton("Ask each time");

        m_imgCurrentDir->setChecked(imgLocation == QLatin1String("currentDir"));
        m_imgCustomDir->setChecked(imgLocation == QLatin1String("customDir"));
        m_imgTempDir->setChecked(imgLocation == QLatin1String("tempDir"));
        m_imgAsk->setChecked(imgLocation == QLatin1String("ask"));
        if (!m_imgCurrentDir->isChecked() && !m_imgCustomDir->isChecked()
            && !m_imgTempDir->isChecked())
            m_imgAsk->setChecked(true);

        importLayout->addWidget(m_imgCurrentDir);
        importLayout->addWidget(m_imgCustomDir);
        importLayout->addWidget(m_imgTempDir);
        importLayout->addWidget(m_imgAsk);

        QHBoxLayout *imgDirRow = new QHBoxLayout();
        m_imgDirEdit = new QLineEdit(settings.value(Preferences::ImportImageDir).toString());
        m_imgDirBrowse = new QPushButton("Browse...");
        stripButtonIcon(m_imgDirBrowse);
        connect(m_imgDirBrowse, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(
                this, "Choose Image Folder", m_imgDirEdit->text());
            if (!dir.isEmpty())
                m_imgDirEdit->setText(dir);
        });
        imgDirRow->addWidget(m_imgDirEdit, 1);
        imgDirRow->addWidget(m_imgDirBrowse);
        auto enableImgDir = [this]() {
            m_imgDirEdit->setEnabled(m_imgCustomDir->isChecked());
            m_imgDirBrowse->setEnabled(m_imgCustomDir->isChecked());
        };
        enableImgDir();
        connect(m_imgCustomDir, &QRadioButton::toggled, this, enableImgDir);
        importLayout->addLayout(imgDirRow);

        QLabel *importHint = new QLabel(
            "When importing a Word document, embedded images are written "
            "according to the chosen location. If \"next to the document\" is "
            "selected while the document has not been saved yet, you will be "
            "asked where to put them.");
        importHint->setWordWrap(true);
        importHint->setStyleSheet("color: gray;");
        importLayout->addWidget(importHint);

        layout->addWidget(importGroup);

        layout->addStretch();

    }
}

void PreferencesDialog::setupPreviewPage()
{
    QSettings settings;

    /* --- Page 3: Preview --- */
    {
        QWidget *page = addPage(tr("Preview"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *renderGroup = new QGroupBox("Rendering");
        QVBoxLayout *renderLayout = new QVBoxLayout(renderGroup);
        renderLayout->addSpacing(8);

        m_hardSoftBreaksCheck = new QCheckBox(
            "Treat single line breaks as hard breaks (<br>) in the preview and exports");
        m_hardSoftBreaksCheck->setToolTip(tr("By default a single newline in a paragraph is a "
            "soft break (rendered as a space). Enable to force every line break to render as a "
            "new line."));
        m_hardSoftBreaksCheck->setChecked(settings.value(Preferences::HardSoftBreaks, false).toBool());
        renderLayout->addWidget(m_hardSoftBreaksCheck);

        layout->addWidget(renderGroup);

        layout->addStretch();

    }
}

void PreferencesDialog::setupPrintingPage()
{
    QSettings settings;

    /* --- Page: Printing --- */
    {
        QWidget *page = addPage(tr("Printing"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *printGroup = new QGroupBox("PDF Typesetting");
        QVBoxLayout *printLayout = new QVBoxLayout(printGroup);
        printLayout->addSpacing(8);

        auto *note = new QLabel(tr(
            "Default typesetting for PDF export. These are the saved defaults; the "
            "export dialog's Typesetting group can override them for a single export."));
        note->setWordWrap(true);
        note->setStyleSheet("color: gray; padding: 8px;");
        printLayout->addWidget(note);

        auto *splitRow = new QHBoxLayout;
        auto *splitLabel = new QLabel(tr("Split code blocks:"));
        m_printCodeSplitCombo = new QComboBox;
        m_printCodeSplitCombo->setObjectName(QStringLiteral("printing-code-split"));
        m_printCodeSplitCombo->addItem(tr("Never"), QVariant(QStringLiteral("never")));
        m_printCodeSplitCombo->addItem(tr("Blocks over 50 lines"), QVariant(QStringLiteral("small")));
        m_printCodeSplitCombo->addItem(tr("Blocks over 100 lines"), QVariant(QStringLiteral("large")));
        const int splitIdx = m_printCodeSplitCombo->findData(
            settings.value(Preferences::PrintCodeSplit, QStringLiteral("never")).toString());
        m_printCodeSplitCombo->setCurrentIndex(qMax(0, splitIdx));
        splitRow->addWidget(splitLabel);
        splitRow->addWidget(m_printCodeSplitCombo, 1);
        printLayout->addLayout(splitRow);

        m_printKeepTablesCheck = new QCheckBox(tr("Keep tables together"));
        m_printKeepTablesCheck->setObjectName(QStringLiteral("printing-keep-tables"));
        m_printKeepTablesCheck->setChecked(settings.value(Preferences::PrintKeepTables, true).toBool());
        printLayout->addWidget(m_printKeepTablesCheck);
        m_printKeepHeadingsCheck = new QCheckBox(tr("Keep headings with following text"));
        m_printKeepHeadingsCheck->setObjectName(QStringLiteral("printing-keep-headings"));
        m_printKeepHeadingsCheck->setChecked(settings.value(Preferences::PrintKeepHeadings, true).toBool());
        printLayout->addWidget(m_printKeepHeadingsCheck);
        m_printKeepFiguresCheck = new QCheckBox(tr("Keep figures and quotes together"));
        m_printKeepFiguresCheck->setObjectName(QStringLiteral("printing-keep-figures"));
        m_printKeepFiguresCheck->setChecked(settings.value(Preferences::PrintKeepFigures, true).toBool());
        printLayout->addWidget(m_printKeepFiguresCheck);
        m_printOrphanControlCheck = new QCheckBox(tr("Avoid orphan/widow lines"));
        m_printOrphanControlCheck->setObjectName(QStringLiteral("printing-orphan-control"));
        m_printOrphanControlCheck->setChecked(settings.value(Preferences::PrintOrphanControl, true).toBool());
        printLayout->addWidget(m_printOrphanControlCheck);

        auto *geoRow = new QHBoxLayout;
        geoRow->addWidget(new QLabel(tr("Margin:")));
        m_printMarginEdit = new QLineEdit;
        m_printMarginEdit->setObjectName(QStringLiteral("printing-margin"));
        m_printMarginEdit->setPlaceholderText(tr("e.g. 18mm"));
        m_printMarginEdit->setText(settings.value(Preferences::PrintPageMargin, QString()).toString());
        geoRow->addWidget(m_printMarginEdit, 1);
        geoRow->addWidget(new QLabel(tr("Page size:")));
        m_printSizeEdit = new QLineEdit;
        m_printSizeEdit->setObjectName(QStringLiteral("printing-size"));
        m_printSizeEdit->setPlaceholderText(tr("e.g. A4"));
        m_printSizeEdit->setText(settings.value(Preferences::PrintPageSize, QString()).toString());
        geoRow->addWidget(m_printSizeEdit, 1);
        printLayout->addLayout(geoRow);

        layout->addWidget(printGroup);
        layout->addStretch();

    }
}

void PreferencesDialog::setupAdvancedPage()
{
    QSettings settings;

    /* --- Page: Advanced --- */
    {
        QWidget *page = addPage(tr("Advanced"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *renderGroup = new QGroupBox("Preview Render Timing");
        QVBoxLayout *renderLayout = new QVBoxLayout(renderGroup);
        renderLayout->addSpacing(8);

        auto *renderLabel = new QLabel("The initial render delay is how long the live preview "
            "waits after an edit before re-rendering the document. The heavy render delay is "
            "how long you must pause typing before diagrams, equations and charts are "
            "re-rendered. Lower values feel more responsive on small documents; higher values "
            "save CPU on large ones.");
        renderLabel->setWordWrap(true);
        renderLayout->addWidget(renderLabel);

        m_previewUpdateDelaySpin = new QSpinBox();
        m_previewUpdateDelaySpin->setRange(10, 1000);
        m_previewUpdateDelaySpin->setSingleStep(10);
        m_previewUpdateDelaySpin->setSuffix(" ms");
        m_previewUpdateDelaySpin->setValue(settings.value(Preferences::PreviewUpdateDelay,
            Preferences::DefaultPreviewUpdateDelay).toInt());

        m_heavyRenderDelaySpin = new QSpinBox();
        m_heavyRenderDelaySpin->setRange(200, 5000);
        m_heavyRenderDelaySpin->setSingleStep(150);
        m_heavyRenderDelaySpin->setSuffix(" ms");
        m_heavyRenderDelaySpin->setValue(settings.value(Preferences::HeavyRenderDelay,
            Preferences::DefaultHeavyRenderDelay).toInt());

        auto *renderForm = new QFormLayout;
        renderForm->addRow("Initial preview render delay:", m_previewUpdateDelaySpin);
        renderForm->addRow("Heavy render delay:", m_heavyRenderDelaySpin);
        renderLayout->addLayout(renderForm);

        layout->addWidget(renderGroup);
        layout->addStretch();

    }
}

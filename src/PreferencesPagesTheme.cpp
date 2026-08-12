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
#include <QRadioButton>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QColorDialog>
#include <QIcon>
#include <QPixmap>
#include <QSize>

void PreferencesDialog::setupThemesPage()
{
    QSettings settings;

    /* --- Page 1: Themes --- */
    {
        QWidget *page = addPage(tr("Themes"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        /* --- Appearance panel --- */
        QGroupBox *appearanceGroup = new QGroupBox("Appearance");
        QVBoxLayout *appearanceLayout = new QVBoxLayout(appearanceGroup);
        appearanceLayout->addSpacing(8);

        m_stripeCheck = new QCheckBox("Alternating table row colors");
        m_stripeCheck->setChecked(settings.value(Preferences::TableStriping, true).toBool());
        appearanceLayout->addWidget(m_stripeCheck);

        m_showCodeLangPreviewCheck = new QCheckBox("Show language label on fenced code blocks (preview)");
        m_showCodeLangPreviewCheck->setChecked(settings.value(Preferences::ShowCodeLangPreview, true).toBool());
        appearanceLayout->addWidget(m_showCodeLangPreviewCheck);

        m_showCodeLangExportCheck = new QCheckBox("Show language label on fenced code blocks (exports)");
        m_showCodeLangExportCheck->setChecked(settings.value(Preferences::ShowCodeLangExport, true).toBool());
        appearanceLayout->addWidget(m_showCodeLangExportCheck);

        appearanceLayout->addSpacing(4);
        auto *uiFontLabel = new QLabel("<b>UI font size</b>");
        appearanceLayout->addWidget(uiFontLabel);

        m_uiFontSizeSpin = new QSpinBox();
        m_uiFontSizeSpin->setRange(8, 24);
        m_uiFontSizeSpin->setSuffix(" pt");
        m_uiFontSizeSpin->setValue(settings.value(Preferences::UiFontSize, Preferences::DefaultUiFontSize).toInt());
        auto *uiFontForm = new QFormLayout;
        uiFontForm->addRow("Dialogs, menus & chrome:", m_uiFontSizeSpin);
        appearanceLayout->addLayout(uiFontForm);
        connect(m_uiFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) { emit uiFontSizeChanged(v); });

        appearanceLayout->addSpacing(4);
        auto *emojiLabel = new QLabel("<b>Emoji rendering</b>");
        appearanceLayout->addWidget(emojiLabel);

        auto mode = Preferences::emojiRenderingFromString(
            settings.value(Preferences::EmojiMode, Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString());
        m_emojiBw = new QRadioButton("Black && White");
        m_emojiColor = new QRadioButton("Color (twemoji)");
        m_emojiBw->setChecked(mode == Preferences::EmojiRendering::Bw);
        m_emojiColor->setChecked(mode == Preferences::EmojiRendering::Color);
        appearanceLayout->addWidget(m_emojiBw);
        appearanceLayout->addWidget(m_emojiColor);

        layout->addWidget(appearanceGroup);

        /* --- Base CSS panel --- */
        QGroupBox *baseCssGroup = new QGroupBox("Base CSS");
        QVBoxLayout *baseCssLayout = new QVBoxLayout(baseCssGroup);
        baseCssLayout->addSpacing(8);

        auto *baseLabel = new QLabel("This stylesheet lays the foundation that all themes build upon.");
        baseLabel->setWordWrap(true);
        baseCssLayout->addWidget(baseLabel);

        m_editPreviewBtn = new QPushButton("Edit Preview Base CSS...");
        baseCssLayout->addWidget(m_editPreviewBtn);

        layout->addWidget(baseCssGroup);

        /* --- Stylesheets panel --- */
        QGroupBox *cssGroup = new QGroupBox("Stylesheets");
        QVBoxLayout *cssLayout = new QVBoxLayout(cssGroup);
        cssLayout->addSpacing(8);

        auto *sheetsLabel = new QLabel("Additional stylesheets to override the visual appearance of the editor, "
            "preview, and chrome (toolbars, menus, etc.).");
        sheetsLabel->setWordWrap(true);
        cssLayout->addWidget(sheetsLabel);

        QHBoxLayout *listRow = new QHBoxLayout();
        m_listWidget = new QListWidget();
        m_listWidget->setFrameShape(QFrame::NoFrame);
        m_listWidget->setObjectName("preferences-stylesheet-list");

        QVBoxLayout *btnLayout = new QVBoxLayout();
        m_addButton = new QPushButton("&Add");
        m_removeButton = new QPushButton("&Remove");
        m_duplicateButton = new QPushButton("&Duplicate");
        m_editButton = new QPushButton("&Edit");
        btnLayout->addWidget(m_addButton);
        btnLayout->addWidget(m_removeButton);
        btnLayout->addWidget(m_duplicateButton);
        btnLayout->addWidget(m_editButton);
        btnLayout->addStretch();

        listRow->addWidget(m_listWidget);
        listRow->addLayout(btnLayout);
        cssLayout->addLayout(listRow);

        layout->addWidget(cssGroup);

        layout->addStretch();

    }
}

void PreferencesDialog::setupEditorPage()
{
    const QString &themeBgColor = m_themeBgColor;
    const QString &themeFgColor = m_themeFgColor;
    QSettings settings;

    /* --- Page 2: Editor --- */
    {
        QWidget *page = addPage(tr("Editor"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *editorGroup = new QGroupBox("Editor Appearance");
        QFormLayout *editorLayout = new QFormLayout(editorGroup);

        m_editorFontCombo = new QComboBox();
        m_editorFontCombo->setEditable(true);
        m_editorFontCombo->addItems({
            "'Consolas', 'Monaco', 'Courier New', monospace",
            "'Menlo', 'Monaco', 'Courier New', monospace",
            "Georgia, 'Times New Roman', serif",
            "'Segoe UI', Roboto, Helvetica, Arial, sans-serif",
            "'Linux Libertine', Georgia, Times, serif",
            "'Source Code Pro', 'Fira Code', monospace",
        });
        QString fontFamily = settings.value(Preferences::EditorFontFamily,
            "'Consolas', 'Monaco', 'Courier New', monospace").toString();
        int idx = m_editorFontCombo->findText(fontFamily);
        if (idx >= 0)
            m_editorFontCombo->setCurrentIndex(idx);
        else
            m_editorFontCombo->setCurrentText(fontFamily);
        editorLayout->addRow("Font family:", m_editorFontCombo);

        m_editorFontSizeSpin = new QSpinBox();
        m_editorFontSizeSpin->setRange(8, 48);
        m_editorFontSizeSpin->setSuffix(" pt");
        m_editorFontSizeSpin->setValue(settings.value(Preferences::EditorFontSize, Preferences::DefaultEditorFontSize).toInt());
        editorLayout->addRow("Font size:", m_editorFontSizeSpin);

        m_editorLineHeightSpin = new QSpinBox();
        m_editorLineHeightSpin->setRange(100, 400);
        m_editorLineHeightSpin->setSuffix(" %");
        m_editorLineHeightSpin->setValue(settings.value(Preferences::EditorLineHeight, Preferences::DefaultEditorLineHeight).toInt());
        editorLayout->addRow("Line height:", m_editorLineHeightSpin);

        m_editorPaddingSpin = new QSpinBox();
        m_editorPaddingSpin->setRange(0, 60);
        m_editorPaddingSpin->setSuffix(" px");
        m_editorPaddingSpin->setValue(settings.value(Preferences::EditorPadding, 12).toInt());
        editorLayout->addRow("Padding:", m_editorPaddingSpin);

        m_editorCaretWidthSpin = new QSpinBox();
        m_editorCaretWidthSpin->setRange(1, 10);
        m_editorCaretWidthSpin->setSuffix(" px");
        m_editorCaretWidthSpin->setValue(settings.value(Preferences::EditorCaretWidth,
            Preferences::DefaultEditorCaretWidth).toInt());
        editorLayout->addRow("Caret width:", m_editorCaretWidthSpin);

        auto emitEditorSettings = [this]() {
            emit editorSettingsChanged(m_editorFontCombo->currentText(),
                m_editorFontSizeSpin->value(), m_editorLineHeightSpin->value(),
                m_editorPaddingSpin->value(), m_editorCaretWidthSpin->value());
        };

        m_editorBgBtn = makeSwatchBtn(
            settings.value(Preferences::EditorBgColor, themeBgColor).toString());
        m_editorFontBtn = makeSwatchBtn(
            settings.value(Preferences::EditorFontColor, themeFgColor).toString());

        m_overrideGroup = new QGroupBox("Override theme colors");
        m_overrideGroup->setCheckable(true);
        m_overrideGroup->setChecked(settings.value(Preferences::EditorColorOverride, false).toBool());
        auto *overrideLayout = new QHBoxLayout(m_overrideGroup);
        overrideLayout->setContentsMargins(6, 18, 6, 6);
        overrideLayout->addWidget(new QLabel("Background:"));
        overrideLayout->addWidget(m_editorBgBtn);
        overrideLayout->addSpacing(12);
        overrideLayout->addWidget(new QLabel("Font:"));
        overrideLayout->addWidget(m_editorFontBtn);
        overrideLayout->addStretch();
        editorLayout->addRow(m_overrideGroup);

        connect(m_editorBgBtn, &QPushButton::clicked, this, [this, emitEditorSettings]() {
            QColor current(m_editorBgBtn->text());
            QColor c = QColorDialog::getColor(current, this, "Editor Background Color");
            if (!c.isValid()) return;
            QSettings s;
            s.setValue(Preferences::EditorBgColor, c.name());
            QPixmap px(16, 16);
            px.fill(c);
            m_editorBgBtn->setIcon(QIcon(px));
            m_editorBgBtn->setText(c.name());
            m_overrideGroup->setChecked(true);
            emitEditorSettings();
        });

        connect(m_editorFontBtn, &QPushButton::clicked, this, [this, emitEditorSettings]() {
            QColor current(m_editorFontBtn->text());
            QColor c = QColorDialog::getColor(current, this, "Editor Font Color");
            if (!c.isValid()) return;
            QSettings s;
            s.setValue(Preferences::EditorFontColor, c.name());
            QPixmap px(16, 16);
            px.fill(c);
            m_editorFontBtn->setIcon(QIcon(px));
            m_editorFontBtn->setText(c.name());
            m_overrideGroup->setChecked(true);
            emitEditorSettings();
        });

        connect(m_overrideGroup, &QGroupBox::toggled, this, [this, emitEditorSettings]() {
            QSettings s;
            s.setValue(Preferences::EditorColorOverride, m_overrideGroup->isChecked());
            emitEditorSettings();
        });
        connect(m_editorFontCombo, &QComboBox::currentTextChanged, this, emitEditorSettings);
        connect(m_editorFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitEditorSettings);
        connect(m_editorLineHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitEditorSettings);
        connect(m_editorPaddingSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitEditorSettings);
        connect(m_editorCaretWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, emitEditorSettings);

        layout->addWidget(editorGroup);

        /* --- Gutter --- */
        QGroupBox *gutterGroup = new QGroupBox("Gutter");
        QVBoxLayout *gutterLayout = new QVBoxLayout(gutterGroup);
        gutterLayout->addSpacing(8);

        m_showLineNumbersCheck = new QCheckBox("Show line numbers");
        m_showLineNumbersCheck->setChecked(settings.value(Preferences::ShowLineNumbers, true).toBool());
        gutterLayout->addWidget(m_showLineNumbersCheck);

        auto emitGutterSettings = [this]() {
            QSettings s;
            s.setValue(Preferences::ShowLineNumbers, m_showLineNumbersCheck->isChecked());
        };

        m_gutterBgBtn = makeSwatchBtn(
            settings.value(Preferences::GutterBgColor, "#f0f0f0").toString());
        m_gutterTextBtn = makeSwatchBtn(
            settings.value(Preferences::GutterTextColor, "#888888").toString());

        m_gutterOverrideGroup = new QGroupBox("Override gutter colors");
        m_gutterOverrideGroup->setCheckable(true);
        m_gutterOverrideGroup->setChecked(settings.value(Preferences::GutterColorOverride, false).toBool());
        auto *gutterOverrideLayout = new QHBoxLayout(m_gutterOverrideGroup);
        gutterOverrideLayout->setContentsMargins(6, 18, 6, 6);
        gutterOverrideLayout->addWidget(new QLabel("Background:"));
        gutterOverrideLayout->addWidget(m_gutterBgBtn);
        gutterOverrideLayout->addSpacing(12);
        gutterOverrideLayout->addWidget(new QLabel("Text:"));
        gutterOverrideLayout->addWidget(m_gutterTextBtn);
        gutterOverrideLayout->addStretch();
        gutterLayout->addWidget(m_gutterOverrideGroup);

        connect(m_gutterBgBtn, &QPushButton::clicked, this, [this, emitGutterSettings]() {
            QColor current(m_gutterBgBtn->text());
            QColor c = QColorDialog::getColor(current, this, "Gutter Background Color");
            if (!c.isValid()) return;
            QSettings s;
            s.setValue(Preferences::GutterBgColor, c.name());
            QPixmap px(16, 16);
            px.fill(c);
            m_gutterBgBtn->setIcon(QIcon(px));
            m_gutterBgBtn->setText(c.name());
            m_gutterOverrideGroup->setChecked(true);
            emitGutterSettings();
        });

        connect(m_gutterTextBtn, &QPushButton::clicked, this, [this, emitGutterSettings]() {
            QColor current(m_gutterTextBtn->text());
            QColor c = QColorDialog::getColor(current, this, "Gutter Text Color");
            if (!c.isValid()) return;
            QSettings s;
            s.setValue(Preferences::GutterTextColor, c.name());
            QPixmap px(16, 16);
            px.fill(c);
            m_gutterTextBtn->setIcon(QIcon(px));
            m_gutterTextBtn->setText(c.name());
            m_gutterOverrideGroup->setChecked(true);
            emitGutterSettings();
        });

        connect(m_gutterOverrideGroup, &QGroupBox::toggled, this, [emitGutterSettings]() {
            emitGutterSettings();
        });

        layout->addWidget(gutterGroup);

        /* --- Tables --- */
        QGroupBox *tablesGroup = new QGroupBox("Tables");
        QVBoxLayout *tablesLayout = new QVBoxLayout(tablesGroup);
        tablesLayout->addSpacing(8);

        m_autoAlignTablesCheck = new QCheckBox("Align markdown table columns when you stop editing");
        m_autoAlignTablesCheck->setToolTip("Spaces out the pipes of a table you have edited "
            "(or just created) so the columns line up, following the "
            "left/center/right alignment marked in the separator row.");
        m_autoAlignTablesCheck->setChecked(settings.value(Preferences::AutoAlignTables, true).toBool());
        tablesLayout->addWidget(m_autoAlignTablesCheck);

        QHBoxLayout *paddingRow = new QHBoxLayout();
        QLabel *paddingLabel = new QLabel("Cell padding:");
        m_tablePaddingSpin = new QSpinBox();
        m_tablePaddingSpin->setRange(1, 4);
        m_tablePaddingSpin->setValue(settings.value(Preferences::TablePadding,
                                                     Preferences::DefaultTablePadding).toInt());
        m_tablePaddingSpin->setToolTip("Number of spaces around each cell's content when the "
            "table columns are aligned. 1 (the default) matches the historic "
            "layout; larger values space the columns out.");
        paddingRow->addWidget(paddingLabel);
        paddingRow->addWidget(m_tablePaddingSpin);
        paddingRow->addStretch();
        tablesLayout->addLayout(paddingRow);

        layout->addWidget(tablesGroup);
        layout->addStretch();

    }
}

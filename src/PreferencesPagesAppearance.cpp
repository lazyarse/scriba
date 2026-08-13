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
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <QIcon>
#include <QPixmap>
#include <QSize>

void PreferencesDialog::setupAppearancePage()
{
    const QString &themeBgColor = m_themeBgColor;
    const QString &themeFgColor = m_themeFgColor;
    QSettings settings;

    {
        QWidget *page = addPage(tr("Appearance"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

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

        QGroupBox *uiFontGroup = new QGroupBox("UI Font Size");
        QFormLayout *uiFontLayout = new QFormLayout(uiFontGroup);
        uiFontLayout->setContentsMargins(12, 12, 12, 12);

        m_uiFontSizeSpin = new QSpinBox();
        m_uiFontSizeSpin->setRange(8, 24);
        m_uiFontSizeSpin->setSuffix(" pt");
        m_uiFontSizeSpin->setValue(settings.value(Preferences::UiFontSize, Preferences::DefaultUiFontSize).toInt());
        uiFontLayout->addRow("Dialogs, menus & chrome:", m_uiFontSizeSpin);
        connect(m_uiFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) { emit uiFontSizeChanged(v); });

        layout->addWidget(uiFontGroup);

        QGroupBox *editorGroup = new QGroupBox("Editor");
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

        QGroupBox *underlineGroup = new QGroupBox("Override underline colors");
        underlineGroup->setCheckable(true);
        underlineGroup->setChecked(settings.value(Preferences::UnderlineColorOverride, false).toBool());
        auto *underlineLayout = new QVBoxLayout(underlineGroup);
        underlineLayout->setContentsMargins(6, 18, 6, 6);
        underlineLayout->setSpacing(6);

        m_spellColorBtn = makeSwatchBtn(
            settings.value(Preferences::SpellUnderlineColor, "#d64050").toString());
        m_grammarColorBtn = makeSwatchBtn(
            settings.value(Preferences::GrammarUnderlineColor, "#00cc66").toString());
        m_linkColorBtn = makeSwatchBtn(
            settings.value(Preferences::LinkUnderlineColor, "#f09000").toString());
        m_markdownColorBtn = makeSwatchBtn(
            settings.value(Preferences::MarkdownUnderlineColor, "#3b82f6").toString());

        auto *underlineRow1 = new QHBoxLayout;
        underlineRow1->setSpacing(6);
        underlineRow1->addWidget(new QLabel("Spelling:"));
        underlineRow1->addWidget(m_spellColorBtn);
        underlineRow1->addSpacing(12);
        underlineRow1->addWidget(new QLabel("Grammar:"));
        underlineRow1->addWidget(m_grammarColorBtn);
        underlineRow1->addStretch();

        auto *underlineRow2 = new QHBoxLayout;
        underlineRow2->setSpacing(6);
        underlineRow2->addWidget(new QLabel("Links:"));
        underlineRow2->addWidget(m_linkColorBtn);
        underlineRow2->addSpacing(12);
        underlineRow2->addWidget(new QLabel("Markdown:"));
        underlineRow2->addWidget(m_markdownColorBtn);
        underlineRow2->addStretch();

        underlineLayout->addLayout(underlineRow1);
        underlineLayout->addLayout(underlineRow2);
        layout->addWidget(underlineGroup);

        auto emitUnderlineColorsChanged = [this]() { emit underlineColorsChanged(); };

        auto connectUnderlineSwatch = [this, emitUnderlineColorsChanged](
            QPushButton *btn, const char *key, const char *title) {
            connect(btn, &QPushButton::clicked, this, [this, btn, key, title, emitUnderlineColorsChanged]() {
                QColor current(btn->text());
                QColor c = QColorDialog::getColor(current, this, QString::fromLatin1(title));
                if (!c.isValid())
                    return;
                QSettings s;
                s.setValue(QString::fromLatin1(key), c.name());
                QPixmap px(16, 16);
                px.fill(c);
                btn->setIcon(QIcon(px));
                btn->setText(c.name());
                m_underlineColorGroup->setChecked(true);
                emitUnderlineColorsChanged();
            });
        };
        connectUnderlineSwatch(m_spellColorBtn, Preferences::SpellUnderlineColor, "Spelling Underline Color");
        connectUnderlineSwatch(m_grammarColorBtn, Preferences::GrammarUnderlineColor, "Grammar Underline Color");
        connectUnderlineSwatch(m_linkColorBtn, Preferences::LinkUnderlineColor, "Link Underline Color");
        connectUnderlineSwatch(m_markdownColorBtn, Preferences::MarkdownUnderlineColor, "Markdown Underline Color");

        m_underlineColorGroup = underlineGroup;
        connect(underlineGroup, &QGroupBox::toggled, this, [this, emitUnderlineColorsChanged]() {
            QSettings s;
            s.setValue(Preferences::UnderlineColorOverride, m_underlineColorGroup->isChecked());
            emitUnderlineColorsChanged();
        });

        layout->addStretch();

    }
}

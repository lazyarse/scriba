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
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
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
    QSettings settings;

    /* --- Page 2: Editor --- */
    {
        QWidget *page = addPage(tr("Editor"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        m_syncCheck = new QCheckBox("Sync editor and preview scrolling");
        m_syncCheck->setChecked(settings.value(Preferences::SyncScroll, true).toBool());
        layout->addWidget(m_syncCheck);

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

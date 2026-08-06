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
#include "StaticHelpers.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "CssEditorDialog.h"
#include "Preferences.h"
#include "Typography.h"
#include "SpellChecker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QSettings>
#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QLabel>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QDir>
#include <QMessageBox>
#include <QStackedWidget>
#include <QScrollArea>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QStyledItemDelegate>
#include <array>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>

namespace {

class FocuslessItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

} // namespace

PreferencesDialog::PreferencesDialog(CssConfig *config, CssLoader *loader, QWidget *parent,
    const QString &themeBgColor, const QString &themeFgColor)
    : QDialog(parent)
    , m_config(config)
    , m_loader(loader)
{
    setupUi(themeBgColor, themeFgColor);
    setWindowTitle("Preferences");
    resize(600, 600);
}

void PreferencesDialog::setupUi(const QString &themeBgColor, const QString &themeFgColor)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    /* --- Sidebar + Pages --- */
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    m_pageList = new QListWidget;
    m_pageList->setMaximumWidth(150);
    m_pageList->setMinimumWidth(120);
    m_pageList->setFrameShape(QFrame::NoFrame);
    QFont pageFont = m_pageList->font();
    pageFont.setPointSize(pageFont.pointSize() + 3);
    m_pageList->setFont(pageFont);
    m_pageList->setObjectName("preferences-page-list");
    m_pageList->setItemDelegate(new FocuslessItemDelegate(m_pageList));
    contentLayout->addWidget(m_pageList);

    m_pages = new QStackedWidget;
    contentLayout->addWidget(m_pages, 1);
    mainLayout->addLayout(contentLayout, 1);

    auto wrapPage = [](QWidget *page) -> QScrollArea * {
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(page);
        return scroll;
    };

    QSettings settings;

    /* --- Page 0: General --- */
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        m_reopenCheck = new QCheckBox("Open last session on startup");
        m_reopenCheck->setChecked(settings.value(Preferences::ReopenLastSession, true).toBool());
        layout->addWidget(m_reopenCheck);

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
        m_centreSingleViewWidthSpin->setEnabled(m_centreSingleViewCheck->isChecked());
        connect(m_centreSingleViewCheck, &QCheckBox::toggled, m_centreSingleViewWidthSpin, &QSpinBox::setEnabled);
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
            connect(autoCheck, &QCheckBox::toggled, spin, &QSpinBox::setEnabled);
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

        layout->addStretch();

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("General");
    }

    /* --- Page 1: Themes --- */
    {
        QWidget *page = new QWidget;
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

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Themes");
    }

    /* --- Page 2: Editor --- */
    {
        QWidget *page = new QWidget;
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

        auto makeSwatchBtn = [](const QString &hex) {
            auto *btn = new QPushButton;
            QPixmap px(16, 16);
            px.fill(QColor(hex));
            btn->setIcon(QIcon(px));
            btn->setIconSize(QSize(16, 16));
            btn->setText(hex);
            btn->setCursor(Qt::PointingHandCursor);
            return btn;
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

        auto makeGutterSwatchBtn = [](const QString &hex) {
            auto *btn = new QPushButton;
            QPixmap px(16, 16);
            px.fill(QColor(hex));
            btn->setIcon(QIcon(px));
            btn->setIconSize(QSize(16, 16));
            btn->setText(hex);
            btn->setCursor(Qt::PointingHandCursor);
            return btn;
        };

        m_gutterBgBtn = makeGutterSwatchBtn(
            settings.value(Preferences::GutterBgColor, "#f0f0f0").toString());
        m_gutterTextBtn = makeGutterSwatchBtn(
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

        layout->addWidget(tablesGroup);
        layout->addStretch();

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Editor");
    }

    /* --- Page 3: Preview --- */
    {
        QWidget *page = new QWidget;
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

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Preview");
    }

    /* --- Page 4: Advanced --- */
    {
        QWidget *page = new QWidget;
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

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Advanced");
    }

    /* --- Page 5: Writing --- */
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        /* --- Status Bar Metrics --- */
        QGroupBox *metricsGroup = new QGroupBox("Status Bar Metrics");
        QVBoxLayout *metricsLayout = new QVBoxLayout(metricsGroup);
        metricsLayout->addSpacing(8);

        constexpr int kMaxMetrics = 8;

        auto *maxLabel = new QLabel(QString("Select up to %1 metrics").arg(kMaxMetrics));
        maxLabel->setStyleSheet("color: gray;");
        metricsLayout->addWidget(maxLabel);

        QStringList selected = settings.value(Preferences::StatusBarMetrics).toStringList();
        if (selected.isEmpty())
            selected = {"words", "sentences", "reading-age", "reading-time", "speaking-time"};

        auto addMetricCheck = [&](const QString &key, const QString &label, const QString &tooltip = QString()) {
            auto *cb = new QCheckBox(label);
            cb->setChecked(selected.contains(key));
            cb->setProperty("metricKey", key);
            if (!tooltip.isEmpty())
                cb->setToolTip(tooltip);
            metricsLayout->addWidget(cb);
            return cb;
        };

        m_wordCountCheck = addMetricCheck("words", "Word count");
        m_sentenceCountCheck = addMetricCheck("sentences", "Sentence count");
        m_paragraphCountCheck = addMetricCheck("paragraphs", "Paragraph count");
        m_charNoSpaceCheck = addMetricCheck("char-nospace", "Character count (no spaces)");
        m_charWithSpaceCheck = addMetricCheck("char-withspace", "Character count (with spaces)");

        metricsLayout->addSpacing(4);
        auto *readLabel = new QLabel("<b>Readability</b>");
        metricsLayout->addWidget(readLabel);

        QHBoxLayout *readingAgeRow = new QHBoxLayout();
        m_readingAgeCheck = new QCheckBox("Reading age");
        m_readingAgeCheck->setChecked(selected.contains("reading-age"));
        m_readingAgeCheck->setProperty("metricKey", "reading-age");
        readingAgeRow->addWidget(m_readingAgeCheck);
        readingAgeRow->addSpacing(8);
        readingAgeRow->addWidget(new QLabel("Formula:"));
        m_readabilityCombo = new QComboBox();
        m_readabilityCombo->addItem("Flesch-Kincaid", static_cast<int>(Preferences::Formula::FleschKincaid));
        m_readabilityCombo->addItem("Coleman-Liau", static_cast<int>(Preferences::Formula::ColemanLiau));
        m_readabilityCombo->addItem("Gunning Fog", static_cast<int>(Preferences::Formula::GunningFog));
        m_readabilityCombo->addItem("SMOG", static_cast<int>(Preferences::Formula::Smog));
        m_readabilityCombo->addItem("ARI", static_cast<int>(Preferences::Formula::ARI));
        auto curFormula = Preferences::formulaFromString(
            settings.value(Preferences::ReadabilityFormula,
                Preferences::formulaToString(Preferences::Formula::FleschKincaid)).toString());
        m_readabilityCombo->setCurrentIndex(static_cast<int>(curFormula));
        m_readabilityCombo->setEnabled(m_readingAgeCheck->isChecked());
        readingAgeRow->addWidget(m_readabilityCombo);
        readingAgeRow->addStretch();
        metricsLayout->addLayout(readingAgeRow);
        connect(m_readingAgeCheck, &QCheckBox::toggled, m_readabilityCombo, &QComboBox::setEnabled);

        m_fleschEaseCheck = addMetricCheck("flesch-ease", "Flesch Reading Ease");

        metricsLayout->addSpacing(4);
        auto *timeLabel = new QLabel("<b>Time</b>");
        metricsLayout->addWidget(timeLabel);

        QHBoxLayout *readingTimeRow = new QHBoxLayout();
        m_readingTimeCheck = new QCheckBox("Reading time");
        m_readingTimeCheck->setChecked(selected.contains("reading-time"));
        m_readingTimeCheck->setProperty("metricKey", "reading-time");
        readingTimeRow->addWidget(m_readingTimeCheck);
        readingTimeRow->addSpacing(8);
        readingTimeRow->addWidget(new QLabel("Speed:"));
        m_wpsSpin = new QDoubleSpinBox();
        m_wpsSpin->setRange(1.0, 20.0);
        m_wpsSpin->setSingleStep(0.5);
        m_wpsSpin->setValue(settings.value(Preferences::WordsPerSecond, 3.33).toDouble());
        m_wpsSpin->setSuffix(" words/sec");
        readingTimeRow->addWidget(m_wpsSpin);
        readingTimeRow->addStretch();
        metricsLayout->addLayout(readingTimeRow);

        QHBoxLayout *speakingTimeRow = new QHBoxLayout();
        m_speakingTimeCheck = new QCheckBox("Speaking time");
        m_speakingTimeCheck->setChecked(selected.contains("speaking-time"));
        m_speakingTimeCheck->setProperty("metricKey", "speaking-time");
        speakingTimeRow->addWidget(m_speakingTimeCheck);
        speakingTimeRow->addSpacing(8);
        speakingTimeRow->addWidget(new QLabel("Speed:"));
        m_spWpmSpin = new QSpinBox();
        m_spWpmSpin->setRange(60, 300);
        m_spWpmSpin->setValue(settings.value(Preferences::SpeakingWpm, 150).toInt());
        m_spWpmSpin->setSuffix(" words/min");
        speakingTimeRow->addWidget(m_spWpmSpin);
        speakingTimeRow->addStretch();
        metricsLayout->addLayout(speakingTimeRow);

        metricsLayout->addSpacing(4);
        auto *vocabLabel = new QLabel("<b>Vocabulary</b>");
        metricsLayout->addWidget(vocabLabel);

        m_syllableCountCheck = addMetricCheck("syllables", "Syllable count");
        m_complexWordsCheck = addMetricCheck("complex-words", "Complex word count (3+ syllables)");
        m_lexicalDensityCheck = addMetricCheck("lexical-density", "Lexical density (%)");

        metricsLayout->addSpacing(4);
        auto *avgLabel = new QLabel("<b>Averages</b>");
        metricsLayout->addWidget(avgLabel);

        m_avgWordsPerSentenceCheck = addMetricCheck("avg-wps", "Average words per sentence");
        m_avgSyllablesPerWordCheck = addMetricCheck("avg-spw", "Average syllables per word");

        m_selectionCountLabel = new QLabel;
        metricsLayout->addWidget(m_selectionCountLabel);

        // connect all checkboxes to limit enforcement
        auto enforceLimit = [this, kMaxMetrics]() {
            QStringList selectedKeys;
            int count = 0;
            for (auto *cb : this->m_metricChecks) {
                if (cb->isChecked()) {
                    ++count;
                    selectedKeys << cb->property("metricKey").toString();
                }
            }
            bool atLimit = count >= kMaxMetrics;
            for (auto *cb : this->m_metricChecks) {
                if (!cb->isChecked())
                    cb->setEnabled(!atLimit);
            }
            m_selectionCountLabel->setText(
                QStringLiteral("%1 / %2 selected %3")
                    .arg(count)
                    .arg(kMaxMetrics)
                    .arg(atLimit ? QString("(max %1 reached)").arg(kMaxMetrics) : ""));
        };

        // collect all metric checkboxes
        m_metricChecks = {
            m_wordCountCheck, m_sentenceCountCheck, m_paragraphCountCheck,
            m_charNoSpaceCheck, m_charWithSpaceCheck,
            m_readingAgeCheck, m_fleschEaseCheck,
            m_readingTimeCheck, m_speakingTimeCheck,
            m_syllableCountCheck, m_complexWordsCheck, m_lexicalDensityCheck,
            m_avgWordsPerSentenceCheck, m_avgSyllablesPerWordCheck
        };
        for (auto *cb : m_metricChecks)
            connect(cb, &QCheckBox::toggled, this, enforceLimit);
        enforceLimit();

        layout->addWidget(metricsGroup);

        layout->addStretch();

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Writing");
    }

    /* --- Page: Typography --- */
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *group = new QGroupBox("Smart Typography");
        QVBoxLayout *groupLayout = new QVBoxLayout(group);
        groupLayout->addSpacing(8);

        auto *note = new QLabel(tr(
            "Replace plain typed punctuation with print-quality equivalents in the "
            "preview and exports (PDF, DOCX, HTML). Your Markdown source is never "
            "changed. Code blocks, inline code and math are left untouched."));
        note->setWordWrap(true);
        note->setStyleSheet("color: gray; padding: 8px;");
        groupLayout->addWidget(note);

        m_typographyQuotesCheck = new QCheckBox(tr("Curly quotes and apostrophes"));
        m_typographyQuotesCheck->setToolTip(tr("\"double\" and 'single' become \u201cdouble\u201d and \u2018single\u2019"));
        m_typographyQuotesCheck->setChecked(settings.value(Preferences::TypographyQuotes, false).toBool());
        groupLayout->addWidget(m_typographyQuotesCheck);

        m_typographyDashesCheck = new QCheckBox(tr("Dashes"));
        m_typographyDashesCheck->setToolTip(tr("- becomes \u2010 (hyphen), -- becomes \u2013 (en dash), --- becomes \u2014 (em dash)"));
        m_typographyDashesCheck->setChecked(settings.value(Preferences::TypographyDashes, false).toBool());
        groupLayout->addWidget(m_typographyDashesCheck);

        m_typographyEllipsisCheck = new QCheckBox(tr("Ellipsis"));
        m_typographyEllipsisCheck->setToolTip(tr("... becomes \u2026"));
        m_typographyEllipsisCheck->setChecked(settings.value(Preferences::TypographyEllipsis, false).toBool());
        groupLayout->addWidget(m_typographyEllipsisCheck);

        m_typographyMultiplicationCheck = new QCheckBox(tr("Multiplication sign"));
        m_typographyMultiplicationCheck->setToolTip(tr("3x4 or 3 x 4 becomes 3\u00d74"));
        m_typographyMultiplicationCheck->setChecked(settings.value(Preferences::TypographyMultiplication, false).toBool());
        groupLayout->addWidget(m_typographyMultiplicationCheck);

        m_typographyDegreeFractionPrimeCheck = new QCheckBox(tr("Degrees, fractions and primes"));
        m_typographyDegreeFractionPrimeCheck->setToolTip(tr("90oF becomes 90\u00b0F, 1/2 becomes \u00bd, 5'10 becomes 5\u203210"));
        m_typographyDegreeFractionPrimeCheck->setChecked(settings.value(Preferences::TypographyDegreeFractionPrime, false).toBool());
        groupLayout->addWidget(m_typographyDegreeFractionPrimeCheck);

        m_typographyNbspCheck = new QCheckBox(tr("Non-breaking spaces"));
        m_typographyNbspCheck->setToolTip(tr("a word and 10 kg get non-breaking spaces"));
        m_typographyNbspCheck->setChecked(settings.value(Preferences::TypographyNbsp, false).toBool());
        groupLayout->addWidget(m_typographyNbspCheck);

        m_typographySymbolsCheck = new QCheckBox(tr("Symbols"));
        m_typographySymbolsCheck->setToolTip(tr("(c) (r) (tm) (p) (sm) become \u00a9 \u00ae \u2122 \u2117 \u2120"));
        m_typographySymbolsCheck->setChecked(settings.value(Preferences::TypographySymbols, false).toBool());
        groupLayout->addWidget(m_typographySymbolsCheck);

        groupLayout->addSpacing(8);
        auto *exampleLabel = new QLabel(tr("<b>Example</b>"));
        groupLayout->addWidget(exampleLabel);

        const QString sample = QStringLiteral("He said \"It's easy -- 3x4 ... 1/2 of 90oF in 10 kg (c) 2026\"");
        m_typographyPlainLabel = new QLabel(sample);
        m_typographyPlainLabel->setWordWrap(true);
        m_typographyPlainLabel->setStyleSheet("color: gray;");
        groupLayout->addWidget(m_typographyPlainLabel);

        m_typographyExampleLabel = new QLabel;
        m_typographyExampleLabel->setWordWrap(true);
        m_typographyExampleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        groupLayout->addWidget(m_typographyExampleLabel);

        auto updateExample = [this, sample]() {
            Typography::Options opts;
            if (m_typographyQuotesCheck->isChecked())
                opts |= Typography::Option::Quotes;
            if (m_typographyDashesCheck->isChecked())
                opts |= Typography::Option::Dashes;
            if (m_typographyEllipsisCheck->isChecked())
                opts |= Typography::Option::Ellipsis;
            if (m_typographyMultiplicationCheck->isChecked())
                opts |= Typography::Option::Multiplication;
            if (m_typographyDegreeFractionPrimeCheck->isChecked())
                opts |= Typography::Option::DegreeFractionPrime;
            if (m_typographyNbspCheck->isChecked())
                opts |= Typography::Option::NonBreakingSpace;
            if (m_typographySymbolsCheck->isChecked())
                opts |= Typography::Option::Symbols;
            Typography::State state;
            m_typographyExampleLabel->setText(Typography::apply(sample, opts, state));
        };
        for (auto *cb : { m_typographyQuotesCheck, m_typographyDashesCheck,
                          m_typographyEllipsisCheck, m_typographyMultiplicationCheck,
                          m_typographyDegreeFractionPrimeCheck, m_typographyNbspCheck,
                          m_typographySymbolsCheck })
            connect(cb, &QCheckBox::toggled, this, updateExample);
        updateExample();

        layout->addWidget(group);
        layout->addStretch();

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Typography");
    }

    /* --- Page 6: Replacements --- */
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *group = new QGroupBox("Typo Autocorrect");
        QVBoxLayout *groupLayout = new QVBoxLayout(group);
        groupLayout->addSpacing(8);

        m_autoCorrectCheck = new QCheckBox("Correct typos as you type");
        m_autoCorrectCheck->setChecked(settings.value(Preferences::AutoCorrectEnabled, true).toBool());
        groupLayout->addWidget(m_autoCorrectCheck);

        auto *note = new QLabel(tr(
            "When a word is completed (space, punctuation or Enter) it is replaced with the "
            "word in the Replaces column whenever the word before the caret matches the Typo "
            "column. Matching ignores case and the case of your typing is preserved, so \"Teh\" "
            "becomes \"The\". Corrections are skipped inside code blocks, inline code and link "
            "URLs, and Ctrl+Z undoes one."));
        note->setWordWrap(true);
        note->setStyleSheet("color: gray; padding: 8px;");
        groupLayout->addWidget(note);

        m_replacementsTable = new QTableWidget(0, 2);
        m_replacementsTable->setHorizontalHeaderLabels({tr("Typo"), tr("Replaces")});
        m_replacementsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_replacementsTable->verticalHeader()->setVisible(false);
        m_replacementsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        groupLayout->addWidget(m_replacementsTable);

        auto stripButtonIcons = [](const QList<QPushButton *> &buttons) {
            for (auto *btn : buttons)
                btn->setIcon(QIcon());
        };

        auto *addBtn = new QPushButton(tr("Ad&d"));
        auto *removeBtn = new QPushButton(tr("R&emove"));
        auto *restoreBtn = new QPushButton(tr("Res&tore Defaults"));
        stripButtonIcons({addBtn, removeBtn, restoreBtn});
        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->addWidget(addBtn);
        btnRow->addWidget(removeBtn);
        btnRow->addWidget(restoreBtn);
        btnRow->addStretch();
        groupLayout->addLayout(btnRow);

        layout->addWidget(group);
        layout->addStretch();

        QStringList pairs = settings.value(Preferences::AutoCorrectPairs).toStringList();
        if (pairs.isEmpty())
            pairs = Preferences::defaultAutoCorrectPairs();
        for (const QString &pair : pairs) {
            const int eq = pair.indexOf('=');
            if (eq <= 0)
                continue;
            const int row = m_replacementsTable->rowCount();
            m_replacementsTable->insertRow(row);
            m_replacementsTable->setItem(row, 0, new QTableWidgetItem(pair.left(eq)));
            m_replacementsTable->setItem(row, 1, new QTableWidgetItem(pair.mid(eq + 1)));
        }

        connect(addBtn, &QPushButton::clicked, this, [this]() {
            const int row = m_replacementsTable->rowCount();
            m_replacementsTable->insertRow(row);
            m_replacementsTable->setItem(row, 0, new QTableWidgetItem);
            m_replacementsTable->setItem(row, 1, new QTableWidgetItem);
            m_replacementsTable->setCurrentCell(row, 0);
            m_replacementsTable->editItem(m_replacementsTable->item(row, 0));
        });
        connect(removeBtn, &QPushButton::clicked, this, [this]() {
            const auto selected = m_replacementsTable->selectionModel()->selectedRows();
            QList<int> rows;
            for (const auto &idx : selected)
                rows << idx.row();
            std::sort(rows.begin(), rows.end(), std::greater<int>());
            for (int r : rows)
                m_replacementsTable->removeRow(r);
        });
        connect(restoreBtn, &QPushButton::clicked, this, [this]() {
            for (const QString &pair : Preferences::defaultAutoCorrectPairs()) {
                const int eq = pair.indexOf('=');
                if (eq <= 0)
                    continue;
                bool exists = false;
                for (int r = 0; r < m_replacementsTable->rowCount(); ++r) {
                    auto *item = m_replacementsTable->item(r, 0);
                    if (item && item->text().toLower() == pair.left(eq).toLower())
                        exists = true;
                }
                if (!exists) {
                    const int row = m_replacementsTable->rowCount();
                    m_replacementsTable->insertRow(row);
                    m_replacementsTable->setItem(row, 0, new QTableWidgetItem(pair.left(eq)));
                    m_replacementsTable->setItem(row, 1, new QTableWidgetItem(pair.mid(eq + 1)));
                }
            }
        });

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Replacements");
    }

    /* --- Page 5: Spelling --- */
    {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *checkGroup = new QGroupBox("Checking");
        QVBoxLayout *checkLayout = new QVBoxLayout(checkGroup);
        checkLayout->addSpacing(8);

        m_spellCheckCheck = new QCheckBox("Check spelling as you type");
        m_spellCheckCheck->setChecked(settings.value(Preferences::SpellCheckEnabled, true).toBool());
        checkLayout->addWidget(m_spellCheckCheck);

        m_grammarCheckCheck = new QCheckBox("Check grammar as you type");
        m_grammarCheckCheck->setChecked(settings.value(Preferences::GrammarCheckEnabled, false).toBool());
        checkLayout->addWidget(m_grammarCheckCheck);

        m_linkCheckCheck = new QCheckBox("Underline broken links as you type");
        m_linkCheckCheck->setChecked(settings.value(Preferences::LinkCheckEnabled, true).toBool());
        checkLayout->addWidget(m_linkCheckCheck);

        m_markdownCheckCheck = new QCheckBox("Underline markdown inconsistencies as you type");
        m_markdownCheckCheck->setChecked(settings.value(Preferences::MarkdownCheckEnabled, false).toBool());
        checkLayout->addWidget(m_markdownCheckCheck);

        struct MdCheckDef {
            const char *key;
            const char *label;
        };
        static const std::array<MdCheckDef, 7> kMdChecks = {{
            {Preferences::MarkdownCheckHeadingLevelSkip, "Heading level &skips"},
            {Preferences::MarkdownCheckDuplicateHeading, "&Duplicate headings"},
            {Preferences::MarkdownCheckTrailingWhitespace, "Trailing &whitespace"},
            {Preferences::MarkdownCheckConsecutiveBlankLines, "Consecutive &blank lines"},
            {Preferences::MarkdownCheckOverlongLine, "Lines &over 120 characters"},
            {Preferences::MarkdownCheckHashNoSpace, "'#' headings without a &space"},
            {Preferences::MarkdownCheckFootnoteReference, "&Unmatched footnote references"},
        }};
        auto *mdSubLayout = new QVBoxLayout;
        mdSubLayout->setContentsMargins(24, 0, 0, 0);
        for (const auto &def : kMdChecks) {
            auto *check = new QCheckBox(tr(def.label));
            check->setChecked(settings.value(QLatin1String(def.key), true).toBool());
            check->setEnabled(m_markdownCheckCheck->isChecked());
            m_markdownSubChecks.append(check);
            mdSubLayout->addWidget(check);
        }
        checkLayout->addLayout(mdSubLayout);
        connect(m_markdownCheckCheck, &QCheckBox::toggled, this, [this](bool checked) {
            for (auto *check : m_markdownSubChecks)
                check->setEnabled(checked);
        });

        layout->addWidget(checkGroup);

        QGroupBox *underlineGroup = new QGroupBox("Override underline colors");
        underlineGroup->setCheckable(true);
        underlineGroup->setChecked(settings.value(Preferences::UnderlineColorOverride, false).toBool());
        auto *underlineLayout = new QVBoxLayout(underlineGroup);
        underlineLayout->setContentsMargins(6, 18, 6, 6);
        underlineLayout->setSpacing(6);

        auto makeUnderlineSwatchBtn = [](const QString &hex) {
            auto *btn = new QPushButton;
            QPixmap px(16, 16);
            px.fill(QColor(hex));
            btn->setIcon(QIcon(px));
            btn->setIconSize(QSize(16, 16));
            btn->setText(hex);
            btn->setCursor(Qt::PointingHandCursor);
            return btn;
        };

        m_spellColorBtn = makeUnderlineSwatchBtn(
            settings.value(Preferences::SpellUnderlineColor, "#d64050").toString());
        m_grammarColorBtn = makeUnderlineSwatchBtn(
            settings.value(Preferences::GrammarUnderlineColor, "#00cc66").toString());
        m_linkColorBtn = makeUnderlineSwatchBtn(
            settings.value(Preferences::LinkUnderlineColor, "#f09000").toString());
        m_markdownColorBtn = makeUnderlineSwatchBtn(
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

        QGroupBox *grammarGroup = new QGroupBox("Grammar");
        QFormLayout *grammarLayout = new QFormLayout(grammarGroup);
        grammarLayout->setContentsMargins(12, 12, 12, 12);

        m_grammarDialectCombo = new QComboBox;
        m_grammarDialectCombo->addItem("American");
        m_grammarDialectCombo->addItem("British");
        m_grammarDialectCombo->addItem("Australian");
        m_grammarDialectCombo->addItem("Indian");
        m_grammarDialectCombo->addItem("Canadian");
        m_grammarDialectCombo->addItem("New Zealand");
        const int dialectIndex = m_grammarDialectCombo->findText(
            settings.value(Preferences::GrammarDialect, QStringLiteral("American")).toString());
        m_grammarDialectCombo->setCurrentIndex(dialectIndex < 0 ? 0 : dialectIndex);
        m_grammarDialectCombo->setEnabled(m_grammarCheckCheck->isChecked());
        grammarLayout->addRow("Dialect:", m_grammarDialectCombo);

        layout->addWidget(grammarGroup);

        auto stripButtonIcons = [](const QList<QPushButton *> &buttons) {
            for (auto *btn : buttons)
                btn->setIcon(QIcon());
        };

        QGroupBox *dictionaryGroup = new QGroupBox("Dictionary");
        QFormLayout *dictionaryLayout = new QFormLayout(dictionaryGroup);
        dictionaryLayout->setContentsMargins(12, 12, 12, 12);

        auto languageLabel = [](const QString &code) {
            if (code == "en_US")
                return QStringLiteral("English (US)");
            if (code == "en_GB")
                return QStringLiteral("English (UK)");
            return code;
        };

        m_languageCombo = new QComboBox;
        // data() == "" ("Follow dialect") lets the grammar dialect select the
        // base dictionary — the default; an explicit language is an override.
        m_languageCombo->addItem(tr("Follow dialect"), QString());
        m_languageCombo->addItem(languageLabel("en_US"), "en_US");
        m_languageCombo->addItem(languageLabel("en_GB"), "en_GB");
        const QString storedLang = settings.value(Preferences::DictionaryLanguage).toString();
        int langIdx = m_languageCombo->findData(storedLang);
        m_languageCombo->setCurrentIndex(langIdx < 0 ? 0 : langIdx);
        dictionaryLayout->addRow(tr("Language:"), m_languageCombo);

        auto *langNote = new QLabel(tr(
            "Follow dialect uses the Grammar dialect setting to pick the base "
            "dictionary (American/Canadian -> English (US), the rest -> English (UK))."));
        langNote->setWordWrap(true);
        langNote->setStyleSheet("color: gray; padding: 8px;");
        dictionaryLayout->addRow(QString(), langNote);

        layout->addWidget(dictionaryGroup);

        QGroupBox *importGroup = new QGroupBox("Imported Word Lists");
        QVBoxLayout *importLayout = new QVBoxLayout(importGroup);
        importLayout->addSpacing(8);

        m_importedList = new QListWidget;
        m_importedList->setMinimumHeight(60);
        importLayout->addWidget(m_importedList);

        auto *importDictBtn = new QPushButton(tr("&Import Word List..."));
        auto *removeDictBtn = new QPushButton(tr("Re&move Word List"));
        stripButtonIcons({importDictBtn, removeDictBtn});
        QHBoxLayout *importButtons = new QHBoxLayout();
        importButtons->addWidget(importDictBtn);
        importButtons->addWidget(removeDictBtn);
        importButtons->addStretch();
        importLayout->addLayout(importButtons);

        auto reloadImported = [this, removeDictBtn]() {
            m_importedList->clear();
            m_importedList->addItems(SpellChecker::importedDictionaries());
            removeDictBtn->setEnabled(m_importedList->currentRow() >= 0);
        };

        // Imported word lists are language-independent unions: they add words
        // to whichever base dictionary is active.
        auto *importNote = new QLabel(tr(
            "Imported word lists and custom words are language-independent for now: "
            "they apply to every language and dialect."));
        importNote->setWordWrap(true);
        importNote->setStyleSheet("color: gray; padding: 8px;");
        importLayout->addWidget(importNote);

        layout->addWidget(importGroup);

        connect(m_importedList, &QListWidget::itemSelectionChanged, this,
                [removeDictBtn, this]() {
                    removeDictBtn->setEnabled(m_importedList->currentItem() != nullptr);
                });
        connect(importDictBtn, &QPushButton::clicked, this, [this, reloadImported]() {
            const QString path = QFileDialog::getOpenFileName(this, tr("Import Word List"),
                QDir::homePath(), tr("Word lists (*.txt)"));
            if (path.isEmpty())
                return;
            const QString base = SpellChecker::installDictionary(path);
            if (base.isEmpty()) {
                QMessageBox::warning(this, tr("Import Word List"),
                    tr("Could not import the file. It must be a plain word list "
                       "(one word per line, .txt) with a safe name (e.g. technical-terms)."));
                return;
            }
            reloadImported();
        });
        connect(removeDictBtn, &QPushButton::clicked, this, [this, reloadImported]() {
            QListWidgetItem *item = m_importedList->currentItem();
            if (!item)
                return;
            const QString base = item->text();
            if (QMessageBox::question(this, tr("Remove Word List"),
                                      tr("Remove the \"%1\" word list?").arg(base))
                != QMessageBox::Yes)
                return;
            if (SpellChecker::removeDictionary(base))
                reloadImported();
        });

        QGroupBox *customGroup = new QGroupBox("Custom Words");
        QVBoxLayout *customLayout = new QVBoxLayout(customGroup);
        customLayout->addSpacing(8);

        m_customWordsList = new QListWidget;
        m_customWordsList->setMinimumHeight(60);
        m_customWordsList->addItems(SpellChecker::readUserDictionaryWords());
        customLayout->addWidget(m_customWordsList);

        QHBoxLayout *customButtons = new QHBoxLayout();
        auto *addWordsBtn = new QPushButton(tr("&Add Word/s..."));
        auto *removeWordBtn = new QPushButton(tr("&Remove Word"));
        auto *importWordsBtn = new QPushButton(tr("&Import from File..."));
        customButtons->addWidget(addWordsBtn);
        customButtons->addWidget(removeWordBtn);
        customButtons->addWidget(importWordsBtn);
        customButtons->addStretch();
        stripButtonIcons({addWordsBtn, removeWordBtn, importWordsBtn});
        customLayout->addLayout(customButtons);

        layout->addWidget(customGroup);

        QGroupBox *ignoredGroup = new QGroupBox("Ignored Words");
        QVBoxLayout *ignoredLayout = new QVBoxLayout(ignoredGroup);
        ignoredLayout->addSpacing(8);

        m_ignoredWordsList = new QListWidget;
        m_ignoredWordsList->setMinimumHeight(60);
        m_ignoredWordsList->addItems(SpellChecker::readIgnoredWords());
        ignoredLayout->addWidget(m_ignoredWordsList);

        QHBoxLayout *ignoredButtons = new QHBoxLayout();
        auto *removeIgnoredBtn = new QPushButton(tr("&Remove Word"));
        auto *removeAllIgnoredBtn = new QPushButton(tr("Remove &All"));
        ignoredButtons->addWidget(removeIgnoredBtn);
        ignoredButtons->addWidget(removeAllIgnoredBtn);
        ignoredButtons->addStretch();
        stripButtonIcons({removeIgnoredBtn, removeAllIgnoredBtn});
        ignoredLayout->addLayout(ignoredButtons);

        auto *ignoredNote = new QLabel(tr(
            "Words \u201cignored always\u201d by Check Spelling. They are not "
            "flagged, but stay separate from your custom dictionary."));
        ignoredNote->setWordWrap(true);
        ignoredNote->setStyleSheet("color: gray; padding: 8px;");
        ignoredLayout->addWidget(ignoredNote);

        layout->addWidget(ignoredGroup);

        connect(m_ignoredWordsList, &QListWidget::itemSelectionChanged, this,
                [this, removeIgnoredBtn]() {
                    removeIgnoredBtn->setEnabled(m_ignoredWordsList->currentItem() != nullptr);
                });
        connect(removeIgnoredBtn, &QPushButton::clicked, this, [this]() {
            delete m_ignoredWordsList->currentItem();
        });
        connect(removeAllIgnoredBtn, &QPushButton::clicked, this, [this]() {
            m_ignoredWordsList->clear();
        });

        layout->addStretch();

        auto mergeParsedWords = [this](const QStringList &words) {
            int added = 0;
            int skipped = 0;
            for (const QString &word : words) {
                bool present = false;
                for (int i = 0; i < m_customWordsList->count(); ++i) {
                    if (m_customWordsList->item(i)->text() == word) {
                        present = true;
                        break;
                    }
                }
                if (present) {
                    ++skipped;
                } else {
                    m_customWordsList->addItem(word);
                    ++added;
                }
            }
            QString message = added == 1
                ? tr("Added 1 word to your custom word list.")
                : tr("Added %1 words to your custom word list.").arg(added);
            if (skipped > 0)
                message += skipped == 1
                    ? tr(" 1 word was already present.")
                    : tr(" %1 words were already present.").arg(skipped);
            QMessageBox::information(this, tr("Custom Words"), message);
        };

        connect(addWordsBtn, &QPushButton::clicked, this, [this, mergeParsedWords]() {
            bool ok = false;
            QString text = QInputDialog::getMultiLineText(this, tr("Add Words"),
                tr("Enter a word, or one word per line:"), QString(), &ok);
            if (!ok)
                return;
            mergeParsedWords(SpellChecker::parseWordList(text));
        });
        connect(removeWordBtn, &QPushButton::clicked, this, [this]() {
            delete m_customWordsList->currentItem();
        });
        connect(importWordsBtn, &QPushButton::clicked, this, [this, mergeParsedWords]() {
            const QString path = QFileDialog::getOpenFileName(this, tr("Import Custom Words"),
                QDir::homePath(), tr("Text files (*.txt *.dic);;All files (*)"));
            if (path.isEmpty())
                return;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::warning(this, tr("Import Custom Words"),
                    tr("Could not read the file."));
                return;
            }
            mergeParsedWords(SpellChecker::parseWordList(QString::fromUtf8(file.readAll())));
        });

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Spelling");
    }

    /* --- Page 6: Security --- */
    {
        QWidget *page = new QWidget;
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

        m_pages->addWidget(wrapPage(page));
        m_pageList->addItem("Security");
    }

    /* --- Connections --- */
    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addStylesheet);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeStylesheet);
    connect(m_duplicateButton, &QPushButton::clicked, this, &PreferencesDialog::duplicateStylesheet);
    connect(m_editButton, &QPushButton::clicked, this, &PreferencesDialog::editStylesheet);
    connect(m_editPreviewBtn, &QPushButton::clicked, this, &PreferencesDialog::editPreviewBaseCss);
    connect(m_listWidget, &QListWidget::currentItemChanged, this, &PreferencesDialog::onCurrentItemChanged);
    connect(m_pageList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_grammarCheckCheck, &QCheckBox::toggled, m_grammarDialectCombo, &QWidget::setEnabled);

    populateStylesheetList();
    m_pageList->setCurrentRow(0);

    /* --- Dialog Buttons --- */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(buttonBox);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        QSettings settings;
        settings.setValue(Preferences::ReopenLastSession, m_reopenCheck->isChecked());
        settings.setValue(Preferences::SyncScroll, m_syncCheck->isChecked());
        settings.setValue(Preferences::TableStriping, m_stripeCheck->isChecked());
        settings.setValue(Preferences::ShowCodeLangPreview, m_showCodeLangPreviewCheck->isChecked());
        settings.setValue(Preferences::ShowCodeLangExport, m_showCodeLangExportCheck->isChecked());
        settings.setValue(Preferences::EmojiMode,
    Preferences::emojiRenderingToString(m_emojiBw->isChecked() ? Preferences::EmojiRendering::Bw : Preferences::EmojiRendering::Color));
        settings.setValue(Preferences::EmojiAutoComplete, m_emojiAutoCompleteCheck->isChecked());
        settings.setValue(Preferences::EmojiCompletionLimit, m_emojiCompletionSpin->value());
        settings.setValue(Preferences::LanguageAutoComplete, m_languageAutoCompleteCheck->isChecked());
        settings.setValue(Preferences::AutoCorrectEnabled, m_autoCorrectCheck->isChecked());
        settings.setValue(Preferences::TypographyQuotes, m_typographyQuotesCheck->isChecked());
        settings.setValue(Preferences::TypographyDashes, m_typographyDashesCheck->isChecked());
        settings.setValue(Preferences::TypographyEllipsis, m_typographyEllipsisCheck->isChecked());
        settings.setValue(Preferences::TypographyMultiplication, m_typographyMultiplicationCheck->isChecked());
        settings.setValue(Preferences::TypographyDegreeFractionPrime, m_typographyDegreeFractionPrimeCheck->isChecked());
        settings.setValue(Preferences::TypographyNbsp, m_typographyNbspCheck->isChecked());
        settings.setValue(Preferences::TypographySymbols, m_typographySymbolsCheck->isChecked());
        QStringList autoCorrectPairs;
        for (int r = 0; r < m_replacementsTable->rowCount(); ++r) {
            const QString typo = m_replacementsTable->item(r, 0)
                ? m_replacementsTable->item(r, 0)->text().trimmed() : QString();
            const QString repl = m_replacementsTable->item(r, 1)
                ? m_replacementsTable->item(r, 1)->text().trimmed() : QString();
            if (typo.isEmpty() || repl.isEmpty() || typo.contains('='))
                continue;
            autoCorrectPairs << typo + "=" + repl;
        }
        settings.setValue(Preferences::AutoCorrectPairs, autoCorrectPairs);
        settings.setValue(Preferences::CentreSingleViewContent, m_centreSingleViewCheck->isChecked());
        settings.setValue(Preferences::CentreSingleViewWidth, m_centreSingleViewWidthSpin->value());
        settings.setValue(Preferences::SplitViewEditorMaxWidth,
            m_splitEditorAutoCheck->isChecked() ? 0 : m_splitEditorWidthSpin->value());
        settings.setValue(Preferences::SplitViewPreviewMaxWidth,
            m_splitPreviewAutoCheck->isChecked() ? 0 : m_splitPreviewWidthSpin->value());
        settings.setValue(Preferences::AutoSaveOnExit, m_autoSaveExitCheck->isChecked());
        int interval = m_autoSaveCheck->isChecked() ? m_autoSaveSpin->value() : 0;
        settings.setValue(Preferences::AutoSaveInterval, interval);
        settings.setValue(Preferences::FileCompletionLimit, m_fileCompletionSpin->value());
         settings.setValue(Preferences::FileAutoComplete, m_filenameAutoCompleteCheck->isChecked());
        settings.setValue(Preferences::EditorFontFamily, m_editorFontCombo->currentText());
        settings.setValue(Preferences::EditorFontSize, m_editorFontSizeSpin->value());
        settings.setValue(Preferences::UiFontSize, m_uiFontSizeSpin->value());
        settings.setValue(Preferences::EditorLineHeight, m_editorLineHeightSpin->value());
        settings.setValue(Preferences::EditorPadding, m_editorPaddingSpin->value());
        settings.setValue(Preferences::EditorCaretWidth, m_editorCaretWidthSpin->value());
        settings.setValue(Preferences::EditorColorOverride, m_overrideGroup->isChecked());
        settings.setValue(Preferences::EditorBgColor, m_editorBgBtn->text());
        settings.setValue(Preferences::EditorFontColor, m_editorFontBtn->text());
        settings.setValue(Preferences::ReadabilityFormula,
            Preferences::formulaToString(
                static_cast<Preferences::Formula>(m_readabilityCombo->currentData().toInt())));
        QStringList checkedMetrics;
        for (auto *cb : m_metricChecks) {
            if (cb->isChecked())
                checkedMetrics << cb->property("metricKey").toString();
        }
        settings.setValue(Preferences::StatusBarMetrics, checkedMetrics);
        settings.setValue(Preferences::WordsPerSecond, m_wpsSpin->value());
        settings.setValue(Preferences::SpeakingWpm, m_spWpmSpin->value());
        settings.setValue(Preferences::StripPreviewScripts, m_stripPreviewScriptsCheck->isChecked());
        settings.setValue(Preferences::StripExportScripts, m_stripExportScriptsCheck->isChecked());
        settings.setValue(Preferences::BlockRawHtmlPreview, m_blockRawHtmlPreviewCheck->isChecked());
        settings.setValue(Preferences::BlockRawHtmlExport, m_blockRawHtmlExportCheck->isChecked());
        settings.setValue(Preferences::EnableCspPreview, m_enableCspPreviewCheck->isChecked());
        settings.setValue(Preferences::EnableCspExport, m_enableCspExportCheck->isChecked());
        settings.setValue(Preferences::ShowLineNumbers, m_showLineNumbersCheck->isChecked());
        settings.setValue(Preferences::AutoAlignTables, m_autoAlignTablesCheck->isChecked());
        settings.setValue(Preferences::GutterColorOverride, m_gutterOverrideGroup->isChecked());
        settings.setValue(Preferences::GutterBgColor, m_gutterBgBtn->text());
        settings.setValue(Preferences::GutterTextColor, m_gutterTextBtn->text());
        settings.setValue(Preferences::HeavyRenderDelay, m_heavyRenderDelaySpin->value());
        settings.setValue(Preferences::PreviewUpdateDelay, m_previewUpdateDelaySpin->value());
        settings.setValue(Preferences::HardSoftBreaks, m_hardSoftBreaksCheck->isChecked());
        settings.setValue(Preferences::SpellCheckEnabled, m_spellCheckCheck->isChecked());
        settings.setValue(Preferences::GrammarCheckEnabled, m_grammarCheckCheck->isChecked());
        settings.setValue(Preferences::LinkCheckEnabled, m_linkCheckCheck->isChecked());
        settings.setValue(Preferences::MarkdownCheckEnabled, m_markdownCheckCheck->isChecked());
        static const std::array<const char *, 7> kMdCheckKeys = {{
            Preferences::MarkdownCheckHeadingLevelSkip,
            Preferences::MarkdownCheckDuplicateHeading,
            Preferences::MarkdownCheckTrailingWhitespace,
            Preferences::MarkdownCheckConsecutiveBlankLines,
            Preferences::MarkdownCheckOverlongLine,
            Preferences::MarkdownCheckHashNoSpace,
            Preferences::MarkdownCheckFootnoteReference,
        }};
        for (qsizetype i = 0; i < m_markdownSubChecks.size(); ++i)
            settings.setValue(QLatin1String(kMdCheckKeys[static_cast<size_t>(i)]),
                              m_markdownSubChecks.at(i)->isChecked());
        settings.setValue(Preferences::UnderlineColorOverride, m_underlineColorGroup->isChecked());
        settings.setValue(Preferences::SpellUnderlineColor, m_spellColorBtn->text());
        settings.setValue(Preferences::GrammarUnderlineColor, m_grammarColorBtn->text());
        settings.setValue(Preferences::LinkUnderlineColor, m_linkColorBtn->text());
        settings.setValue(Preferences::MarkdownUnderlineColor, m_markdownColorBtn->text());
        settings.setValue(Preferences::DictionaryLanguage, m_languageCombo->currentData().toString());
        settings.setValue(Preferences::GrammarDialect, m_grammarDialectCombo->currentText());
        QStringList customWords;
        for (int i = 0; i < m_customWordsList->count(); ++i)
            customWords << m_customWordsList->item(i)->text();
        SpellChecker::writeUserDictionaryWords(customWords);
        QStringList ignoredWords;
        for (int i = 0; i < m_ignoredWordsList->count(); ++i)
            ignoredWords << m_ignoredWordsList->item(i)->text();
        SpellChecker::writeIgnoredWords(ignoredWords);
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PreferencesDialog::populateStylesheetList()
{
    m_listWidget->clear();
    QString active = m_config->activeStylesheet();

    for (const QString &path : m_config->stylesheets()) {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_listWidget->addItem(item);
        if (path == active)
            m_listWidget->setCurrentItem(item);
    }
}

void PreferencesDialog::addStylesheet()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select CSS Files", QString(), "CSS Files (*.css)");
    if (files.isEmpty()) return;

    QStringList existing = m_config->stylesheets();
    for (const QString &file : files) {
        if (!existing.contains(file))
            existing.append(file);
    }
    m_config->setStylesheets(existing);
    populateStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::removeStylesheet()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();
    QStringList existing = m_config->stylesheets();
    existing.removeAll(path);

    if (path == m_config->activeStylesheet())
        m_config->setActiveStylesheet(QString());

    m_config->setStylesheets(existing);
    populateStylesheetList();
    emit stylesheetChanged();
}

void PreferencesDialog::duplicateStylesheet()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QString src = item->data(Qt::UserRole).toString();
    QString suggested = QFileInfo(src).completeBaseName() + "-copy";

    bool ok = false;
    QString name = QInputDialog::getText(this, "Duplicate Theme",
        "Name for the new theme.\nWill be stored in:\n" + m_loader->themesDir() + "/",
        QLineEdit::Normal, suggested, &ok);
    if (!ok) return;
    if (name.trimmed().isEmpty())
        name = suggested;

    QString newPath = duplicateCssFile(src, m_loader->themesDir(), name);
    if (newPath.isEmpty()) {
        QMessageBox::warning(this, "Duplicate Theme",
            "Could not duplicate the selected stylesheet. The name may already be in use.");
        return;
    }

    QStringList existing = m_config->stylesheets();
    if (!existing.contains(newPath))
        existing.append(newPath);
    m_config->setStylesheets(existing);
    populateStylesheetList();

    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->data(Qt::UserRole).toString() == newPath) {
            m_listWidget->setCurrentRow(i);
            break;
        }
    }
    emit stylesheetChanged();
}

void PreferencesDialog::editStylesheet()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();
    if (path.startsWith(":/") || path.startsWith("qrc:")) return;

    QString css = readResourceFile(path);
    if (css.isEmpty()) return;

    CssEditorDialog dlg("Edit Theme - " + QFileInfo(path).fileName(), css, css, m_loader->themeCss(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(dlg.css().toUtf8());
            m_loader->invalidateCache();
            emit stylesheetChanged();
        }
    }
}

void PreferencesDialog::editPreviewBaseCss()
{
    CssEditorDialog dlg("Edit Preview Base CSS", m_loader->previewBaseCss(),
        readResourceFile(":/preview-base.css"), m_loader->themeCss(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_loader->setPreviewBaseCss(dlg.css());
        emit stylesheetChanged();
    }
}

void PreferencesDialog::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current) {
        m_editButton->setEnabled(false);
        return;
    }
    QString path = current->data(Qt::UserRole).toString();
    bool bundled = path.startsWith(":/") || path.startsWith("qrc:");
    m_editButton->setEnabled(!bundled);
    m_editButton->setToolTip(bundled
        ? "Bundled themes can't be edited. Use Duplicate first."
        : QString());
    m_config->setActiveStylesheet(path);
    emit stylesheetChanged();
}

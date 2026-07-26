#include "PreferencesDialog.h"
#include "StaticHelpers.h"
#include "CssConfig.h"
#include "CssLoader.h"
#include "CssEditorDialog.h"
#include "Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QSettings>
#include <QGroupBox>
#include <QLabel>
#include <QIcon>
#include <QFile>
#include <QStackedWidget>
#include <QColorDialog>

PreferencesDialog::PreferencesDialog(CssConfig *config, CssLoader *loader, QWidget *parent,
    const QString &themeBgColor, const QString &themeFgColor)
    : QDialog(parent)
    , m_config(config)
    , m_loader(loader)
{
    setupUi(themeBgColor, themeFgColor);
    setWindowTitle("Preferences");
    resize(450, 600);
}

void PreferencesDialog::setupUi(const QString &themeBgColor, const QString &themeFgColor)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    /* --- Sidebar + Pages --- */
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    m_categoryList = new QListWidget;
    m_categoryList->setMaximumWidth(150);
    m_categoryList->setMinimumWidth(120);
    m_categoryList->setFrameShape(QFrame::NoFrame);
    QFont catFont = m_categoryList->font();
    catFont.setPointSize(catFont.pointSize() + 3);
    m_categoryList->setFont(catFont);
    m_categoryList->setObjectName("category-list");
    contentLayout->addWidget(m_categoryList);

    m_pages = new QStackedWidget;
    contentLayout->addWidget(m_pages, 1);
    mainLayout->addLayout(contentLayout, 1);

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

        QGroupBox *filenameAutoCompleteGroup = new QGroupBox("Filename Autocomplete");
        QVBoxLayout *filenameAutoCompleteLayout = new QVBoxLayout(filenameAutoCompleteGroup);
        filenameAutoCompleteLayout->addSpacing(8);

        m_filenameAutoCompleteCheck = new QCheckBox("Enable filename autocomplete");
        m_filenameAutoCompleteCheck->setChecked(settings.value(Preferences::FileAutoComplete, true).toBool());
        filenameAutoCompleteLayout->addWidget(m_filenameAutoCompleteCheck);

        QHBoxLayout *compRow = new QHBoxLayout();
        compRow->addWidget(new QLabel("Filename autocomplete limit:"));
        m_fileCompletionSpin = new QSpinBox();
        m_fileCompletionSpin->setRange(2, 100);
        m_fileCompletionSpin->setValue(settings.value(Preferences::FileCompletionLimit, 20).toInt());
        compRow->addWidget(m_fileCompletionSpin);
        compRow->addStretch();
        filenameAutoCompleteLayout->addLayout(compRow);
        layout->addWidget(filenameAutoCompleteGroup);

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

        QGroupBox *emojiGroup = new QGroupBox("Emojis");
        QVBoxLayout *emojiLayout = new QVBoxLayout(emojiGroup);
        emojiLayout->addSpacing(8);

        m_emojiAutoCompleteCheck = new QCheckBox("Use emoji auto-complete");
        m_emojiAutoCompleteCheck->setChecked(settings.value(Preferences::EmojiAutoComplete, true).toBool());
        emojiLayout->addWidget(m_emojiAutoCompleteCheck);

        auto mode = Preferences::emojiRenderingFromString(
            settings.value(Preferences::EmojiMode, Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString());
        m_emojiBw = new QRadioButton("Black && White");
        m_emojiColor = new QRadioButton("Color (twemoji)");
        m_emojiBw->setChecked(mode == Preferences::EmojiRendering::Bw);
        m_emojiColor->setChecked(mode == Preferences::EmojiRendering::Color);
        emojiLayout->addWidget(m_emojiBw);
        emojiLayout->addWidget(m_emojiColor);

        layout->addWidget(emojiGroup);

        layout->addStretch();

        m_pages->addWidget(page);
        m_categoryList->addItem("General");
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

        QVBoxLayout *btnLayout = new QVBoxLayout();
        m_addButton = new QPushButton("Add");
        m_removeButton = new QPushButton("Remove");
        btnLayout->addWidget(m_addButton);
        btnLayout->addWidget(m_removeButton);
        btnLayout->addStretch();

        listRow->addWidget(m_listWidget);
        listRow->addLayout(btnLayout);
        cssLayout->addLayout(listRow);

        layout->addWidget(cssGroup);

        layout->addStretch();

        m_pages->addWidget(page);
        m_categoryList->addItem("Themes");
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
        m_editorFontSizeSpin->setValue(settings.value(Preferences::EditorFontSize, 18).toInt());
        editorLayout->addRow("Font size:", m_editorFontSizeSpin);

        m_editorLineHeightSpin = new QSpinBox();
        m_editorLineHeightSpin->setRange(100, 400);
        m_editorLineHeightSpin->setSuffix(" %");
        m_editorLineHeightSpin->setValue(settings.value(Preferences::EditorLineHeight, 240).toInt());
        editorLayout->addRow("Line height:", m_editorLineHeightSpin);

        m_editorPaddingSpin = new QSpinBox();
        m_editorPaddingSpin->setRange(0, 60);
        m_editorPaddingSpin->setSuffix(" px");
        m_editorPaddingSpin->setValue(settings.value(Preferences::EditorPadding, 12).toInt());
        editorLayout->addRow("Padding:", m_editorPaddingSpin);

        auto emitEditorSettings = [this]() {
            emit editorSettingsChanged(m_editorFontCombo->currentText(),
                m_editorFontSizeSpin->value(), m_editorLineHeightSpin->value(),
                m_editorPaddingSpin->value());
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

        layout->addWidget(editorGroup);
        layout->addStretch();

        m_pages->addWidget(page);
        m_categoryList->addItem("Editor");
    }

    /* --- Connections --- */
    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addStylesheet);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeStylesheet);
    connect(m_editPreviewBtn, &QPushButton::clicked, this, &PreferencesDialog::editPreviewBaseCss);
    connect(m_listWidget, &QListWidget::currentItemChanged, this, &PreferencesDialog::onCurrentItemChanged);
    connect(m_categoryList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);

    populateStylesheetList();
    m_categoryList->setCurrentRow(0);

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
        settings.setValue(Preferences::EmojiMode,
    Preferences::emojiRenderingToString(m_emojiBw->isChecked() ? Preferences::EmojiRendering::Bw : Preferences::EmojiRendering::Color));
        settings.setValue(Preferences::EmojiAutoComplete, m_emojiAutoCompleteCheck->isChecked());
        settings.setValue(Preferences::CentreSingleViewContent, m_centreSingleViewCheck->isChecked());
        settings.setValue(Preferences::CentreSingleViewWidth, m_centreSingleViewWidthSpin->value());
        settings.setValue(Preferences::AutoSaveOnExit, m_autoSaveExitCheck->isChecked());
        int interval = m_autoSaveCheck->isChecked() ? m_autoSaveSpin->value() : 0;
        settings.setValue(Preferences::AutoSaveInterval, interval);
        settings.setValue(Preferences::FileCompletionLimit, m_fileCompletionSpin->value());
         settings.setValue(Preferences::FileAutoComplete, m_filenameAutoCompleteCheck->isChecked());
        settings.setValue(Preferences::EditorFontFamily, m_editorFontCombo->currentText());
        settings.setValue(Preferences::EditorFontSize, m_editorFontSizeSpin->value());
        settings.setValue(Preferences::EditorLineHeight, m_editorLineHeightSpin->value());
        settings.setValue(Preferences::EditorPadding, m_editorPaddingSpin->value());
        settings.setValue(Preferences::EditorColorOverride, m_overrideGroup->isChecked());
        settings.setValue(Preferences::EditorBgColor, m_editorBgBtn->text());
        settings.setValue(Preferences::EditorFontColor, m_editorFontBtn->text());
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
    if (!current) return;
    m_config->setActiveStylesheet(current->data(Qt::UserRole).toString());
    emit stylesheetChanged();
}

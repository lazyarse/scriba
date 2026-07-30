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
#include <QDoubleSpinBox>

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
        m_editorFontSizeSpin->setSuffix(" px");
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

        /* --- Gutter --- */
        QGroupBox *gutterGroup = new QGroupBox("Gutter");
        QVBoxLayout *gutterLayout = new QVBoxLayout(gutterGroup);
        gutterLayout->addSpacing(8);

        m_showLineNumbersCheck = new QCheckBox("Show line numbers");
        m_showLineNumbersCheck->setChecked(settings.value(Preferences::ShowLineNumbers, true).toBool());
        gutterLayout->addWidget(m_showLineNumbersCheck);

        m_showFoldIconsCheck = new QCheckBox("Enable code folding");
        m_showFoldIconsCheck->setChecked(settings.value(Preferences::ShowFoldIcons, true).toBool());
        gutterLayout->addWidget(m_showFoldIconsCheck);

        auto emitGutterSettings = [this]() {
            QSettings s;
            s.setValue(Preferences::ShowLineNumbers, m_showLineNumbersCheck->isChecked());
            s.setValue(Preferences::ShowFoldIcons, m_showFoldIconsCheck->isChecked());
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
        layout->addStretch();

        m_pages->addWidget(page);
        m_categoryList->addItem("Editor");
    }

    /* --- Page 3: Writing --- */
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
        maxLabel->setStyleSheet("color: gray; font-size: 11px;");
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

        m_pages->addWidget(page);
        m_categoryList->addItem("Writing");
    }

    /* --- Page 4: Security --- */
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
            "(KaTeX, Mermaid, highlight.js, Vega-Lite) and the app's own initialization code use inline "
            "scripts and styles. A stricter CSP would break rendering. "
            "The current policy blocks inline event handlers (onclick, onerror), javascript: URLs, "
            "and external network requests.");
        cspNote->setWordWrap(true);
        cspNote->setStyleSheet("color: gray; font-size: small; padding: 8px;");
        layout->addWidget(cspNote);

        layout->addStretch();

        m_pages->addWidget(page);
        m_categoryList->addItem("Security");
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
        settings.setValue(Preferences::ShowFoldIcons, m_showFoldIconsCheck->isChecked());
        settings.setValue(Preferences::GutterColorOverride, m_gutterOverrideGroup->isChecked());
        settings.setValue(Preferences::GutterBgColor, m_gutterBgBtn->text());
        settings.setValue(Preferences::GutterTextColor, m_gutterTextBtn->text());
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

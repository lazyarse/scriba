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
#include "spell/SpellChecker.h"
#include "preview/Typography.h"
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QListWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>

void PreferencesDialog::setupWritingPage()
{
    QSettings settings;

    /* --- Page 5: Metrics --- */
    {
        QWidget *page = addPage(tr("Metrics"));
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

        metricsLayout->addSpacing(4);
        auto *countsLabel = new QLabel("<b>Counts</b>");
        metricsLayout->addWidget(countsLabel);

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

        // connect all checkboxes to limit enforcement
        auto enforceLimit = [this, kMaxMetrics, maxLabel]() {
            int count = 0;
            for (auto *cb : this->m_metricChecks) {
                if (cb->isChecked())
                    ++count;
            }
            bool atLimit = count >= kMaxMetrics;
            for (auto *cb : this->m_metricChecks) {
                if (!cb->isChecked())
                    cb->setEnabled(!atLimit);
            }
            maxLabel->setText(
                QStringLiteral("Select up to %1 metrics (%2/%1)%3")
                    .arg(kMaxMetrics)
                    .arg(count)
                    .arg(atLimit ? QString(" (max %1 reached)").arg(kMaxMetrics) : ""));
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

    }
}

void PreferencesDialog::setupTypographyPage()
{
    QSettings settings;

    /* --- Page: Typography --- */
    {
        QWidget *page = addPage(tr("Typography"));
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

        m_typographyArrowsCheck = new QCheckBox(tr("Arrows"));
        m_typographyArrowsCheck->setToolTip(tr("-> <- => <-> become \u2192 \u2190 \u21d2 \u2194; <= >= != +- become \u2264 \u2265 \u2260 \u00b1"));
        m_typographyArrowsCheck->setChecked(settings.value(Preferences::TypographyArrows, false).toBool());
        groupLayout->addWidget(m_typographyArrowsCheck);

        groupLayout->addSpacing(8);
        auto *exampleLabel = new QLabel(tr("<b>Example</b>"));
        groupLayout->addWidget(exampleLabel);

        const QString sample = QStringLiteral("He said \"It's easy -- 3x4 ... 1/2 of 90oF in 10 kg (c) 2026\", use x -> y or a <- b");
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
            if (m_typographyArrowsCheck->isChecked())
                opts |= Typography::Option::Arrows;
            Typography::State state;
            m_typographyExampleLabel->setText(Typography::apply(sample, opts, state));
        };
        for (auto *cb : { m_typographyQuotesCheck, m_typographyDashesCheck,
                          m_typographyEllipsisCheck, m_typographyMultiplicationCheck,
                          m_typographyDegreeFractionPrimeCheck, m_typographyNbspCheck,
                          m_typographySymbolsCheck, m_typographyArrowsCheck })
            connect(cb, &QCheckBox::toggled, this, updateExample);
        updateExample();

        layout->addWidget(group);

        QGroupBox *listGroup = new QGroupBox("Ordered list numbering");
        QVBoxLayout *listLayout = new QVBoxLayout(listGroup);
        listLayout->addSpacing(8);

        auto *listNote = new QLabel(tr(
            "How ordered lists are numbered in the preview and exports. Your "
            "Markdown source is never changed \u2014 it keeps whatever delimiter "
            "(1. or 1)) you typed."));
        listNote->setWordWrap(true);
        listNote->setStyleSheet("color: gray; padding: 8px;");
        listLayout->addWidget(listNote);

        auto *listRow = new QHBoxLayout;
        auto *listLabel = new QLabel(tr("Marker format:"));
        m_orderedListMarkerCombo = new QComboBox;
        m_orderedListMarkerCombo->setObjectName(QStringLiteral("ordered-list-marker"));
        m_orderedListMarkerCombo->addItem(tr("1. 2. 3."),  QVariant(QStringLiteral("decimal")));
        m_orderedListMarkerCombo->addItem(tr("1) 2) 3)"), QVariant(QStringLiteral("decimal-paren")));
        m_orderedListMarkerCombo->addItem(tr("a. b. c."),  QVariant(QStringLiteral("alpha")));
        m_orderedListMarkerCombo->addItem(tr("a) b) c)"), QVariant(QStringLiteral("alpha-paren")));
        m_orderedListMarkerCombo->addItem(tr("i. ii. iii."), QVariant(QStringLiteral("roman")));
        m_orderedListMarkerCombo->addItem(tr("i) ii) iii)"), QVariant(QStringLiteral("roman-paren")));
        const QString marker = settings.value(Preferences::OrderedListMarker,
            Preferences::defaultOrderedListMarker()).toString();
        const int markerIdx = m_orderedListMarkerCombo->findData(marker);
        m_orderedListMarkerCombo->setCurrentIndex(markerIdx >= 0 ? markerIdx : 0);
        listRow->addWidget(listLabel);
        listRow->addWidget(m_orderedListMarkerCombo, 1);
        listLayout->addLayout(listRow);

        layout->addWidget(listGroup);
        layout->addStretch();

    }
}

void PreferencesDialog::setupReplacementsPage()
{
    QSettings settings;

    /* --- Page 6: Auto-correct --- */
    {
        QWidget *page = addPage(tr("Auto-correct"));
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
            "When you finish a word (space, punctuation or Enter), it is replaced if it "
            "matches a Typo entry — e.g. \"teh\" becomes \"The\". Matching ignores case and "
            "keeps your typing's case. Ctrl+Z undoes one; corrections are skipped in code "
            "blocks, inline code and link URLs. Unlike Smart Typography, these replacements "
            "edit the Markdown source itself."));
        note->setWordWrap(true);
        note->setStyleSheet("color: gray; padding: 8px;");
        groupLayout->addWidget(note);

        m_replacementsTable = new QTableWidget(0, 2);
        m_replacementsTable->setHorizontalHeaderLabels({tr("Typo"), tr("Replacement")});
        m_replacementsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_replacementsTable->verticalHeader()->setVisible(false);
        m_replacementsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        groupLayout->addWidget(m_replacementsTable);

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

    }
}

void PreferencesDialog::setupSpellingPage()
{
    QSettings settings;

    /* --- Page 5: Proofing --- */
    {
        QWidget *page = addPage(tr("Proofing"));
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 16, 0, 0);
        layout->setSpacing(8);

        QGroupBox *checkGroup = new QGroupBox("Check:");
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

        m_markdownCheckCheck = new QCheckBox("Check markdown as you type");
        m_markdownCheckCheck->setChecked(settings.value(Preferences::MarkdownCheckEnabled, false).toBool());
        checkLayout->addWidget(m_markdownCheckCheck);

        layout->addWidget(checkGroup);

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

        if (m_corpus && !m_corpus->filePath.isEmpty()) {
            // A corpus's dictionary overrides the global language/dialect for
            // spell-checking while the corpus is active; show its values
            // read-only so the global selections are preserved untouched.
            const CorpusDictionary &dict = m_corpus->dictionary;
            if (!dict.language.isEmpty()) {
                const int li = m_languageCombo->findData(dict.language);
                if (li >= 0)
                    m_languageCombo->setCurrentIndex(li);
                m_languageCombo->setEnabled(false);
            }
            if (!dict.dialect.isEmpty()) {
                const int di = m_grammarDialectCombo->findText(dict.dialect);
                if (di >= 0)
                    m_grammarDialectCombo->setCurrentIndex(di);
                m_grammarDialectCombo->setEnabled(false);
            }
            if (!dict.language.isEmpty() || !dict.dialect.isEmpty()) {
                auto *corpusNote = new QLabel(tr(
                    "A corpus is open; its dictionary language/dialect override "
                    "these selections for spell-checking while the corpus is active."));
                corpusNote->setWordWrap(true);
                corpusNote->setStyleSheet("color: gray; padding: 8px;");
                dictionaryLayout->addRow(QString(), corpusNote);
            }
        }

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

        QGroupBox *corpusDictGroup = new QGroupBox("Corpus Dictionary");
        QVBoxLayout *corpusDictLayout = new QVBoxLayout(corpusDictGroup);
        corpusDictLayout->addSpacing(8);

        auto *corpusDictLabel = new QLabel(tr("When a Corpus is open, its custom words:"));
        corpusDictLabel->setWordWrap(true);
        corpusDictLayout->addWidget(corpusDictLabel);

        m_corpusDictOverride = new QRadioButton(tr("&Replace the global dictionary"));
        m_corpusDictOverride->setObjectName("corpus-dict-override");
        m_corpusDictMerge = new QRadioButton(tr("&Merge with the global dictionary"));
        m_corpusDictMerge->setObjectName("corpus-dict-merge");
        if (settings.value(Preferences::CorpusDictionaryMode, QStringLiteral("override"))
                .toString() == QLatin1String("merge"))
            m_corpusDictMerge->setChecked(true);
        else
            m_corpusDictOverride->setChecked(true);
        corpusDictLayout->addWidget(m_corpusDictOverride);
        corpusDictLayout->addWidget(m_corpusDictMerge);

        auto *corpusDictNote = new QLabel(tr(
            "Replace uses only the corpus's custom words; merge adds them to your "
            "global user dictionary."));
        corpusDictNote->setWordWrap(true);
        corpusDictNote->setStyleSheet("color: gray; padding: 8px;");
        corpusDictLayout->addWidget(corpusDictNote);

        layout->addWidget(corpusDictGroup);

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

    }
}

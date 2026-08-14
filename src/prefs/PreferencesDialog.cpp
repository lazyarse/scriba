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
#include "css/CssConfig.h"
#include "css/CssLoader.h"
#include "css/CssEditorDialog.h"
#include "Preferences.h"
#include "preview/Typography.h"
#include "spell/SpellChecker.h"
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
#include <QStyle>
#include <QShortcut>
#include <QTextDocumentFragment>
#include <QRegularExpression>
#include <QAbstractButton>
#include <QSet>
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
    const QString &themeBgColor, const QString &themeFgColor, Corpus *corpus)
    : QDialog(parent)
    , m_config(config)
    , m_loader(loader)
    , m_corpus(corpus)
    , m_themeBgColor(themeBgColor)
    , m_themeFgColor(themeFgColor)
{
    setupUi(themeBgColor, themeFgColor);
    setWindowTitle("Preferences");
    resize(750, 600);
}

QWidget *PreferencesDialog::addPage(const QString &name)
{
    QWidget *page = new QWidget;
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);
    m_pages->addWidget(scroll);
    m_pageList->addItem(name);
    return page;
}

void PreferencesDialog::setupUi(const QString &themeBgColor, const QString &themeFgColor)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search settings... (Ctrl+F)"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setObjectName("preferences-search");
    mainLayout->addWidget(m_searchEdit);

    m_searchInfoLabel = new QLabel;
    m_searchInfoLabel->setObjectName("preferences-search-info");
    m_searchInfoLabel->setWordWrap(true);
    m_searchInfoLabel->setStyleSheet("color: gray;");
    m_searchInfoLabel->setVisible(false);
    mainLayout->addWidget(m_searchInfoLabel);

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

    setupGeneralPage();
    setupAppearancePage();
    setupThemesPage();
    setupEditorPage();
    setupPreviewPage();
    setupPrintingPage();
    setupWritingPage();
    setupTypographyPage();
    setupReplacementsPage();
    setupSpellingPage();
    setupCorpusPage();
    setupSecurityPage();

    /* --- Connections --- */
    connect(m_addButton, &QPushButton::clicked, this, &PreferencesDialog::addStylesheet);
    connect(m_removeButton, &QPushButton::clicked, this, &PreferencesDialog::removeStylesheet);
    connect(m_duplicateButton, &QPushButton::clicked, this, &PreferencesDialog::duplicateStylesheet);
    connect(m_editButton, &QPushButton::clicked, this, &PreferencesDialog::editStylesheet);
    connect(m_editPreviewBtn, &QPushButton::clicked, this, &PreferencesDialog::editPreviewBaseCss);
    connect(m_listWidget, &QListWidget::currentItemChanged, this, &PreferencesDialog::onCurrentItemChanged);
    connect(m_pageList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_grammarCheckCheck, &QCheckBox::toggled, m_grammarDialectCombo, &QWidget::setEnabled);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PreferencesDialog::onSearchTextChanged);
    connect(m_pageList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_searchEdit->text().trimmed().isEmpty())
            applySearchDim(row);
    });

    auto *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });

    populateStylesheetList();
    m_pageList->setCurrentRow(0);
    buildSearchIndex();

    /* --- Dialog Buttons --- */
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(buttonBox);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        QSettings settings;
        settings.setValue(Preferences::ReopenLastCorpus, m_reopenCheck->isChecked());
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
        settings.setValue(Preferences::TypographyArrows, m_typographyArrowsCheck->isChecked());
        settings.setValue(Preferences::OrderedListMarker,
            m_orderedListMarkerCombo->currentData().toString());
        settings.setValue(Preferences::PrintCodeSplit, m_printCodeSplitCombo->currentData().toString());
        settings.setValue(Preferences::PrintKeepTables, m_printKeepTablesCheck->isChecked());
        settings.setValue(Preferences::PrintKeepHeadings, m_printKeepHeadingsCheck->isChecked());
        settings.setValue(Preferences::PrintKeepFigures, m_printKeepFiguresCheck->isChecked());
        settings.setValue(Preferences::PrintOrphanControl, m_printOrphanControlCheck->isChecked());
        settings.setValue(Preferences::PrintPageMargin, m_printMarginEdit->text().trimmed());
        settings.setValue(Preferences::PrintPageSize, m_printSizeEdit->text().trimmed());
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
        settings.setValue(Preferences::TabBarAlwaysShow, m_tabBarAlwaysShowCheck->isChecked());
        if (m_showPageBreaksCheck)
            settings.setValue(Preferences::PreviewShowPageBreaks, m_showPageBreaksCheck->isChecked());
        settings.setValue(Preferences::SplitViewEditorMaxWidth,
            m_splitEditorAutoCheck->isChecked() ? 0 : m_splitEditorWidthSpin->value());
        settings.setValue(Preferences::SplitViewPreviewMaxWidth,
            m_splitPreviewAutoCheck->isChecked() ? 0 : m_splitPreviewWidthSpin->value());
        settings.setValue(Preferences::EditorWrapEnabled, m_wrapModeCombo->currentIndex() != 0);
        settings.setValue(Preferences::EditorWrapMode, m_wrapModeCombo->currentData().toString());
        settings.setValue(Preferences::EditorWrapColumn, m_wrapColumnSpin->value());
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
        settings.setValue(Preferences::TablePadding, m_tablePaddingSpin->value());
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
        // An active corpus's dictionary language/dialect override the global
        // selections for spell-checking; when a corpus is open the combos show
        // the corpus values read-only, so don't clobber the global prefs.
        if (!m_corpus || m_corpus->filePath.isEmpty()) {
            settings.setValue(Preferences::DictionaryLanguage, m_languageCombo->currentData().toString());
            settings.setValue(Preferences::GrammarDialect, m_grammarDialectCombo->currentText());
        }
        QStringList customWords;
        for (int i = 0; i < m_customWordsList->count(); ++i)
            customWords << m_customWordsList->item(i)->text();
        SpellChecker::writeUserDictionaryWords(customWords);
        QStringList ignoredWords;
        for (int i = 0; i < m_ignoredWordsList->count(); ++i)
            ignoredWords << m_ignoredWordsList->item(i)->text();
        SpellChecker::writeIgnoredWords(ignoredWords);
        QString imgLocation = QStringLiteral("ask");
        if (m_imgCurrentDir->isChecked()) imgLocation = QStringLiteral("currentDir");
        else if (m_imgCustomDir->isChecked()) imgLocation = QStringLiteral("customDir");
        else if (m_imgTempDir->isChecked()) imgLocation = QStringLiteral("tempDir");
        settings.setValue(Preferences::ImportImageLocation, imgLocation);
        settings.setValue(Preferences::ImportImageDir, m_imgDirEdit->text());
        QStringList recentCorpora;
        for (int i = 0; i < m_recentCorpusList->count(); ++i)
            recentCorpora << m_recentCorpusList->item(i)->text();
        settings.setValue(Preferences::RecentCorpora, recentCorpora);
        settings.setValue(Preferences::CorpusExternalEditPolicy,
            m_corpusEditPolicyCombo->currentData().toString());
        settings.setValue(Preferences::CorpusLinkRewritePolicy,
            m_linkRewritePolicyCombo->currentData().toString());
        settings.setValue(Preferences::CorpusLinkRewriteScope,
            m_linkRewriteScopeCombo->currentData().toString());
        settings.setValue(Preferences::CorpusUnsavedDocs,
            m_corpusUnsavedCombo->currentData().toString());
        const bool exportExternal =
            m_externalExportCombo->currentData().toString() == QLatin1String("subfolder");
        settings.setValue(Preferences::CorpusExternalExportDirName,
            exportExternal ? m_externalExportDirEdit->text().trimmed() : QString());
        settings.setValue(Preferences::CorpusDictionaryMode,
            m_corpusDictMerge->isChecked() ? QStringLiteral("merge") : QStringLiteral("override"));
        if (m_corpus && m_corpusMonitorCheck)
            m_corpus->monitor = m_corpusMonitorCheck->isChecked();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
#include <QColor>
#include <QPixmap>
#include <QSize>

QPushButton *PreferencesDialog::makeSwatchBtn(const QString &hex)
{
    auto *btn = new QPushButton;
    QPixmap px(16, 16);
    px.fill(QColor(hex));
    btn->setIcon(QIcon(px));
    btn->setIconSize(QSize(16, 16));
    btn->setText(hex);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void PreferencesDialog::updateContentWidthEnable()
{
    const bool columnMode = m_wrapModeCombo
        && m_wrapModeCombo->currentData().toString() == QLatin1String("column");
    if (m_centreSingleViewCheck)
        m_centreSingleViewWidthSpin->setEnabled(!columnMode && m_centreSingleViewCheck->isChecked());
    if (m_splitEditorAutoCheck) {
        m_splitEditorAutoCheck->setEnabled(!columnMode);
        m_splitEditorWidthSpin->setEnabled(!columnMode && m_splitEditorAutoCheck->isChecked());
    }
    if (m_splitPreviewAutoCheck)
        m_splitPreviewWidthSpin->setEnabled(m_splitPreviewAutoCheck->isChecked());
    if (m_wrapColumnSpin)
        m_wrapColumnSpin->setEnabled(columnMode);
}

namespace {

QString normalizeSearchText(QString s)
{
    s.replace(QStringLiteral("&&"), QChar('\x01'));
    s.remove(QChar('&'));
    s.replace(QChar('\x01'), QChar('&'));
    s.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    s = QTextDocumentFragment::fromHtml(s).toPlainText();
    return s.simplified().toLower();
}

QStringList searchTextsForWidget(const QWidget *w)
{
    QStringList texts;
    if (const auto *btn = qobject_cast<const QAbstractButton *>(w))
        texts << btn->text();
    if (const auto *group = qobject_cast<const QGroupBox *>(w))
        texts << group->title();
    if (const auto *label = qobject_cast<const QLabel *>(w))
        texts << label->text();
    if (const auto *combo = qobject_cast<const QComboBox *>(w))
        for (int i = 0; i < combo->count(); ++i)
            texts << combo->itemText(i);
    if (const auto *table = qobject_cast<const QTableWidget *>(w))
        for (int c = 0; c < table->columnCount(); ++c)
            if (const auto *h = table->horizontalHeaderItem(c))
                texts << h->text();
    if (!w->toolTip().isEmpty())
        texts << w->toolTip();
    return texts;
}

QStringList ancestorGroupTitles(const QWidget *w, const QWidget *top)
{
    QStringList titles;
    for (QWidget *p = w->parentWidget(); p && p != top; p = p->parentWidget())
        if (const auto *g = qobject_cast<const QGroupBox *>(p))
            titles << g->title();
    return titles;
}

bool widgetMatches(const QStringList &entryTexts, const QStringList &tokens)
{
    const QString combined = entryTexts.join(QLatin1Char(' '));
    for (const QString &tok : tokens) {
        if (!combined.contains(tok))
            return false;
    }
    return true;
}

void setWidgetDimmed(QWidget *w, bool dimmed)
{
    if (w->property("scribaPrefDim").toBool() == dimmed)
        return;
    w->setProperty("scribaPrefDim", dimmed);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

void setWidgetHighlighted(QWidget *w, bool highlighted)
{
    if (w->property("scribaPrefMatch").toBool() == highlighted)
        return;
    w->setProperty("scribaPrefMatch", highlighted);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

void PreferencesDialog::buildSearchIndex()
{
    m_searchIndex.clear();
    const int pageCount = m_pages->count();
    for (int i = 0; i < pageCount; ++i) {
        QList<SearchEntry> entries;
        QWidget *pageWidget = m_pages->widget(i);
        if (auto *scroll = qobject_cast<QScrollArea *>(pageWidget))
            pageWidget = scroll->widget();
        if (!pageWidget)
            continue;

        // The page name itself is searchable, so typing "General" finds it.
        if (QListWidgetItem *item = m_pageList->item(i)) {
            SearchEntry pageEntry;
            pageEntry.widget = pageWidget;
            pageEntry.texts << normalizeSearchText(item->text());
            entries << pageEntry;
        }

        for (QWidget *w : pageWidget->findChildren<QWidget *>()) {
            QStringList texts = searchTextsForWidget(w);
            texts += ancestorGroupTitles(w, pageWidget);
            QStringList normalized;
            for (const QString &t : texts) {
                const QString n = normalizeSearchText(t);
                if (!n.isEmpty())
                    normalized << n;
            }
            if (normalized.isEmpty())
                continue;
            SearchEntry entry;
            entry.widget = w;
            entry.texts = normalized;
            entries << entry;
        }

        m_searchIndex.push_back(entries);
    }
}

void PreferencesDialog::onSearchTextChanged(const QString &text)
{
    const QStringList tokens = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        for (int i = 0; i < m_pageList->count(); ++i)
            m_pageList->item(i)->setHidden(false);
        m_searchInfoLabel->setVisible(false);
        applySearchDim(-1);
        return;
    }

    int firstMatchRow = -1;
    for (int i = 0; i < m_pageList->count(); ++i) {
        const bool matches = i < static_cast<int>(m_searchIndex.size())
            && [&] {
                for (const auto &entry : m_searchIndex[static_cast<size_t>(i)])
                    if (widgetMatches(entry.texts, tokens))
                        return true;
                return false;
            }();
        m_pageList->item(i)->setHidden(!matches);
        if (matches && firstMatchRow < 0)
            firstMatchRow = i;
    }

    if (firstMatchRow < 0) {
        m_searchInfoLabel->setText(tr("No settings match \"%1\"").arg(text));
        m_searchInfoLabel->setVisible(true);
        applySearchDim(-1);
        return;
    }
    m_searchInfoLabel->setVisible(false);

    const int current = m_pages->currentIndex();
    if (current < 0 || current >= m_pageList->count() || m_pageList->item(current)->isHidden())
        m_pageList->setCurrentRow(firstMatchRow);
    else
        applySearchDim(current);
}

void PreferencesDialog::applySearchDim(int pageIndex)
{
    for (const auto &entries : m_searchIndex)
        for (const auto &entry : entries) {
            setWidgetDimmed(entry.widget, false);
            setWidgetHighlighted(entry.widget, false);
        }

    const QStringList tokens = m_searchEdit->text().toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty() || pageIndex < 0 || pageIndex >= static_cast<int>(m_searchIndex.size()))
        return;

    const QList<SearchEntry> &entries = m_searchIndex[static_cast<size_t>(pageIndex)];
    const QWidget *pageTop = m_pages->widget(pageIndex);
    if (const auto *scroll = qobject_cast<const QScrollArea *>(pageTop))
        pageTop = scroll->widget();

    // Widgets that match stay visible; containers that hold a matching widget
    // stay visible too (e.g. a group box whose title doesn't match but whose
    // child spin box does). Everything else on the page is dimmed.
    QSet<QWidget *> keep;
    QSet<QWidget *> matches;
    for (const auto &entry : entries)
        if (widgetMatches(entry.texts, tokens)) {
            keep.insert(entry.widget);
            matches.insert(entry.widget);
        }
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &entry : entries) {
            if (keep.contains(entry.widget))
                continue;
            for (QWidget *p = entry.widget->parentWidget(); p; p = p->parentWidget()) {
                if (keep.contains(p)) {
                    keep.insert(entry.widget);
                    changed = true;
                    break;
                }
            }
        }
    }

    for (const auto &entry : entries)
        if (!keep.contains(entry.widget))
            setWidgetDimmed(entry.widget, true);

    // Matches get a highlight (bold text + a subtle background tint) so they
    // stand out from the dimmed rest. The highlight also covers the group box
    // that holds a match so label and control read as one block, but never the
    // whole page widget itself.
    QSet<QWidget *> highlighted;
    for (QWidget *m : matches) {
        if (m == pageTop)
            continue;
        highlighted.insert(m);
        for (QWidget *p = m->parentWidget(); p && p != pageTop; p = p->parentWidget())
            if (qobject_cast<const QGroupBox *>(p))
                highlighted.insert(p);
    }
    for (QWidget *w : highlighted)
        setWidgetHighlighted(w, true);
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

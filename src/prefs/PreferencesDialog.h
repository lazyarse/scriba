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
#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <vector>


#include <QComboBox>
#include <QTableWidget>
#include <QLineEdit>
#include <QVector>
#include <QStringList>
#include <utility>

#include "corpus/Corpus.h"

class CssConfig;
class CssLoader;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(CssConfig *config, CssLoader *loader, QWidget *parent,
        const QString &themeBgColor, const QString &themeFgColor,
        Corpus *corpus = nullptr);

signals:
    void stylesheetChanged();
    void editorSettingsChanged(const QString &fontFamily, int fontSize, int lineHeight, int padding, int caretWidth);
    void uiFontSizeChanged(int uiFontSizePt);
    void underlineColorsChanged();

private slots:
    void addStylesheet();
    void removeStylesheet();
    void duplicateStylesheet();
    void editStylesheet();
    void editPreviewBaseCss();
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onSearchTextChanged(const QString &text);
private:
    void populateStylesheetList();
    void setupUi(const QString &themeBgColor, const QString &themeFgColor);
    // Builds one preferences page: wraps the page widget in a scroll area (so
    // it winds like the rest of the dialog) and registers it in the sidebar.
    QWidget *addPage(const QString &name);
    void setupGeneralPage();
    void setupAppearancePage();
    void setupThemesPage();
    void setupEditorPage();
    void setupPreviewPage();
    void setupPrintingPage();
    void setupWritingPage();
    void setupTypographyPage();
    void setupReplacementsPage();
    void setupSpellingPage();
    void setupCorpusPage();
    void setupSecurityPage();
    void buildSearchIndex();
    // Dims the widgets on the given page that don't match the active search
    // query (containers holding a match stay normal). Clears all dimming when
    // the search box is empty.
    void applySearchDim(int pageIndex);
    // Enables/disables the editor content-width controls (single-view content
    // width, split-view editor max width) based on the wrap mode: when the
    // editor wraps at a fixed column count, the column count takes over as the
    // editor's max width, so the px controls are greyed out.
    void updateContentWidthEnable();
    // Builds the 16x16 color-swatch buttons used on the Editor (appearance
    // overrides), Gutter and Spelling (underline color) pages.
    QPushButton *makeSwatchBtn(const QString &hex);

    CssConfig *m_config;
    CssLoader *m_loader;
    QComboBox *m_readabilityCombo = nullptr;
    QCheckBox *m_wordCountCheck = nullptr;
    QCheckBox *m_sentenceCountCheck = nullptr;
    QCheckBox *m_paragraphCountCheck = nullptr;
    QCheckBox *m_charNoSpaceCheck = nullptr;
    QCheckBox *m_charWithSpaceCheck = nullptr;
    QCheckBox *m_readingAgeCheck = nullptr;
    QCheckBox *m_fleschEaseCheck = nullptr;
    QCheckBox *m_readingTimeCheck = nullptr;
    QCheckBox *m_speakingTimeCheck = nullptr;
    QCheckBox *m_syllableCountCheck = nullptr;
    QCheckBox *m_complexWordsCheck = nullptr;
    QCheckBox *m_lexicalDensityCheck = nullptr;
    QCheckBox *m_avgWordsPerSentenceCheck = nullptr;
    QCheckBox *m_avgSyllablesPerWordCheck = nullptr;
    QDoubleSpinBox *m_wpsSpin = nullptr;
    QSpinBox *m_spWpmSpin = nullptr;
    std::vector<QCheckBox *> m_metricChecks;
    QListWidget *m_pageList;
    QStackedWidget *m_pages;
    QListWidget *m_listWidget;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_duplicateButton;
    QPushButton *m_editButton;
    QPushButton *m_editPreviewBtn;
    QCheckBox *m_reopenCheck;
    QCheckBox *m_syncCheck;
    QCheckBox *m_stripPreviewScriptsCheck;
    QCheckBox *m_stripExportScriptsCheck;
    QCheckBox *m_blockRawHtmlPreviewCheck;
    QCheckBox *m_blockRawHtmlExportCheck;
    QCheckBox *m_enableCspPreviewCheck;
    QCheckBox *m_enableCspExportCheck;
    QCheckBox *m_stripeCheck;
    QCheckBox *m_showCodeLangPreviewCheck;
    QCheckBox *m_showCodeLangExportCheck;
    QCheckBox *m_filenameAutoCompleteCheck;
    QCheckBox *m_autoSaveExitCheck;
    QCheckBox *m_autoSaveCheck;
    QSpinBox *m_autoSaveSpin;
    QRadioButton *m_imgCurrentDir;
    QRadioButton *m_imgCustomDir;
    QRadioButton *m_imgTempDir;
    QRadioButton *m_imgAsk;
    QLineEdit *m_imgDirEdit;
    QPushButton *m_imgDirBrowse;
    QSpinBox *m_fileCompletionSpin;
    QRadioButton *m_emojiBw;
    QRadioButton *m_emojiColor;
    QCheckBox *m_emojiAutoCompleteCheck;
    QSpinBox *m_emojiCompletionSpin;
    QCheckBox *m_languageAutoCompleteCheck;
    QCheckBox *m_autoCorrectCheck;
    QTableWidget *m_replacementsTable;

    // Typography
    QCheckBox *m_typographyQuotesCheck = nullptr;
    QCheckBox *m_typographyDashesCheck = nullptr;
    QCheckBox *m_typographyEllipsisCheck = nullptr;
    QCheckBox *m_typographyMultiplicationCheck = nullptr;
    QCheckBox *m_typographyDegreeFractionPrimeCheck = nullptr;
    QCheckBox *m_typographyNbspCheck = nullptr;
    QCheckBox *m_typographySymbolsCheck = nullptr;
    QCheckBox *m_typographyArrowsCheck = nullptr;
    QLabel *m_typographyPlainLabel = nullptr;
    QLabel *m_typographyExampleLabel = nullptr;
    QComboBox *m_orderedListMarkerCombo = nullptr;
    QComboBox *m_printCodeSplitCombo = nullptr;
    QCheckBox *m_printKeepTablesCheck = nullptr;
    QCheckBox *m_printKeepHeadingsCheck = nullptr;
    QCheckBox *m_printKeepFiguresCheck = nullptr;
    QCheckBox *m_printOrphanControlCheck = nullptr;
    QLineEdit *m_printMarginEdit = nullptr;
    QLineEdit *m_printSizeEdit = nullptr;
    QCheckBox *m_centreSingleViewCheck;
    QSpinBox *m_centreSingleViewWidthSpin;
    QCheckBox *m_tabBarAlwaysShowCheck = nullptr;
    QCheckBox *m_showPageBreaksCheck = nullptr;
    QCheckBox *m_splitEditorAutoCheck;
    QCheckBox *m_splitPreviewAutoCheck;
    QSpinBox *m_splitEditorWidthSpin;
    QSpinBox *m_splitPreviewWidthSpin;
    QComboBox *m_wrapModeCombo = nullptr;
    QSpinBox *m_wrapColumnSpin = nullptr;
    QComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QSpinBox *m_uiFontSizeSpin;
    QSpinBox *m_editorLineHeightSpin;
    QSpinBox *m_editorPaddingSpin;
    QSpinBox *m_editorCaretWidthSpin;
    QGroupBox *m_overrideGroup;
    QPushButton *m_editorBgBtn;
    QPushButton *m_editorFontBtn;

    // Tables
    QCheckBox *m_autoAlignTablesCheck;
    QSpinBox *m_tablePaddingSpin = nullptr;

    // Preview
    QCheckBox *m_hardSoftBreaksCheck;

    // Advanced
    QSpinBox *m_previewUpdateDelaySpin;
    QSpinBox *m_heavyRenderDelaySpin;

    // Gutter
    QCheckBox *m_showLineNumbersCheck;
    QGroupBox *m_gutterOverrideGroup;
    QPushButton *m_gutterBgBtn;
    QPushButton *m_gutterTextBtn;

    // Spelling
    QCheckBox *m_spellCheckCheck;
    QCheckBox *m_grammarCheckCheck;
    QCheckBox *m_linkCheckCheck;
    QCheckBox *m_markdownCheckCheck;
    QVector<QCheckBox*> m_markdownSubChecks; // per-rule real-time markdown checks
    QComboBox *m_languageCombo;
    QComboBox *m_grammarDialectCombo;
    QListWidget *m_customWordsList;
    QListWidget *m_importedList;
    QListWidget *m_ignoredWordsList;
    QGroupBox *m_underlineColorGroup;
    QPushButton *m_spellColorBtn;
    QPushButton *m_grammarColorBtn;
    QPushButton *m_linkColorBtn;
    QPushButton *m_markdownColorBtn;

    // Corpus
    Corpus *m_corpus = nullptr;
    QCheckBox *m_corpusMonitorCheck = nullptr;
    QListWidget *m_recentCorpusList = nullptr;
    QComboBox *m_corpusEditPolicyCombo = nullptr;
    QComboBox *m_linkRewritePolicyCombo = nullptr;
    QComboBox *m_linkRewriteScopeCombo = nullptr;
    QComboBox *m_corpusUnsavedCombo = nullptr;
    QComboBox *m_externalExportCombo = nullptr;
    QLineEdit *m_externalExportDirEdit = nullptr;
    QRadioButton *m_corpusDictOverride = nullptr;
    QRadioButton *m_corpusDictMerge = nullptr;

    // Settings search
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_searchInfoLabel = nullptr;
    // Theme colors handed to setupUi by the caller; the Editor page builder
    // reads them as defaults for the color-override buttons.
    QString m_themeBgColor;
    QString m_themeFgColor;
    struct SearchEntry {
        QWidget *widget = nullptr;
        QStringList texts; // normalized searchable text fragments
    };
    std::vector<QList<SearchEntry>> m_searchIndex; // one entry list per page
};


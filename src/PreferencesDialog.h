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

class CssConfig;
class CssLoader;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(CssConfig *config, CssLoader *loader, QWidget *parent,
        const QString &themeBgColor, const QString &themeFgColor);

signals:
    void stylesheetChanged();
    void editorSettingsChanged(const QString &fontFamily, int fontSize, int lineHeight, int padding);

private slots:
    void addStylesheet();
    void removeStylesheet();
    void duplicateStylesheet();
    void editStylesheet();
    void editPreviewBaseCss();
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
private:
    void populateStylesheetList();
    void setupUi(const QString &themeBgColor, const QString &themeFgColor);

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
    QLabel *m_selectionCountLabel = nullptr;
    std::vector<QCheckBox *> m_metricChecks;
    QListWidget *m_categoryList;
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
    QCheckBox *m_filenameAutoCompleteCheck;
    QCheckBox *m_autoSaveExitCheck;
    QCheckBox *m_autoSaveCheck;
    QSpinBox *m_autoSaveSpin;
    QSpinBox *m_fileCompletionSpin;
    QRadioButton *m_emojiBw;
    QRadioButton *m_emojiColor;
    QCheckBox *m_emojiAutoCompleteCheck;
    QCheckBox *m_languageAutoCompleteCheck;
    QCheckBox *m_centreSingleViewCheck;
    QSpinBox *m_centreSingleViewWidthSpin;
    QCheckBox *m_splitEditorAutoCheck;
    QCheckBox *m_splitPreviewAutoCheck;
    QSpinBox *m_splitEditorWidthSpin;
    QSpinBox *m_splitPreviewWidthSpin;
    QComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QSpinBox *m_editorLineHeightSpin;
    QSpinBox *m_editorPaddingSpin;
    QGroupBox *m_overrideGroup;
    QPushButton *m_editorBgBtn;
    QPushButton *m_editorFontBtn;

    // Gutter
    QCheckBox *m_showLineNumbersCheck;
    QCheckBox *m_showFoldIconsCheck;
    QGroupBox *m_gutterOverrideGroup;
    QPushButton *m_gutterBgBtn;
    QPushButton *m_gutterTextBtn;
};


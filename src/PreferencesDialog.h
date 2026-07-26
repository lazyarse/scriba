#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>


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
    void editPreviewBaseCss();
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
private:
    void populateStylesheetList();
    void setupUi(const QString &themeBgColor, const QString &themeFgColor);

    CssConfig *m_config;
    CssLoader *m_loader;
    QListWidget *m_categoryList;
    QStackedWidget *m_pages;
    QListWidget *m_listWidget;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_editPreviewBtn;
    QCheckBox *m_reopenCheck;
    QCheckBox *m_syncCheck;
    QCheckBox *m_stripeCheck;
    QCheckBox *m_filenameAutoCompleteCheck;
    QCheckBox *m_autoSaveExitCheck;
    QCheckBox *m_autoSaveCheck;
    QSpinBox *m_autoSaveSpin;
    QSpinBox *m_fileCompletionSpin;
    QRadioButton *m_emojiBw;
    QRadioButton *m_emojiColor;
    QCheckBox *m_emojiAutoCompleteCheck;
    QCheckBox *m_centreSingleViewCheck;
    QSpinBox *m_centreSingleViewWidthSpin;
    QComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QSpinBox *m_editorLineHeightSpin;
    QSpinBox *m_editorPaddingSpin;
    QGroupBox *m_overrideGroup;
    QPushButton *m_editorBgBtn;
    QPushButton *m_editorFontBtn;
};


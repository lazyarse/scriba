#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFontComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include "CssManager.h"

class Editor;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(CssManager *cssManager, Editor *editor, QWidget *parent = nullptr);

private slots:
    void selectDirectory();
    void addCssFile();
    void removeCssFile();
    void pickFontColor();
    void pickBgColor();
    void saveFontSettings();

private:
    void setupUi();

    CssManager *m_cssManager;
    Editor *m_editor;
    QListWidget *m_listWidget;
    QLabel *m_previewLabel;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QFontComboBox *m_fontCombo;
    QSpinBox *m_fontSizeSpin;
    QPushButton *m_colorBtn;
    QPushButton *m_bgColorBtn;
    QColor m_fontColor;
    QColor m_bgColor;
    QCheckBox *m_reopenCheck;
    QCheckBox *m_syncCheck;
};

#endif

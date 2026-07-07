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

signals:
    void stylesheetChanged();

private slots:
    void addStylesheet();
    void removeStylesheet();
    void onItemChanged(QListWidgetItem *item);
    void pickFontColor();
    void pickBgColor();
    void saveFontSettings();

private:
    void populateStylesheetList();
    void setupUi();

    CssManager *m_cssManager;
    Editor *m_editor;
    QListWidget *m_listWidget;
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
    bool m_updatingCheckState = false;
};

#endif

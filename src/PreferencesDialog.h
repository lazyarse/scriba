#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include "CssManager.h"

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(CssManager *cssManager, QWidget *parent = nullptr);

signals:
    void stylesheetChanged();

private slots:
    void addStylesheet();
    void removeStylesheet();
    void addPrintStylesheet();
    void removePrintStylesheet();
    void editEditorBaseCss();
    void editPreviewBaseCss();
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onPrintCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

private:
    void populateStylesheetList();
    void populatePrintStylesheetList();
    void setupUi();

    CssManager *m_cssManager;
    QListWidget *m_listWidget;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QListWidget *m_printListWidget;
    QPushButton *m_printAddBtn;
    QPushButton *m_printRemoveBtn;
    QPushButton *m_editEditorBtn;
    QPushButton *m_editPreviewBtn;
    QCheckBox *m_reopenCheck;
    QCheckBox *m_syncCheck;
    QComboBox *m_editorPositionCombo;
};

#endif

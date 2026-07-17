#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>


class CssConfig;
class CssLoader;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(CssConfig *config, CssLoader *loader, QWidget *parent = nullptr);

signals:
    void stylesheetChanged();

private slots:
    void addStylesheet();
    void removeStylesheet();
    void editEditorBaseCss();
    void editPreviewBaseCss();
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
private:
    void populateStylesheetList();
    void setupUi();

    CssConfig *m_config;
    CssLoader *m_loader;
    QListWidget *m_listWidget;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_editEditorBtn;
    QPushButton *m_editPreviewBtn;
    QCheckBox *m_reopenCheck;
    QCheckBox *m_syncCheck;
    QCheckBox *m_stripeCheck;
};

#endif

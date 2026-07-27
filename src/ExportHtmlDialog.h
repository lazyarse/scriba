#pragma once

#include <QDialog>
#include <JsRenderEngine.h>

class QComboBox;
class CssConfig;
class CssLoader;

class ExportHtmlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportHtmlDialog(CssConfig *config, CssLoader *loader,
                              const QString &defaultFilePath,
                              QWidget *parent = nullptr);

    QString selectedThemePath() const;
    ScriptHandling selectedScriptHandling() const;

private:
    void setupUi();

    CssConfig *m_config;
    CssLoader *m_loader;
    QString m_defaultFilePath;
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_scriptCombo = nullptr;
};

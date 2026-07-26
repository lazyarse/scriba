#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>

class CssHighlighter;

class CssEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CssEditorDialog(const QString &title, const QString &css, const QString &defaultCss,
                             const QString &themeCss, QWidget *parent = nullptr);
    QString css() const;

private:
    void applyFontPreset();

    QString m_defaultCss;
    QPlainTextEdit *m_editor;
    QComboBox *m_fontCombo;
    QSpinBox *m_fontSizeSpin;
};


#ifndef CSSEDITORDIALOG_H
#define CSSEDITORDIALOG_H

#include <QDialog>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>

class CssEditorDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { EditorBase, PreviewBase };

    explicit CssEditorDialog(const QString &title, Mode mode, const QString &css, const QString &defaultCss, QWidget *parent = nullptr);
    QString css() const;

private:
    void applyFontPreset();

    Mode m_mode;
    QString m_defaultCss;
    QPlainTextEdit *m_editor;
    QComboBox *m_fontCombo;
    QSpinBox *m_fontSizeSpin;
};

#endif

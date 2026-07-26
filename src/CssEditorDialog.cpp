#include "CssEditorDialog.h"
#include "CssHighlighter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QRegularExpression>

CssEditorDialog::CssEditorDialog(const QString &title, const QString &css, const QString &defaultCss,
                                 const QString &themeCss, QWidget *parent)
    : QDialog(parent), m_defaultCss(defaultCss)
{
    setWindowTitle(title);
    resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *fontRow = new QHBoxLayout();

    fontRow->addWidget(new QLabel("Font:"));
    m_fontCombo = new QComboBox();
    m_fontCombo->setEditable(true);
    m_fontCombo->addItems({
        "'Consolas', 'Monaco', 'Courier New', monospace",
        "'Menlo', 'Monaco', 'Courier New', monospace",
        "Georgia, 'Times New Roman', serif",
        "'Segoe UI', Roboto, Helvetica, Arial, sans-serif",
        "'Linux Libertine', Georgia, Times, serif",
        "'Source Code Pro', 'Fira Code', monospace",
    });
    fontRow->addWidget(m_fontCombo);

    fontRow->addWidget(new QLabel("Size:"));
    m_fontSizeSpin = new QSpinBox();
    m_fontSizeSpin->setRange(8, 48);
    m_fontSizeSpin->setSuffix(" px");
    m_fontSizeSpin->setValue(20);
    fontRow->addWidget(m_fontSizeSpin);

    fontRow->addStretch();
    layout->addLayout(fontRow);

    m_editor = new QPlainTextEdit(this);
    m_editor->setPlainText(css);
    m_editor->setTabStopDistance(20);

    QFont font("monospace");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(12);
    m_editor->setFont(font);

    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);

    new CssHighlighter(themeCss, m_editor->document());

    layout->addWidget(m_editor, 1);

    QHBoxLayout *btnLayout = new QHBoxLayout();

    QPushButton *resetBtn = new QPushButton(tr("&Reset"), this);
    resetBtn->setIcon(QIcon());
    btnLayout->addWidget(resetBtn);

    btnLayout->addStretch();

    QPushButton *saveBtn = new QPushButton(tr("&Save"), this);
    saveBtn->setIcon(QIcon());
    QPushButton *cancelBtn = new QPushButton(tr("&Cancel"), this);
    cancelBtn->setIcon(QIcon());

    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // Parse initial CSS to select matching preset
    QRegularExpression familyRe(
        R"(body\s*\{[^}]*font-family\s*:\s*([^;\}]+))");
    auto familyMatch = familyRe.match(css);
    if (familyMatch.hasMatch()) {
        QString val = familyMatch.captured(1).trimmed();
        int idx = m_fontCombo->findText(val);
        if (idx >= 0)
            m_fontCombo->setCurrentIndex(idx);
        else
            m_fontCombo->setCurrentText(val);
    }

    QRegularExpression sizeRe(
        R"(body\s*\{[^}]*font-size\s*:\s*([^;\}]+))");
    auto sizeMatch = sizeRe.match(css);
    if (sizeMatch.hasMatch()) {
        QString val = sizeMatch.captured(1).trimmed();
        val.remove("!important");
        val = val.trimmed();
        if (val.endsWith("px"))
            m_fontSizeSpin->setValue(val.chopped(2).toInt());
    }

    connect(m_fontCombo, &QComboBox::currentTextChanged, this, &CssEditorDialog::applyFontPreset);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CssEditorDialog::applyFontPreset);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_editor->setPlainText(m_defaultCss);
        QRegularExpression familyRe(R"(body\s*\{[^}]*font-family\s*:\s*([^;\}]+))");
        auto fm = familyRe.match(m_defaultCss);
        if (fm.hasMatch()) {
            QString val = fm.captured(1).trimmed();
            int idx = m_fontCombo->findText(val);
            if (idx >= 0) m_fontCombo->setCurrentIndex(idx);
        }
        QRegularExpression sizeRe(R"(body\s*\{[^}]*font-size\s*:\s*([^;\}]+))");
        auto sm = sizeRe.match(m_defaultCss);
        if (sm.hasMatch()) {
            QString v = sm.captured(1).trimmed();
            v.remove("!important");
            v = v.trimmed();
            if (v.endsWith("px")) m_fontSizeSpin->setValue(v.chopped(2).toInt());
        }
    });
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void CssEditorDialog::applyFontPreset()
{
    QString css = m_editor->toPlainText();

    QString fontFamily = m_fontCombo->currentText().trimmed();
    int fontSize = m_fontSizeSpin->value();
    QString fontSizeStr = QString("%1px").arg(fontSize);

    QRegularExpression familyRe(
        R"(body\s*\{[^}]*font-family\s*:\s*)([^;}]+)");
    auto fm = familyRe.match(css);
    if (fm.hasMatch()) {
        bool imp = fm.captured(2).contains("!important");
        css.replace(familyRe, "\\1" + fontFamily + (imp ? " !important" : ""));
    } else {
        css.replace(QRegularExpression(R"(body\s*\{)"),
                    "\\1\n    font-family: " + fontFamily + ";");
    }

    QRegularExpression sizeRe(
        R"(body\s*\{[^}]*font-size\s*:\s*)([^;}]+)");
    auto sm = sizeRe.match(css);
    if (sm.hasMatch()) {
        bool imp = sm.captured(2).contains("!important");
        css.replace(sizeRe, "\\1" + fontSizeStr + (imp ? " !important" : ""));
    } else {
        css.replace(QRegularExpression(R"(body\s*\{)"),
                    "\\1\n    font-size: " + fontSizeStr + ";");
    }

    m_editor->setPlainText(css);
}

QString CssEditorDialog::css() const
{
    return m_editor->toPlainText();
}

#include "CssEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFont>

CssEditorDialog::CssEditorDialog(const QString &title, const QString &css, const QString &defaultCss, QWidget *parent)
    : QDialog(parent), m_defaultCss(defaultCss)
{
    setWindowTitle(title);
    resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_editor = new QPlainTextEdit(this);
    m_editor->setPlainText(css);
    m_editor->setTabStopDistance(20);

    QFont font("monospace");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(12);
    m_editor->setFont(font);

    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_editor);

    QHBoxLayout *btnLayout = new QHBoxLayout();

    QPushButton *resetBtn = new QPushButton("Reset", this);
    btnLayout->addWidget(resetBtn);

    btnLayout->addStretch();

    QPushButton *saveBtn = new QPushButton("Save", this);
    QPushButton *cancelBtn = new QPushButton("Cancel", this);

    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(resetBtn, &QPushButton::clicked, this, [this]() { m_editor->setPlainText(m_defaultCss); });
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString CssEditorDialog::css() const
{
    return m_editor->toPlainText();
}

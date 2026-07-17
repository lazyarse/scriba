#include "FindDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QIcon>

FindDialog::FindDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Find");
    setFixedSize(320, 150);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel("Find:"));
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search text...");
    inputLayout->addWidget(m_searchInput);
    layout->addLayout(inputLayout);

    QHBoxLayout *checkLayout = new QHBoxLayout();
    m_regexCheck = new QCheckBox("Regex");
    m_regexCheck->setChecked(false);
    checkLayout->addWidget(m_regexCheck);
    m_caseCheck = new QCheckBox("Case sensitive");
    m_caseCheck->setChecked(false);
    checkLayout->addWidget(m_caseCheck);
    layout->addLayout(checkLayout);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Find");
    for (auto *btn : buttons->buttons())
        btn->setIcon(QIcon());
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_searchInput->setFocus();
}

QString FindDialog::searchTerm() const
{
    return m_searchInput->text();
}

bool FindDialog::regexEnabled() const
{
    return m_regexCheck->isChecked();
}

bool FindDialog::caseSensitive() const
{
    return m_caseCheck->isChecked();
}

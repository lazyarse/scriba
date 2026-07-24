#include "FindDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>

FindDialog::FindDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Find & Replace");
    setFixedSize(480, 200);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);

    auto *findRow = new QHBoxLayout();
    findRow->addWidget(new QLabel("Find:"));
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search text...");
    findRow->addWidget(m_searchInput);
    auto *findPrevBtn = new QPushButton("< Prev");
    auto *findNextBtn = new QPushButton("Next >");
    findPrevBtn->setIcon(QIcon());
    findNextBtn->setIcon(QIcon());
    findRow->addWidget(findPrevBtn);
    findRow->addWidget(findNextBtn);
    layout->addLayout(findRow);

    auto *replaceRow = new QHBoxLayout();
    replaceRow->addWidget(new QLabel("Rplc:"));
    m_replaceInput = new QLineEdit();
    m_replaceInput->setPlaceholderText("Replace with...");
    replaceRow->addWidget(m_replaceInput);
    auto *replaceBtn = new QPushButton("Replace");
    auto *replaceAllBtn = new QPushButton("Replace All");
    replaceBtn->setIcon(QIcon());
    replaceAllBtn->setIcon(QIcon());
    replaceRow->addWidget(replaceBtn);
    replaceRow->addWidget(replaceAllBtn);
    layout->addLayout(replaceRow);

    auto *checkRow = new QHBoxLayout();
    m_regexCheck = new QCheckBox("Regex");
    m_regexCheck->setChecked(false);
    checkRow->addWidget(m_regexCheck);
    m_caseCheck = new QCheckBox("Case sensitive");
    m_caseCheck->setChecked(false);
    checkRow->addWidget(m_caseCheck);
    checkRow->addStretch();
    layout->addLayout(checkRow);

    connect(findNextBtn, &QPushButton::clicked, this, &FindDialog::emitFindNext);
    connect(findPrevBtn, &QPushButton::clicked, this, &FindDialog::emitFindPrev);
    connect(replaceBtn, &QPushButton::clicked, this, &FindDialog::emitReplace);
    connect(replaceAllBtn, &QPushButton::clicked, this, &FindDialog::emitReplaceAll);

    connect(m_searchInput, &QLineEdit::returnPressed, this, &FindDialog::emitFindNext);
    connect(m_replaceInput, &QLineEdit::returnPressed, this, &FindDialog::emitReplace);

    m_searchInput->setFocus();
}

void FindDialog::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

QString FindDialog::searchTerm() const { return m_searchInput->text(); }
QString FindDialog::replaceTerm() const { return m_replaceInput->text(); }
bool FindDialog::regexEnabled() const { return m_regexCheck->isChecked(); }
bool FindDialog::caseSensitive() const { return m_caseCheck->isChecked(); }

void FindDialog::focusSearchInput() { m_searchInput->setFocus(); }
void FindDialog::focusReplaceInput() { m_replaceInput->setFocus(); }

void FindDialog::emitFindNext()
{
    if (!m_searchInput->text().isEmpty())
        emit findNextRequested(m_searchInput->text(), m_regexCheck->isChecked(), m_caseCheck->isChecked());
}

void FindDialog::emitFindPrev()
{
    if (!m_searchInput->text().isEmpty())
        emit findPrevRequested(m_searchInput->text(), m_regexCheck->isChecked(), m_caseCheck->isChecked());
}

void FindDialog::emitReplace()
{
    if (!m_searchInput->text().isEmpty())
        emit replaceRequested(m_searchInput->text(), m_replaceInput->text(), m_regexCheck->isChecked(), m_caseCheck->isChecked());
}

void FindDialog::emitReplaceAll()
{
    if (!m_searchInput->text().isEmpty())
        emit replaceAllRequested(m_searchInput->text(), m_replaceInput->text(), m_regexCheck->isChecked(), m_caseCheck->isChecked());
}

#include "FindDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>

FindDialog::FindDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Find & Replace");
    setFixedSize(640, 180);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);
    auto *grid = new QGridLayout();
    auto *findLabel = new QLabel("Find:");
    auto *replaceLabel = new QLabel("Replace:");
    grid->addWidget(findLabel, 0, 0);
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search text...");
    grid->addWidget(m_searchInput, 0, 1);
    auto *findPrevBtn = new QPushButton("< &Prev");
    auto *findNextBtn = new QPushButton("&Next >");
    stripButtonIcon(findPrevBtn);
    stripButtonIcon(findNextBtn);
    grid->addWidget(findPrevBtn, 0, 2);
    grid->addWidget(findNextBtn, 0, 3);
    grid->addWidget(replaceLabel, 1, 0);
    m_replaceInput = new QLineEdit();
    m_replaceInput->setPlaceholderText("Replace with... (\\1, \\2 for regex groups)");
    grid->addWidget(m_replaceInput, 1, 1);
    auto *replaceBtn = new QPushButton("&Replace");
    auto *replaceAllBtn = new QPushButton("Replace &All");
    stripButtonIcon(replaceBtn);
    stripButtonIcon(replaceAllBtn);
    grid->addWidget(replaceBtn, 1, 2);
    grid->addWidget(replaceAllBtn, 1, 3);
    layout->addLayout(grid);

    auto *checkRow = new QHBoxLayout();
    m_regexCheck = new QCheckBox("Regex");
    m_regexCheck->setChecked(false);
    checkRow->addWidget(m_regexCheck);
    m_caseCheck = new QCheckBox("Case sensitive");
    m_caseCheck->setChecked(false);
    checkRow->addWidget(m_caseCheck);
    checkRow->addStretch();
    m_matchCountLabel = new QLabel("No matches");
    m_matchCountLabel->setStyleSheet("color: gray;");
    checkRow->addWidget(m_matchCountLabel);
    layout->addLayout(checkRow);

    connect(findNextBtn, &QPushButton::clicked, this, &FindDialog::emitFindNext);
    connect(findPrevBtn, &QPushButton::clicked, this, &FindDialog::emitFindPrev);
    connect(replaceBtn, &QPushButton::clicked, this, &FindDialog::emitReplace);
    connect(replaceAllBtn, &QPushButton::clicked, this, &FindDialog::emitReplaceAll);

    connect(m_searchInput, &QLineEdit::returnPressed, this, &FindDialog::emitFindNext);
    connect(m_replaceInput, &QLineEdit::returnPressed, this, &FindDialog::emitReplace);

    auto emitSearchChanged = [this]() {
        emit searchTextChanged(m_searchInput->text(), m_regexCheck->isChecked(), m_caseCheck->isChecked());
    };
    connect(m_searchInput, &QLineEdit::textChanged, this, emitSearchChanged);
    connect(m_regexCheck, &QCheckBox::checkStateChanged, this, emitSearchChanged);
    connect(m_caseCheck, &QCheckBox::checkStateChanged, this, emitSearchChanged);

    auto *hintRow = new QHBoxLayout();
    auto *hintLabel = new QLabel("Tip: Use \\1, \\2 for captured groups in regex replace");
    hintLabel->setStyleSheet("color: gray; font-size: 11px;");
    hintRow->addWidget(hintLabel);
    hintRow->addStretch();
    layout->addLayout(hintRow);

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

void FindDialog::setMatchCount(int count)
{
    if (count == 0)
        m_matchCountLabel->setText("No matches");
    else if (count == 1)
        m_matchCountLabel->setText("1 match");
    else
        m_matchCountLabel->setText(QString("%1 matches").arg(count));
}

QString FindDialog::matchCountText() const
{
    return m_matchCountLabel->text();
}

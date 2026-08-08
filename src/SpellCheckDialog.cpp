// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "SpellCheckDialog.h"
#include "Editor.h"
#include "SpellChecker.h"
#include "SpellHighlighter.h"
#include "StaticHelpers.h"
#include <QCloseEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

SpellCheckDialog::SpellCheckDialog(Editor *editor, QWidget *parent)
    : QDialog(parent)
    , m_editor(editor)
{
    setWindowTitle(tr("Check Spelling"));
    // Modeless: the user keeps the editor editable while the panel is open.
    // The error list stays in sync with the document via the spell
    // highlighter's incremental rescans (see onSpellHitsChanged()).
    setModal(false);

    auto *layout = new QVBoxLayout(this);

    auto *notInRow = new QHBoxLayout();
    auto *notInLabel = new QLabel(tr("Not in dictionary:"));
    notInRow->addWidget(notInLabel);
    notInRow->addStretch();
    layout->addLayout(notInRow);

    m_changeTo = new QLineEdit();
    m_changeTo->setPlaceholderText(tr("Correction..."));
    layout->addWidget(m_changeTo);

    m_suggestions = new QListWidget();
    m_suggestions->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_suggestions, 1);

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_statusLabel);

    auto *buttonRow = new QHBoxLayout();
    m_changeBtn = new QPushButton(tr("&Change"));
    m_ignoreOnceBtn = new QPushButton(tr("&Ignore Once"));
    m_ignoreAlwaysBtn = new QPushButton(tr("Ignore &Always"));
    m_addDictBtn = new QPushButton(tr("&Add to Dictionary"));
    m_prevBtn = new QPushButton(tr("&Previous"));
    m_nextBtn = new QPushButton(tr("&Next"));
    auto *closeBtn = new QPushButton(tr("&Close"));
    for (QPushButton *btn : {m_changeBtn, m_ignoreOnceBtn, m_ignoreAlwaysBtn,
                             m_addDictBtn, m_prevBtn, m_nextBtn, closeBtn}) {
        stripButtonIcon(btn);
        btn->setAutoDefault(false);
    }
    buttonRow->addWidget(m_changeBtn);
    buttonRow->addWidget(m_ignoreOnceBtn);
    buttonRow->addWidget(m_ignoreAlwaysBtn);
    buttonRow->addWidget(m_addDictBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(m_prevBtn);
    buttonRow->addWidget(m_nextBtn);
    buttonRow->addWidget(closeBtn);
    layout->addLayout(buttonRow);

    connect(m_changeBtn, &QPushButton::clicked, this, &SpellCheckDialog::changeCurrent);
    connect(m_ignoreOnceBtn, &QPushButton::clicked, this, &SpellCheckDialog::ignoreOnceCurrent);
    connect(m_ignoreAlwaysBtn, &QPushButton::clicked, this, &SpellCheckDialog::ignoreAlwaysCurrent);
    connect(m_addDictBtn, &QPushButton::clicked, this, &SpellCheckDialog::addToDictionaryCurrent);
    connect(m_prevBtn, &QPushButton::clicked, this, &SpellCheckDialog::prevError);
    connect(m_nextBtn, &QPushButton::clicked, this, &SpellCheckDialog::nextError);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    // A clicked suggestion fills the correction box; double-click applies it.
    connect(m_suggestions, &QListWidget::itemSelectionChanged, this, [this]() {
        QListWidgetItem *item = m_suggestions->currentItem();
        if (item)
            m_changeTo->setText(item->text());
    });
    connect(m_suggestions, &QListWidget::itemDoubleClicked, this, [this]() {
        changeCurrent();
    });

    retarget(editor);
}

SpellCheckDialog::~SpellCheckDialog()
{
    if (m_editor)
        m_editor->clearSpellCheckHighlight();
}

void SpellCheckDialog::retarget(Editor *editor)
{
    if (m_editor) {
        m_editor->clearSpellCheckHighlight();
        if (SpellHighlighter *hl = m_editor->spellHighlighter())
            disconnect(hl, &SpellHighlighter::spellHitsChanged,
                       this, &SpellCheckDialog::onSpellHitsChanged);
        disconnect(m_editor->document(), &QTextDocument::contentsChange,
                   this, &SpellCheckDialog::onDocumentEdited);
    }

    m_editor = editor;
    m_done = false;
    m_handledAny = false;
    m_index = 0;

    if (m_editor) {
        if (SpellHighlighter *hl = m_editor->spellHighlighter())
            connect(hl, &SpellHighlighter::spellHitsChanged,
                    this, &SpellCheckDialog::onSpellHitsChanged);
        connect(m_editor->document(), &QTextDocument::contentsChange,
                this, &SpellCheckDialog::onDocumentEdited);
    }

    rebuildIssuesFromCache();
    showCurrent();
}

void SpellCheckDialog::closeEvent(QCloseEvent *event)
{
    if (m_editor)
        m_editor->clearSpellCheckHighlight();
    event->accept();
    QDialog::closeEvent(event);
}

// The dialog's issue list is derived from the highlighter's per-block cache
// (only the blocks an edit touched are re-checked), so keeping the list in
// sync while the user edits costs no full-document rescan.
void SpellCheckDialog::rebuildIssuesFromCache()
{
    m_issues.clear();
    if (!m_editor || !m_editor->spellChecker() || !m_editor->spellChecker()->isLoaded())
        return;
    SpellHighlighter *hl = m_editor->spellHighlighter();
    if (!hl)
        return;

    QTextDocument *doc = m_editor->document();
    for (QTextBlock block = doc->firstBlock(); block.isValid(); block = block.next()) {
        const int blockNumber = block.blockNumber();
        for (const SpellHighlighter::GrammarHit &hit : hl->spellHitsInBlock(blockNumber)) {
            const QString word = block.text().mid(hit.start, hit.length);
            if (word.isEmpty() || isIgnoredOnce(blockNumber, hit.start, word))
                continue;
            m_issues.append({blockNumber, hit.start, hit.length, word});
        }
    }
}

bool SpellCheckDialog::isIgnoredOnce(int block, int start, const QString &word) const
{
    for (const IgnoredOnce &ignored : m_ignoredOnce) {
        if (ignored.editor == m_editor && ignored.block == block
            && ignored.start == start && ignored.word == word)
            return true;
    }
    return false;
}

// The highlighter rescanned (an edit, a change/ignore action, a dictionary
// update...). Rebuild the list, but never move the editor's cursor or
// highlight on a background rescan — that would interrupt typing. Navigation
// and the action buttons re-point explicitly.
void SpellCheckDialog::onSpellHitsChanged()
{
    if (!m_editor)
        return;
    rebuildIssuesFromCache();

    if (SpellHighlighter *hl = m_editor->spellHighlighter())
        if (!hl->spellCheckingEnabled())
            m_editor->clearSpellCheckHighlight();

    if (m_issues.isEmpty()) {
        setDone(m_handledAny);
        return;
    }
    // The document changed under us: drop the "complete" state and re-enter
    // the list. Edit-related pointer invalidation was handled by
    // onDocumentEdited(); a rescan purely from an ignore/add action leaves a
    // stale highlight, so clear it and let navigation re-point.
    m_done = false;
    m_index = qBound(0, m_index, m_issues.size() - 1);
    m_editor->clearSpellCheckHighlight();
    refreshDisplay();
    updateButtons();
}

// A real edit (skip the (0,0) format-only emissions from highlighting).
// The background highlight may now point at shifted text — drop it; the
// rescan that follows will re-sync the list and navigation re-points.
void SpellCheckDialog::onDocumentEdited(int, int charsRemoved, int charsAdded)
{
    if (charsRemoved == 0 && charsAdded == 0)
        return;
    if (m_editor)
        m_editor->clearSpellCheckHighlight();
}

void SpellCheckDialog::showCurrent()
{
    if (m_done) {
        updateButtons();
        return;
    }
    if (m_issues.isEmpty()) {
        setDone(m_handledAny);
        return;
    }
    m_index = qBound(0, m_index, m_issues.size() - 1);
    refreshDisplay();
    pointAtCurrent();
    updateButtons();
}

void SpellCheckDialog::refreshDisplay()
{
    const SpellHighlighter::SpellIssue &issue = m_issues.at(m_index);
    m_changeTo->setText(issue.word);
    m_suggestions->clear();
    if (SpellChecker *checker = m_editor ? m_editor->spellChecker() : nullptr) {
        for (const QString &suggestion : checker->suggestions(issue.word))
            m_suggestions->addItem(suggestion);
    }
    m_statusLabel->setText(tr("Error %1 of %2")
                               .arg(m_index + 1)
                               .arg(m_issues.size()));
}

void SpellCheckDialog::pointAtCurrent()
{
    if (m_done || !m_editor || m_index < 0 || m_index >= m_issues.size())
        return;
    const SpellHighlighter::SpellIssue &issue = m_issues.at(m_index);
    m_editor->setSpellCheckHighlight(issue.blockNumber, issue.start, issue.length);
    m_editor->centerCursor();
}

void SpellCheckDialog::setDone(bool foundAny)
{
    m_done = true;
    m_index = -1;
    if (m_editor)
        m_editor->clearSpellCheckHighlight();
    m_statusLabel->setText(foundAny ? tr("Spelling check complete.")
                                    : tr("No spelling errors found."));
    m_changeTo->clear();
    m_suggestions->clear();
    updateButtons();
}

void SpellCheckDialog::updateButtons()
{
    const bool active = !m_done && m_index >= 0 && m_index < m_issues.size();
    m_changeBtn->setEnabled(active);
    m_ignoreOnceBtn->setEnabled(active);
    m_ignoreAlwaysBtn->setEnabled(active);
    m_addDictBtn->setEnabled(active);
    m_prevBtn->setEnabled(active && m_index > 0);
    m_nextBtn->setEnabled(active && m_index < m_issues.size() - 1);
    m_changeTo->setReadOnly(!active);
    m_suggestions->setEnabled(active);
}

void SpellCheckDialog::nextError()
{
    if (m_done || m_issues.isEmpty())
        return;
    if (m_index < m_issues.size() - 1)
        ++m_index;
    showCurrent();
}

void SpellCheckDialog::prevError()
{
    if (m_done || m_issues.isEmpty())
        return;
    if (m_index > 0)
        --m_index;
    showCurrent();
}

void SpellCheckDialog::changeCurrent()
{
    if (m_done || m_index < 0 || m_index >= m_issues.size())
        return;
    const SpellHighlighter::SpellIssue &issue = m_issues.at(m_index);
    const QString replacement = m_changeTo->text();
    if (replacement.isEmpty() || replacement == issue.word)
        return;

    QTextBlock block = m_editor->document()->findBlockByNumber(issue.blockNumber);
    if (!block.isValid())
        return;
    QTextCursor cursor(m_editor->document());
    cursor.beginEditBlock();
    cursor.setPosition(block.position() + issue.start);
    cursor.setPosition(block.position() + issue.start + issue.length,
                       QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
    cursor.endEditBlock();

    m_handledAny = true;
    // Force an immediate synchronous rescan (the replacement may not end in
    // a word separator, so the debounce alone would lag the list), then point
    // at the next error.
    if (SpellHighlighter *hl = m_editor->spellHighlighter())
        hl->refresh();
    showCurrent();
}

void SpellCheckDialog::ignoreOnceCurrent()
{
    if (m_done || m_index < 0 || m_index >= m_issues.size())
        return;
    // Session-local: the occurrence is skipped, the squiggle stays. Keyed on
    // (editor, block, offset, word) so it survives later rescans until that
    // line's text changes.
    const SpellHighlighter::SpellIssue &issue = m_issues.at(m_index);
    m_ignoredOnce.append({m_editor, issue.blockNumber, issue.start, issue.word});
    m_issues.removeAt(m_index);
    showCurrent();
}

void SpellCheckDialog::ignoreAlwaysCurrent()
{
    if (m_done || m_index < 0 || m_index >= m_issues.size())
        return;
    if (SpellChecker *checker = m_editor ? m_editor->spellChecker() : nullptr) {
        checker->addToIgnored(m_issues.at(m_index).word);
        if (m_editor->spellHighlighter())
            m_editor->spellHighlighter()->refresh();
    }
    m_handledAny = true;
    showCurrent();
}

void SpellCheckDialog::addToDictionaryCurrent()
{
    if (m_done || m_index < 0 || m_index >= m_issues.size())
        return;
    if (SpellChecker *checker = m_editor ? m_editor->spellChecker() : nullptr) {
        checker->addToUserDictionary(m_issues.at(m_index).word);
        if (m_editor->spellHighlighter())
            m_editor->spellHighlighter()->refresh();
    }
    m_handledAny = true;
    showCurrent();
}

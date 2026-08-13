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
#pragma once

#include "SpellHighlighter.h"
#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QVector>

class Editor;
class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
// Modeless spelling check panel (Tools → Check Spelling). Unlike a classic
// modal check-spelling walk, the panel stays open while the user edits: the
// error list follows the document through the SpellHighlighter's incremental
// per-block rescans (spellHitsChanged), so a word fixed in the editor drops
// out of the list and a new typo appears without a full rescan. The current
// error is highlighted in the editor with a background highlight (not an
// underline); editing clears the highlight and navigation re-points it.
//
// "Ignore once" is a session filter keyed on (editor, block, offset, word) —
// exactly the suppressed occurrence — so it stays hidden across rescans until
// that line's text is edited (the offset shifts). "Ignore always" and "Add to
// dictionary" persist like before.
class SpellCheckDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpellCheckDialog(Editor *editor, QWidget *parent = nullptr);
    ~SpellCheckDialog() override;

    // Points the panel at a (possibly different) editor: disconnects the old
    // editor's signals, connects the new one's, and rescans. Used by
    // MainWindow to follow the active tab. The per-editor "ignore once" set
    // is preserved so switching back keeps earlier suppressions.
    void retarget(Editor *editor);
    Editor *targetEditor() const;

    // Test-facing state: the current error list (document order).
    QVector<SpellHighlighter::SpellIssue> issues() const { return m_issues; }
    int currentIndex() const { return m_index; }
    void nextError();
    void prevError();
    // Applies the current "Change to" text to the current error.
    void changeCurrent();
    void ignoreOnceCurrent();
    void ignoreAlwaysCurrent();
    void addToDictionaryCurrent();
    // Test-facing UI accessors.
    QLineEdit *changeToEdit() const { return m_changeTo; }
    QString statusText() const { return m_statusLabel->text(); }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // A session "ignore once": the occurrence (block + offset + word) stays out
    // of the list for this dialog, for this editor, until its line's text is
    // edited (the offset then shifts and no longer matches).
    struct IgnoredOnce {
        Editor *editor = nullptr;
        int block = 0;
        int start = 0;
        QString word;
    };

    void rebuildIssuesFromCache();
    void onSpellHitsChanged();
    void onDocumentEdited(int position, int charsRemoved, int charsAdded);
    bool isIgnoredOnce(int block, int start, const QString &word) const;
    void showCurrent();
    void refreshDisplay();
    void pointAtCurrent();
    void setDone(bool foundAny);
    void updateButtons();

    // The targeted editor. QPointer so a closed/deleted editor nulls it and
    // the panel (and its destructor) never dereferences freed memory.
    QPointer<Editor> m_editor;
    QVector<SpellHighlighter::SpellIssue> m_issues;
    QVector<IgnoredOnce> m_ignoredOnce;
    int m_index = -1;
    bool m_done = false;
    bool m_handledAny = false;

    QLineEdit *m_changeTo = nullptr;
    QListWidget *m_suggestions = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_changeBtn = nullptr;
    QPushButton *m_ignoreOnceBtn = nullptr;
    QPushButton *m_ignoreAlwaysBtn = nullptr;
    QPushButton *m_addDictBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
};

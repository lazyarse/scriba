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
#include <QVector>

class Editor;
class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
// Modal full-document spelling check (Tools → Check Spelling). Scans the
// editor with the same markdown-aware logic the underlines use
// (SpellHighlighter::scanDocument), then lets the user work through each
// misspelled word: apply a stoppard suggestion (or a typed correction),
// ignore the single occurrence, ignore the word always (persisted), or add
// it to the custom dictionary. The current error is highlighted in the
// editor with a background highlight (not an underline).
class SpellCheckDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpellCheckDialog(Editor *editor, QWidget *parent = nullptr);
    ~SpellCheckDialog() override;

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
    void rebuildIssues();
    void showCurrent();
    void setDone(bool foundAny);
    void updateButtons();

    Editor *m_editor = nullptr;
    QVector<SpellHighlighter::SpellIssue> m_issues;
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

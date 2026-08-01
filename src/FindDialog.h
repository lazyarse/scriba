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

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTextDocument>

class FindDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindDialog(QWidget *parent = nullptr);

    QString searchTerm() const;
    QString replaceTerm() const;
    bool regexEnabled() const;
    bool caseSensitive() const;

    void focusSearchInput();
    void focusReplaceInput();

    void setMatchCount(int count);
    QString matchCountText() const;

signals:
    void findNextRequested(const QString &text, bool useRegex, bool caseSensitive);
    void findPrevRequested(const QString &text, bool useRegex, bool caseSensitive);
    void searchTextChanged(const QString &text, bool useRegex, bool caseSensitive);
    void replaceRequested(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);
    void replaceAllRequested(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);

private slots:
    void emitFindNext();
    void emitFindPrev();
    void emitReplace();
    void emitReplaceAll();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLineEdit *m_searchInput;
    QLineEdit *m_replaceInput;
    QCheckBox *m_regexCheck;
    QCheckBox *m_caseCheck;
    QLabel *m_matchCountLabel;
};


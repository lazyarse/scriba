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

#include "ValidationReport.h"
#include <QDialog>
#include <QVector>

class QCheckBox;
class QDialogButtonBox;

// Lets the user pick which checks the validation report should run: one
// checkbox per category plus one per markdown-consistency sub-check. The
// selection is persisted in QSettings and restored on the next open.
class ValidationReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ValidationReportDialog(QWidget *parent = nullptr);

    ValidationReport::ValidationOptions options() const;

    // Persists the current selection to QSettings, then closes the dialog.
    void accept() override;

private slots:
    void updateButtons();
    void updateMarkdownMaster(bool checked);

private:
    void buildUi();
    void load();

    QCheckBox *m_spelling;
    QCheckBox *m_grammar;
    QCheckBox *m_links;
    QCheckBox *m_markdown;
    QVector<QCheckBox*> m_markdownChecks; // parallel to MarkdownCheck enum order
    QDialogButtonBox *m_buttonBox;
};
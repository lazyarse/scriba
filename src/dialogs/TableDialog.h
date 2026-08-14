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

class QSpinBox;
class QCheckBox;
class QRadioButton;

class TableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TableDialog(QWidget *parent = nullptr);

    QString generateTable() const;
    bool hasHeader() const;
    bool isHtml() const;

private:
    QSpinBox *m_columns;
    QCheckBox *m_includeHeader;
    QRadioButton *m_markdownRadio;
    QRadioButton *m_htmlRadio;
    QWidget *m_formatWidget;
};


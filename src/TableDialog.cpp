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
#include "TableDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QIcon>

TableDialog::TableDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Insert Table");
    setFixedSize(260, 170);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QFormLayout *form = new QFormLayout();
    m_columns = new QSpinBox();
    m_columns->setRange(1, 20);
    m_columns->setValue(3);
    form->addRow("Columns:", m_columns);
    layout->addLayout(form);

    m_includeHeader = new QCheckBox("Include header row");
    m_includeHeader->setChecked(true);
    layout->addWidget(m_includeHeader);

    m_formatWidget = new QWidget();
    QHBoxLayout *fmtLayout = new QHBoxLayout(m_formatWidget);
    fmtLayout->setContentsMargins(0, 0, 0, 0);
    m_markdownRadio = new QRadioButton("Markdown");
    m_htmlRadio = new QRadioButton("HTML");
    m_markdownRadio->setChecked(true);
    fmtLayout->addWidget(m_markdownRadio);
    fmtLayout->addWidget(m_htmlRadio);
    layout->addWidget(m_formatWidget);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("&Insert");
    buttons->button(QDialogButtonBox::Cancel)->setText("&Cancel");
    stripButtonIcons(buttons);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_includeHeader, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked) {
            m_htmlRadio->setChecked(true);
            m_formatWidget->setEnabled(false);
        } else {
            m_formatWidget->setEnabled(true);
        }
    });

    m_columns->setFocus();
}

QString TableDialog::generateTable() const
{
    int cols = m_columns->value();

    if (m_markdownRadio->isChecked()) {
        QString result;
        result += "|";
        for (int c = 0; c < cols; ++c)
            result += "  |";
        result += "\n|";
        for (int c = 0; c < cols; ++c)
            result += "---|";
        result += "\n|";
        for (int c = 0; c < cols; ++c)
            result += "  |";
        result += "\n";
        return result;
    } else {
        QString tag = m_includeHeader->isChecked() ? "th" : "td";
        QString result = "<table>\n<tr>";
        for (int c = 0; c < cols; ++c)
            result += "<" + tag + "></" + tag + ">";
        result += "</tr>\n</table>\n";
        return result;
    }
}

bool TableDialog::hasHeader() const
{
    return m_includeHeader->isChecked();
}

bool TableDialog::isHtml() const
{
    return m_htmlRadio->isChecked();
}

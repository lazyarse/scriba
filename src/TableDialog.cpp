#include "TableDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QIcon>

TableDialog::TableDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Insert Table");
    setFixedSize(260, 130);

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

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("&Insert");
    buttons->button(QDialogButtonBox::Cancel)->setText("&Cancel");
    stripButtonIcons(buttons);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_columns->setFocus();
}

QString TableDialog::generateTable() const
{
    int cols = m_columns->value();

    if (m_includeHeader->isChecked()) {
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
        QString result = "<table>\n<tr>";
        for (int c = 0; c < cols; ++c)
            result += "<td></td>";
        result += "</tr>\n</table>\n";
        return result;
    }
}

bool TableDialog::hasHeader() const
{
    return m_includeHeader->isChecked();
}

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
#include "CsvColumnMapDialog.h"
#include "StaticHelpers.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QIcon>

CsvColumnMapDialog::CsvColumnMapDialog(const QStringList &chartFields, QWidget *parent)
    : QDialog(parent)
    , m_chartFields(chartFields)
{
    setWindowTitle("Import CSV Data");
    resize(480, 320);
    buildUi();
}

void CsvColumnMapDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *btnLayout = new QHBoxLayout();
    auto *pasteBtn = new QPushButton("&Paste CSV", this);
    auto *openBtn = new QPushButton("&Open File...", this);
    btnLayout->addWidget(pasteBtn);
    btnLayout->addWidget(openBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    m_headersCheck = new QCheckBox("First row is &headers", this);
    m_headersCheck->setChecked(true);
    mainLayout->addWidget(m_headersCheck);

    m_mappingContainer = new QWidget(this);
    auto *mapLayout = new QGridLayout(m_mappingContainer);
    mapLayout->setContentsMargins(0, 4, 0, 4);
    m_mappingContainer->setVisible(false);
    mainLayout->addWidget(m_mappingContainer);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Import"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Ca&ncel"));
    stripButtonIcons(buttonBox);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    mainLayout->addWidget(buttonBox);

    connect(pasteBtn, &QPushButton::clicked, this, &CsvColumnMapDialog::showPasteDialog);
    connect(openBtn, &QPushButton::clicked, this, &CsvColumnMapDialog::onOpenFile);
    connect(m_headersCheck, &QCheckBox::toggled, this, &CsvColumnMapDialog::onHeadersToggled);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CsvColumnMapDialog::showPasteDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Paste CSV Data");
    dlg.resize(500, 400);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("Paste comma-separated data:", &dlg));
    auto *edit = new QPlainTextEdit(&dlg);
    layout->addWidget(edit);
    auto *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
    box->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(box);
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString text = edit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    reloadData(text);
}

void CsvColumnMapDialog::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open CSV File", QString(), "CSV Files (*.csv *.tsv *.txt);;All Files (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import Error", "Cannot open file: " + path);
        return;
    }
    QString text = QString::fromUtf8(f.readAll());
    reloadData(text);
}

void CsvColumnMapDialog::onHeadersToggled(bool checked)
{
    m_firstRowIsHeaders = checked;
    if (!m_rawText.isEmpty())
        reloadData(m_rawText);
}

void CsvColumnMapDialog::reloadData(const QString &rawText)
{
    m_rawText = rawText;
    m_csvData = CsvReader::readFromString(rawText, m_firstRowIsHeaders);

    if (m_csvData.headers.isEmpty() && m_csvData.rows.isEmpty()) {
        QMessageBox::warning(this, "Import Error", "No data found.");
        return;
    }

    populateCombos();
    m_mappingContainer->setVisible(true);

    auto *buttonBox = findChild<QDialogButtonBox*>();
    if (buttonBox)
        buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void CsvColumnMapDialog::populateCombos()
{
    // Remove old combos
    for (auto *combo : m_fieldCombos)
        combo->deleteLater();
    m_fieldCombos.clear();

    // Clear the grid layout
    auto *grid = qobject_cast<QGridLayout*>(m_mappingContainer->layout());
    if (!grid) return;
    while (grid->count() > 0) {
        auto *item = grid->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (m_csvData.headers.isEmpty() && m_csvData.rows.isEmpty())
        return;

    QStringList csvColumns = m_csvData.headers;
    csvColumns.prepend("— unused —");

    for (int i = 0; i < m_chartFields.size(); ++i) {
        auto *label = new QLabel(m_chartFields[i] + ":", m_mappingContainer);
        auto *combo = new QComboBox(m_mappingContainer);
        combo->addItems(csvColumns);

        // Try to auto-match by name
        int matchIdx = -1;
        for (int c = 1; c < csvColumns.size(); ++c) {
            if (csvColumns[c].compare(m_chartFields[i], Qt::CaseInsensitive) == 0) {
                matchIdx = c;
                break;
            }
        }
        if (matchIdx > 0)
            combo->setCurrentIndex(matchIdx);
        else if (i < csvColumns.size() - 1)
            combo->setCurrentIndex(i + 1);

        grid->addWidget(label, i, 0);
        grid->addWidget(combo, i, 1);
        m_fieldCombos.append(combo);

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CsvColumnMapDialog::onUpdate);
    }
}

void CsvColumnMapDialog::onUpdate()
{
}

QHash<QString, int> CsvColumnMapDialog::mapping() const
{
    QHash<QString, int> result;
    for (int i = 0; i < m_chartFields.size() && i < m_fieldCombos.size(); ++i) {
        int idx = m_fieldCombos[i]->currentIndex() - 1; // -1 for "— unused —"
        if (idx >= 0)
            result[m_chartFields[i]] = idx;
    }
    return result;
}



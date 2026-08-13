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
#include "ExportCorpusDialog.h"

#include "Preferences.h"
#include "StaticHelpers.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

#include <miniz.h>

#include <cstring>

bool createZipArchive(const QString &dirToZip, const QString &zipPath)
{
    // Snapshot the recursive file list BEFORE the archive file exists so a
    // zipPath inside dirToZip does not include the archive itself.
    QStringList files;
    QDirIterator it(dirToZip, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files.append(QDir(dirToZip).relativeFilePath(it.filePath()));
    }
    if (files.isEmpty())
        return false;

    const QByteArray zipPathBuf = zipPath.toUtf8();
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, zipPathBuf.constData(), 0))
        return false;

    bool ok = true;
    for (const QString &rel : files) {
        const QByteArray arcName = rel.toUtf8();
        const QByteArray fullPath = QDir(dirToZip).filePath(rel).toUtf8();
        if (!mz_zip_writer_add_file(&zip, arcName.constData(),
                                    fullPath.constData(), nullptr, 0,
                                    MZ_BEST_COMPRESSION))
            ok = false;
    }
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    mz_zip_writer_end(&zip);
    return ok;
}

ExportCorpusDialog::ExportCorpusDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export Corpus"));
    resize(460, 300);
    setupUi();
}

ExportCorpusDialog::Format ExportCorpusDialog::format() const
{
    if (m_docxRadio && m_docxRadio->isChecked())
        return Format::Docx;
    if (m_pdfRadio && m_pdfRadio->isChecked())
        return Format::Pdf;
    return Format::Html;
}

QString ExportCorpusDialog::outputDir() const
{
    return m_outputDir;
}

bool ExportCorpusDialog::compressToZip() const
{
    return m_zipCheck && m_zipCheck->isChecked();
}

bool ExportCorpusDialog::exportExternal() const
{
    return m_externalCheck && m_externalCheck->isChecked();
}

QString ExportCorpusDialog::externalDirName() const
{
    return m_externalNameEdit ? m_externalNameEdit->text().trimmed() : QString();
}

void ExportCorpusDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *formatLabel = new QLabel(tr("Format:"), this);
    layout->addWidget(formatLabel);

    m_htmlRadio = new QRadioButton(tr("&HTML"), this);
    m_htmlRadio->setChecked(true);
    m_docxRadio = new QRadioButton(tr("&Word (DOCX)"), this);
    m_pdfRadio = new QRadioButton(tr("&PDF"), this);
    layout->addWidget(m_htmlRadio);
    layout->addWidget(m_docxRadio);
    layout->addWidget(m_pdfRadio);

    layout->addSpacing(8);

    auto *dirRow = new QHBoxLayout();
    auto *dirLabel = new QLabel(tr("Output &directory:"), this);
    dirRow->addWidget(dirLabel);
    m_dirEdit = new QLineEdit(this);
    m_dirEdit->setReadOnly(true);
    dirLabel->setBuddy(m_dirEdit);
    dirRow->addWidget(m_dirEdit, 1);
    auto *browseBtn = new QPushButton(tr("&Browse…"), this);
    stripButtonIcon(browseBtn);
    connect(browseBtn, &QPushButton::clicked, this, &ExportCorpusDialog::chooseDirectory);
    dirRow->addWidget(browseBtn);
    layout->addLayout(dirRow);

    m_zipCheck = new QCheckBox(tr("&Compress into a .zip archive"), this);
    layout->addWidget(m_zipCheck);

    layout->addSpacing(8);

    m_externalCheck = new QCheckBox(tr("Export documents &outside the corpus root"), this);
    layout->addWidget(m_externalCheck);

    auto *extRow = new QHBoxLayout();
    auto *extLabel = new QLabel(tr("Subfolder &name:"), this);
    extRow->addWidget(extLabel);
    m_externalNameEdit = new QLineEdit(this);
    extLabel->setBuddy(m_externalNameEdit);
    extRow->addWidget(m_externalNameEdit, 1);
    auto *extBrowseBtn = new QPushButton(tr("&Browse…"), this);
    stripButtonIcon(extBrowseBtn);
    connect(extBrowseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Choose Export Subfolder"), m_externalNameEdit->text());
        if (!dir.isEmpty())
            m_externalNameEdit->setText(dir);
    });
    extRow->addWidget(extBrowseBtn);
    layout->addLayout(extRow);

    layout->addStretch();

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("&Export"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, this, &ExportCorpusDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);

    const QString extName = QSettings()
        .value(Preferences::CorpusExternalExportDirName, QStringLiteral("external"))
        .toString();
    m_externalNameEdit->setText(extName);
    m_externalCheck->setChecked(!extName.isEmpty());
    m_externalNameEdit->setEnabled(m_externalCheck->isChecked());
    extBrowseBtn->setEnabled(m_externalCheck->isChecked());
    connect(m_externalCheck, &QCheckBox::toggled, this,
            [this, extBrowseBtn](bool checked) {
                m_externalNameEdit->setEnabled(checked);
                extBrowseBtn->setEnabled(checked);
            });
}

void ExportCorpusDialog::chooseDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Export Corpus to Directory"), QString());
    if (dir.isEmpty())
        return;
    m_dirEdit->setText(dir);
}

void ExportCorpusDialog::accept()
{
    if (m_dirEdit->text().trimmed().isEmpty()) {
        m_dirEdit->setFocus();
        return;
    }
    m_outputDir = m_dirEdit->text().trimmed();
    QDialog::accept();
}

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

class QCheckBox;
class QLineEdit;
class QRadioButton;

// Recursively zips dirToZip into zipPath (miniz). Relative paths use '/' as
// archive separators. The file list is snapshotted before the archive is
// created, so zipPath may live inside dirToZip without including itself.
// Returns false on any failure.
bool createZipArchive(const QString &dirToZip, const QString &zipPath);

class ExportCorpusDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Format { Html, Docx, Pdf };
    explicit ExportCorpusDialog(QWidget *parent = nullptr);

    Format format() const;
    QString outputDir() const;            // "" if the user backed out of the dir picker
    bool compressToZip() const;
    bool exportExternal() const;
    QString externalDirName() const;

private:
    void setupUi();
    void chooseDirectory();
    void accept() override;

    QRadioButton *m_htmlRadio = nullptr;
    QRadioButton *m_docxRadio = nullptr;
    QRadioButton *m_pdfRadio = nullptr;
    QLineEdit *m_dirEdit = nullptr;
    QCheckBox *m_zipCheck = nullptr;
    QCheckBox *m_externalCheck = nullptr;
    QLineEdit *m_externalNameEdit = nullptr;
    QString m_outputDir;
};

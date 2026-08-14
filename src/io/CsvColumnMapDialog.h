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
#include "CsvReader.h"

class QComboBox;
class QCheckBox;
class QPlainTextEdit;

class CsvColumnMapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CsvColumnMapDialog(const QStringList &chartFields, QWidget *parent = nullptr);

    QHash<QString, int> mapping() const;
    CsvData csvData() const { return m_csvData; }

private slots:
    void showPasteDialog();
    void onOpenFile();
    void onHeadersToggled(bool checked);
    void onUpdate();

private:
    void buildUi();
    void reloadData(const QString &rawText);
    void populateCombos();

    QStringList m_chartFields;
    CsvData m_csvData;
    QString m_rawText;
    bool m_firstRowIsHeaders = true;

    QCheckBox *m_headersCheck;
    QWidget *m_mappingContainer;
    QList<QComboBox*> m_fieldCombos;
};

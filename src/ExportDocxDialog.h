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
#include <QCheckBox>
#include <QDoubleSpinBox>
#include "DocxExporter.h"

class QRadioButton;

class ExportDocxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDocxDialog(QWidget *parent = nullptr);

    DocxMathMode selectedMathMode() const;
    bool isLandscape() const;
    double marginTop() const;
    double marginBottom() const;
    double marginLeft() const;
    double marginRight() const;
    bool hasPageNumbers() const;

protected:
    void accept() override;

private:
    void setupUi();

    QRadioButton *m_imagesRadio = nullptr;
    QRadioButton *m_ommlRadio = nullptr;
    QCheckBox *m_landscapeCheck = nullptr;
    QDoubleSpinBox *m_marginTopSpin = nullptr;
    QDoubleSpinBox *m_marginBottomSpin = nullptr;
    QDoubleSpinBox *m_marginLeftSpin = nullptr;
    QDoubleSpinBox *m_marginRightSpin = nullptr;
    QCheckBox *m_pageNumbersCheck = nullptr;
};

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
#include "ExportDocxDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QSettings>
#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>

static const QString kMathModeKey = QStringLiteral("DocxExport/MathMode");

ExportDocxDialog::ExportDocxDialog(const QString &themeCss, QWidget *parent)
    : QDialog(parent)
    , m_themeCss(themeCss)
{
    setWindowTitle("Export as Word (DOCX)");
    resize(400, 480);
    setupUi();
}

DocxMathMode ExportDocxDialog::selectedMathMode() const
{
    if (m_ommlRadio && m_ommlRadio->isChecked())
        return DocxMathMode::Omml;
    return DocxMathMode::Images;
}

bool ExportDocxDialog::isLandscape() const { return m_landscapeCheck->isChecked(); }
double ExportDocxDialog::marginTop() const { return m_marginTopSpin->value(); }
double ExportDocxDialog::marginBottom() const { return m_marginBottomSpin->value(); }
double ExportDocxDialog::marginLeft() const { return m_marginLeftSpin->value(); }
double ExportDocxDialog::marginRight() const { return m_marginRightSpin->value(); }
bool ExportDocxDialog::hasPageNumbers() const { return m_pageNumbersCheck->isChecked(); }

QString ExportDocxDialog::templatePath() const
{
    if (!m_useTemplateCheck || !m_useTemplateCheck->isChecked())
        return QString();
    return m_templatePathEdit ? m_templatePathEdit->text() : QString();
}

void ExportDocxDialog::updateTemplateState()
{
    const bool useTpl = m_useTemplateCheck && m_useTemplateCheck->isChecked();
    if (m_templatePathEdit)
        m_templatePathEdit->setEnabled(useTpl);
    if (m_templateBrowseButton)
        m_templateBrowseButton->setEnabled(useTpl);
    // The template owns page setup (its sectPr incl. headers/footers).
    if (m_landscapeCheck)      m_landscapeCheck->setEnabled(!useTpl);
    if (m_marginTopSpin)       m_marginTopSpin->setEnabled(!useTpl);
    if (m_marginBottomSpin)    m_marginBottomSpin->setEnabled(!useTpl);
    if (m_marginLeftSpin)      m_marginLeftSpin->setEnabled(!useTpl);
    if (m_marginRightSpin)     m_marginRightSpin->setEnabled(!useTpl);
    if (m_pageNumbersCheck)    m_pageNumbersCheck->setEnabled(!useTpl);
}

void ExportDocxDialog::browseTemplate()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Word Template"), QString(), tr("Word Documents (*.docx)"));
    if (path.isEmpty())
        return;
    m_templatePathEdit->setText(path);
}

void ExportDocxDialog::saveThemeTemplate()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Current Theme as Word Template"), QString(),
        tr("Word Documents (*.docx)"));
    if (path.isEmpty())
        return;
    if (!DocxExporter::saveAsTemplate(path, m_themeCss)) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write the template. Check that the path is writable."));
        return;
    }
    m_templatePathEdit->setText(path);
    m_useTemplateCheck->setChecked(true);
}

static QDoubleSpinBox *createMarginSpin(QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(0.5, 10.0);
    spin->setValue(2.54);
    spin->setSingleStep(0.5);
    spin->setDecimals(1);
    spin->setSuffix(" cm");
    return spin;
}

void ExportDocxDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    QSettings s;

    auto *mathGroup = new QGroupBox(tr("Math equations"), this);
    auto *mathVbox = new QVBoxLayout(mathGroup);

    auto *mathDesc = new QLabel(
        tr("Choose how to render LaTeX math equations in the exported document:"),
        mathGroup);
    mathDesc->setWordWrap(true);
    mathVbox->addWidget(mathDesc);

    m_imagesRadio = new QRadioButton(
        tr("Static images (pixel-perfect, non-editable)"), mathGroup);
    m_ommlRadio = new QRadioButton(
        tr("Native Word math (editable in equation editor)"), mathGroup);
    mathVbox->addWidget(m_imagesRadio);
    mathVbox->addWidget(m_ommlRadio);

    layout->addWidget(mathGroup);

    // Style template group box
    auto *tplGroup = new QGroupBox(tr("Style template"), this);
    auto *tplVbox = new QVBoxLayout(tplGroup);

    auto *tplDesc = new QLabel(
        tr("Optionally style the document from a Word (.docx) template: its "
           "styles, theme, headers, footers and page setup are used as-is. "
           "Use \u201cSave current theme as template\u201d to start from the "
           "current theme and customize it in Word."), tplGroup);
    tplDesc->setWordWrap(true);
    tplVbox->addWidget(tplDesc);

    m_useTemplateCheck = new QCheckBox(tr("Use &Word template"), tplGroup);
    m_useTemplateCheck->setObjectName("useTemplateCheck");
    m_useTemplateCheck->setChecked(s.value("DocxExport/UseTemplate", false).toBool());
    tplVbox->addWidget(m_useTemplateCheck);

    auto *tplRow = new QHBoxLayout();
    m_templatePathEdit = new QLineEdit(s.value("DocxExport/Template").toString(), tplGroup);
    m_templatePathEdit->setObjectName("templatePathEdit");
    m_templatePathEdit->setReadOnly(true);
    m_templatePathEdit->setPlaceholderText(tr("No template selected"));
    tplRow->addWidget(m_templatePathEdit, 1);
    m_templateBrowseButton = new QPushButton(tr("&Browse..."), tplGroup);
    m_templateBrowseButton->setObjectName("templateBrowseButton");
    m_templateBrowseButton->setIcon(QIcon());
    tplRow->addWidget(m_templateBrowseButton);
    tplVbox->addLayout(tplRow);

    m_saveTemplateButton = new QPushButton(
        tr("Save Current Theme as &Template..."), tplGroup);
    m_saveTemplateButton->setObjectName("saveTemplateButton");
    m_saveTemplateButton->setIcon(QIcon());
    m_saveTemplateButton->setToolTip(tr(
        "Write a template with the current theme's heading and admonition "
        "colors, for customization in Word."));
    tplVbox->addWidget(m_saveTemplateButton);

    layout->addWidget(tplGroup);

    connect(m_useTemplateCheck, &QCheckBox::toggled,
            this, &ExportDocxDialog::updateTemplateState);
    connect(m_templateBrowseButton, &QPushButton::clicked,
            this, &ExportDocxDialog::browseTemplate);
    connect(m_saveTemplateButton, &QPushButton::clicked,
            this, &ExportDocxDialog::saveThemeTemplate);

    // Restore last used mode
    int lastMode = s.value(kMathModeKey, 0).toInt();
    if (lastMode == 1)
        m_ommlRadio->setChecked(true);
    else
        m_imagesRadio->setChecked(true);

    // Page Layout group box
    auto *pageGroup = new QGroupBox(tr("Page Layout"), this);
    auto *pageVbox = new QVBoxLayout(pageGroup);

    m_landscapeCheck = new QCheckBox(tr("&Landscape orientation"), pageGroup);
    m_landscapeCheck->setToolTip(tr("Swap page width and height"));
    m_landscapeCheck->setObjectName("landscapeCheck");
    m_landscapeCheck->setChecked(s.value("DocxExport/Landscape", false).toBool());
    pageVbox->addWidget(m_landscapeCheck);

    auto *marginGrid = new QGridLayout();
    auto *marginLabel = new QLabel(tr("Margins (cm):"), pageGroup);
    marginGrid->addWidget(marginLabel, 0, 0, 1, 4);

    m_marginTopSpin = createMarginSpin(pageGroup);
    m_marginTopSpin->setObjectName("marginTopSpin");
    m_marginTopSpin->setValue(s.value("DocxExport/MarginTop", 2.54).toDouble());
    marginGrid->addWidget(new QLabel(tr("Top:"), pageGroup), 1, 0);
    marginGrid->addWidget(m_marginTopSpin, 1, 1);

    m_marginBottomSpin = createMarginSpin(pageGroup);
    m_marginBottomSpin->setObjectName("marginBottomSpin");
    m_marginBottomSpin->setValue(s.value("DocxExport/MarginBottom", 2.54).toDouble());
    marginGrid->addWidget(new QLabel(tr("Bottom:"), pageGroup), 1, 2);
    marginGrid->addWidget(m_marginBottomSpin, 1, 3);

    m_marginLeftSpin = createMarginSpin(pageGroup);
    m_marginLeftSpin->setObjectName("marginLeftSpin");
    m_marginLeftSpin->setValue(s.value("DocxExport/MarginLeft", 2.54).toDouble());
    marginGrid->addWidget(new QLabel(tr("Left:"), pageGroup), 2, 0);
    marginGrid->addWidget(m_marginLeftSpin, 2, 1);

    m_marginRightSpin = createMarginSpin(pageGroup);
    m_marginRightSpin->setObjectName("marginRightSpin");
    m_marginRightSpin->setValue(s.value("DocxExport/MarginRight", 2.54).toDouble());
    marginGrid->addWidget(new QLabel(tr("Right:"), pageGroup), 2, 2);
    marginGrid->addWidget(m_marginRightSpin, 2, 3);

    pageVbox->addLayout(marginGrid);

    m_pageNumbersCheck = new QCheckBox(tr("Page &numbers in footer"), pageGroup);
    m_pageNumbersCheck->setToolTip(tr("Add page numbers to the document footer"));
    m_pageNumbersCheck->setObjectName("pageNumbersCheck");
    m_pageNumbersCheck->setChecked(s.value("DocxExport/PageNumbers", false).toBool());
    pageVbox->addWidget(m_pageNumbersCheck);

    layout->addWidget(pageGroup);

    layout->addStretch();

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("&Export"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);
}

void ExportDocxDialog::accept()
{
    QSettings s;
    s.setValue(kMathModeKey, m_ommlRadio->isChecked() ? 1 : 0);
    s.setValue("DocxExport/Landscape", m_landscapeCheck->isChecked());
    s.setValue("DocxExport/MarginTop", m_marginTopSpin->value());
    s.setValue("DocxExport/MarginBottom", m_marginBottomSpin->value());
    s.setValue("DocxExport/MarginLeft", m_marginLeftSpin->value());
    s.setValue("DocxExport/MarginRight", m_marginRightSpin->value());
    s.setValue("DocxExport/PageNumbers", m_pageNumbersCheck->isChecked());
    s.setValue("DocxExport/UseTemplate", m_useTemplateCheck->isChecked());
    s.setValue("DocxExport/Template", templatePath());
    QDialog::accept();
}

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

static const QString kMathModeKey = QStringLiteral("DocxExport/MathMode");

ExportDocxDialog::ExportDocxDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Export as Word (DOCX)");
    resize(400, 350);
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

    auto *mathLabel = new QLabel("Math equations:", this);
    layout->addWidget(mathLabel);

    auto *mathDesc = new QLabel(
        "Choose how to render LaTeX math equations in the exported document:", this);
    mathDesc->setWordWrap(true);
    layout->addWidget(mathDesc);

    m_imagesRadio = new QRadioButton("Static images (pixel-perfect, non-editable)", this);
    m_ommlRadio = new QRadioButton("Native Word math (editable in equation editor)", this);
    layout->addWidget(m_imagesRadio);
    layout->addWidget(m_ommlRadio);

    // Restore last used mode
    QSettings s;
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
    QDialog::accept();
}

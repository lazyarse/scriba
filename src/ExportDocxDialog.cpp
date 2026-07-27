#include "ExportDocxDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QSettings>
#include <QPushButton>

static const QString kMathModeKey = QStringLiteral("DocxExport/MathMode");

ExportDocxDialog::ExportDocxDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Export as Word (DOCX)");
    resize(400, 200);
    setupUi();
}

DocxMathMode ExportDocxDialog::selectedMathMode() const
{
    if (m_ommlRadio && m_ommlRadio->isChecked())
        return DocxMathMode::Omml;
    return DocxMathMode::Images;
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
    // Save the selected mode
    QSettings s;
    s.setValue(kMathModeKey, m_ommlRadio->isChecked() ? 1 : 0);
    QDialog::accept();
}

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

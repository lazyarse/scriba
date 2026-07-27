#pragma once

#include <QDialog>

class QRadioButton;

enum class DocxMathMode {
    Images,  // Convert KaTeX to PNG images (pixel-perfect, non-editable)
    Omml     // Convert KaTeX to Office Math Markup Language (editable in Word)
};

class ExportDocxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDocxDialog(QWidget *parent = nullptr);

    DocxMathMode selectedMathMode() const;

protected:
    void accept() override;

private:
    void setupUi();

    QRadioButton *m_imagesRadio = nullptr;
    QRadioButton *m_ommlRadio = nullptr;
};

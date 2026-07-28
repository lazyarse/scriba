#pragma once

#include <QString>

enum class DocxMathMode {
    Images,
    Omml
};

struct DocxExportOptions {
    DocxMathMode mathMode = DocxMathMode::Images;
    bool landscape = false;
    double marginTopCm = 2.54;
    double marginBottomCm = 2.54;
    double marginLeftCm = 2.54;
    double marginRightCm = 2.54;
    bool pageNumbers = false;
};

class DocxExporter
{
public:
    static bool exportToDocx(const QString &html, const QString &outputPath,
                             const QString &css = QString(),
                             const DocxExportOptions &options = {});
};

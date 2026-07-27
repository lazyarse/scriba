#pragma once

#include <QString>
#include <ExportDocxDialog.h>

class DocxExporter
{
public:
    static bool exportToDocx(const QString &html, const QString &outputPath,
                             const QString &css = QString(),
                             DocxMathMode mathMode = DocxMathMode::Images);
};

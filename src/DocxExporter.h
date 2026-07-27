#pragma once

#include <QString>

class DocxExporter
{
public:
    static bool exportToDocx(const QString &html, const QString &outputPath,
                             const QString &css = QString());
};

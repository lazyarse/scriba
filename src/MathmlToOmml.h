#pragma once

#include <QString>

class QXmlStreamWriter;

class MathmlToOmml
{
public:
    static bool convert(const QString &mathmlXml, QXmlStreamWriter &w);
};

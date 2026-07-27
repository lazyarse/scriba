#pragma once

#include <QString>

class HtmlToOoxml
{
public:
    static QString convert(const QString &html, const QString &themeCss = {});
    static QString buildStylesXml(const QString &themeCss);
    static QString buildNumberingXml();
};

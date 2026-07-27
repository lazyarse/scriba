#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct OoxmlImage {
    QString relId;
    QString fileName;
    QByteArray pngData;
    int cxEmu = 0;  // width in EMUs
    int cyEmu = 0;  // height in EMUs
};

struct OoxmlResult {
    QString bodyXml;
    QVector<OoxmlImage> images;
};

class HtmlToOoxml
{
public:
    static OoxmlResult convert(const QString &html, const QString &themeCss = {});
    static QString buildStylesXml(const QString &themeCss);
    static QString buildNumberingXml();
};

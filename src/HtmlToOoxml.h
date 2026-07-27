#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

enum class OoxmlRelType {
    Styles,
    Numbering,
    Image,
    Hyperlink
};

inline QString ooxmlRelTypeUri(OoxmlRelType t)
{
    switch (t) {
    case OoxmlRelType::Styles:
        return QStringLiteral(
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles");
    case OoxmlRelType::Numbering:
        return QStringLiteral(
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering");
    case OoxmlRelType::Image:
        return QStringLiteral(
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image");
    case OoxmlRelType::Hyperlink:
        return QStringLiteral(
            "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink");
    }
    return {};
}

struct OoxmlImage {
    QString relId;
    QString fileName;
    QByteArray pngData;
    int cxEmu = 0;  // width in EMUs
    int cyEmu = 0;  // height in EMUs
};

struct OoxmlHyperlink {
    QString relId;
    QString target;
};

struct OoxmlResult {
    QString bodyXml;
    QVector<OoxmlImage> images;
    QVector<OoxmlHyperlink> hyperlinks;
};

class HtmlToOoxml
{
public:
    static OoxmlResult convert(const QString &html, const QString &themeCss = {});
    static QString buildStylesXml(const QString &themeCss);
    static QString buildNumberingXml();
};

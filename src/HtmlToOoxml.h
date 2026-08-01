// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
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

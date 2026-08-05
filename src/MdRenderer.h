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

#include <QString>
#include <md4c.h>

#include "Typography.h"

class MdRenderer
{
public:
    MdRenderer();

    QString render(const char *input, MD_SIZE size, unsigned parserFlags);

    // Enables smart-typography conversion of normal text runs. Options default
    // to none, so rendering is byte-for-byte unchanged until set.
    void setTypography(Typography::Options opts) { m_typography = opts; }

private:
    struct ImageState {
        bool inside = false;
        QString alt;
        QString src;
        QString title;
    };

    static int enterBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int leaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int enterSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int leaveSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata);

    void writeHtml(const char *data, MD_SIZE size);
    void writeHtml(const QString &str);
    static QString escapeHtml(const QString &str);
    static QString escapeAttr(const QString &str);
    static QString alignmentStyle(MD_ALIGN align);
    static void parseDimensions(const QString &src, QString &cleanSrc, int &width, int &height);

    void enterCodeBlock(void *detail);
    void enterListItem(void *detail);
    void enterAdmonition(void *detail);
    void enterAlignedCell(void *detail, const char *tag);

    QString m_output;
    int m_currentLine = 1;
    ImageState m_img;
    Typography::Options m_typography;
    Typography::State m_typoState;
};


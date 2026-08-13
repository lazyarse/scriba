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
#include <QStringList>
#include <md4c.h>

#include "Typography.h"

class MdRenderer
{
public:
    // How ordered lists are numbered in the rendered output. The source keeps
    // whatever delimiter the user typed (`1.` or `1)`); this only picks the
    // presentation, driven by Preferences::OrderedListMarker.
    enum class OrderedListStyle {
        Decimal,          // 1. 2. 3.
        DecimalParen,     // 1) 2) 3)
        LowerAlpha,       // a. b. c.
        LowerAlphaParen,  // a) b) c)
        LowerRoman,       // i. ii. iii.
        LowerRomanParen   // i) ii) iii)
    };

    MdRenderer();

    QString render(const char *input, MD_SIZE size, unsigned parserFlags);

    // Enables smart-typography conversion of normal text runs. Options default
    // to none, so rendering is byte-for-byte unchanged until set.
    void setTypography(Typography::Options opts) { m_typography = opts; }

    // Sets the ordered-list numbering style (default Decimal: plain `<ol>`).
    void setOrderedListStyle(OrderedListStyle style) { m_listStyle = style; }

    // Maps a Preferences::OrderedListMarker string ("decimal", "alpha-paren",
    // ...) to an OrderedListStyle; unknown values fall back to Decimal.
    static OrderedListStyle orderedListStyleFromString(const QString &s);
    // The CSS class emitted on `<ol>` for the style ("" for plain decimal).
    static QString orderedListClass(OrderedListStyle style);

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
    void enterOrderedList(void *detail);
    void enterListItem(void *detail);
    void enterAdmonition(void *detail);
    void enterAlignedCell(void *detail, const char *tag);
    void leaveMathSpan();

    // Directive support (Task 0.2): top-level paragraphs are captured so the
    // renderer can strip SCRIBADIR[KB]<n> tokens and convert them into
    // scriba-keep / scriba-page-break classes on the next top-level block.
    void startParagraphCapture();
    void finishParagraphCapture();
    // Merges m_pendingClasses into `tag` if the current block is top-level.
    QString withPendingClasses(const QString &tag);
    static QString injectClasses(const QString &tag, const QStringList &classes);
    static QString stripTokens(const QString &str);

    // 0 = not in math, 1 = inline ($...$), 2 = display ($$...$$)
    int m_mathType = 0;
    QString m_mathBuf;
    int m_mathLine = 1;

    QString m_output;
    // Block-start source lines: the local md4c patch exposes each block's
    // byte offset in the input (`beg`); we convert it to a 1-based line by
    // counting newlines since the previous block (blocks fire in source order).
    const char *m_docText = nullptr;
    MD_SIZE m_docSize = 0;
    MD_OFFSET m_lastBeg = 0;
    int m_blockLine = 1;
    void advanceToBeg(void *detail);
    int m_blockDepth = 0;
    QString m_pBuf;
    QString *m_capture = nullptr;
    QStringList m_pendingClasses;
    ImageState m_img;
    Typography::Options m_typography;
    Typography::State m_typoState;
    OrderedListStyle m_listStyle = OrderedListStyle::Decimal;
};


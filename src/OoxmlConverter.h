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

#include "OoxmlToHtml.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QXmlStreamReader>

// OOXML (WordprocessingML) -> HTML body converter. Parses the parts of a .docx
// (OPC) package into an HTML fragment (paragraphs, runs, styles, tables, lists,
// hyperlinks, images, footnotes, OMML math). This struct is an implementation
// detail of OoxmlToHtml::convert(); it lives in its own file so OoxmlToHtml.cpp
// stays a thin public entry point.
namespace ooxmlconv {

struct Rel {
    QString target;
    QString type;      // last path segment of the relationship type URI
    bool external = false;
};

struct StyleInfo {
    int headingLevel = 0;
    bool bold = false;
    bool italic = false;
    bool mono = false;
};

struct ListLevel {
    bool ordered = false;
};

struct RunProps {
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool underline = false;
    bool superscript = false;
    bool subscript = false;
    bool mono = false;
};

struct Para {
    QString inner;
    int heading = 0;        // 1..6, or 0 for a plain paragraph
    int listLevel = -1;     // -1 = plain paragraph, else 0-based list level
    bool listOrdered = false;
    bool loose = false;     // list item has inline w:spacing -> loose list

    bool isListItem() const { return listLevel >= 0; }
};

struct Cell {
    QString html;
    int colspan = 0;        // 0 = "not specified"
    bool vMergeContinue = false;  // continuation of a vertically merged cell
};

struct ListState {
    QVector<bool> open;     // one flag per open <ol> (true) / <ul> (false)
};

struct Converter {
    const QHash<QString, QByteArray> &parts;

    QHash<QString, Rel> rels;
    QHash<QString, StyleInfo> styles;
    QHash<QString, QString> numToAbstract;
    QHash<QString, QHash<int, ListLevel>> numLevels;   // abstractNumId -> level
    QHash<QString, QString> footnoteTexts;
    QHash<QString, int> imageIndex;
    QVector<OoxmlImportedImage> images;                        // first-use order

    QStringList warnings;
    QStringList headers;
    QStringList footers;

    QByteArray part(const QString &path) const
    {
        const auto it = parts.constFind(path);
        return it == parts.constEnd() ? QByteArray() : it.value();
    }

    // ── package level ───────────────────────────────────────────────────────
    void parseRelationships();
    void parseStyles();
    static bool toggleVal(QXmlStreamReader &r);
    void parseNumbering();
    bool listOrdered(const QString &numId, int level) const;
    void parseFootnotes();
    QString plainParagraph(QXmlStreamReader &r);   // at <w:p>, consumes it
    QString plainPart(const QByteArray &bytes);
    void gatherHeadersFooters();

    // ── body ────────────────────────────────────────────────────────────────
    OoxmlToHtmlResult run(const QByteArray &doc);
    void emitBlocks(QXmlStreamReader &r, QStringList &frags, ListState &st);
    Para emitParagraph(QXmlStreamReader &r);
    void parseParaProps(QXmlStreamReader &r, Para &para);
    void emitRun(QXmlStreamReader &r, QString &content);
    void parseRunProps(QXmlStreamReader &r, RunProps &props);
    QString applyFormatting(const RunProps &props, const QString &content) const;
    void emitHyperlink(QXmlStreamReader &r, QString &content);
    QString image(QXmlStreamReader &r);
    QString mathLatex(QXmlStreamReader &r);
    void warn(const QString &msg);

    // ── lists ───────────────────────────────────────────────────────────────
    void closeLists(QStringList &frags, ListState &st);
    void addPara(QStringList &frags, ListState &st, const Para &para);

    // ── tables ──────────────────────────────────────────────────────────────
    void emitTable(QXmlStreamReader &r, QStringList &frags);
    Cell parseCell(QXmlStreamReader &r);

    // ── result assembly ─────────────────────────────────────────────────────
    OoxmlToHtmlResult finalize(const QStringList &frags);
};

} // namespace ooxmlconv

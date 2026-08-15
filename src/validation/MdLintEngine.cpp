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
#include "MdLintEngine.h"

#include "LinkValidator.h"
#include "MdLintRules.h"

#include <md4c.h>

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStack>
#include <QStringList>

#include <algorithm>

namespace {

// ---- document model -------------------------------------------------------

struct Token {
    MD_TEXTTYPE type = MD_TEXT_NORMAL;
    int beg = 0;   // byte offset into the input
    int len = 0;
};

struct Span {
    MD_SPANTYPE type = MD_SPAN_EM;
    int beg = -1;  // byte offset of first text token inside (-1 = no text)
    int end = -1;  // byte offset just past the last text token inside
    QString href, src, title, footnoteLabel;
    bool autolink = false;
};

struct Heading {
    int line = 0;     // 1-based line of the heading text
    int level = 1;
    int depth = 0;        // enclosing UL/OL/QUOTE blocks
    QString enclKey;      // enclosing block types, e.g. "Q", "UL", "UL,Q"
    bool closed = false;   // atx_closed style (raw-line detection)
    bool setext = false;
    QString text;          // reconstructed from tokens
};

struct CodeBlock {
    int begLine = 0;       // 1-based, fence line
    int endLine = 0;       // 1-based, closing fence line
    QChar fenceChar;       // '\0' for indented
    int fenceLen = 0;
    QString lang;
    bool indented = false;
};

struct ListItem {
    int line = 0;          // 1-based
    int depth = 0;         // 0-based nesting
    QChar mark;            // UL bullet; 0 for OL
    QChar olDelim;         // '.' or ')' for OL; 0 otherwise
    int markerCol = 1;     // 1-based column of the marker
    int contentCol = 1;    // 1-based column where content starts
};

struct Table {
    int begLine = 0;       // 1-based, header line
    int endLine = 0;       // 1-based, last body line
    QVector<int> rowLines; // 1-based lines of each row (header + body)
};

struct Model {
    QByteArray utf8;
    QStringList lines;           // split('\n'), no terminators
    QVector<int> lineStarts;     // byte offset of each line start
    QVector<Heading> headings;
    QVector<CodeBlock> codeBlocks;
    QVector<ListItem> listItems;
    QVector<Table> tables;
    QVector<int> hrLines;        // 1-based (raw scan, see collectHrs)
    QVector<int> looseListStarts; // 1-based start line of each loose list block
    QVector<QPair<int, int>> quotes;   // (begLine, endLine) 1-based (raw scan)
    QVector<QPair<int, int>> htmlBlocks; // (begLine, endLine) 1-based
    QVector<QPair<int, int>> paragraphs; // (begByte, lastEndByte)
    QVector<Token> tokens;
    QVector<Span> spans;
    QSet<QString> footnoteLabelsUsed;  // labels of MD_SPAN_FOOTNOTE_REF
    QSet<QString> headingSlugs;        // LinkValidator::headingSlug of each heading text
    QVector<int> altLessImgs;          // byte offset of "![" (no alt text)
    int lastByte = 0;                  // total input size
};

// ---- offset <-> line/col mapping ------------------------------------------

int lineIndexOf(const Model &m, int byteOff)
{
    const auto it = std::upper_bound(m.lineStarts.begin(), m.lineStarts.end(), byteOff);
    return static_cast<int>(it - m.lineStarts.begin()) - 1;
}

// Raw source text between two byte offsets.
QString rawSpan(const Model &m, int begByte, int endByte)
{
    return QString::fromUtf8(m.utf8.mid(begByte, qMax(0, endByte - begByte)));
}

// 1-based character column for a byte offset.
int byteToCol(const Model &m, int byteOff)
{
    const int li = lineIndexOf(m, byteOff);
    if (li < 0)
        return 1;
    const int rel = byteOff - m.lineStarts.value(li);
    const QByteArray lineUtf8 = m.lines.at(li).toUtf8();
    const int clamp = qMin(rel, lineUtf8.size());
    return QString::fromUtf8(lineUtf8.left(clamp)).size() + 1;
}

// Split a table row on unescaped pipes.
QStringList splitTableRow(const QString &line)
{
    QStringList cells;
    QString cur;
    bool escaped = false;
    for (const QChar ch : line) {
        if (escaped) {
            cur.append(ch);
            escaped = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }
        if (ch == QLatin1Char('|')) {
            cells.append(cur);
            cur.clear();
            continue;
        }
        cur.append(ch);
    }
    cells.append(cur);
    return cells;
}

// True when `line` (a table row with its trailing whitespace run already
// stripped) ends in an unescaped `|` in a fully bordered row (`|...|`) —
// i.e. the whitespace sat *after* the row's final border pipe, not inside
// its last cell. A borderless row never starts with a pipe, so its final
// `|` is a column separator and the trailing whitespace is the last cell's
// interior padding; an escaped border pipe (`\|`) is cell content. Such rows
// return false.
bool endsWithBorderPipe(const QString &line)
{
    if (line.isEmpty() || line.front() != QLatin1Char('|'))
        return false;
    if (line.back() != QLatin1Char('|'))
        return false;
    int backslashes = 0;
    for (int i = line.size() - 2; i >= 0 && line.at(i) == QLatin1Char('\\'); --i)
        ++backslashes;
    return backslashes % 2 == 0;
}

// Number of cells in a raw table row, per markdownlint: pipe count, minus a
// leading and a trailing pipe when present.
int rawColumnCount(const QString &raw)
{
    const QString line = raw.trimmed();
    int count = 1;
    bool escaped = false;
    for (const QChar ch : line) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }
        if (ch == QLatin1Char('|'))
            ++count;
    }
    if (line.startsWith(QLatin1Char('|')))
        --count;
    if (line.endsWith(QLatin1Char('|')))
        --count;
    return count;
}

// Human description of a row's leading/trailing pipe presence.
QString pipeStyleDesc(bool lead, bool trail)
{
    if (lead && trail)
        return QStringLiteral("leading and trailing pipes");
    if (!lead && !trail)
        return QStringLiteral("no leading or trailing pipes");
    return lead ? QStringLiteral("leading pipe only")
                : QStringLiteral("trailing pipe only");
}

// Byte range of the full source of a span, expanding past its markers:
// left marker run (chars in `markers`) adjacent to the first token, and the
// matching right run. Returns (-1,-1) when the span has no text tokens.
QPair<int, int> spanSourceBytes(const Model &m, const Span &s, const char *markers)
{
    if (s.beg < 0 || s.end < 0)
        return {-1, -1};
    const QByteArray &u = m.utf8;
    int l = s.beg;
    int r = s.end;
    while (l > 0 && strchr(markers, u.at(l - 1)))
        --l;
    while (r < u.size() && strchr(markers, u.at(r)))
        ++r;
    return {l, r};
}

// ---- collector ------------------------------------------------------------

struct Collector {
    Model m;

    // Block stack: type + line of block start. Used to track the end offset
    // of each open block and to assemble heading text.
    struct OpenBlock {
        MD_BLOCKTYPE type;
        int begByte;    // -1 = unknown (block detail was NULL); filled by the
                        // first text token inside (see onText)
        int lastEnd;    // max token end seen inside
        QStringList textParts;
    };
    QVector<OpenBlock> blocks;

    // Span stack.
    struct OpenSpan {
        MD_SPANTYPE type;
        Span span;
    };
    QVector<OpenSpan> spans;

    // End of the most recent text token (anchor for alt-less image positions).
    int lastTextEnd = 0;

    // List nesting depth (incremented on UL/OL enter).
    int listDepth = 0;
    // Heading nesting depth (enclosing UL/OL/QUOTE blocks).
    int hDepth = 0;
    // Current list marks, one entry per depth level.
    QVector<QPair<QChar, QChar>> listMarks;   // (mark, olDelim)
    // Stack index of the open MD_BLOCK_TABLE, -1 when none.
    int m_tableBlockIdx = -1;

    void onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size)
    {
        const int beg = static_cast<int>(text - m.utf8.constData());
        Token tok{type, beg, static_cast<int>(size)};
        m.tokens.append(tok);
        const int end = beg + static_cast<int>(size);
        lastTextEnd = qMax(lastTextEnd, end);
        for (auto &b : blocks) {
            if (b.begByte < 0)
                b.begByte = beg;
            b.lastEnd = qMax(b.lastEnd, end);
        }
        for (auto &s : spans) {
            if (s.span.beg < 0)
                s.span.beg = beg;
            s.span.end = qMax(s.span.end, end);
        }
        // Heading text assembly (block text outside code).
        for (auto &b : blocks) {
            if (b.type == MD_BLOCK_H && type != MD_TEXT_CODE)
                b.textParts << QString::fromUtf8(m.utf8.mid(beg, size));
        }
    }

    void enterBlock(MD_BLOCKTYPE type, void *detail)
    {
        int begByte = 0;
        if (detail) {
            struct Begged { MD_OFFSET beg; };
            begByte = static_cast<int>(static_cast<Begged *>(detail)->beg);
        }
        // Blocks without detail (HR, QUOTE, HTML) have no reliable start
        // offset; mark unknown and let the first text token pin the line.
        OpenBlock b{type, detail ? begByte : -1, detail ? begByte : -1, {}};
        if (type == MD_BLOCK_UL) {
            auto *d = static_cast<MD_BLOCK_UL_DETAIL *>(detail);
            if (!d->is_tight)
                m.looseListStarts.append(lineIndexOf(m, begByte) + 1);
            listMarks.resize(listDepth + 1);
            listMarks[listDepth] = {QChar(d->mark), QChar()};
            ++listDepth;
            ++hDepth;
        } else if (type == MD_BLOCK_OL) {
            auto *d = static_cast<MD_BLOCK_OL_DETAIL *>(detail);
            if (!d->is_tight)
                m.looseListStarts.append(lineIndexOf(m, begByte) + 1);
            listMarks.resize(listDepth + 1);
            listMarks[listDepth] = {QChar(), QChar(d->mark_delimiter)};
            ++listDepth;
            ++hDepth;
        } else if (type == MD_BLOCK_QUOTE) {
            ++hDepth;
        } else if (type == MD_BLOCK_LI) {
            auto *d = static_cast<MD_BLOCK_LI_DETAIL *>(detail);
            const int line = lineIndexOf(m, begByte) + 1;
            const QPair<QChar, QChar> marks =
                listMarks.isEmpty() ? QPair<QChar, QChar>{QLatin1Char('-'), QChar()}
                                    : listMarks.value(listDepth - 1);
            const QString raw = m.lines.value(line - 1);
            // markerCol: 1-based column of the bullet/digit run; contentCol:
            // 1-based column of the first non-space char after the marker.
            int i = 0;
            while (i < raw.size() && (raw.at(i) == QLatin1Char(' ') || raw.at(i) == QLatin1Char('\t')))
                ++i;
            const int markerCol = i + 1;
            if (i < raw.size() && raw.at(i) == marks.first)
                ++i;   // bullet
            else if (i < raw.size() && raw.at(i).isDigit())
                while (i < raw.size() && raw.at(i).isDigit())
                    ++i;
            int c = i;
            while (c < raw.size() && (raw.at(c) == QLatin1Char(' ') || raw.at(c) == QLatin1Char('\t')))
                ++c;
            m.listItems.append({line, qMax(0, listDepth - 1), marks.first, marks.second,
                                markerCol, c + 1});
        }
        blocks.append(b);
        if (type == MD_BLOCK_TABLE)
            m_tableBlockIdx = blocks.size() - 1;
        else if ((type == MD_BLOCK_TH || type == MD_BLOCK_TD) && m_tableBlockIdx >= 0
                 && !m.tables.isEmpty()) {
            const int line = lineIndexOf(m, begByte) + 1;
            if (!m.tables.last().rowLines.contains(line))
                m.tables.last().rowLines.append(line);
        }
    }

    void leaveBlock(MD_BLOCKTYPE type, void *detail)
    {
        const OpenBlock b = blocks.takeLast();
        if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
            --listDepth;
            if (hDepth > 0)
                --hDepth;
        } else if (type == MD_BLOCK_QUOTE) {
            if (hDepth > 0)
                --hDepth;
        } else if (type == MD_BLOCK_TABLE) {
            m_tableBlockIdx = -1;
        }

        if (type == MD_BLOCK_H) {
            Heading h;
            h.line = lineIndexOf(m, b.begByte) + 1;
            h.level = detail ? static_cast<MD_BLOCK_H_DETAIL *>(detail)->level : 1;
            h.depth = hDepth;
            QStringList encl;
            for (const auto &blk : blocks) {
                switch (blk.type) {
                case MD_BLOCK_UL: encl << QStringLiteral("UL"); break;
                case MD_BLOCK_OL: encl << QStringLiteral("OL"); break;
                case MD_BLOCK_QUOTE: encl << QStringLiteral("Q"); break;
                default: break;
                }
            }
            h.enclKey = encl.join(QLatin1Char(','));
            const QString raw = m.lines.value(h.line - 1);
            h.setext = !raw.startsWith(QLatin1Char('#'));
            h.closed = !h.setext && QRegularExpression(
                QStringLiteral(R"(^#{1,6}\s+.+\s+#{1,6}\s*$)")).match(raw).hasMatch();
            h.text = b.textParts.join(QString());
            m.headings.append(h);
        } else if (type == MD_BLOCK_CODE) {
            const auto *d = static_cast<MD_BLOCK_CODE_DETAIL *>(detail);
            CodeBlock cb;
            cb.begLine = lineIndexOf(m, b.begByte) + 1;
            cb.indented = d->fence_char == 0;
            cb.fenceChar = QChar(d->fence_char);
            const QString raw = m.lines.value(cb.begLine - 1);
            cb.fenceLen = 0;
            if (!cb.indented) {
                int k = 0;
                while (k < raw.size() && (raw.at(k) == QLatin1Char(' ') || raw.at(k) == QLatin1Char('\t')))
                    ++k;
                while (k < raw.size() && raw.at(k) == cb.fenceChar) {
                    ++cb.fenceLen;
                    ++k;
                }
            }
            cb.lang = QString::fromUtf8(d->lang.text, d->lang.size);
            const int lastTokLine = lineIndexOf(m, b.lastEnd) + 1;
            // Fenced blocks end at the closing fence line (one past the last
            // content token); indented blocks end at the last content line.
            cb.endLine = cb.indented ? qMax(cb.begLine, lastTokLine)
                                     : qMax(cb.begLine + 1, lastTokLine + 1);
            m.codeBlocks.append(cb);
        } else if (type == MD_BLOCK_TABLE) {
            const auto *d = static_cast<MD_BLOCK_TABLE_DETAIL *>(detail);
            Q_UNUSED(d);
            Table t;
            t.begLine = lineIndexOf(m, b.begByte) + 1;
            const int lastTokLine = lineIndexOf(m, b.lastEnd) + 1;
            // md4c folds lazy continuation lines into the table's last row
            // (e.g. `| b |\ntext`), so the physical end is the last line that
            // looks like a row (starts with `|`, or contains one — permissive
            // body rows).
            t.endLine = qMax(t.begLine, lastTokLine);
            for (int i = t.begLine; i < m.lines.size(); ++i) {
                const QString tr = m.lines.at(i).trimmed();
                if (tr.isEmpty() || (!tr.startsWith(QLatin1Char('|')) && !tr.contains(QLatin1Char('|'))))
                    break;
                t.endLine = i + 1;
            }
            m.tables.append(t);
        } else if (type == MD_BLOCK_HTML) {
            if (b.begByte >= 0)
                m.htmlBlocks.append({lineIndexOf(m, b.begByte) + 1,
                                     lineIndexOf(m, b.lastEnd) + 1});
        } else if (type == MD_BLOCK_P) {
            if (b.begByte >= 0)
                m.paragraphs.append({b.begByte, b.lastEnd});
        }
    }

    void enterSpan(MD_SPANTYPE type, void *detail)
    {
        Span s;
        s.type = type;
        switch (type) {
        case MD_SPAN_A: {
            auto *d = static_cast<MD_SPAN_A_DETAIL *>(detail);
            s.href = QString::fromUtf8(d->href.text, d->href.size);
            s.title = QString::fromUtf8(d->title.text, d->title.size);
            s.autolink = d->is_autolink != 0;
            break;
        }
        case MD_SPAN_IMG: {
            auto *d = static_cast<MD_SPAN_IMG_DETAIL *>(detail);
            s.src = QString::fromUtf8(d->src.text, d->src.size);
            s.title = QString::fromUtf8(d->title.text, d->title.size);
            break;
        }
        case MD_SPAN_FOOTNOTE_REF: {
            auto *d = static_cast<MD_SPAN_FOOTNOTE_REF_DETAIL *>(detail);
            s.footnoteLabel = QString::fromUtf8(d->label.text, d->label.size);
            m.footnoteLabelsUsed.insert(s.footnoteLabel);
            break;
        }
        default:
            break;
        }
        spans.append({type, s});
    }

    void leaveSpan(MD_SPANTYPE type, void *detail)
    {
        Q_UNUSED(detail);
        if (spans.isEmpty())
            return;
        const OpenSpan s = spans.takeLast();
        if (s.type == type) {
            m.spans.append(s.span);
            if (type == MD_SPAN_IMG && s.span.beg < 0) {
                // No alt text: anchor the position at the first `![` after the
                // most recent text token (md4c gives the span no offset).
                const int pos = m.utf8.indexOf("![", lastTextEnd);
                if (pos >= 0)
                    m.altLessImgs.append(pos);
            }
        }
    }
};

// ---- callbacks ------------------------------------------------------------

int enterBlockCb(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    static_cast<Collector *>(userdata)->enterBlock(type, detail);
    return 0;
}
int leaveBlockCb(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    static_cast<Collector *>(userdata)->leaveBlock(type, detail);
    return 0;
}
int enterSpanCb(MD_SPANTYPE type, void *detail, void *userdata)
{
    static_cast<Collector *>(userdata)->enterSpan(type, detail);
    return 0;
}
int leaveSpanCb(MD_SPANTYPE type, void *detail, void *userdata)
{
    static_cast<Collector *>(userdata)->leaveSpan(type, detail);
    return 0;
}
int textCb(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    static_cast<Collector *>(userdata)->onText(type, text, size);
    return 0;
}

// ---- front matter ---------------------------------------------------------

// Returns (startLine, endLine) 1-based of a leading YAML/TOML/JSON front
// matter block, or (-1, -1). Mirrors markdownlint's frontMatter option.
QPair<int, int> frontMatterRange(const Model &m)
{
    if (m.lines.isEmpty())
        return {-1, -1};
    const QString first = m.lines.first();
    const QChar opener = first.trimmed().startsWith(QLatin1String("---")) ? QLatin1Char('-')
                         : first.trimmed().startsWith(QLatin1String("+++")) ? QLatin1Char('+')
                         : first.trimmed().startsWith(QLatin1Char('{')) ? QLatin1Char('{')
                                                                        : QChar();
    if (opener.isNull())
        return {-1, -1};
    const QString close1 = opener == QLatin1Char('{') ? QStringLiteral("}") : QString(3, opener);
    for (int i = 1; i < m.lines.size(); ++i) {
        const QString t = m.lines.at(i).trimmed();
        if (opener == QLatin1Char('{') ? t.startsWith(QLatin1Char('}')) : t.startsWith(close1))
            return {1, i + 1};
    }
    // Unterminated delimiter is not front matter (a leading `---` is a
    // horizontal rule) — mirrors markdownlint's frontMatter handling.
    return {-1, -1};
}

// ---- raw-line helpers -----------------------------------------------------

bool isBlank(const QString &line) { return line.trimmed().isEmpty(); }

bool isCodeLine(const Model &m, int line /*1-based*/)
{
    for (const auto &cb : m.codeBlocks)
        if (line >= cb.begLine && line <= cb.endLine)
            return true;
    return false;
}

bool isFrontMatterLine(const Model &m, int line /*1-based*/)
{
    const auto fm = frontMatterRange(m);
    return line >= fm.first && line <= fm.second;
}

bool isInQuote(const Model &m, int line /*1-based*/)
{
    for (const auto &q : m.quotes)
        if (line >= q.first && line <= q.second)
            return true;
    return false;
}

bool isHeadingLine(const Model &m, int line /*1-based*/)
{
    for (const auto &h : m.headings)
        if (h.line == line)
            return true;
    return false;
}

bool isTableLine(const Model &m, int line /*1-based*/)
{
    for (const auto &t : m.tables)
        if (line >= t.begLine && line <= t.endLine)
            return true;
    return false;
}

bool isListItemLine(const Model &m, int line /*1-based*/)
{
    for (const auto &li : m.listItems)
        if (li.line == line)
            return true;
    return false;
}

// ---- issue helpers --------------------------------------------------------

struct RuleCtx {
    const Model &m;
    const MdLintConfig &cfg;
    QVector<MdLintIssue> issues;
};

// Appends one issue, honoring enable/severity; skips lines in front matter
// (callers opt back in per rule) and, by default, code lines (the legacy
// MarkdownChecker only scanned checkable lines — parity). skipQuote
// suppresses findings inside blockquote ranges.
void emitIssue(RuleCtx &ctx, const QString &ruleId, int line /*1-based*/, int col,
               int length, const QString &detail, bool skipCode = true, bool skipQuote = false)
{
    const MdLintRule *rule = MdLintRules::byKey(ruleId);
    if (!rule || !ctx.cfg.enabled(ruleId))
        return;
    if (isFrontMatterLine(ctx.m, line))
        return;
    if (skipCode && isCodeLine(ctx.m, line))
        return;
    if (skipQuote && isInQuote(ctx.m, line))
        return;
    ctx.issues.append({ruleId, rule->alias, rule->description,
                       ctx.cfg.severity(ruleId), line, col, length, detail});
}

} // namespace

QVector<MdLintIssue> MdLintEngine::lint(const QString &text, const MdLintConfig &inConfig)
{
    // ---- parse ------------------------------------------------------------
    const QString parsed = text.endsWith(QLatin1Char('\n')) ? text : text + QLatin1Char('\n');
    Collector c;
    c.m.utf8 = parsed.toUtf8();
    c.m.lines = parsed.split(QLatin1Char('\n'));
    if (!c.m.lines.isEmpty() && c.m.lines.last().isEmpty())
        c.m.lines.removeLast();   // trailing empty element from final '\n'
    int off = 0;
    for (const auto &line : c.m.lines) {
        c.m.lineStarts.append(off);
        off += line.toUtf8().size() + 1;
    }
    c.m.lastByte = c.m.utf8.size();

    MD_PARSER parser = {};
    parser.abi_version = 0;
    parser.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS
                 | MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_ADMONITIONS
                 | MD_FLAG_HIGHLIGHT | MD_FLAG_SUPERSCRIPTS | MD_FLAG_SUBSCRIPTS
                 | MD_FLAG_FOOTNOTES | MD_FLAG_LATEXMATHSPANS | MD_FLAG_DEFINITIONLISTS;
    parser.enter_block = enterBlockCb;
    parser.leave_block = leaveBlockCb;
    parser.enter_span = enterSpanCb;
    parser.leave_span = leaveSpanCb;
    parser.text = textCb;
    md_parse(c.m.utf8.constData(), static_cast<MD_SIZE>(c.m.utf8.size()), &parser, &c);

    // ---- model post-pass --------------------------------------------------
    for (const auto &h : c.m.headings)
        c.m.headingSlugs.insert(LinkValidator::headingSlug(h.text));
    // HR lines: raw scan (md4c HR blocks carry no detail).
    static const QRegularExpression hrRe(
        QStringLiteral(R"(^\s{0,3}(?:([*+-])\s*){3,}$|^\s{0,3}(?:_\s*){3,}$)"));
    const QSet<int> setextUnderlines = [&c] {
        QSet<int> out;
        for (const auto &h : c.m.headings)
            if (h.setext)
                out.insert(h.line + 1);
        return out;
    }();
    for (int i = 0; i < c.m.lines.size(); ++i) {
        const int line = i + 1;
        if (isCodeLine(c.m, line) || isFrontMatterLine(c.m, line) || setextUnderlines.contains(line))
            continue;
        if (hrRe.match(c.m.lines.at(i)).hasMatch())
            c.m.hrLines.append(line);
    }
    // Quote ranges: raw scan (md4c QUOTE blocks carry no detail).
    int qBeg = -1;
    for (int i = 0; i < c.m.lines.size(); ++i) {
        const int line = i + 1;
        const bool isQ = !isCodeLine(c.m, line) && !isFrontMatterLine(c.m, line)
                      && QRegularExpression(QStringLiteral(R"(^\s{0,3}>)"))
                             .match(c.m.lines.at(i)).hasMatch();
        if (isQ && qBeg < 0)
            qBeg = line;
        else if (!isQ && qBeg >= 0) {
            c.m.quotes.append({qBeg, line - 1});
            qBeg = -1;
        }
    }
    if (qBeg >= 0)
        c.m.quotes.append({qBeg, c.m.lines.size()});

    // ---- inline config directives (configure-file) --------------------------
    // `<!-- markdownlint-configure-file {...} -->` comments merge JSON over
    // the base config (document order, later wins); the merged config drives
    // every rule pass and emit() below.
    MdLintConfig effective = inConfig;
    {
        static const QRegularExpression cfgFileRe(
            QStringLiteral(R"(<!--\s*markdownlint-configure-file\s+(\{.*\})\s*-->)"));
        for (const auto &line : c.m.lines) {
            const auto cm = cfgFileRe.match(line);
            if (cm.hasMatch()) {
                const QJsonDocument doc =
                    QJsonDocument::fromJson(cm.captured(1).toUtf8());
                if (doc.isObject())
                    effective.applyJson(doc.object());
            }
        }
    }
    const MdLintConfig &config = effective;

    // ---- rules ------------------------------------------------------------
    RuleCtx ctx{c.m, config, {}};

    // --- raw-line rules: MD009, MD010, MD011, MD012, MD013, MD018-MD021,
    //     MD027, MD028, MD900 ------------------------------------------------
    static const QRegularExpression noSpaceHashRe(QStringLiteral(R"(^\#{1,6}+\S)"));
    static const QRegularExpression tooManySpacesRe(QStringLiteral(R"(^(\#{1,6})( {2,}))"));
    static const QRegularExpression closedNoSpaceRe(QStringLiteral(R"(^(\#{1,6}\s+.+)([^#\s])(\#+\s*)$)"));
    static const QRegularExpression closedExtraSpaceRe(QStringLiteral(R"(^(\#{1,6}\s+.+)( {2,})(\#+\s*)$)"));
    static const QRegularExpression quoteExtraSpaceRe(QStringLiteral(R"(^(\s{0,3}>)( {2,}))"));
    static const QRegularExpression reversedLinkRe(QStringLiteral(R"(\(([^)]*)\)\[([^\]]*)\])"));
    static const QRegularExpression footnoteDefRe(QStringLiteral(R"(^\[\^([^\]]+)\]:)"));
    static const QRegularExpression footnoteUseRe(QStringLiteral(R"(\[\^([^\]\n]+)\])"));
    static const QRegularExpression inlineCodeRe(QStringLiteral(R"(`[^`\n]*`)"));
    const int kMaxLineLength = config.param(QStringLiteral("MD013"), "line_length", 80).toInt();
    QSet<QString> footnoteDefs;
    struct FootnoteUse { int line; int col; int len; QString label; };
    QVector<FootnoteUse> footnoteUses;
    int blankRun = 0;
    for (int i = 0; i < c.m.lines.size(); ++i) {
        const int line = i + 1;
        const QString &raw = c.m.lines.at(i);
        const bool codeLine = isCodeLine(c.m, line);
        const bool fmLine = isFrontMatterLine(c.m, line);

        // MD013 — line length. Runs before the code/front-matter skip so code
        // lines can still be measured when `code_blocks` allows it.
        if (config.enabled(QStringLiteral("MD013")) && !fmLine) {
            int limit = kMaxLineLength;
            bool skip = false;
            if (codeLine) {
                skip = !config.param(QStringLiteral("MD013"), "code_blocks", true).toBool();
                const int cl = config.param(QStringLiteral("MD013"), "code_block_line_length", 0).toInt();
                if (cl > 0)
                    limit = cl;
            } else if (isHeadingLine(c.m, line)) {
                skip = !config.param(QStringLiteral("MD013"), "headings", true).toBool();
                const int hl = config.param(QStringLiteral("MD013"), "heading_line_length", 0).toInt();
                if (hl > 0)
                    limit = hl;
            } else if (isTableLine(c.m, line)) {
                skip = !config.param(QStringLiteral("MD013"), "tables", true).toBool();
            }
            const bool strict = config.param(QStringLiteral("MD013"), "strict", false).toBool();
            if (!skip && raw.size() > limit
                && (strict || !raw.endsWith(QStringLiteral("  "))))
                emitIssue(ctx, QStringLiteral("MD013"), line, limit + 1,
                     raw.size() - limit,
                     QStringLiteral("Line length: %1").arg(raw.size()),
                     /*skipCode=*/false, /*skipQuote=*/false);
        }

        // Code and front-matter lines are invisible to the remaining checks
        // (legacy MarkdownChecker's `checkable` rule), except MD010 when
        // `code_blocks` asks for code lines to be scanned.
        if (fmLine || codeLine) {
            if (codeLine && config.enabled(QStringLiteral("MD010"))
                && !config.param(QStringLiteral("MD010"), "code_blocks", true).toBool())
                for (int j = 0; j < raw.size(); ++j)
                    if (raw.at(j) == QLatin1Char('\t'))
                        emitIssue(ctx, QStringLiteral("MD010"), line, j + 1, 1,
                             QStringLiteral("Column: %1").arg(j + 1), /*skipCode=*/false);
            blankRun = 0;
            continue;
        }

        // MD012 — runs of blank lines beyond `maximum`
        if (isBlank(raw)) {
            ++blankRun;
            const int maximum = config.param(QStringLiteral("MD012"), "maximum", 1).toInt();
            if (blankRun == maximum + 1)
                emitIssue(ctx, QStringLiteral("MD012"), line, 1, 0,
                     QStringLiteral("Expected: %1; Actual: %2").arg(maximum).arg(blankRun));
            // MD028 — blank line sandwiched between blockquote lines
            if (config.enabled(QStringLiteral("MD028")) && line > 1 && line < c.m.lines.size()
                && isInQuote(c.m, line - 1) && isInQuote(c.m, line + 1))
                emitIssue(ctx, QStringLiteral("MD028"), line, 1, 0,
                     QStringLiteral("Blank line inside blockquote"));
            continue;
        }
        blankRun = 0;

        // MD009 — trailing spaces
        if (config.enabled(QStringLiteral("MD009"))) {
            int ws = raw.size();
            while (ws > 0) {
                const QChar ch = raw.at(ws - 1);
                if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t'))
                    break;
                --ws;
            }
            if (ws < raw.size()) {
                // Table rows: the auto-align formatter writes end-of-row
                // padding (borderless tables) that the next realign pass
                // re-inserts — flagging it is a self-inflicted, unfixable
                // warning. Exempt everything except whitespace that sits
                // after the row's final border pipe; the `tables` param
                // re-enables strict checking.
                if (isTableLine(c.m, line)
                    && !config.param(QStringLiteral("MD009"), "tables", false).toBool()
                    && !endsWithBorderPipe(raw.left(ws)))
                    ws = raw.size();
                if (ws < raw.size()) {
                    const int n = raw.size() - ws;
                    // br_spaces: allow up to N trailing spaces (non-strict).
                    const bool strict = config.param(QStringLiteral("MD009"), "strict", false).toBool();
                    const int brSpaces = config.param(QStringLiteral("MD009"), "br_spaces", 2).toInt();
                    if (strict || n > brSpaces)
                        emitIssue(ctx, QStringLiteral("MD009"), line, ws + 1, n,
                             QStringLiteral("Trailing spaces: %1").arg(n));
                }
            }
        }

        // MD010 — hard tabs (code lines are exempt via the collector skip
        // above unless `code_blocks` enables them)
        if (config.enabled(QStringLiteral("MD010"))) {
            for (int j = 0; j < raw.size(); ++j)
                if (raw.at(j) == QLatin1Char('\t'))
                    emitIssue(ctx, QStringLiteral("MD010"), line, j + 1, 1,
                         QStringLiteral("Column: %1").arg(j + 1));
        }

        // MD011 — reversed link syntax, e.g. (text)[url]
        if (config.enabled(QStringLiteral("MD011"))) {
            QVector<QPair<int, int>> codeRanges;
            auto cit = inlineCodeRe.globalMatch(raw);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                codeRanges.append({cm.capturedStart(), cm.capturedLength()});
            }
            auto rit = reversedLinkRe.globalMatch(raw);
            while (rit.hasNext()) {
                const auto rm = rit.next();
                bool inCode = false;
                for (const auto &r : codeRanges)
                    if (rm.capturedStart() >= r.first && rm.capturedStart() < r.first + r.second)
                        inCode = true;
                if (!inCode)
                    emitIssue(ctx, QStringLiteral("MD011"), line,
                         static_cast<int>(rm.capturedStart() + 1),
                         static_cast<int>(rm.capturedLength()), rm.captured(0));
            }
        }

        // MD018 — `#` heading without a space
        const auto noSpace = noSpaceHashRe.match(raw);
        if (noSpace.hasMatch())
            emitIssue(ctx, QStringLiteral("MD018"), line, 1, noSpace.capturedLength(),
                 QStringLiteral("Expected: 1 space; Actual: 0"));

        // MD019 — multiple spaces after the hashes
        const auto tooManySpaces = tooManySpacesRe.match(raw);
        if (tooManySpaces.hasMatch())
            emitIssue(ctx, QStringLiteral("MD019"), line,
                 static_cast<int>(tooManySpaces.capturedStart(2) + 1),
                 static_cast<int>(tooManySpaces.capturedLength(2)),
                 QStringLiteral("Expected: 1 space; Actual: %1")
                     .arg(tooManySpaces.capturedLength(2)));

        // MD020 — closed atx heading with no space before the closing hashes
        // ([^#\s] keeps the content's last char from backtracing into the
        // closing run, so `## Title ##` stays valid)
        const auto closedNoSpace = closedNoSpaceRe.match(raw);
        if (closedNoSpace.hasMatch())
            emitIssue(ctx, QStringLiteral("MD020"), line,
                 static_cast<int>(closedNoSpace.capturedEnd(2) + 1), 1,
                 QStringLiteral("Expected: 1 space; Actual: 0"));

        // MD021 — closed atx heading with multiple spaces before the closing
        // hashes
        const auto closedExtraSpace = closedExtraSpaceRe.match(raw);
        if (closedExtraSpace.hasMatch())
            emitIssue(ctx, QStringLiteral("MD021"), line,
                 static_cast<int>(closedExtraSpace.capturedStart(2) + 1),
                 static_cast<int>(closedExtraSpace.capturedLength(2)),
                 QStringLiteral("Expected: 1 space; Actual: %1")
                     .arg(closedExtraSpace.capturedLength(2)));

        // MD027 — multiple spaces after the blockquote marker
        const auto quoteExtraSpace = quoteExtraSpaceRe.match(raw);
        if (quoteExtraSpace.hasMatch())
            emitIssue(ctx, QStringLiteral("MD027"), line,
                 static_cast<int>(quoteExtraSpace.capturedStart(2) + 1),
                 static_cast<int>(quoteExtraSpace.capturedLength(2)),
                 QStringLiteral("Expected: 1 space; Actual: %1")
                     .arg(quoteExtraSpace.capturedLength(2)));

        // MD900 — footnote definitions + usages (raw scan; inline code spans
        // excluded so `[^x]` inside backticks is not reported).
        const auto defMatch = footnoteDefRe.match(raw);
        if (defMatch.hasMatch()) {
            footnoteDefs.insert(defMatch.captured(1));
        } else {
            QVector<QPair<int, int>> codeRanges;
            auto cit = inlineCodeRe.globalMatch(raw);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                codeRanges.append({cm.capturedStart(), cm.capturedLength()});
            }
            auto uit = footnoteUseRe.globalMatch(raw);
            while (uit.hasNext()) {
                const auto um = uit.next();
                const int useStart = um.capturedStart();
                bool inCode = false;
                for (const auto &r : codeRanges)
                    if (useStart >= r.first && useStart < r.first + r.second)
                        inCode = true;
                if (!inCode)
                    footnoteUses.append({line, static_cast<int>(useStart + 1),
                                         static_cast<int>(um.capturedLength()),
                                         um.captured(1)});
            }
        }
    }

    // MD014 — a `$` command line followed by another `$` command line inside a
    // fenced block (the first command shows no output)
    if (config.enabled(QStringLiteral("MD014"))) {
        for (const auto &cb : c.m.codeBlocks) {
            if (cb.indented)
                continue;
            for (int i = cb.begLine - 1; i < cb.endLine; ++i) {
                const QString &l = c.m.lines.at(i);
                if (!l.startsWith(QStringLiteral("$ ")))
                    continue;
                bool nextIsDollar = false;
                for (int k = i + 1; k < cb.endLine; ++k) {
                    if (isBlank(c.m.lines.at(k)))
                        continue;
                    nextIsDollar = c.m.lines.at(k).startsWith(QStringLiteral("$ "));
                    break;
                }
                if (nextIsDollar)
                    emitIssue(ctx, QStringLiteral("MD014"), i + 1, 1, 1,
                         QStringLiteral("Dollar sign used before a command without output"),
                         /*skipCode=*/false);
            }
        }
    }

    if (config.enabled(QStringLiteral("MD900"))) {
        // Usages resolved by md4c (footnoteLabelsUsed) are fine; a definition
        // collected above satisfies a usage too; anything else is unmatched.
        for (const auto &u : footnoteUses) {
            if (!c.m.footnoteLabelsUsed.contains(u.label) && !footnoteDefs.contains(u.label))
                emitIssue(ctx, QStringLiteral("MD900"), u.line, u.col, u.len,
                     QStringLiteral("Unmatched footnote reference: [^%1]").arg(u.label));
        }
    }

    // --- model rules: MD001, MD003-MD007, MD022-MD026, MD024, MD029-MD032,
    //     MD035, MD040, MD041, MD043, MD046, MD047, MD048, MD058 -------------
    int prevLevel = 0;
    for (const auto &h : c.m.headings) {
        if (config.enabled(QStringLiteral("MD001")) && prevLevel > 0 && h.level > prevLevel + 1)
            emitIssue(ctx, QStringLiteral("MD001"), h.line, 1, 0,
                 QStringLiteral("Expected: %1; Actual: %2").arg(prevLevel + 1).arg(h.level));
        prevLevel = h.level;
    }

    // MD003 — heading style
    if (config.enabled(QStringLiteral("MD003"))) {
        const QString style =
            config.param(QStringLiteral("MD003"), "style", QStringLiteral("consistent"))
                .toString();
        QString consistent;
        for (const auto &h : c.m.headings) {
            QString actual;
            if (h.setext)
                actual = QStringLiteral("setext");
            else if (h.closed)
                actual = QStringLiteral("atx_closed");
            else
                actual = QStringLiteral("atx");
            if (consistent.isEmpty())
                consistent = actual;
            QString expected = style == QLatin1String("consistent") ? consistent : style;
            if (style == QLatin1String("setext_with_atx"))
                expected = h.level <= 2 ? QStringLiteral("setext") : QStringLiteral("atx");
            else if (style == QLatin1String("setext_with_atx_closed"))
                expected = h.level <= 2 ? QStringLiteral("setext")
                                        : QStringLiteral("atx_closed");
            if (actual != expected)
                emitIssue(ctx, QStringLiteral("MD003"), h.line, 1, 0,
                     QStringLiteral("Expected: %1; Actual: %2").arg(expected, actual));
        }
    }

    // MD004 — unordered list marker style
    if (config.enabled(QStringLiteral("MD004"))) {
        const QString style =
            config.param(QStringLiteral("MD004"), "style", QStringLiteral("consistent"))
                .toString();
        QVector<QChar> depthMark;   // first mark seen per depth level
        for (const auto &li : c.m.listItems) {
            if (li.mark.isNull())
                continue;
            if (depthMark.size() <= li.depth)
                depthMark.resize(li.depth + 1);
            if (depthMark[li.depth].isNull())
                depthMark[li.depth] = li.mark;
            QChar expected;
            if (style == QLatin1String("asterisk"))
                expected = QLatin1Char('*');
            else if (style == QLatin1String("dash"))
                expected = QLatin1Char('-');
            else if (style == QLatin1String("plus"))
                expected = QLatin1Char('+');
            else if (style == QLatin1String("sublist"))
                expected = li.depth > 0 ? depthMark.value(li.depth - 1) : QChar();
            else   // consistent
                expected = depthMark.first();
            if (expected.isNull() || li.mark == expected)
                continue;
            emitIssue(ctx, QStringLiteral("MD004"), li.line, li.markerCol, 1,
                 QStringLiteral("Expected: %1; Actual: %2").arg(expected, li.mark));
        }
    }

    // MD005 — consistent list item indentation per depth level
    if (config.enabled(QStringLiteral("MD005"))) {
        QVector<int> depthCol;
        for (const auto &li : c.m.listItems) {
            if (depthCol.size() <= li.depth)
                depthCol.resize(li.depth + 1, -1);
            if (depthCol[li.depth] < 0)
                depthCol[li.depth] = li.markerCol;
            else if (li.markerCol != depthCol[li.depth])
                emitIssue(ctx, QStringLiteral("MD005"), li.line, li.markerCol, 1,
                     QStringLiteral("Expected: %1; Actual: %2")
                         .arg(depthCol[li.depth]).arg(li.markerCol));
        }
    }

    // MD007 — unordered list indentation (marker column per depth level)
    if (config.enabled(QStringLiteral("MD007"))) {
        const int indent = config.param(QStringLiteral("MD007"), "indent", 2).toInt();
        for (const auto &li : c.m.listItems) {
            if (li.mark.isNull())
                continue;
            const int expected = indent * li.depth + 1;
            if (li.markerCol != expected)
                emitIssue(ctx, QStringLiteral("MD007"), li.line, li.markerCol, 1,
                     QStringLiteral("Expected: %1; Actual: %2").arg(expected).arg(li.markerCol));
        }
    }

    // MD022 — blank lines around headings
    if (config.enabled(QStringLiteral("MD022"))) {
        const int above = config.param(QStringLiteral("MD022"), "lines_above", 1).toInt();
        const int below = config.param(QStringLiteral("MD022"), "lines_below", 1).toInt();
        for (const auto &h : c.m.headings) {
            const int bottomLine = h.setext ? h.line + 1 : h.line;
            if (h.line > 1 && !isFrontMatterLine(c.m, h.line - 1)) {
                int blanks = 0;
                for (int k = h.line - 2; k >= 0 && isBlank(c.m.lines.at(k)); --k)
                    ++blanks;
                if (blanks < above)
                    emitIssue(ctx, QStringLiteral("MD022"), h.line, 1, 0,
                         QStringLiteral("Expected: %1 blank line(s) above; Actual: %2")
                             .arg(above).arg(blanks));
            }
            if (bottomLine < c.m.lines.size()) {
                int blanks = 0;
                for (int k = bottomLine; k < c.m.lines.size() && isBlank(c.m.lines.at(k)); ++k)
                    ++blanks;
                if (blanks < below)
                    emitIssue(ctx, QStringLiteral("MD022"), h.line, 1, 0,
                         QStringLiteral("Expected: %1 blank line(s) below; Actual: %2")
                             .arg(below).arg(blanks));
            }
        }
    }

    // MD023 — leading spaces on heading lines
    if (config.enabled(QStringLiteral("MD023"))) {
        for (const auto &h : c.m.headings) {
            const QString &raw = c.m.lines.at(h.line - 1);
            int n = 0;
            while (n < raw.size() && raw.at(n) == QLatin1Char(' '))
                ++n;
            if (n > 0)
                emitIssue(ctx, QStringLiteral("MD023"), h.line, 1, n,
                     QStringLiteral("Expected: 0 spaces; Actual: %1").arg(n));
        }
    }

    // MD024 — duplicate headings (with nesting constraints)
    if (config.enabled(QStringLiteral("MD024"))) {
        const bool siblingsOnly =
            config.param(QStringLiteral("MD024"), "siblings_only", false).toBool();
        const bool allowDiffNesting =
            config.param(QStringLiteral("MD024"), "allow_different_nesting", false).toBool();
        QHash<QString, QVector<int>> seen;   // slug -> heading indices
        for (int idx = 0; idx < c.m.headings.size(); ++idx) {
            const auto &h = c.m.headings.at(idx);
            const QString slug = LinkValidator::headingSlug(h.text);
            if (slug.isEmpty())
                continue;
            const auto prevIdxs = seen.value(slug);
            bool dup = false;
            for (const int p : prevIdxs) {
                const auto &prev = c.m.headings.at(p);
                bool same = true;
                if (siblingsOnly && (prev.enclKey != h.enclKey || prev.depth != h.depth))
                    same = false;
                if (allowDiffNesting && prev.depth != h.depth)
                    same = false;
                if (same) {
                    dup = true;
                    break;
                }
            }
            if (dup)
                emitIssue(ctx, QStringLiteral("MD024"), h.line, 1, 0,
                          QStringLiteral("Duplicate heading: %1").arg(h.text.trimmed()));
            else
                seen[slug].append(idx);
        }
    }

    // MD025 — multiple top-level headings (front-matter title exempt)
    if (config.enabled(QStringLiteral("MD025"))) {
        const int level = config.param(QStringLiteral("MD025"), "level", 1).toInt();
        const QString fmTitle =
            config.param(QStringLiteral("MD025"), "front_matter_title", QString()).toString();
        bool seen = false;
        for (const auto &h : c.m.headings) {
            if (h.level != level)
                continue;
            if (!fmTitle.isEmpty() && h.text.trimmed().compare(fmTitle, Qt::CaseInsensitive) == 0)
                continue;
            if (seen)
                emitIssue(ctx, QStringLiteral("MD025"), h.line, 1, 0,
                     QStringLiteral("Multiple top-level headings in the same document"));
            seen = true;
        }
    }

    // MD026 — trailing punctuation on headings
    if (config.enabled(QStringLiteral("MD026"))) {
        const QString punct =
            config.param(QStringLiteral("MD026"), "punctuation",
                         QStringLiteral(".,;:!。，；：！")).toString();
        for (const auto &h : c.m.headings) {
            const QString content = h.text.trimmed();
            if (content.isEmpty())
                continue;
            const QChar last = content.at(content.size() - 1);
            if (!punct.contains(last))
                continue;
            const QString &raw = c.m.lines.at(h.line - 1);
            const int pos = raw.lastIndexOf(content);
            const int col = (pos >= 0 ? pos : 0) + content.size();
            emitIssue(ctx, QStringLiteral("MD026"), h.line, col, 1,
                 QStringLiteral("Remove trailing punctuation: %1").arg(last));
        }
    }

    // MD029 — ordered list numbering style, grouped into runs of consecutive
    // same-depth items
    if (config.enabled(QStringLiteral("MD029"))) {
        const QString style =
            config.param(QStringLiteral("MD029"), "style", QStringLiteral("one")).toString();
        int runIndex = 0;
        int runDepth = -1;
        int runPrevLine = -2;
        for (const auto &li : c.m.listItems) {
            if (!li.mark.isNull() || li.depth != runDepth || li.line != runPrevLine + 1) {
                runIndex = 0;
                runDepth = li.depth;
            }
            runPrevLine = li.line;
            const QString &raw = c.m.lines.at(li.line - 1);
            int j = 0;
            while (j < raw.size() && raw.at(j) == QLatin1Char(' '))
                ++j;
            int num = 0;
            while (j < raw.size() && raw.at(j).isDigit()) {
                num = num * 10 + raw.at(j).digitValue();
                ++j;
            }
            if (style == QLatin1String("one_or_ordered")) {
                if (num != 1 && num != runIndex + 1)
                    emitIssue(ctx, QStringLiteral("MD029"), li.line, 1, 0,
                         QStringLiteral("Expected: %1 or %2; Actual: %3")
                             .arg(1).arg(runIndex + 1).arg(num));
            } else {
                int expected = 1;
                if (style == QLatin1String("zero"))
                    expected = runIndex;
                else if (style == QLatin1String("ordered"))
                    expected = runIndex + 1;
                if (num != expected)
                    emitIssue(ctx, QStringLiteral("MD029"), li.line, 1, 0,
                         QStringLiteral("Expected: %1; Actual: %2").arg(expected).arg(num));
            }
            ++runIndex;
        }
    }

    // MD030 — spaces after the list marker
    if (config.enabled(QStringLiteral("MD030"))) {
        const int ulMulti = config.param(QStringLiteral("MD030"), "ul_multi", 3).toInt();
        const int olMulti = config.param(QStringLiteral("MD030"), "ol_multi", 2).toInt();
        for (int idx = 0; idx < c.m.listItems.size(); ++idx) {
            const auto &li = c.m.listItems.at(idx);
            const QString &raw = c.m.lines.at(li.line - 1);
            int j = 0;
            while (j < raw.size() && raw.at(j) == QLatin1Char(' '))
                ++j;
            const int markerStart = j;
            if (li.mark.isNull()) {
                while (j < raw.size() && raw.at(j).isDigit())
                    ++j;
                if (j < raw.size())
                    ++j;
            } else {
                ++j;
            }
            int spaces = 0;
            while (j < raw.size() && (raw.at(j) == QLatin1Char(' ')
                                      || raw.at(j) == QLatin1Char('\t'))) {
                ++spaces;
                ++j;
            }
            if (spaces <= 1)
                continue;
            // multiline: any following line before the next item at this depth
            // or deeper, indented to at least the content column
            bool multiline = false;
            const int nextLine = (idx + 1 < c.m.listItems.size()
                                  && c.m.listItems.at(idx + 1).depth <= li.depth)
                                     ? c.m.listItems.at(idx + 1).line
                                     : c.m.lines.size() + 1;
            for (int k = li.line + 1; k < nextLine; ++k) {
                const QString &l2 = c.m.lines.at(k);
                if (isBlank(l2))
                    continue;
                int ws = 0;
                while (ws < l2.size() && l2.at(ws) == QLatin1Char(' '))
                    ++ws;
                if (ws >= li.contentCol - 1) {
                    multiline = true;
                    break;
                }
            }
            const int allowed = multiline ? (li.mark.isNull() ? olMulti : ulMulti) : 1;
            if (spaces > allowed)
                emitIssue(ctx, QStringLiteral("MD030"), li.line, markerStart + 1, spaces,
                     QStringLiteral("Expected: %1 space(s); Actual: %2")
                         .arg(allowed).arg(spaces));
        }
    }

    // MD031 — blank lines around fenced code blocks
    if (config.enabled(QStringLiteral("MD031"))) {
        const bool listItems = config.param(QStringLiteral("MD031"), "list_items", true).toBool();
        for (const auto &cb : c.m.codeBlocks) {
            if (cb.indented)
                continue;
            if (listItems && isListItemLine(c.m, cb.begLine))
                continue;
            if (cb.begLine > 1 && !isFrontMatterLine(c.m, cb.begLine - 1)
                && !isBlank(c.m.lines.at(cb.begLine - 2)))
                emitIssue(ctx, QStringLiteral("MD031"), cb.begLine, 1, 0,
                     QStringLiteral("Expected: 1 blank line before; Actual: 0"),
                     /*skipCode=*/false);
            if (cb.endLine < c.m.lines.size() && !isBlank(c.m.lines.at(cb.endLine)))
                emitIssue(ctx, QStringLiteral("MD031"), cb.endLine, 1, 0,
                     QStringLiteral("Expected: 1 blank line after; Actual: 0"),
                     /*skipCode=*/false);
        }
    }

    // MD032 — blank lines around lists (grouped runs of same-depth items)
    if (config.enabled(QStringLiteral("MD032"))) {
        int g = 0;
        while (g < c.m.listItems.size()) {
            const int depth = c.m.listItems.at(g).depth;
            int gEnd = g;
            while (gEnd + 1 < c.m.listItems.size()
                   && c.m.listItems.at(gEnd + 1).depth == depth)
                ++gEnd;
            const int firstLine = c.m.listItems.at(g).line;
            const int lastLine = c.m.listItems.at(gEnd).line;
            if (firstLine > 1 && !isFrontMatterLine(c.m, firstLine - 1)
                && !isBlank(c.m.lines.at(firstLine - 2))
                && !isListItemLine(c.m, firstLine - 1))
                emitIssue(ctx, QStringLiteral("MD032"), firstLine, 1, 0,
                     QStringLiteral("Expected: 1 blank line before; Actual: 0"));
            if (lastLine < c.m.lines.size() && !isBlank(c.m.lines.at(lastLine))
                && !isListItemLine(c.m, lastLine + 1))
                emitIssue(ctx, QStringLiteral("MD032"), lastLine, 1, 0,
                     QStringLiteral("Expected: 1 blank line after; Actual: 0"));
            g = gEnd + 1;
        }
    }

    // MD901 — loose lists: a blank line between items flips the whole list
    // loose (see docs/gotchas.md "Tight vs Loose Lists").
    if (config.enabled(QStringLiteral("MD901"))) {
        for (const int line : c.m.looseListStarts)
            emitIssue(ctx, QStringLiteral("MD901"), line, 1, 0,
                 QStringLiteral("List is loose (blank line between items); remove the blank line to keep it tight"));
    }

    // MD035 — horizontal rule marker consistency
    if (config.enabled(QStringLiteral("MD035"))) {
        const QString style =
            config.param(QStringLiteral("MD035"), "style", QStringLiteral("consistent"))
                .toString();
        QChar consistent = QChar();
        for (const int line : c.m.hrLines) {
            const QString &raw = c.m.lines.at(line - 1);
            const QString trimmed = raw.trimmed();
            QChar marker = QChar();
            for (int k = 0; k < trimmed.size(); ++k) {
                const QChar ch = trimmed.at(k);
                if (ch == QLatin1Char('-') || ch == QLatin1Char('*') || ch == QLatin1Char('_')) {
                    marker = ch;
                    break;
                }
            }
            bool ok = false;
            if (style == QLatin1String("consistent")) {
                if (consistent.isNull())
                    consistent = marker;
                ok = marker == consistent;
            } else {
                const QChar want = style == QLatin1String("***")   ? QLatin1Char('*')
                                   : style == QLatin1String("___") ? QLatin1Char('_')
                                                                   : QLatin1Char('-');
                ok = marker == want;
            }
            if (!ok)
                emitIssue(ctx, QStringLiteral("MD035"), line, 1, raw.size(),
                     QStringLiteral("Expected: %1; Actual: %2").arg(style, trimmed));
        }
    }

    // MD047 — exactly one trailing newline at end of file
    if (config.enabled(QStringLiteral("MD047"))) {
        int n = 0;
        for (int k = text.size() - 1; k >= 0 && text.at(k) == QLatin1Char('\n'); --k)
            ++n;
        if (!text.isEmpty() && n != 1)
            emitIssue(ctx, QStringLiteral("MD047"), c.m.lines.size(), 1, 0,
                 QStringLiteral("Expected: 1 trailing newline; Actual: %1").arg(n));
    }

    // MD040 — fenced code blocks should declare a language
    if (config.enabled(QStringLiteral("MD040"))) {
        const QStringList allowed =
            config.param(QStringLiteral("MD040"), "allowed_languages", QStringList())
                .toStringList();
        const bool languageOnly =
            config.param(QStringLiteral("MD040"), "language_only", false).toBool();
        for (const auto &cb : c.m.codeBlocks) {
            if (cb.indented)
                continue;
            QString info = cb.lang.trimmed();
            if (languageOnly)
                info = info.section(QLatin1Char(' '), 0, 0);
            if (info.isEmpty()) {
                emitIssue(ctx, QStringLiteral("MD040"), cb.begLine, 1, 0,
                     QStringLiteral("Fenced code blocks should have a language specified"),
                     /*skipCode=*/false);
            } else if (!allowed.isEmpty()) {
                bool known = false;
                for (const auto &lang : allowed)
                    if (info.compare(lang, Qt::CaseInsensitive) == 0) {
                        known = true;
                        break;
                    }
                if (!known)
                    emitIssue(ctx, QStringLiteral("MD040"), cb.begLine, 1, 0,
                         QStringLiteral("Allowed languages: %1; Actual: %2")
                             .arg(allowed.join(QLatin1String(", ")), info),
                         /*skipCode=*/false);
            }
        }
    }

    // MD041 — first line of the file should be a heading
    if (config.enabled(QStringLiteral("MD041"))) {
        const int level = config.param(QStringLiteral("MD041"), "level", 1).toInt();
        const QString fmTitle =
            config.param(QStringLiteral("MD041"), "front_matter_title", QString()).toString();
        const auto fm = frontMatterRange(c.m);
        const int firstLine = fm.first > 0 ? fm.second + 1 : 1;
        const Heading *first = nullptr;
        for (const auto &h : c.m.headings)
            if (h.line >= firstLine) {
                first = &h;
                break;
            }
        bool exempt = false;
        if (!fmTitle.isEmpty() && fm.first > 0)
            for (int i = fm.first - 1; i < fm.second && i < c.m.lines.size(); ++i)
                if (c.m.lines.at(i).trimmed() == fmTitle)
                    exempt = true;
        if (!exempt && (!first || first->line != firstLine || first->level != level))
            emitIssue(ctx, QStringLiteral("MD041"), firstLine, 1, 0,
                 QStringLiteral("Expected: a heading at the beginning of the file"));
    }

    // MD043 — required heading structure
    if (config.enabled(QStringLiteral("MD043"))) {
        const QStringList required =
            config.param(QStringLiteral("MD043"), "headings", QStringList()).toStringList();
        const bool matchCase =
            config.param(QStringLiteral("MD043"), "match_case", false).toBool();
        struct Req { int level; QString text; };
        QVector<Req> wanted;
        for (const auto &entry : required) {
            const QString t = entry.trimmed();
            if (t.startsWith(QLatin1Char('#'))) {
                int lvl = 0;
                while (lvl < t.size() && t.at(lvl) == QLatin1Char('#'))
                    ++lvl;
                wanted.append({lvl, t.mid(lvl).trimmed()});
            } else {
                wanted.append({-1, t});
            }
        }
                for (int i = 0; i < wanted.size(); ++i) {
            const auto &want = wanted.at(i);
            if (i >= c.m.headings.size()) {
                // Doc ran out of headings — flag the last heading (or line 1).
                const auto &h = c.m.headings.isEmpty() ? Heading{} : c.m.headings.last();
                emitIssue(ctx, QStringLiteral("MD043"), h.line > 0 ? h.line : 1, 1, 0,
                     QStringLiteral("Required heading structure: Not in file"));
                break;
            }
            const auto &h = c.m.headings.at(i);
            const bool sameText = matchCase ? h.text.trimmed() == want.text
                                            : h.text.trimmed().compare(want.text,
                                                                       Qt::CaseInsensitive) == 0;
            if (want.level != -1 && h.level != want.level || !sameText)
                emitIssue(ctx, QStringLiteral("MD043"), h.line, 1, 0,
                     QStringLiteral("Required heading structure: Not in file"));
        }
    }

    // MD046 — code block style (fenced vs indented)
    if (config.enabled(QStringLiteral("MD046"))) {
        const QString style =
            config.param(QStringLiteral("MD046"), "style", QStringLiteral("consistent"))
                .toString();
        QString consistent;
        for (const auto &cb : c.m.codeBlocks) {
            const QString actual = cb.indented ? QStringLiteral("indented")
                                               : QStringLiteral("fenced");
            if (consistent.isEmpty())
                consistent = actual;
            const QString expected =
                style == QLatin1String("consistent") ? consistent : style;
            if (actual != expected)
                emitIssue(ctx, QStringLiteral("MD046"), cb.begLine, 1, 0,
                     QStringLiteral("Expected: %1; Actual: %2").arg(expected, actual),
                     /*skipCode=*/false);
        }
    }

    // MD048 — code fence character style
    if (config.enabled(QStringLiteral("MD048"))) {
        const QString style =
            config.param(QStringLiteral("MD048"), "style", QStringLiteral("consistent"))
                .toString();
        QChar consistent = QChar();
        for (const auto &cb : c.m.codeBlocks) {
            if (cb.indented)
                continue;
            if (consistent.isNull())
                consistent = cb.fenceChar;
            const QChar expected = style == QLatin1String("consistent")
                                       ? consistent
                                       : (style == QLatin1String("tilde")
                                              ? QLatin1Char('~')
                                              : QLatin1Char('`'));
            if (cb.fenceChar != expected)
                emitIssue(ctx, QStringLiteral("MD048"), cb.begLine, 1, 0,
                     QStringLiteral("Expected: %1; Actual: %2")
                         .arg(expected == QLatin1Char('~') ? QStringLiteral("tilde")
                                                           : QStringLiteral("backtick"),
                              cb.fenceChar == QLatin1Char('~') ? QStringLiteral("tilde")
                                                               : QStringLiteral("backtick")),
                     /*skipCode=*/false);
        }
    }

    // MD058 — blank lines around tables
    if (config.enabled(QStringLiteral("MD058"))) {
        for (const auto &t : c.m.tables) {
            if (t.begLine > 1 && !isFrontMatterLine(c.m, t.begLine - 1)
                && !isBlank(c.m.lines.at(t.begLine - 2)))
                emitIssue(ctx, QStringLiteral("MD058"), t.begLine, 1, 0,
                     QStringLiteral("Expected: 1 blank line before; Actual: 0"));
            if (t.endLine < c.m.lines.size() && !isBlank(c.m.lines.at(t.endLine)))
                emitIssue(ctx, QStringLiteral("MD058"), t.endLine, 1, 0,
                     QStringLiteral("Expected: 1 blank line after; Actual: 0"));
        }
    }

    // MD055 — table pipe style
    if (config.enabled(QStringLiteral("MD055"))) {
        const QString style = config
                                  .param(QStringLiteral("MD055"), "style",
                                         QStringLiteral("consistent"))
                                  .toString();
        for (const auto &t : c.m.tables) {
            bool haveRef = false;
            bool refLead = false, refTrail = false;
            for (int i = t.begLine; i <= t.endLine && i <= c.m.lines.size(); ++i) {
                const QString tr = c.m.lines.at(i - 1).trimmed();
                if (tr.isEmpty())
                    continue;
                const bool lead = tr.startsWith(QLatin1Char('|'));
                const bool trail = tr.endsWith(QLatin1Char('|'));
                if (!haveRef) {
                    haveRef = true;
                    refLead = lead;
                    refTrail = trail;
                }
                bool bad = false;
                if (style == QLatin1String("leading_and_trailing"))
                    bad = !(lead && trail);
                else if (style == QLatin1String("no_leading_or_trailing"))
                    bad = lead && trail;
                else
                    bad = lead != refLead || trail != refTrail;
                if (bad)
                    emitIssue(ctx, QStringLiteral("MD055"), i, 1, tr.size(),
                         QStringLiteral("Expected: %1; Actual: %2")
                             .arg(style == QLatin1String("leading_and_trailing")
                                      ? QStringLiteral("leading and trailing pipes")
                                      : style == QLatin1String("no_leading_or_trailing")
                                            ? QStringLiteral("no leading or trailing pipes")
                                            : pipeStyleDesc(refLead, refTrail),
                                  pipeStyleDesc(lead, trail)));
            }
        }
    }

    // MD056 — table column count
    if (config.enabled(QStringLiteral("MD056"))) {
        for (const auto &t : c.m.tables) {
            const int headerCount = rawColumnCount(c.m.lines.value(t.begLine - 1));
            for (int i = t.begLine + 1; i <= t.endLine && i <= c.m.lines.size(); ++i) {
                const QString &row = c.m.lines.at(i - 1);
                const int n = rawColumnCount(row);
                if (n != headerCount)
                    emitIssue(ctx, QStringLiteral("MD056"), i, 1, row.size(),
                         QStringLiteral("Expected: %1 columns; Actual: %2")
                             .arg(headerCount)
                             .arg(n));
            }
        }
    }

    // MD060 — table column style
    if (config.enabled(QStringLiteral("MD060"))) {
        const QString style = config
                                  .param(QStringLiteral("MD060"), "style",
                                         QStringLiteral("consistent"))
                                  .toString();
        for (const auto &t : c.m.tables) {
            QVector<QPair<bool, bool>> ref;   // per column: (left pad, right pad)
            for (int i = t.begLine; i <= t.endLine && i <= c.m.lines.size(); ++i) {
                const QString tr = c.m.lines.at(i - 1).trimmed();
                if (tr.isEmpty())
                    continue;
                QStringList cells = splitTableRow(tr);
                if (!cells.isEmpty() && cells.first().isEmpty())
                    cells.removeFirst();
                if (!cells.isEmpty() && cells.last().isEmpty())
                    cells.removeLast();
                if (ref.isEmpty()) {
                    for (const QString &cell : cells)
                        ref.append({cell.startsWith(QLatin1Char(' ')),
                                    cell.endsWith(QLatin1Char(' '))});
                    continue;
                }
                for (int ci = 0; ci < ref.size() && ci < cells.size(); ++ci) {
                    const bool l = cells.at(ci).startsWith(QLatin1Char(' '));
                    const bool r = cells.at(ci).endsWith(QLatin1Char(' '));
                    const bool wantL =
                        style == QLatin1String("leading_and_trailing")
                            ? true
                            : style == QLatin1String("no_leading_or_trailing")
                                  ? false
                                  : ref.at(ci).first;
                    const bool wantR =
                        style == QLatin1String("leading_and_trailing")
                            ? true
                            : style == QLatin1String("no_leading_or_trailing")
                                  ? false
                                  : ref.at(ci).second;
                    if (l != wantL || r != wantR) {
                        emitIssue(ctx, QStringLiteral("MD060"), i, 1, tr.size(),
                             QStringLiteral("Expected: %1; Actual: %2")
                                 .arg(pipeStyleDesc(wantL, wantR),
                                      pipeStyleDesc(l, r)));
                        break;
                    }
                }
            }
        }
    }

    // ---- reference scans (MD052/053) ------------------------------------------
    struct RefScan {
        QHash<QString, int> defLine;              // label -> 1-based line
        QVector<QPair<QString, int>> uses;        // (label, 1-based line)
    };
    static const QRegularExpression refDefRe(QStringLiteral(R"(^\s{0,3}(?:>\s*)*\[([^\]]+)\]:)"));
    static const QRegularExpression shortcutUseRe(
        QStringLiteral(R"((?<!\])(?<!\[)\[([^\]\n]+)\](?!\()(?!\[))"));
    static const QRegularExpression collapsedUseRe(QStringLiteral(R"(\[([^\]\n]+)\]\[\])"));
    static const QRegularExpression fullUseRe(QStringLiteral(R"(\[([^\]\n]+)\]\[([^\]\n]+)\])"));
    auto scanReferences = [&](const QString &ruleId) {
        const bool shortcut = config.param(ruleId, "shortcut_syntax", true).toBool();
        const bool collapsed = config.param(ruleId, "collapsed_syntax", true).toBool();
        RefScan out;
        for (int i = 0; i < c.m.lines.size(); ++i) {
            if (isCodeLine(c.m, i + 1))
                continue;
            const QString &line = c.m.lines.at(i);
            QVector<QPair<int, int>> codeRanges;
            auto cit = inlineCodeRe.globalMatch(line);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                codeRanges.append({cm.capturedStart(), cm.capturedLength()});
            }
            auto inCode = [&codeRanges](int pos) {
                for (const auto &r : codeRanges)
                    if (pos >= r.first && pos < r.first + r.second)
                        return true;
                return false;
            };
            const auto dm = refDefRe.match(line);
            if (dm.hasMatch() && !dm.captured(1).startsWith(QLatin1Char('^'))) {
                const QString label = dm.captured(1).trimmed();
                if (!label.isEmpty() && !out.defLine.contains(label))
                    out.defLine.insert(label, i + 1);
                continue;
            }
            if (shortcut) {
                auto it = shortcutUseRe.globalMatch(line);
                while (it.hasNext()) {
                    const auto um = it.next();
                    const QString label = um.captured(1).trimmed();
                    if (!label.isEmpty() && !label.startsWith(QLatin1Char('^'))
                        && !inCode(um.capturedStart(1)))
                        out.uses.append({label, i + 1});
                }
            }
            if (collapsed) {
                auto it = collapsedUseRe.globalMatch(line);
                while (it.hasNext()) {
                    const auto um = it.next();
                    const QString label = um.captured(1).trimmed();
                    if (!label.isEmpty() && !label.startsWith(QLatin1Char('^'))
                        && !inCode(um.capturedStart(1)))
                        out.uses.append({label, i + 1});
                }
            }
            {
                auto it = fullUseRe.globalMatch(line);
                while (it.hasNext()) {
                    const auto um = it.next();
                    const QString label = um.captured(2).trimmed();
                    if (!label.isEmpty() && !label.startsWith(QLatin1Char('^'))
                        && !inCode(um.capturedStart(2)))
                        out.uses.append({label, i + 1});
                }
            }
        }
        return out;
    };

    // --- span/link rules: MD033-039, MD042, MD044, MD045, MD049-054, MD059
    // MD033 — no inline HTML
    if (config.enabled(QStringLiteral("MD033"))) {
        const QStringList allowed =
            config.param(QStringLiteral("MD033"), "allowed_elements", QStringList())
                .toStringList();
        for (const auto &t : c.m.tokens) {
            if (t.type != MD_TEXT_HTML)
                continue;
            const QString text = rawSpan(c.m, t.beg, t.beg + t.len);
            QString elem;
            int k = 0;
            while (k < text.size()
                   && (text.at(k) == QLatin1Char('<') || text.at(k) == QLatin1Char('/')
                       || text.at(k).isSpace()))
                ++k;
            while (k < text.size()
                   && (text.at(k).isLetterOrNumber() || text.at(k) == QLatin1Char('-'))) {
                elem += text.at(k).toLower();
                ++k;
            }
            if (elem.isEmpty() || allowed.contains(elem))
                continue;
            emitIssue(ctx, QStringLiteral("MD033"), lineIndexOf(c.m, t.beg) + 1,
                 byteToCol(c.m, t.beg), t.len, QStringLiteral("Element: %1").arg(elem));
        }
    }

    // MD034 — bare URLs used in text
    if (config.enabled(QStringLiteral("MD034"))) {
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_A || !s.autolink || s.beg < 0)
                continue;
            // `<url>` autolinks have the angle bracket right before the text;
            // permissive autolinks (`url` bare) don't.
            if (s.beg > 0 && c.m.utf8.at(s.beg - 1) == '<')
                continue;
            const QString url = rawSpan(c.m, s.beg, s.end);
            emitIssue(ctx, QStringLiteral("MD034"), lineIndexOf(c.m, s.beg) + 1,
                 byteToCol(c.m, s.beg), s.end - s.beg,
                 QStringLiteral("Bare URL used: %1").arg(url));
        }
    }

    // MD036 — emphasis used instead of a heading (a paragraph consisting of a
    // single EM/STRONG span and nothing else)
    if (config.enabled(QStringLiteral("MD036"))) {
        for (const auto &p : c.m.paragraphs) {
            const Span *only = nullptr;
            for (const auto &s : c.m.spans)
                if (s.beg >= p.first && s.end <= p.second) {
                    if (only)
                        only = nullptr;   // more than one span
                    else if (only == nullptr)
                        only = &s;
                }
            if (!only || (only->type != MD_SPAN_EM && only->type != MD_SPAN_STRONG))
                continue;
            bool clean = true;
            for (const auto &t : c.m.tokens) {
                const int tEnd = t.beg + t.len;
                if (t.beg >= p.first && tEnd <= p.second
                    && !(t.beg >= only->beg && tEnd <= only->end)) {
                    clean = false;
                    break;
                }
            }
            if (!clean)
                continue;
            emitIssue(ctx, QStringLiteral("MD036"), lineIndexOf(c.m, only->beg) + 1,
                 byteToCol(c.m, only->beg), only->end - only->beg,
                 QStringLiteral("Emphasis used instead of a heading"));
        }
    }

    // MD037 — spaces inside emphasis markers
    if (config.enabled(QStringLiteral("MD037"))) {
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_EM && s.type != MD_SPAN_STRONG)
                continue;
            const auto src = spanSourceBytes(c.m, s, "*_");
            if (src.first < 0)
                continue;
            const QByteArray &u = c.m.utf8;
            int l = src.first;
            while (l < src.second && (u.at(l) == '*' || u.at(l) == '_'))
                ++l;
            int r = src.second;
            while (r > l && (u.at(r - 1) == '*' || u.at(r - 1) == '_'))
                --r;
            if (l >= r)
                continue;
            int bad = -1;
            if (u.at(l) == QLatin1Char(' ') || u.at(l) == QLatin1Char('\t'))
                bad = l;
            else if (u.at(r - 1) == QLatin1Char(' ') || u.at(r - 1) == QLatin1Char('\t'))
                bad = r - 1;
            if (bad >= 0)
                emitIssue(ctx, QStringLiteral("MD037"), lineIndexOf(c.m, bad) + 1,
                     byteToCol(c.m, bad), 1,
                     QStringLiteral("Expected: 0 spaces; Actual: 1"));
        }
        // Raw fallback: md4c does not parse emphasis with a space next to a
        // marker (CommonMark), so pair up marker runs in the source text
        // (mirrors markdownlint's bare-token pairing).
        static const QRegularExpression markerRunRe(QStringLiteral("[*_]{1,3}"));
        for (int i = 0; i < c.m.lines.size(); ++i) {
            const int line = i + 1;
            if (isCodeLine(c.m, line))
                continue;
            const QString &raw = c.m.lines.at(i);
            QVector<QPair<int, int>> runs;
            QVector<QPair<int, int>> codeSpans;
            auto cit = inlineCodeRe.globalMatch(raw);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                codeSpans.append({cm.capturedStart(), cm.capturedEnd()});
            }
            auto mit = markerRunRe.globalMatch(raw);
            while (mit.hasNext()) {
                const auto m2 = mit.next();
                bool same = true;
                for (int k = 1; k < m2.capturedLength(); ++k)
                    if (m2.captured(0).at(k) != m2.captured(0).at(0))
                        same = false;
                if (!same)
                    continue;
                const int r0 = m2.capturedStart();
                bool inCode = false;
                for (const auto &cs : codeSpans)
                    if (r0 >= cs.first && r0 < cs.second) {
                        inCode = true;
                        break;
                    }
                if (!inCode)
                    runs.append({r0, m2.capturedLength()});
            }
            int k2 = 0;
            while (k2 + 1 < runs.size()) {
                const int c0 = raw.at(runs[k2].first).toLatin1();
                if (c0 != raw.at(runs[k2 + 1].first).toLatin1()) {
                    ++k2;
                    continue;
                }
                const int sEnd = runs[k2].first + runs[k2].second;
                const int ePos = runs[k2 + 1].first;
                if (sEnd + 1 < raw.size() && raw.at(sEnd).isSpace()
                    && !raw.at(sEnd + 1).isSpace())
                    emitIssue(ctx, QStringLiteral("MD037"), line, sEnd + 1, 1,
                         QStringLiteral("Expected: 0 spaces; Actual: 1"));
                else if (ePos >= 2 && raw.at(ePos - 1).isSpace()
                         && !raw.at(ePos - 2).isSpace())
                    emitIssue(ctx, QStringLiteral("MD037"), line, ePos, 1,
                         QStringLiteral("Expected: 0 spaces; Actual: 1"));
                k2 += 2;
            }
        }
    }

    // MD038 — spaces inside code spans
    if (config.enabled(QStringLiteral("MD038"))) {
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_CODE)
                continue;
            const auto src = spanSourceBytes(c.m, s, "`");
            if (src.first < 0)
                continue;
            const QByteArray &u = c.m.utf8;
            // md4c strips one space from each side of the content (CommonMark
            // code-span rule), so expand past markers AND spaces to reach the
            // padding; only spaces sitting between marker and content count.
            int l = src.first;
            while (l > 0 && (u.at(l - 1) == '`' || u.at(l - 1) == ' '))
                --l;
            int r = src.second;
            while (r < u.size() && (u.at(r) == '`' || u.at(r) == ' '))
                ++r;
            while (l < r && u.at(l) == '`')
                ++l;
            int padL = 0;
            while (l < r && u.at(l) == ' ') {
                ++padL;
                ++l;
            }
            while (r > l && u.at(r - 1) == '`')
                --r;
            int padR = 0;
            while (r > l && u.at(r - 1) == ' ') {
                ++padR;
                --r;
            }
            if (padL > 0 || padR > 0)
                emitIssue(ctx, QStringLiteral("MD038"), lineIndexOf(c.m, src.first) + 1,
                     byteToCol(c.m, padL > 0 ? l - padL : r), 1,
                     QStringLiteral("Expected: 0 spaces; Actual: 1"));
        }
    }

    // MD039 — spaces inside link text
    if (config.enabled(QStringLiteral("MD039"))) {
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_A || s.autolink)
                continue;
            const auto src = spanSourceBytes(c.m, s, "[]");
            if (src.first < 0)
                continue;
            const QByteArray &u = c.m.utf8;
            int l = src.first;
            while (l < src.second && u.at(l) == '[')
                ++l;
            int r = src.second;
            while (r > l && u.at(r - 1) == ']')
                --r;
            if (l >= r)
                continue;
            int bad = -1;
            if (u.at(l) == ' ' || u.at(l) == '\t')
                bad = l;
            else if (u.at(r - 1) == ' ' || u.at(r - 1) == '\t')
                bad = r - 1;
            if (bad >= 0)
                emitIssue(ctx, QStringLiteral("MD039"), lineIndexOf(c.m, bad) + 1,
                     byteToCol(c.m, bad), 1,
                     QStringLiteral("Expected: 0 spaces; Actual: 1"));
        }
    }

    // MD042 — empty link text
    if (config.enabled(QStringLiteral("MD042"))) {
        static const QRegularExpression emptyLinkRe(
            QStringLiteral(R"(\[([^\]\\]|\\.)*\]\(\s*(?:<[^<>]*>|(?:[^)\\]|\\.)*)\s*\))"));
        for (int i = 0; i < c.m.lines.size(); ++i) {
            const int line = i + 1;
            if (isCodeLine(c.m, line))
                continue;
            const QString &raw = c.m.lines.at(i);
            QVector<QPair<int, int>> codeRanges;
            auto cit = inlineCodeRe.globalMatch(raw);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                codeRanges.append({cm.capturedStart(), cm.capturedLength()});
            }
            auto it = emptyLinkRe.globalMatch(raw);
            while (it.hasNext()) {
                const auto m2 = it.next();
                bool inCode = false;
                for (const auto &r : codeRanges)
                    if (m2.capturedStart() >= r.first && m2.capturedStart() < r.first + r.second)
                        inCode = true;
                if (inCode)
                    continue;
                bool empty = true;
                for (int k = m2.capturedStart(1); k < m2.capturedEnd(1); ++k)
                    if (!raw.at(k).isSpace()) {
                        empty = false;
                        break;
                    }
                if (empty)
                    emitIssue(ctx, QStringLiteral("MD042"), line,
                         static_cast<int>(m2.capturedStart() + 1),
                         static_cast<int>(m2.capturedLength()),
                         QStringLiteral("Empty link"));
            }
        }
    }

    // MD044 — proper names
    if (config.enabled(QStringLiteral("MD044"))) {
        const QStringList names =
            config.param(QStringLiteral("MD044"), "names", QStringList()).toStringList();
        const bool codeBlocks =
            config.param(QStringLiteral("MD044"), "code_blocks", true).toBool();
        const bool htmlElements =
            config.param(QStringLiteral("MD044"), "html_elements", true).toBool();
        auto inHtmlBlock = [&c](int line /*1-based*/) {
            for (const auto &h : c.m.htmlBlocks)
                if (line >= h.first && line <= h.second)
                    return true;
            return false;
        };
        for (int i = 0; i < c.m.lines.size(); ++i) {
            const int line = i + 1;
            if (codeBlocks && isCodeLine(c.m, line))
                continue;
            if (htmlElements && inHtmlBlock(line))
                continue;
            QString scan = c.m.lines.at(i);
            if (codeBlocks) {
                auto cit = inlineCodeRe.globalMatch(scan);
                while (cit.hasNext()) {
                    const auto cm = cit.next();
                    for (int k = cm.capturedStart(); k < cm.capturedEnd(); ++k)
                        scan[k] = QLatin1Char(' ');
                }
            }
            for (const auto &name : names) {
                const QRegularExpression re(
                    QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(name)),
                    QRegularExpression::CaseInsensitiveOption);
                auto it = re.globalMatch(scan);
                while (it.hasNext()) {
                    const auto m2 = it.next();
                    if (m2.captured(0) == name)
                        continue;
                    emitIssue(ctx, QStringLiteral("MD044"), line,
                         static_cast<int>(m2.capturedStart() + 1),
                         static_cast<int>(m2.capturedLength()),
                         QStringLiteral("Expected: %1; Actual: %2").arg(name, m2.captured(0)));
                }
            }
        }
    }

    // MD045 — images should have alternate text
    if (config.enabled(QStringLiteral("MD045"))) {
        for (const int pos : c.m.altLessImgs)
            emitIssue(ctx, QStringLiteral("MD045"), lineIndexOf(c.m, pos) + 1,
                 byteToCol(c.m, pos), 2,
                 QStringLiteral("Images should have alternate text"));
    }

    // MD049/MD050 — emphasis / strong marker style
    if (config.enabled(QStringLiteral("MD049"))
        || config.enabled(QStringLiteral("MD050"))) {
        QChar firstEm, firstStrong;
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_EM && s.type != MD_SPAN_STRONG)
                continue;
            const auto src = spanSourceBytes(c.m, s, "*_");
            if (src.first < 0)
                continue;
            const QChar marker = QChar(c.m.utf8.at(src.first));
            const QString ruleId = s.type == MD_SPAN_EM ? QStringLiteral("MD049")
                                                        : QStringLiteral("MD050");
            if (!config.enabled(ruleId))
                continue;
            QChar &first = s.type == MD_SPAN_EM ? firstEm : firstStrong;
            if (first.isNull())
                first = marker;
            const QString style =
                config.param(ruleId, "style", QStringLiteral("consistent")).toString();
            QChar expected;
            if (style == QLatin1String("asterisk"))
                expected = QLatin1Char('*');
            else if (style == QLatin1String("underscore"))
                expected = QLatin1Char('_');
            else
                expected = first;
            if (marker != expected)
                emitIssue(ctx, ruleId, lineIndexOf(c.m, src.first) + 1,
                     byteToCol(c.m, src.first), 1,
                     QStringLiteral("Expected: %1; Actual: %2").arg(expected, marker));
        }
    }

    // MD051 — link fragments should point to an existing heading
    if (config.enabled(QStringLiteral("MD051"))) {
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_A || !s.href.startsWith(QLatin1Char('#')) || s.beg < 0)
                continue;
            const QString frag = s.href.mid(1);
            if (frag.isEmpty() || c.m.headingSlugs.contains(frag))
                continue;
            emitIssue(ctx, QStringLiteral("MD051"), lineIndexOf(c.m, s.beg) + 1,
                 byteToCol(c.m, s.beg), s.end - s.beg,
                 QStringLiteral("Link fragment not found: #%1").arg(frag));
        }
    }

    // MD052 — missing reference links/images
    if (config.enabled(QStringLiteral("MD052"))) {
        const RefScan scan = scanReferences(QStringLiteral("MD052"));
        for (const auto &use : scan.uses)
            if (!scan.defLine.contains(use.first))
                emitIssue(ctx, QStringLiteral("MD052"), use.second, 1, 0,
                     QStringLiteral("Missing reference: %1").arg(use.first));
    }

    // MD053 — unused link/image reference definitions
    if (config.enabled(QStringLiteral("MD053"))) {
        const RefScan scan = scanReferences(QStringLiteral("MD053"));
        for (auto it = scan.defLine.constBegin(); it != scan.defLine.constEnd(); ++it) {
            bool used = false;
            for (const auto &use : scan.uses)
                if (use.first == it.key()) {
                    used = true;
                    break;
                }
            if (!used)
                emitIssue(ctx, QStringLiteral("MD053"), it.value(), 1, 0,
                     QStringLiteral("Definition is not used: %1").arg(it.key()));
        }
    }

    // MD054 — link/image style
    if (config.enabled(QStringLiteral("MD054"))) {
        const QString style =
            config.param(QStringLiteral("MD054"), "style", QStringLiteral("consistent"))
                .toString();
        QString consistent;
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_A && s.type != MD_SPAN_IMG)
                continue;
            if (s.beg < 0)
                continue;
            QString actual;
            const auto src = spanSourceBytes(c.m, s, "[]");
            if (s.autolink)
                actual = QStringLiteral("autolink");
            else if (src.first >= 0) {
                const int after = src.second;
                if (after < c.m.utf8.size()) {
                    const char ch = c.m.utf8.at(after);
                    if (ch == '(')
                        actual = QStringLiteral("inlined");
                    else if (ch == '[')
                        actual = QStringLiteral("full");
                    else if (ch == ']')
                        actual = QStringLiteral("collapsed");
                }
                if (actual.isEmpty())
                    actual = QStringLiteral("shortcut");
            }
            if (actual.isEmpty())
                continue;
            if (consistent.isEmpty())
                consistent = actual;
            const QString expected =
                style == QLatin1String("consistent") ? consistent : style;
            if (actual != expected)
                emitIssue(ctx, QStringLiteral("MD054"), lineIndexOf(c.m, s.beg) + 1,
                     byteToCol(c.m, s.beg), s.end - s.beg,
                     QStringLiteral("Expected: %1; Actual: %2").arg(expected, actual));
        }
        // Raw fallback: md4c leaves unresolved shortcut references as plain
        // text, so scan the source for bracket groups that are not part of a
        // parsed link and classify them as shortcut style.
        static const QRegularExpression bracketGroupRe(
            QStringLiteral(R"((?<!\])(?<!\[)\[[^\]\n]+\](?!\()(?!\[))"));
        QVector<QPair<int, int>> spanSources;
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_A && s.type != MD_SPAN_IMG)
                continue;
            if (s.beg < 0)
                continue;
            const auto src = spanSourceBytes(c.m, s, "[]");
            if (src.first >= 0)
                spanSources.append(src);
        }
        for (int i = 0; i < c.m.lines.size(); ++i) {
            const QString &raw = c.m.lines.at(i);
            auto bit = bracketGroupRe.globalMatch(raw);
            while (bit.hasNext()) {
                const auto bm = bit.next();
                const int b0 = bm.capturedStart() + c.m.lineStarts.value(i);
                const int b1 = bm.capturedEnd() + c.m.lineStarts.value(i);
                bool inSpan = false;
                for (const auto &sr : spanSources)
                    if (b0 >= sr.first && b1 <= sr.second) {
                        inSpan = true;
                        break;
                    }
                if (inSpan)
                    continue;
                if (consistent.isEmpty())
                    consistent = QStringLiteral("shortcut");
                const QString expected =
                    style == QLatin1String("consistent") ? consistent : style;
                if (expected != QLatin1String("shortcut"))
                    emitIssue(ctx, QStringLiteral("MD054"), i + 1,
                         bm.capturedStart() + 1, bm.capturedLength(),
                         QStringLiteral("Expected: %1; Actual: %2")
                             .arg(expected, QStringLiteral("shortcut")));
            }
        }
    }

    // MD059 — descriptive link text
    if (config.enabled(QStringLiteral("MD059"))) {
        const QString test =
            config.param(QStringLiteral("MD059"), "test", QStringLiteral("markdownlint"))
                .toString();
        static const QStringList weak{QStringLiteral("here"), QStringLiteral("click here"),
                                      QStringLiteral("link"), QStringLiteral("this"),
                                      QStringLiteral("this link"), QStringLiteral("this page")};
        static const QStringList weakGithub{QStringLiteral("click"), QStringLiteral("here"),
                                            QStringLiteral("more"), QStringLiteral("read more")};
        for (const auto &s : c.m.spans) {
            if (s.type != MD_SPAN_A || s.beg < 0)
                continue;
            const QString text = rawSpan(c.m, s.beg, s.end).trimmed();
            bool hit = weak.contains(text, Qt::CaseInsensitive);
            if (!hit && test == QLatin1String("markdownlint-github"))
                hit = weakGithub.contains(text, Qt::CaseInsensitive);
            if (hit)
                emitIssue(ctx, QStringLiteral("MD059"), lineIndexOf(c.m, s.beg) + 1,
                     byteToCol(c.m, s.beg), s.end - s.beg,
                     QStringLiteral("Link text is not descriptive"));
        }
    }

    // ---- inline config directives (post-filter) -------------------------------
    // All other `<!-- markdownlint-* -->` directives filter the emitted issues.
    // Rules never see them; this is a pure post-pass.
    {
        static const QRegularExpression directiveRe(
            QStringLiteral(R"(<!--\s*markdownlint-(\w[\w-]*)((?:\s+[\w.-]+)*)\s*-->)"));
        struct Directive {
            int line;
            QString kind;
            QStringList rules;   // empty = all rules
        };
        QVector<Directive> directives;
        for (int i = 0; i < c.m.lines.size(); ++i) {
            auto mit = directiveRe.globalMatch(c.m.lines.at(i));
            while (mit.hasNext()) {
                const auto dm = mit.next();
                const QString kind = dm.captured(1).toLower();
                if (kind == QLatin1String("configure-file"))
                    continue;   // handled above
                QStringList rules;
                const QString body = dm.captured(2);
                if (!body.trimmed().isEmpty())
                    rules = body.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                directives.append({i + 1, kind, rules});
            }
        }
        auto resolveRules = [](const QStringList &names) {
            QSet<QString> ids;
            for (const QString &name : names)
                if (const MdLintRule *r = MdLintRules::byKey(name))
                    ids.insert(r->id);
            return ids;
        };
        auto allRuleIds = [] {
            QSet<QString> ids;
            for (const auto &r : MdLintRules::all())
                ids.insert(r.id);
            return ids;
        };
        QSet<QString> enabledSet = allRuleIds();
        auto applyRules = [&](const QStringList &names, bool enable) {
            if (names.isEmpty()) {
                enabledSet = enable ? allRuleIds() : QSet<QString>{};
                return;
            }
            const QSet<QString> ids = resolveRules(names);
            if (enable)
                enabledSet.unite(ids);
            else
                enabledSet.subtract(ids);
        };
        // File-wide directives first, in document order.
        for (const auto &d : directives) {
            if (d.kind == QLatin1String("disable-file"))
                applyRules(d.rules, false);
            else if (d.kind == QLatin1String("enable-file"))
                applyRules(d.rules, true);
        }
        // Per-line walk; directives take effect on their own line.
        QVector<QPair<int, QSet<QString>>> states;
        states.append({0, enabledSet});
        QStack<QSet<QString>> snapshots;
        QHash<int, QSet<QString>> lineDisables;     // disable-line: line -> rules
        QHash<int, QSet<QString>> nextLineDisables; // disable-next-line: line -> rules
        int di = 0;
        for (int line = 1; line <= c.m.lines.size(); ++line) {
            while (di < directives.size() && directives.at(di).line == line) {
                const auto &d = directives.at(di);
                if (d.kind == QLatin1String("disable"))
                    applyRules(d.rules, false);
                else if (d.kind == QLatin1String("enable"))
                    applyRules(d.rules, true);
                else if (d.kind == QLatin1String("capture"))
                    snapshots.push(enabledSet);
                else if (d.kind == QLatin1String("restore") && !snapshots.isEmpty())
                    enabledSet = snapshots.pop();
                else if (d.kind == QLatin1String("disable-line"))
                    lineDisables.insert(line, resolveRules(d.rules));
                else if (d.kind == QLatin1String("disable-next-line"))
                    nextLineDisables.insert(line, resolveRules(d.rules));
                ++di;
            }
            states.append({line, enabledSet});
        }
        // Filter issues against the per-line state and line-targeted directives.
        QVector<MdLintIssue> kept;
        kept.reserve(ctx.issues.size());
        for (const auto &issue : ctx.issues) {
            auto it = std::upper_bound(
                states.begin(), states.end(), issue.line,
                [](int l, const QPair<int, QSet<QString>> &s) { return l < s.first; });
            --it;
            if (!it->second.contains(issue.rule))
                continue;
            const auto ld = lineDisables.constFind(issue.line);
            if (ld != lineDisables.constEnd()
                && (ld->isEmpty() || ld->contains(issue.rule)))
                continue;
            const auto nd = nextLineDisables.constFind(issue.line - 1);
            if (nd != nextLineDisables.constEnd()
                && (nd->isEmpty() || nd->contains(issue.rule)))
                continue;
            kept.append(issue);
        }
        ctx.issues = std::move(kept);
    }

    std::sort(ctx.issues.begin(), ctx.issues.end(), [](const MdLintIssue &a, const MdLintIssue &b) {
        if (a.line != b.line)
            return a.line < b.line;
        if (a.col != b.col)
            return a.col < b.col;
        return a.rule < b.rule;
    });
    return ctx.issues;
}

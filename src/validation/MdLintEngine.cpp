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
#include <QRegularExpression>
#include <QSet>
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
    QVector<QPair<int, int>> quotes;   // (begLine, endLine) 1-based (raw scan)
    QVector<QPair<int, int>> htmlBlocks; // (begLine, endLine) 1-based
    QVector<Token> tokens;
    QVector<Span> spans;
    QSet<QString> footnoteLabelsUsed;  // labels of MD_SPAN_FOOTNOTE_REF
    QSet<QString> headingSlugs;        // LinkValidator::headingSlug of each heading text
    int lastByte = 0;                  // total input size
};

// ---- offset <-> line/col mapping ------------------------------------------

int lineIndexOf(const Model &m, int byteOff)
{
    const auto it = std::upper_bound(m.lineStarts.begin(), m.lineStarts.end(), byteOff);
    return static_cast<int>(it - m.lineStarts.begin()) - 1;
}

// 1-based character column for a byte offset on the given (0-based) line.
int charColAt(const QString &line, int byteCol)
{
    const QByteArray lineUtf8 = line.toUtf8();
    const int clamp = qMin(byteCol, lineUtf8.size());
    return QString::fromUtf8(lineUtf8.left(clamp)).size() + 1;
}

// Raw source text between two byte offsets.
QString rawSpan(const Model &m, int begByte, int endByte)
{
    return QString::fromUtf8(m.utf8.mid(begByte, qMax(0, endByte - begByte)));
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

    // List nesting depth (incremented on UL/OL enter).
    int listDepth = 0;
    // Current list marks, one entry per depth level.
    QVector<QPair<QChar, QChar>> listMarks;   // (mark, olDelim)

    void onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size)
    {
        const int beg = static_cast<int>(text - m.utf8.constData());
        Token tok{type, beg, static_cast<int>(size)};
        m.tokens.append(tok);
        const int end = beg + static_cast<int>(size);
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
            listMarks.resize(listDepth + 1);
            listMarks[listDepth] = {QChar(d->mark), QChar()};
            ++listDepth;
        } else if (type == MD_BLOCK_OL) {
            auto *d = static_cast<MD_BLOCK_OL_DETAIL *>(detail);
            listMarks.resize(listDepth + 1);
            listMarks[listDepth] = {QChar(), QChar(d->mark_delimiter)};
            ++listDepth;
        } else if (type == MD_BLOCK_LI) {
            auto *d = static_cast<MD_BLOCK_LI_DETAIL *>(detail);
            const int line = lineIndexOf(m, begByte) + 1;
            const QPair<QChar, QChar> marks =
                listMarks.isEmpty() ? QPair<QChar, QChar>{QLatin1Char('-'), QChar()}
                                    : listMarks.value(listDepth - 1);
            const QString raw = m.lines.value(line - 1);
            int contentCol = 1;
            // contentCol: first non-space char after the marker run.
            int i = 0;
            while (i < raw.size() && (raw.at(i) == QLatin1Char(' ') || raw.at(i) == QLatin1Char('\t')))
                ++i;
            if (i < raw.size() && raw.at(i) == marks.first)
                ++i;   // bullet
            else if (i < raw.size() && raw.at(i).isDigit())
                while (i < raw.size() && raw.at(i).isDigit())
                    ++i;
            while (i < raw.size() && (raw.at(i) == QLatin1Char(' ') || raw.at(i) == QLatin1Char('\t')))
                ++i;
            contentCol = i + 1;
            m.listItems.append({line, qMax(0, listDepth - 1), marks.first, marks.second,
                                charColAt(raw, 0) + static_cast<int>(
                                    raw.indexOf(marks.first, 0) < 0 && marks.first.isNull()
                                        ? qMax(0, raw.indexOf(marks.second, 0))
                                        : qMax(0, raw.indexOf(marks.first, 0))),
                                contentCol});
        }
        blocks.append(b);
    }

    void leaveBlock(MD_BLOCKTYPE type, void *detail)
    {
        const OpenBlock b = blocks.takeLast();
        if (type == MD_BLOCK_UL || type == MD_BLOCK_OL)
            --listDepth;

        if (type == MD_BLOCK_H) {
            Heading h;
            h.line = lineIndexOf(m, b.begByte) + 1;
            h.level = detail ? static_cast<MD_BLOCK_H_DETAIL *>(detail)->level : 1;
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
            cb.fenceLen = 0;   // filled in Task 5 (raw fence run)
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
            t.endLine = lineIndexOf(m, b.lastEnd) + 1;
            m.tables.append(t);
        } else if (type == MD_BLOCK_HTML) {
            if (b.begByte >= 0)
                m.htmlBlocks.append({lineIndexOf(m, b.begByte) + 1,
                                     lineIndexOf(m, b.lastEnd) + 1});
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
        if (s.type == type)
            m.spans.append(s.span);
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
    return {1, m.lines.size()};   // unterminated front matter swallows the file
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

// ---- issue helpers --------------------------------------------------------

struct RuleCtx {
    const Model &m;
    const MdLintConfig &cfg;
    QVector<MdLintIssue> issues;
};

// Appends one issue, honoring enable/severity; skips lines in front matter
// (callers opt back in per rule) and, by default, code lines (the legacy
// MarkdownChecker only scanned checkable lines — parity).
void emitIssue(RuleCtx &ctx, const QString &ruleId, int line /*1-based*/, int col,
          int length, const QString &detail, bool skipCode = true)
{
    const MdLintRule *rule = MdLintRules::byKey(ruleId);
    if (!rule || !ctx.cfg.enabled(ruleId))
        return;
    if (isFrontMatterLine(ctx.m, line))
        return;
    if (skipCode && isCodeLine(ctx.m, line))
        return;
    ctx.issues.append({ruleId, rule->alias, rule->description,
                       ctx.cfg.severity(ruleId), line, col, length, detail});
}

} // namespace

QVector<MdLintIssue> MdLintEngine::lint(const QString &text, const MdLintConfig &config)
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

    // ---- rules ------------------------------------------------------------
    RuleCtx ctx{c.m, config, {}};

    // --- raw-line rules: MD009, MD012, MD013, MD018, MD900 (parity set) ----
    static const QRegularExpression noSpaceHashRe(QStringLiteral(R"(^\#{1,6}+\S)"));
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

        // Code and front-matter lines are invisible to every parity check
        // (legacy MarkdownChecker's `checkable` rule).
        if (isCodeLine(c.m, line) || isFrontMatterLine(c.m, line)) {
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
                const int n = raw.size() - ws;
                // br_spaces: allow up to N trailing spaces (non-strict).
                const bool strict = config.param(QStringLiteral("MD009"), "strict", false).toBool();
                const int brSpaces = config.param(QStringLiteral("MD009"), "br_spaces", 2).toInt();
                if (strict || n > brSpaces)
                    emitIssue(ctx, QStringLiteral("MD009"), line, ws + 1, n,
                         QStringLiteral("Trailing spaces: %1").arg(n));
            }
        }

        // MD013 — line length
        if (config.enabled(QStringLiteral("MD013")) && raw.size() > kMaxLineLength)
            emitIssue(ctx, QStringLiteral("MD013"), line, kMaxLineLength + 1,
                 raw.size() - kMaxLineLength,
                 QStringLiteral("Line length: %1").arg(raw.size()));

        // MD018 — `#` heading without a space
        const auto noSpace = noSpaceHashRe.match(raw);
        if (noSpace.hasMatch())
            emitIssue(ctx, QStringLiteral("MD018"), line, 1, noSpace.capturedLength(),
                 QStringLiteral("Expected: 1 space; Actual: 0"));

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
    if (config.enabled(QStringLiteral("MD900"))) {
        // Usages resolved by md4c (footnoteLabelsUsed) are fine; a definition
        // collected above satisfies a usage too; anything else is unmatched.
        for (const auto &u : footnoteUses) {
            if (!c.m.footnoteLabelsUsed.contains(u.label) && !footnoteDefs.contains(u.label))
                emitIssue(ctx, QStringLiteral("MD900"), u.line, u.col, u.len,
                     QStringLiteral("Unmatched footnote reference: [^%1]").arg(u.label));
        }
    }

    // --- model rules: MD001, MD024 (parity set) ----------------------------
    int prevLevel = 0;
    for (const auto &h : c.m.headings) {
        if (config.enabled(QStringLiteral("MD001")) && prevLevel > 0 && h.level > prevLevel + 1)
            emitIssue(ctx, QStringLiteral("MD001"), h.line, 1, 0,
                 QStringLiteral("Expected: %1; Actual: %2").arg(prevLevel + 1).arg(h.level));
        prevLevel = h.level;
    }
    if (config.enabled(QStringLiteral("MD024"))) {
        QSet<QString> seen;
        for (const auto &h : c.m.headings) {
            const QString slug = LinkValidator::headingSlug(h.text);
            if (slug.isEmpty())
                continue;
            if (seen.contains(slug))
                emitIssue(ctx, QStringLiteral("MD024"), h.line, 1, 0,
                          QStringLiteral("Duplicate heading: %1").arg(h.text.trimmed()));
            else
                seen.insert(slug);
        }
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

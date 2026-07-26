#include "StaticHelpers.h"
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <QFile>
#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>

QString escapeJsString(const QString &s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\r", "\\r");
    return r;
}

int extractContentWidth(const QString &css)
{
    QRegularExpression mwRe(R"(\bbody\s*\{[^}]*\bmax-width\s*:\s*(\d+)\s*px)");
    auto mw = mwRe.match(css);
    int maxW = mw.hasMatch() ? mw.captured(1).toInt() : 800;

    auto px = [&](const QString &p) -> int {
        QRegularExpression re(R"(\bbody\s*\{[^}]*\b)" + p + R"(\s*:\s*(\d+)\s*px)");
        auto m = re.match(css);
        return m.hasMatch() ? m.captured(1).toInt() : 0;
    };
    int pl = px("padding-left"), pr = px("padding-right");
    if (pl || pr) return maxW + pl + pr;

    QRegularExpression padRe(R"(\bbody\s*\{[^}]*\bpadding\s*:\s*(\d+)\s*px)");
    auto pad = padRe.match(css);
    if (pad.hasMatch()) return maxW + pad.captured(1).toInt() * 2;

    return maxW + 40;
}

static const QRegularExpression &listUnorderedRe()
{
    static QRegularExpression re(R"(^(\s*)([-*+])\s?)");
    return re;
}

static const QRegularExpression &listOrderedRe()
{
    static QRegularExpression re(R"(^(\s*)(\d+)\.\s?)");
    return re;
}

QString handleListReturn(const QString &line)
{
    static const QChar clearSentinel(0x2412);
    static QRegularExpression taskRe(R"(^(\s*)([-*+])\s+\[[ xX]\]\s?)");
    auto taskMatch = taskRe.match(line);
    if (taskMatch.hasMatch()) {
        QString rest = line.mid(taskMatch.capturedEnd()).trimmed();
        if (rest.isEmpty())
            return QString(clearSentinel);
        return taskMatch.captured(1) + taskMatch.captured(2) + " [ ] ";
    }
    auto match = listUnorderedRe().match(line);
    if (match.hasMatch()) {
        QString rest = line.mid(match.capturedEnd()).trimmed();
        if (rest.isEmpty())
            return QString(clearSentinel);
        return match.captured(1) + match.captured(2) + " ";
    }
    match = listOrderedRe().match(line);
    if (match.hasMatch()) {
        QString rest = line.mid(match.capturedEnd()).trimmed();
        if (rest.isEmpty())
            return QString(clearSentinel);
        int next = match.captured(2).toInt() + 1;
        return match.captured(1) + QString::number(next) + ". ";
    }
    return {};
}

static int listIndentWidth(const QString &line)
{
    int spaces = 0;
    for (QChar c : line) {
        if (c == ' ') ++spaces;
        else break;
    }
    return spaces;
}

QString handleTableReturn(const QString &line, const QString &prevLine)
{
    if (line.startsWith('|')) {
        // Separator row → no continuation
        if (line.contains("---"))
            return {};

        int cols = line.count('|') - 1;
        if (cols <= 0) return {};

        // Blank row → exit table
        QStringList cells = line.split('|');
        bool allEmpty = true;
        for (int i = 1; i < cells.size(); ++i) {
            if (!cells[i].trimmed().isEmpty()) {
                allEmpty = false;
                break;
            }
        }
        if (allEmpty) {
            static const QChar clearSentinel(0x2412);
            return QString(clearSentinel);
        }

        // Previous line is also a table row → data row continuation
        if (prevLine.startsWith('|'))
            return makeEmptyTableRow(cols);

        // First row of a new table → separator + data row
        return "|" + QString("---|").repeated(cols) + "\n" + makeEmptyTableRow(cols);
    }

    if (line.contains("<tr>") && line.contains("<td>")) {
        int cols = line.count("<td>");
        if (cols <= 0) return {};

        // Blank HTML row → exit autocomplete
        {
            QXmlStreamReader xml(line);
            bool allEmpty = true;
            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isCharacters() && !xml.isWhitespace()) {
                    allEmpty = false;
                    break;
                }
            }
            if (allEmpty) {
                static const QChar clearSentinel(0x2412);
                return QString(clearSentinel);
            }
        }

        return makeEmptyHtmlTableRow(cols);
    }

    return {};
}

QString makeEmptyTableRow(int cols)
{
    return "|" + QString("  |").repeated(cols);
}

QString makeEmptyHtmlTableRow(int cols)
{
    return "<tr>" + QString("<td></td>").repeated(cols) + "</tr>";
}

/* Returns block-relative cursor position of next/previous cell in a markdown table row.
   - forward=true:  find the next cell after cursorPos
   - forward=false: find the previous cell before cursorPos
   Returns -1 if at first cell (going backward) or last cell (going forward),
   signaling the caller should navigate to another row. */
int tableNavCell(const QString &line, int cursorPos, bool forward)
{
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|') pipes.append(i);

    if (pipes.size() < 2)
        return -1;

    if (forward) {
        int idx = -1;
        for (int i = 0; i < pipes.size(); ++i) {
            if (pipes[i] > cursorPos) { idx = i; break; }
        }
        if (idx == -1 || idx >= pipes.size() - 1)
            return -1;  // at last cell
        int cellPos = pipes[idx] + 1;
        if (cellPos < line.size() && line[cellPos] == ' ')
            ++cellPos;
        return cellPos;
    } else {
        int idx = -1;
        for (int i = pipes.size() - 1; i >= 0; --i) {
            if (pipes[i] < cursorPos) { idx = i; break; }
        }
        if (idx <= 0)
            return -1;  // at first cell
        int cellPos = pipes[idx - 1] + 1;
        if (cellPos < line.size() && line[cellPos] == ' ')
            ++cellPos;
        return cellPos;
    }
}

int tableNavHtmlCell(const QString &line, int cursorPos, bool forward)
{
    QList<int> tdStarts;
    int idx = 0;
    while ((idx = line.indexOf("<td>", idx)) >= 0) {
        tdStarts.append(idx);
        idx += 4;
    }
    if (tdStarts.isEmpty())
        return -1;

    if (forward) {
        for (int i = 0; i < tdStarts.size(); ++i) {
            int contentStart = tdStarts[i] + 4;
            if (contentStart > cursorPos)
                return contentStart;
        }
        return -1;
    } else {
        for (int i = tdStarts.size() - 1; i >= 0; --i) {
            int contentStart = tdStarts[i] + 4;
            if (contentStart < cursorPos)
                return contentStart;
        }
        return -1;
    }
}

QString indentListLine(const QString &line)
{
    auto match = listUnorderedRe().match(line);
    if (!match.hasMatch())
        match = listOrderedRe().match(line);
    if (!match.hasMatch())
        return line;
    int indent = listIndentWidth(line);
    return line.left(indent) + "  " + line.mid(indent);
}

QString outdentListLine(const QString &line)
{
    auto match = listUnorderedRe().match(line);
    if (!match.hasMatch())
        match = listOrderedRe().match(line);
    if (!match.hasMatch())
        return line;
    int indent = listIndentWidth(line);
    int remove = qMin(indent, 2);
    return line.mid(remove);
}

QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column)
{
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    return cursor;
}

QString firstFontFamily(const QString &cssFontStack)
{
    QString first = cssFontStack.section(QLatin1Char(','), 0, 0).trimmed();
    if (first.startsWith(QLatin1Char('\'')) || first.startsWith(QLatin1Char('"'))) {
        int end = first.indexOf(first[0], 1);
        if (end > 1)
            first = first.mid(1, end - 1);
    }
    return first;
}

int countSentences(const QString &text)
{
    if (text.isEmpty()) return 1;
    int count = 0;
    for (int i = 0; i < text.size(); ++i) {
        QChar c = text[i];
        if (c == '!' || c == '?') {
            ++count;
        } else if (c == '.') {
            if (i == text.size() - 1) {
                ++count;
            } else {
                int wordStart = i - 1;
                while (wordStart >= 0 && text[wordStart] != ' ')
                    --wordStart;
                int wordLen = i - wordStart - 1;
                if (wordLen > 2)
                    ++count;
            }
        }
    }
    return qMax(1, count);
}

int estimateSyllables(const QString &word)
{
    if (word.isEmpty()) return 0;
    static const QSet<QChar> vowels = {'a','e','i','o','u','y'};
    QString lower = word.toLower();
    int count = 0;
    bool inGroup = false;
    for (int i = 0; i < lower.size(); ++i) {
        bool isVowel = vowels.contains(lower[i]);
        if (isVowel && !inGroup) {
            ++count;
            inGroup = true;
        } else if (!isVowel) {
            inGroup = false;
        }
    }
    if (lower.endsWith('e') && !lower.endsWith("le") && count > 1)
        --count;
    return qMax(1, count);
}

double fleschKincaidGrade(int words, int sentences, int syllables)
{
    if (words == 0 || sentences == 0) return 0.0;
    return 0.39 * (static_cast<double>(words) / sentences)
         + 11.8 * (static_cast<double>(syllables) / words)
         - 15.59;
}

QIcon themedIcon(const QString &svgPath, const QColor &color, int size)
{
    QFile f(svgPath);
    if (!f.open(QIODevice::ReadOnly))
        return QIcon(svgPath);
    QString svg = QString::fromUtf8(f.readAll());
    svg.replace("currentColor", color.name());
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    renderer.render(&painter);
    painter.end();
    return QIcon(pix);
}

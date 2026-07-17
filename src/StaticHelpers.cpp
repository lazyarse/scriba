#include "StaticHelpers.h"
#include <QRegularExpression>

QString escapeJsString(const QString &s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n");
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

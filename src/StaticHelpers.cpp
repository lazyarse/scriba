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
#include "StaticHelpers.h"
#include <algorithm>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QAbstractButton>
#include <QPair>
#include <QVector>

QString escapeJsString(const QString &s)
{
    QString r = s;
    r.replace("\\", "\\\\").replace("'", "\\'").replace("\"", "\\\"").replace("`", "\\`").replace("\n", "\\n").replace("\r", "\\r");
    return r;
}

bool isSafePreviewImage(const QString &filePath)
{
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile())
        return false;
    static const QStringList imageSuffixes{
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
        QStringLiteral("svg"), QStringLiteral("avif"), QStringLiteral("ico"),
        QStringLiteral("tif"), QStringLiteral("tiff"),
    };
    return imageSuffixes.contains(fi.suffix().toLower());
}

static const QRegularExpression &unorderedListRe()
{
    static QRegularExpression re(R"(^(\s*)([-*+])\s?)");
    return re;
}

static const QRegularExpression &orderedListRe()
{
    static QRegularExpression re(R"(^(\s*)(\d+)\.\s?)");
    return re;
}

bool isThematicBreak(const QString &line)
{
    static QRegularExpression re(R"(^\s*([-*_])(?:\s*\1){2,}\s*$)");
    return re.match(line).hasMatch();
}

QString handleListReturn(const QString &line)
{
    if (isThematicBreak(line))
        return {};
    static QRegularExpression taskRe(R"(^(\s*)([-*+])\s+\[[ xX]\]\s?)");
    auto taskMatch = taskRe.match(line);
    if (taskMatch.hasMatch()) {
        QString rest = line.mid(taskMatch.capturedEnd()).trimmed();
        if (rest.isEmpty())
            return QString(clearSentinel);
        return taskMatch.captured(1) + taskMatch.captured(2) + " [ ] ";
    }
    auto match = unorderedListRe().match(line);
    if (match.hasMatch()) {
        QString rest = line.mid(match.capturedEnd()).trimmed();
        if (rest.isEmpty())
            return QString(clearSentinel);
        return match.captured(1) + match.captured(2) + " ";
    }
    match = orderedListRe().match(line);
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
    if (isThematicBreak(line))
        return line;
    auto match = unorderedListRe().match(line);
    if (!match.hasMatch())
        match = orderedListRe().match(line);
    if (!match.hasMatch())
        return line;
    int indent = listIndentWidth(line);
    return line.left(indent) + "  " + line.mid(indent);
}

QString outdentListLine(const QString &line)
{
    if (isThematicBreak(line))
        return line;
    auto match = unorderedListRe().match(line);
    if (!match.hasMatch())
        match = orderedListRe().match(line);
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

void stripButtonIcon(QAbstractButton *btn)
{
    btn->setIcon(QIcon());
}

void stripButtonIcons(QDialogButtonBox *box)
{
    for (auto *btn : box->buttons())
        stripButtonIcon(btn);
}

QString readResourceFile(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return {};
}

QString duplicateCssFile(const QString &sourcePath, const QString &destDir, const QString &baseName)
{
    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString css = QString::fromUtf8(src.readAll());
    if (css.isEmpty())
        return {};

    QDir().mkpath(destDir);
    QString name = baseName.trimmed();
    if (name.isEmpty())
        name = QFileInfo(sourcePath).completeBaseName() + "-copy";
    name.replace(QRegularExpression("[\\\\/:*?\"<>|\\x00-\\x1f]"), "-");
    if (!name.endsWith(".css", Qt::CaseInsensitive))
        name += ".css";

    QString candidate = destDir + "/" + name;
    if (QFileInfo::exists(candidate))
        return {};

    QFile out(candidate);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    out.write(css.toUtf8());
    return candidate;
}

DebounceTimer::DebounceTimer(int interval, QObject *parent)
    : QTimer(parent)
{
    setSingleShot(true);
    setInterval(interval);
}

void DebounceTimer::arm()
{
    start();
}

bool extractLinkPath(const QString &line, int cursorPos, QString &partialPath)
{
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(cursorPos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return false;

    QString between = line.mid(parenPos, cursorPos - parenPos);
    if (between.contains(')'))
        return false;

    partialPath = between;
    return true;
}

int linkPathReplaceStart(const QString &line, int cursorPos)
{
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(cursorPos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return -1;

    QString between = line.mid(parenPos, cursorPos - parenPos);
    int lastSlash = between.lastIndexOf('/');
    return lastSlash >= 0 ? parenPos + lastSlash + 1 : parenPos;
}

bool extractHtmlPath(const QString &line, int cursorPos, QString &value)
{
    static const QRegularExpression re(R"((?:src|href)\s*=\s*["']([^"']*)$)");
    auto match = re.match(line.left(cursorPos));
    if (!match.hasMatch())
        return false;

    value = match.captured(1);
    return true;
}

int htmlPathReplaceStart(const QString &line, int cursorPos)
{
    static const QRegularExpression re(R"((?:src|href)\s*=\s*["']([^"']*)$)");
    auto match = re.match(line.left(cursorPos));
    if (!match.hasMatch())
        return -1;

    QString value = match.captured(1);
    int valueStart = cursorPos - value.length();
    int lastSlash = value.lastIndexOf('/');
    return lastSlash >= 0 ? valueStart + lastSlash + 1 : valueStart;
}

bool extractEmojiCode(const QString &line, int cursorPos, QString &partialCode)
{
    // Inside an open link path the file completion wins, not emoji
    QString ignored;
    if (extractLinkPath(line, cursorPos, ignored))
        return false;

    static const QRegularExpression re(R"(:([a-zA-Z0-9+-][a-zA-Z0-9_+-]*)$)");
    auto match = re.match(line.left(cursorPos));
    if (!match.hasMatch())
        return false;

    partialCode = match.captured(1);
    return true;
}

QString autoCorrectWord(const QString &line, int cursorPos, const QStringList &pairs,
                        bool separatorTyped, int *wordStart, int *wordLength)
{
    if (wordStart)
        *wordStart = -1;
    if (wordLength)
        *wordLength = 0;
    if (pairs.isEmpty() || cursorPos <= 0 || cursorPos > line.size())
        return {};

    // A word (apostrophes/hyphens included), optionally followed by the
    // separator(s) just typed. When separatorTyped, at least one trailing
    // non-letter must be present so an in-progress word is never corrected.
    static const QRegularExpression wordRe(R"(([A-Za-z]+(?:['-][A-Za-z]+)*)([^A-Za-z]*)$)");
    auto match = wordRe.match(line.left(cursorPos));
    if (!match.hasMatch())
        return {};
    const QString typed = match.captured(1);
    const QString trailing = match.captured(2);
    if (separatorTyped && trailing.isEmpty())
        return {};

    const int start = match.capturedStart(1);
    if (start > 0) {
        const QChar prev = line[start - 1];
        // Must not be glued to a larger token: letters/digits, or characters
        // that appear inside URLs, emails, paths, identifiers or code spans.
        if (prev.isLetterOrNumber() || prev == '@' || prev == '.'
            || prev == '/' || prev == '_' || prev == '-' || prev == '`' || prev == '\\')
            return {};
    }

    QString replacement;
    const QString lower = typed.toLower();
    for (const QString &entry : pairs) {
        const int eq = entry.indexOf('=');
        if (eq <= 0 || eq >= entry.size() - 1)
            continue;
        if (entry.left(eq).toLower() != lower)
            continue;
        replacement = entry.mid(eq + 1);
        break;
    }
    if (replacement.isEmpty() || replacement == typed)
        return {};

    // Preserve the case of what was typed.
    bool allUpper = typed.size() > 1;
    for (QChar c : typed) {
        if (c.isLower()) {
            allUpper = false;
            break;
        }
    }
    if (allUpper)
        replacement = replacement.toUpper();
    else if (typed[0].isUpper())
        replacement[0] = replacement[0].toUpper();

    if (wordStart)
        *wordStart = start;
    if (wordLength)
        *wordLength = typed.size();
    return replacement;
}

// Sequential (fuzzy) match: every character of the fragment appears in the
// entry in order, possibly with skipped characters in between. Subsumes plain
// substring containment, so "scrsvg" matches "scriba.svg". The score ranks
// matches: fewer skipped characters and an earlier first-match position make a
// match feel tighter (a prefix match scores 0/0 and always ranks first).
struct FileMatchScore
{
    bool matched = false;
    int gaps = 0;      // characters skipped between matched fragment chars
    int firstPos = 0;  // position of the first matched character
};

static FileMatchScore scoreFileMatch(const QString &entry, const QString &fragment)
{
    if (fragment.isEmpty())
        return {true, 0, 0};
    int f = 0;
    int prev = -1;
    int gaps = 0;
    int first = -1;
    for (int i = 0; i < entry.size() && f < fragment.size(); ++i) {
        if (entry[i].toLower() == fragment[f].toLower()) {
            if (prev >= 0)
                gaps += i - prev - 1;
            else
                first = i;
            prev = i;
            ++f;
        }
    }
    if (f < fragment.size())
        return {false, 0, 0};
    return {true, gaps, first};
}

FileCompletionResult matchFileEntries(const QString &partialPath, const QDir &baseDir,
                                      int limit)
{
    QString normalized = partialPath;
    bool trailingSep = normalized.endsWith('/') || normalized.endsWith('\\');
    if (trailingSep)
        normalized.chop(1);
    QFileInfo pfi(normalized);
    QString dirPart = trailingSep ? pfi.filePath() : pfi.path();
    QString filePart = trailingSep ? QString() : pfi.fileName();

    QString searchDir = baseDir.absoluteFilePath(dirPart.isEmpty() ? "." : dirPart);
    QDir search(searchDir);
    if (!search.exists())
        return {};

    QVector<QPair<QString, FileMatchScore>> scored;
    QStringList all = search.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const QString &entry : all) {
        if (entry.startsWith('.') && !filePart.startsWith('.'))
            continue;
        FileMatchScore score = scoreFileMatch(entry, filePart);
        if (score.matched)
            scored.append({entry, score});
    }

    std::sort(scored.begin(), scored.end(),
        [](const QPair<QString, FileMatchScore> &a, const QPair<QString, FileMatchScore> &b) {
            if (a.second.gaps != b.second.gaps)
                return a.second.gaps < b.second.gaps;
            if (a.second.firstPos != b.second.firstPos)
                return a.second.firstPos < b.second.firstPos;
            return a.first < b.first;
        });

    QStringList entries;
    for (const auto &match : scored) {
        if (entries.size() >= limit)
            break;
        const QString &entry = match.first;
        if (QFileInfo(search, entry).isDir())
            entries.append(entry + "/");
        else
            entries.append(entry);
    }

    return {entries, filePart};
}



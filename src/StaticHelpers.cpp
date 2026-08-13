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
#include "Preferences.h"
#include <algorithm>
#include <QRegularExpression>
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
#include <QSet>
#include <QSettings>
#include <QFont>
#include <QRect>

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
    static QRegularExpression re(R"(^(\s*)([-*+])(?=\s|$))");
    return re;
}

static const QRegularExpression &orderedListRe()
{
    static QRegularExpression re(R"(^(\s*)(\d+)([.)])(?=\s|$))");
    return re;
}

bool isThematicBreak(const QString &line)
{
    static QRegularExpression re(R"(^\s*([-*_])(?:\s*\1){2,}\s*$)");
    return re.match(line).hasMatch();
}

// Returns the list-marker continuation prefix for a markdown list item line
// (e.g. "  - ", "2. ", "- [ ] "), or an empty QString when `line` isn't a list
// item or is a thematic break. Ordered lists are incremented; task checkboxes
// are reset to unchecked.
static QString listMarkerPrefix(const QString &line)
{
    if (isThematicBreak(line))
        return {};
    static QRegularExpression taskRe(R"(^(\s*)([-*+])\s+\[[ xX]\]\s?)");
    auto taskMatch = taskRe.match(line);
    if (taskMatch.hasMatch())
        return taskMatch.captured(1) + taskMatch.captured(2) + " [ ] ";
    auto match = unorderedListRe().match(line);
    if (match.hasMatch())
        return match.captured(1) + match.captured(2) + " ";
    match = orderedListRe().match(line);
    if (match.hasMatch())
        return match.captured(1) + QString::number(match.captured(2).toInt() + 1)
               + match.captured(3) + " ";
    return {};
}

QString handleListReturn(const QString &line)
{
    const QString prefix = listMarkerPrefix(line);
    if (prefix.isEmpty())
        return {};
    if (line.mid(prefix.length()).trimmed().isEmpty())
        return QString(clearSentinel);
    return prefix;
}

QString handleListSplitReturn(const QString &line, int caretPos)
{
    const QString prefix = listMarkerPrefix(line);
    if (prefix.isEmpty())
        return {};
    if (caretPos < prefix.length())
        return {};
    if (line.mid(caretPos).trimmed().isEmpty())
        return {};
    return prefix;
}

// Continuation prefix for an indented or '>' blockquote line: the leading
// whitespace run plus any '>' blockquote markers (e.g. "    ", "> ", "    > ",
// "> > "), normalized to a single trailing space after the last marker.
// Empty when the line starts with neither whitespace nor a blockquote marker
// (plain text, list items, table rows, thematic breaks — those are handled
// elsewhere).
static QString indentationPrefix(const QString &line)
{
    int i = 0;
    const int n = line.size();
    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        ++i;
    static const QRegularExpression quotePrefixRe(R"(^(?:[ ]{0,3}>[ \t]?)+)");
    const QRegularExpressionMatch m = quotePrefixRe.match(line.mid(i));
    if (!m.hasMatch())
        return i > 0 ? line.left(i) : QString();
    QString prefix = line.left(i) + m.captured(0);
    if (!prefix.isEmpty() && !prefix.back().isSpace())
        prefix += ' ';
    return prefix;
}

// Returns the continuation prefix to place on the new line when Enter is
// pressed at the end of an indented or blockquote line, keeping the caret
// inside the blockquote/indented block. Empty for non-indented plain lines
// (lists/tables/thematic breaks are handled elsewhere), and clearSentinel
// when the line is only that prefix — Enter then clears it, the "second
// Enter exits" behavior shared with lists and tables.
QString handleIndentReturn(const QString &line)
{
    const QString prefix = indentationPrefix(line);
    if (prefix.isEmpty())
        return {};
    if (line.mid(prefix.length()).trimmed().isEmpty())
        return QString(clearSentinel);
    return prefix;
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

// How many spaces Tab adds / Shift+Tab removes when indenting a list item,
// matching the item's content column so the result nests correctly under
// CommonMark: 2 for bullets ("- ", "+ ", "* "), digits+delimiter+1 for
// ordered ("1." → 3, "10." → 4, "1)" → 3), 0 for non-list lines.
static int listIndentStep(const QString &line)
{
    auto match = unorderedListRe().match(line);
    if (match.hasMatch())
        return 2;
    match = orderedListRe().match(line);
    if (match.hasMatch())
        return match.captured(2).length() + match.captured(3).length() + 1;
    return 0;
}


QString indentListLine(const QString &line)
{
    if (isThematicBreak(line))
        return line;
    const int step = listIndentStep(line);
    if (step == 0)
        return line;
    int indent = listIndentWidth(line);
    return line.left(indent) + QString(step, ' ') + line.mid(indent);
}

QString outdentListLine(const QString &line)
{
    if (isThematicBreak(line))
        return line;
    const int step = listIndentStep(line);
    if (step == 0)
        return line;
    int indent = listIndentWidth(line);
    int remove = qMin(indent, step);
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

void stripButtonIcons(const QList<QAbstractButton *> &buttons)
{
    for (auto *btn : buttons)
        stripButtonIcon(btn);
}

QString readResourceFile(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return {};
}

QList<EmojiEntry> emojiCatalog()
{
    static QList<EmojiEntry> catalog;
    if (!catalog.isEmpty())
        return catalog;

    static const QRegularExpression re(R"('([a-z0-9_+\-]+)'\s*:\s*'([^']+)')");
    auto it = re.globalMatch(readResourceFile(":/emoji.js"));
    QSet<QString> seen;
    while (it.hasNext()) {
        auto match = it.next();
        EmojiEntry entry;
        entry.shortcode = match.captured(1);
        entry.unicode = match.captured(2);

        QStringList parts;
        for (uint cp : entry.unicode.toUcs4())
            parts.append(QString::number(cp, 16));
        QStringList stripped = parts;
        stripped.removeAll("fe0f");
        QString strippedStr = stripped.join("-");
        entry.codePoint = QFile::exists(QString(":/twemoji/svg/%1.svg").arg(strippedStr))
            ? strippedStr : parts.join("-");

        if (seen.contains(entry.shortcode))
            continue;
        seen.insert(entry.shortcode);
        catalog.append(entry);
    }
    std::sort(catalog.begin(), catalog.end(),
              [](const EmojiEntry &a, const EmojiEntry &b) { return a.shortcode < b.shortcode; });
    return catalog;
}

QString emojiTwemojiPath(const QString &unicode)
{
    QStringList parts;
    for (uint cp : unicode.toUcs4())
        parts.append(QString::number(cp, 16));
    QStringList stripped = parts;
    stripped.removeAll("fe0f");
    for (const QString &candidate : {stripped.join("-"), parts.join("-")}) {
        QString path = QString(":/twemoji/svg/%1.svg").arg(candidate);
        if (QFile::exists(path))
            return path;
    }
    return {};
}

QPixmap renderEmojiPixmap(const QString &unicode, int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    bool color = Preferences::emojiRenderingFromString(
        QSettings().value(Preferences::EmojiMode,
                          Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString())
        == Preferences::EmojiRendering::Color;

    if (color) {
        QString svgPath = emojiTwemojiPath(unicode);
        if (!svgPath.isEmpty()) {
            QSvgRenderer renderer(svgPath);
            renderer.render(&painter, QRectF(0, 0, size, size));
        } else {
            QFont font("Symbola");
            font.setPixelSize(size - 2);
            painter.setFont(font);
            painter.setPen(Qt::black);
            painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, unicode);
        }
    } else {
        QFont font("Symbola");
        font.setPixelSize(size - 2);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, unicode);
    }

    return pix;
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
FuzzyScore fuzzyMatchScore(const QString &entry, const QString &fragment)
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

    QVector<QPair<QString, FuzzyScore>> scored;
    QStringList all = search.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const QString &entry : all) {
        if (entry.startsWith('.') && !filePart.startsWith('.'))
            continue;
        FuzzyScore score = fuzzyMatchScore(entry, filePart);
        if (score.matched)
            scored.append({entry, score});
    }

    std::sort(scored.begin(), scored.end(),
        [](const QPair<QString, FuzzyScore> &a, const QPair<QString, FuzzyScore> &b) {
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



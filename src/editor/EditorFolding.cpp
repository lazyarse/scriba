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
#include "Editor.h"
#include "Gutter.h"
#include "prefs/Preferences.h"
#include "StaticHelpers.h"
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QSettings>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>
#include <climits>

namespace {


// Normalized view of a line for list folding: quoteDepth counts leading '>'
// blockquote markers, indent is the content column after stripping them, and
// isList is true when a list marker (bullet, ordered, or task) begins the
// content. Used by both the fold scan and the list fold-range walk.
struct ListFoldInfo
{
    int quoteDepth = 0;
    int indent = 0;
    QString content;
    bool isList = false;
};

ListFoldInfo listFoldInfo(const QString &line)
{
    ListFoldInfo info;
    QString rem = line;
    static const QRegularExpression quotePrefixRe(R"(^(?:[ ]{0,3}>[ \t]?)+)");
    QRegularExpressionMatch m = quotePrefixRe.match(rem);
    if (m.hasMatch()) {
        info.quoteDepth = m.captured(0).count('>');
        rem = rem.mid(m.capturedLength());
    }
    int i = 0;
    while (i < rem.size()) {
        const QChar c = rem[i];
        if (c == ' ') {
            ++info.indent;
            ++i;
        } else if (c == '\t') {
            info.indent += 4;
            ++i;
        } else {
            break;
        }
    }
    info.content = rem.mid(i);
    static const QRegularExpression listMarkerRe(R"(^[-*+](?=\s|$)|\d+[.)](?=\s|$))");
    info.isList = !isThematicBreak(info.content) && listMarkerRe.match(info.content).hasMatch();
    return info;
}


} // namespace

void Editor::updateGutter()
{
    if (!m_gutter)
        return;
    int gutterW = m_gutter->width();
    m_gutter->setGeometry(0, 0, gutterW, viewport()->height() + height() - viewport()->height());
    if (m_underlineOverlay) {
        m_underlineOverlay->setGeometry(0, 0, viewport()->width(), viewport()->height());
        m_underlineOverlay->update();
    }
}

void Editor::updateGutterSettings()
{
    if (!m_gutter)
        return;
    QSettings s;
    m_gutter->setLineNumbersVisible(s.value(Preferences::ShowLineNumbers, true).toBool());
    applyGutterColors();
    updateGutterWidth();
}

void Editor::refreshGutter()
{
    if (m_gutter)
        m_gutter->update();
}

void Editor::updateGutterDelayed()
{
    QTimer::singleShot(0, this, &Editor::updateGutter);
}

void Editor::setupGutter()
{
    m_gutter = new Gutter(this);
    connect(m_gutter, &Gutter::foldToggled, this, &Editor::toggleFold);
    connect(m_gutter, &Gutter::chartEditRequested, this, &Editor::chartEditRequested);
    connect(this, &Editor::cursorPositionChanged, this, [this]() {
        if (m_gutter)
            m_gutter->update();
    });
    connect(document(), &QTextDocument::blockCountChanged, this, &Editor::updateGutterWidth);
    applyGutterColors();
    updateGutter();
    updateGutterWidth();
    scanHeadersAndFolds();
}

void Editor::updateGutterWidth()
{
    if (!m_gutter)
        return;
    m_gutter->updateWidth();
    updateViewportMargins();
    updateGutterDelayed();
}

void Editor::applyGutterColors()
{
    if (!m_gutter)
        return;
    QSettings s;
    bool colorOverride = s.value(Preferences::GutterColorOverride, false).toBool();
    QString css;
    if (colorOverride) {
        QString bg = s.value(Preferences::GutterBgColor, "#f0f0f0").toString();
        QString fg = s.value(Preferences::GutterTextColor, "#888888").toString();
        css = QString("background-color: %1; color: %2;").arg(bg, fg);
    } else {
        css.clear();
    }
    m_gutter->setStyleSheet(css);
}

void Editor::scanHeadersAndFolds()
{
    m_headerLevel.clear();
    m_codeFences.clear();
    m_chartFences.clear();
    m_mdTableSeparators.clear();
    m_htmlTables.clear();
    m_listItems.clear();

    QTextBlock block = document()->firstBlock();
    int codeDepth = 0;
    QRegularExpression atxRe("^(#{1,6})\\s");
    QRegularExpression setextUnderlineRe("^(={3,}|-{3,})\\s*$");
    QRegularExpression mdTableSepRe(
        "^\\s*\\|?\\s*:?-+:?\\s*(\\|\\s*:?-+:?\\s*)+\\|?\\s*$");

    // First pass: detect headers and fenced-code openings
    QTextBlock prevBlock;
    while (block.isValid()) {
        QString text = block.text();
        int bn = block.blockNumber();

        if (text.trimmed().startsWith("```")) {
            if (codeDepth == 0) {
                m_codeFences.insert(bn);
                // A chart block is a fence whose language is mermaid or ec;
                // the gutter pencil makes it editable without leaving the editor.
                const QString lang = text.trimmed().mid(3).trimmed()
                                         .section(QRegularExpression("[\\s{].*"), 0, 0)
                                         .toLower();
                if (lang == QLatin1String("mermaid") || lang == QLatin1String("ec"))
                    m_chartFences.insert(bn);
            }
            codeDepth ^= 1;
            prevBlock = block;
            block = block.next();
            continue;
        }

        if (codeDepth == 0) {
            auto atxMatch = atxRe.match(text);
            if (atxMatch.hasMatch()) {
                int level = atxMatch.captured(1).size();
                m_headerLevel[bn] = level;
                prevBlock = block;
                block = block.next();
                continue;
            }

            // Setext: check if PREVIOUS line was not a header and THIS line is === or ---
            if (prevBlock.isValid() && !m_headerLevel.contains(prevBlock.blockNumber())) {
                auto setextMatch = setextUnderlineRe.match(text.trimmed());
                if (setextMatch.hasMatch()) {
                    QChar ch = setextMatch.captured(1).at(0);
                    int level = (ch == '=') ? 1 : 2;
                    m_headerLevel[prevBlock.blockNumber()] = level;
                }
            }

            // Markdown table separator row (the fold anchor). Must be a line of
            // dashes/colons delimited by pipes, preceded by a non-blank header
            // row (so it is really a table delimiter, not a stray line).
            if (mdTableSepRe.match(text).hasMatch()
                && prevBlock.isValid() && !prevBlock.text().trimmed().isEmpty()) {
                m_mdTableSeparators.insert(bn);
            }

            // HTML table opening line: foldable only when multi-line, i.e. the
            // closing </table> is NOT on the same line.
            const QString t = text.trimmed();
            if (t.startsWith("<table") && !t.contains("</table>"))
                m_htmlTables.insert(bn);

            // List item marker line (bullet, ordered, task): a fold anchor whose
            // range runs to the next sibling/shallower item or the list end.
            // Leaf items whose fold would hide nothing get no anchor at all.
            if (listFoldInfo(text).isList && listAnchorHasContent(bn))
                m_listItems.insert(bn);
        }

        prevBlock = block;
        block = block.next();
    }

    // Update gutter with foldable info
    QSet<int> foldableSet;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it)
        foldableSet.insert(it.key());
    foldableSet.unite(m_codeFences);
    foldableSet.unite(m_mdTableSeparators);
    foldableSet.unite(m_htmlTables);
    foldableSet.unite(m_listItems);
    if (m_gutter) {
        m_gutter->setFoldableBlocks(foldableSet);
        m_gutter->setChartBlocks(m_chartFences);
    }

    // Re-apply folds
    m_updatingFolds = true;
    QSet<int> stillValid;
    for (int bn : m_foldedBlocks) {
        if (isFoldableBlock(bn)) {
            applyFold(bn, true);
            stillValid.insert(bn);
        }
    }
    m_foldedBlocks = stillValid;
    for (auto it = m_foldEndPins.begin(); it != m_foldEndPins.end();) {
        if ((!m_headerLevel.contains(it.key()) && !m_listItems.contains(it.key()))
            || !document()->findBlock(it.value()).isValid())
            it = m_foldEndPins.erase(it);
        else
            ++it;
    }
    if (m_gutter)
        m_gutter->setFoldedBlocks(m_foldedBlocks);
    m_updatingFolds = false;
}

bool Editor::isFoldableBlock(int blockNumber) const
{
    return m_headerLevel.contains(blockNumber) || m_codeFences.contains(blockNumber)
        || m_mdTableSeparators.contains(blockNumber) || m_htmlTables.contains(blockNumber)
        || m_listItems.contains(blockNumber);
}

bool Editor::foldRegionContains(int startBlock, int blockNumber) const
{
    return startBlock <= blockNumber && blockNumber < foldEnd(startBlock);
}

void Editor::applyFold(int startBlock, bool hide)
{
    int end = foldEnd(startBlock);
    QTextBlock block = document()->findBlockByNumber(startBlock + 1);
    while (block.isValid() && block.blockNumber() < end) {
        block.setVisible(!hide);
        block = block.next();
    }
}

int Editor::foldEnd(int startBlock) const
{
    if (m_codeFences.contains(startBlock)) {
        QTextBlock block = document()->findBlockByNumber(startBlock + 1);
        while (block.isValid()) {
            if (block.text().trimmed().startsWith("```"))
                return block.blockNumber() + 1;
            block = block.next();
        }
        return document()->blockCount();
    }

    // Markdown table: the separator row is the anchor; the table continues over
    // each following non-blank, non-block-start line (mirrors the GFM table
    // termination md4c now implements: the fold stops at the same boundaries
    // that end a rendered table).
    if (m_mdTableSeparators.contains(startBlock)) {
        QTextBlock block = document()->findBlockByNumber(startBlock + 1);
        static const QRegularExpression blockStartRe(
            "^\\s*(?:(?:#{1,6})\\s|```|~~~|[>\\-+*]\\s|\\d+[.)]\\s|</?[a-zA-Z#]|<!--|---+\\s*$|\\*{3,}\\s*$|_{3,}\\s*$)");
        while (block.isValid()) {
            const QString t = block.text();
            if (t.trimmed().isEmpty()
                || blockStartRe.match(t).hasMatch()
                || m_headerLevel.contains(block.blockNumber())
                || m_codeFences.contains(block.blockNumber())
                || m_htmlTables.contains(block.blockNumber()))
            {
                return block.blockNumber();
            }
            block = block.next();
        }
        return document()->blockCount();
    }

    // HTML table: fold to the matching </table> (if any), else to EOF.
    if (m_htmlTables.contains(startBlock)) {
        QTextBlock block = document()->findBlockByNumber(startBlock + 1);
        while (block.isValid()) {
            if (block.text().contains("</table>"))
                return block.blockNumber() + 1;
            block = block.next();
        }
        return document()->blockCount();
    }

    if (m_listItems.contains(startBlock)) {
        int end = listFoldEnd(startBlock);
        auto pinIt = m_foldEndPins.find(startBlock);
        if (pinIt != m_foldEndPins.end()) {
            QTextBlock pinned = document()->findBlock(pinIt.value());
            if (pinned.isValid() && pinned.blockNumber() > startBlock && pinned.blockNumber() <= end)
                end = pinned.blockNumber();
        }
        return end;
    }

    int end = sectionEndBlock(startBlock, m_headerLevel.value(startBlock));
    auto pinIt = m_foldEndPins.find(startBlock);
    if (pinIt != m_foldEndPins.end()) {
        QTextBlock pinned = document()->findBlock(pinIt.value());
        if (pinned.isValid() && pinned.blockNumber() > startBlock && pinned.blockNumber() <= end)
            end = pinned.blockNumber();
    }
    return end;
}

bool Editor::listItemIsFirst(int startBlock, int anchorQuoteDepth, int anchorIndent) const
{
    QTextBlock block = document()->findBlockByNumber(startBlock).previous();
    while (block.isValid()) {
        const QString text = block.text();
        if (text.trimmed().isEmpty()) {
            block = block.previous();
            continue;
        }
        const ListFoldInfo info = listFoldInfo(text);
        if (info.quoteDepth < anchorQuoteDepth)
            break;
        if (info.isList && info.quoteDepth == anchorQuoteDepth && info.indent <= anchorIndent)
            return false;
        if (!info.isList)
            break;
        block = block.previous();
    }
    return true;
}

bool Editor::listAnchorHasContent(int blockNumber) const
{
    const int end = listFoldEnd(blockNumber);
    if (end <= blockNumber + 1)
        return false;
    for (int i = blockNumber + 1; i < end; ++i) {
        if (!document()->findBlockByNumber(i).text().trimmed().isEmpty())
            return true;
    }
    return false;
}

// CommonMark only nests an *ordered* list whose first item is numbered 1 — a
// "nested" marker like 2. or 3. under an item is lazily-continued as paragraph
// text instead. After Tab indents an ordered item, renumber the consecutive
// run of same-indent ordered items starting at `startBlock` to 1, 2, 3... so
// the new sublist actually renders. Top-level lists (indent 0) are untouched,
// preserving intentional start numbers like "2024.".
void Editor::renumberNestedOrderedList(int startBlock)
{
    static const QRegularExpression orderedRe(R"(^(\s*)(\d+)([.)]))");
    QTextBlock start = document()->findBlockByNumber(startBlock);
    QRegularExpressionMatch head = orderedRe.match(start.text());
    if (!head.hasMatch())
        return;
    const int newIndent = head.captured(1).length();
    if (newIndent == 0)
        return;

    // Walk back to the head of the contiguous same-indent ordered run so the
    // nested list is numbered 1..n as a whole, not each freshly-tabbed line
    // independently.
    QTextBlock block = start;
    QTextBlock prev = block.previous();
    while (prev.isValid()) {
        const QRegularExpressionMatch m = orderedRe.match(prev.text());
        if (!m.hasMatch() || m.captured(1).length() != newIndent)
            break;
        block = prev;
        prev = prev.previous();
    }

    int counter = 1;
    while (block.isValid()) {
        const QRegularExpressionMatch m = orderedRe.match(block.text());
        if (!m.hasMatch() || m.captured(1).length() != newIndent)
            break;
        QTextCursor c(block);
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newIndent);
        c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m.captured(2).length());
        c.insertText(QString::number(counter));
        block = block.next();
        ++counter;
    }
}

// Mirror of renumberNestedOrderedList for Shift+Tab: when an ordered item is
// outdented it rejoins the list at a shallower level, so renumber it (and its
// contiguous same-indent ordered run) to continue the enclosing sequence. The
// base is the nearest preceding ordered item at the new indent + 1; when none
// precedes it (blank line, paragraph, new list context), the item's own number
// is preserved so intentional starts like "2024." survive.
void Editor::renumberOutdentedOrderedList(int startBlock)
{
    static const QRegularExpression orderedRe(R"(^(\s*)(\d+)([.)]))");
    QTextBlock start = document()->findBlockByNumber(startBlock);
    QRegularExpressionMatch head = orderedRe.match(start.text());
    if (!head.hasMatch())
        return;
    const int newIndent = head.captured(1).length();

    int base = head.captured(2).toInt();
    QTextBlock prev = start.previous();
    while (prev.isValid()) {
        const QString text = prev.text();
        if (text.trimmed().isEmpty()) {
            prev = prev.previous();
            continue;
        }
        int indent = 0;
        for (QChar c : text) {
            if (c == ' ') ++indent;
            else break;
        }
        const QRegularExpressionMatch m = orderedRe.match(text);
        if (indent <= newIndent && m.hasMatch()) {
            base = m.captured(2).toInt() + 1;
            break;
        }
        // At or above the target level but not an ordered item (paragraph,
        // bullet, heading, ...) — the outdented item starts its own context.
        if (indent <= newIndent)
            break;
        // Deeper-indented block: part of a sub-list, skip up toward the
        // enclosing (shallower) list the outdented item rejoins.
        prev = prev.previous();
    }

    QTextBlock block = start;
    int counter = base;
    while (block.isValid()) {
        const QRegularExpressionMatch m = orderedRe.match(block.text());
        if (!m.hasMatch() || m.captured(1).length() != newIndent)
            break;
        QTextCursor c(block);
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, newIndent);
        c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m.captured(2).length());
        c.insertText(QString::number(counter));
        block = block.next();
        ++counter;
    }
}

int Editor::listFoldEnd(int startBlock) const
{
    const ListFoldInfo anchor = listFoldInfo(document()->findBlockByNumber(startBlock).text());
    const bool firstItem = listItemIsFirst(startBlock, anchor.quoteDepth, anchor.indent);
    const int contentColumn = anchor.indent + 2;
    auto terminatesList = [&](const ListFoldInfo &info) {
        const bool sameLevel = info.quoteDepth == anchor.quoteDepth
                               && info.indent <= anchor.indent;
        return sameLevel && (!firstItem || info.indent < anchor.indent);
    };
    QTextBlock block = document()->findBlockByNumber(startBlock + 1);
    while (block.isValid()) {
        const QString text = block.text();
        if (text.trimmed().isEmpty()) {
            QTextBlock next = block.next();
            while (next.isValid() && next.text().trimmed().isEmpty())
                next = next.next();
            if (!next.isValid())
                return document()->blockCount();
            const ListFoldInfo info = listFoldInfo(next.text());
            if (info.quoteDepth < anchor.quoteDepth || terminatesList(info)
                || (!info.isList && info.indent < contentColumn))
                return block.blockNumber();
            block = next;
            continue;
        }

        const ListFoldInfo info = listFoldInfo(text);
        if (info.quoteDepth < anchor.quoteDepth)
            return block.blockNumber();
        if (info.isList) {
            if (terminatesList(info))
                return block.blockNumber();
            block = block.next();
            continue;
        }
        if (info.indent < contentColumn) {
            static const QRegularExpression outerBlockStartRe(
                "^(?:#{1,6}\\s|```|~~~|>\\s|[-*+]\\s|\\d+[.)]\\s|</?[a-zA-Z#]|<!--|---+\\s*$|\\*{3,}\\s*$|_{3,}\\s*$)");
            if (outerBlockStartRe.match(info.content).hasMatch())
                return block.blockNumber();
        }
        block = block.next();
    }
    return document()->blockCount();
}

QPair<int, int> Editor::fencedCodeBlockRange(int blockNumber) const
{
    if (!m_codeFences.contains(blockNumber))
        return {-1, -1};
    return {blockNumber, foldEnd(blockNumber)};
}

QString Editor::blockRangeText(int firstBlock, int endExclusive) const
{
    QStringList parts;
    QTextBlock block = document()->findBlockByNumber(firstBlock);
    while (block.isValid() && block.blockNumber() < endExclusive) {
        parts.append(block.text());
        block = block.next();
    }
    return parts.join('\n');
}

void Editor::replaceBlockRange(int firstBlock, int endExclusive, const QString &text)
{
    QTextDocument *doc = document();
    if (firstBlock < 0 || firstBlock >= doc->blockCount())
        return;
    QTextCursor cur(doc);
    cur.beginEditBlock();
    QTextBlock first = doc->findBlockByNumber(firstBlock);
    cur.setPosition(first.position());
    cur.movePosition(QTextCursor::StartOfBlock);
    int last = endExclusive - 1;
    if (last >= doc->blockCount())
        last = doc->blockCount() - 1;
    if (last < firstBlock)
        last = firstBlock;
    QTextBlock endBlock = doc->findBlockByNumber(last);
    // Select through the last line's content but not its paragraph separator.
    cur.setPosition(endBlock.position() + qMax(0, endBlock.length() - 1),
                    QTextCursor::KeepAnchor);
    cur.removeSelectedText();
    cur.insertText(text);
    cur.endEditBlock();
    scanHeadersAndFolds();
}

// Matches `$$…$$` (display) before `$…$` (inline, single line, non-empty).
static const QRegularExpression &inlineMathRe()
{
    static const QRegularExpression re(
        QStringLiteral(R"(\$\$[\s\S]+?\$\$|\$[^$\n]+\$)"));
    return re;
}

int Editor::findNthInlineMath(int blockNumber, int index) const
{
    if (blockNumber < 0 || blockNumber >= document()->blockCount())
        return -1;
    const QString line = document()->findBlockByNumber(blockNumber).text();
    int i = 0;
    QRegularExpressionMatchIterator it = inlineMathRe().globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (i++ == index)
            return document()->findBlockByNumber(blockNumber).position() + m.capturedStart();
    }
    return -1;
}

void Editor::replaceInlineMath(int blockNumber, int index, const QString &replacement)
{
    if (blockNumber < 0 || blockNumber >= document()->blockCount())
        return;
    const QString line = document()->findBlockByNumber(blockNumber).text();
    int i = 0;
    QRegularExpressionMatchIterator it = inlineMathRe().globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (i++ == index) {
            QTextBlock block = document()->findBlockByNumber(blockNumber);
            QTextCursor cur(document());
            cur.setPosition(block.position() + m.capturedStart());
            cur.setPosition(block.position() + m.capturedEnd(), QTextCursor::KeepAnchor);
            cur.insertText(replacement);
            return;
        }
    }
}

QString Editor::fenceBody(int blockNumber) const
{
    const QPair<int, int> range = fencedCodeBlockRange(blockNumber);
    if (range.first < 0)
        return QString();
    QStringList lines = blockRangeText(range.first, range.second).split(QLatin1Char('\n'));
    if (lines.size() < 3)
        return QString();
    lines.removeFirst(); // opening fence
    lines.removeLast();  // closing fence
    return lines.join(QLatin1Char('\n'));
}

// The preview's `<code>` text ends with a line terminator that the editor's
// block range does not carry; tolerate that when comparing.
static bool chartBodiesMatch(const QString &a, const QString &b)
{
    return a == b || a == b + QLatin1Char('\n') || b == a + QLatin1Char('\n');
}

int Editor::findFenceByBody(const QString &body, int hintLine) const
{
    if (body.isEmpty())
        return -1;
    QList<int> fences = m_codeFences.values();
    std::sort(fences.begin(), fences.end());
    int best = -1;
    int bestDist = INT_MAX;
    for (int bn : fences) {
        if (!chartBodiesMatch(body, fenceBody(bn)))
            continue;
        const int dist = qAbs(bn - (hintLine - 1));
        if (dist < bestDist) {
            bestDist = dist;
            best = bn;
        }
    }
    return best;
}

QString Editor::fenceLanguage(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return QString();
    const QString text = block.text().trimmed();
    if (!text.startsWith(QLatin1String("```")))
        return QString();
    return text.mid(3).trimmed().section(QRegularExpression("[\\s{].*"), 0, 0).toLower();
}

// Strips the `$$` / `$` delimiters the regex captures, matching the inner
// text the preview's edit anchors carry.
static QString mathInnerText(const QString &match)
{
    if (match.startsWith(QLatin1String("$$")))
        return match.mid(2, match.size() - 4);
    if (match.size() >= 2)
        return match.mid(1, match.size() - 2);
    return QString();
}

int Editor::findMathByContent(const QString &innerTex, int hintLine, int index,
                              int *outIndex) const
{
    if (innerTex.isEmpty())
        return -1;
    int best = -1, bestOutIndex = -1, bestDist = INT_MAX;
    int exactBest = -1, exactOutIndex = -1, exactDist = INT_MAX;
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        const QString line = block.text();
        int i = 0;
        QRegularExpressionMatchIterator it = inlineMathRe().globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (mathInnerText(m.captured(0)) != innerTex) {
                ++i;
                continue;
            }
            const int dist = qAbs(block.blockNumber() - (hintLine - 1));
            if (i == index && dist < exactDist) {
                exactBest = block.blockNumber();
                exactOutIndex = i;
                exactDist = dist;
            }
            if (dist < bestDist) {
                best = block.blockNumber();
                bestOutIndex = i;
                bestDist = dist;
            }
            ++i;
        }
        block = block.next();
    }
    if (exactBest >= 0) {
        if (outIndex) *outIndex = exactOutIndex;
        return exactBest;
    }
    if (best >= 0 && outIndex) *outIndex = bestOutIndex;
    return best;
}

int Editor::sectionEndBlock(int fromBlock, int level) const
{
    QTextBlock block = document()->findBlockByNumber(fromBlock + 1);
    while (block.isValid()) {
        int bn = block.blockNumber();
        if (m_headerLevel.contains(bn)) {
            int otherLevel = m_headerLevel[bn];
            if (otherLevel <= level)
                return bn;
        }
        block = block.next();
    }
    return document()->blockCount();
}

void Editor::toggleFold(int blockNumber)
{
    if (!isFoldableBlock(blockNumber))
        return;

    m_updatingFolds = true;

    if (m_foldedBlocks.contains(blockNumber)) {
        // Unfold
        applyFold(blockNumber, false);
        m_foldedBlocks.remove(blockNumber);
        m_foldEndPins.remove(blockNumber);
    } else {
        // Fold
        applyFold(blockNumber, true);
        m_foldedBlocks.insert(blockNumber);
    }

    m_updatingFolds = false;

    if (m_gutter)
        m_gutter->setFoldedBlocks(m_foldedBlocks);

    document()->markContentsDirty(0, document()->characterCount());
    update();
}

int Editor::findPrevHeader(int fromBlock) const
{
    int best = -1;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it) {
        if (it.key() < fromBlock && (best < 0 || it.key() > best))
            best = it.key();
    }
    return best;
}

int Editor::findNextHeader(int fromBlock) const
{
    int best = -1;
    for (auto it = m_headerLevel.constBegin(); it != m_headerLevel.constEnd(); ++it) {
        if (it.key() > fromBlock && (best < 0 || it.key() < best))
            best = it.key();
    }
    return best;
}

bool Editor::isHeaderBlock(int blockNumber) const
{
    return m_headerLevel.contains(blockNumber);
}

int Editor::headerLevelAt(int blockNumber) const
{
    auto it = m_headerLevel.find(blockNumber);
    return it != m_headerLevel.end() ? it.value() : 0;
}

bool Editor::insideFencedCode(int blockNumber) const
{
    int depth = 0;
    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() <= blockNumber) {
        if (block.text().trimmed().startsWith("```"))
            depth ^= 1;
        block = block.next();
    }
    return depth != 0;
}

void Editor::restoreFolds(const QList<int> &foldedBlocks)
{
    QSet<int> newFolds;
    for (int bn : foldedBlocks) {
        if (isFoldableBlock(bn))
            newFolds.insert(bn);
    }

    m_updatingFolds = true;
    // Unhide previously-folded blocks no longer in the set
    for (int bn : m_foldedBlocks) {
        if (!newFolds.contains(bn))
            applyFold(bn, false);
    }
    // Hide newly-folded blocks
    for (int bn : newFolds) {
        if (!m_foldedBlocks.contains(bn))
            applyFold(bn, true);
    }
    m_foldedBlocks = newFolds;
    for (auto it = m_foldEndPins.begin(); it != m_foldEndPins.end();) {
        if (!newFolds.contains(it.key()))
            it = m_foldEndPins.erase(it);
        else
            ++it;
    }
    m_updatingFolds = false;

    if (m_gutter)
        m_gutter->setFoldedBlocks(m_foldedBlocks);

    document()->markContentsDirty(0, document()->characterCount());
}

QList<int> Editor::foldedBlockNumbers() const
{
    QList<int> result;
    for (int bn : m_foldedBlocks)
        result.append(bn);
    std::sort(result.begin(), result.end());
    return result;
}

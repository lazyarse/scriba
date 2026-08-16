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
#include "prefs/Preferences.h"
#include "StaticHelpers.h"
#include <QAbstractItemView>
#include <QChar>
#include <QCompleter>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QIcon>
#include <QPoint>
#include <QRect>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QVector>

namespace {


// Vertical gap (px) between the bottom of the caret/active line and the top of
// the autocompletion popup. Kept small so the suggestions hug the text.
constexpr int kCompletionPopupGap = 4;


} // namespace

bool Editor::isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const
{
    return extractLinkPath(cursor.block().text(), cursor.positionInBlock(), partialPath);
}

bool Editor::isInsideHtmlPathContext(const QTextCursor &cursor, QString &partialPath) const
{
    return extractHtmlPath(cursor.block().text(), cursor.positionInBlock(), partialPath);
}

void Editor::positionCompletionPopup(const QRect &cursorRect)
{
    if (!m_completer || !m_completer->popup())
        return;
    QAbstractItemView *popup = m_completer->popup();
    QRect screen = viewport()->screen()->availableGeometry();

    QPoint caretBottom = viewport()->mapToGlobal(
        QPoint(cursorRect.x(), cursorRect.y() + cursorRect.height()));
    QPoint caretTop = viewport()->mapToGlobal(QPoint(cursorRect.x(), cursorRect.y()));
    int ph = popup->height();
    if (ph <= 0)
        ph = popup->sizeHint().height();

    // Prefer below the caret; flip above when the popup would fall off-screen.
    QPoint pos = caretBottom + QPoint(0, kCompletionPopupGap);
    if (pos.y() + ph > screen.bottom()) {
        int top = caretTop.y() - kCompletionPopupGap - ph;
        if (top < screen.top()) {
            top = screen.top();
            int h = caretTop.y() - kCompletionPopupGap - top;
            if (h > 0)
                popup->resize(popup->width(), h);
        }
        pos.setY(top);
    }
    pos.setX(qBound(screen.left(), pos.x(), screen.right() - qMin(popup->width(), screen.width())));
    popup->move(pos);
}

bool Editor::showFileCompletion(const QString &partialPath)
{
    if (m_currentFile.isEmpty() ||
        !QSettings().value(Preferences::FileAutoComplete, true).toBool())
        return false;

    QFileInfo fi(m_currentFile);
    QDir dir = fi.absoluteDir();

    FileCompletionResult result = matchFileEntries(partialPath, dir,
        QSettings().value(Preferences::FileCompletionLimit, 20).toInt());
    if (result.entries.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStringListModel *model = new QStringListModel(result.entries, m_completer);
    m_completer->setModel(model);
    m_completer->setCompletionPrefix(result.filePart);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : result.entries)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

void Editor::acceptCompletion(const QString &completion)
{
    if (m_completer)
        m_completer->popup()->hide();

    if (completion.startsWith(':') && completion.endsWith(':')) {
        acceptEmojiCompletion(completion);
        return;
    }

    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int pos = cursor.positionInBlock();
    int blockStart = cursor.block().position();

    // HTML comment context: replace the typed `<!--` prefix
    if (isInsideHtmlCommentContext(cursor)) {
        cursor.setPosition(blockStart + pos - 4, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);
        return;
    }

    // Code fence language context: replace text right after ```
    QString partialLang;
    if (isInsideLanguageContext(cursor, partialLang)) {
        cursor.setPosition(blockStart + 3, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);
        return;
    }

    // Markdown link context: [text](path) or ![text](path)
    int replaceStart = linkPathReplaceStart(line, pos);
    if (replaceStart >= 0) {
        cursor.setPosition(blockStart + replaceStart, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);

        if (!completion.endsWith('/')) {
            QTextCursor c = textCursor();
            QChar next = document()->characterAt(c.position());
            if (next != ')') {
                c.insertText(QStringLiteral(")"));
                setTextCursor(c);
            }
        }
        return;
    }

    // HTML attribute context: src="path" or href='path'
    replaceStart = htmlPathReplaceStart(line, pos);
    if (replaceStart >= 0) {
        cursor.setPosition(blockStart + replaceStart, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);
    }
}

void Editor::loadEmojiShortcodes()
{
    for (const EmojiEntry &entry : emojiCatalog()) {
        m_emojiShortcodes.append(entry.shortcode);
        m_emojiUnicode[entry.shortcode] = entry.unicode;
    }
    m_emojiShortcodes.removeDuplicates();
    m_emojiShortcodes.sort();
}

bool Editor::isInsideEmojiContext(const QTextCursor &cursor, QString &partialCode) const
{
    return extractEmojiCode(cursor.block().text(), cursor.positionInBlock(), partialCode);
}

bool Editor::showEmojiCompletion(const QString &partialCode)
{
    QVector<QPair<QString, FuzzyScore>> scored;
    for (const QString &sc : m_emojiShortcodes) {
        FuzzyScore score = fuzzyMatchScore(sc, partialCode);
        if (score.matched)
            scored.append({sc, score});
    }

    if (scored.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    std::sort(scored.begin(), scored.end(),
        [](const QPair<QString, FuzzyScore> &a, const QPair<QString, FuzzyScore> &b) {
            if (a.second.gaps != b.second.gaps)
                return a.second.gaps < b.second.gaps;
            if (a.second.firstPos != b.second.firstPos)
                return a.second.firstPos < b.second.firstPos;
            return a.first < b.first;
        });

    int limit = QSettings().value(Preferences::EmojiCompletionLimit, 100).toInt();
    if (scored.size() > limit)
        scored = scored.mid(0, limit);

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStandardItemModel *model = new QStandardItemModel(m_completer);
    for (const auto &match : scored) {
        auto *item = new QStandardItem(QString(":%1:").arg(match.first));
        item->setIcon(QIcon(renderEmojiIcon(m_emojiUnicode.value(match.first))));
        model->appendRow(item);
    }

    m_completer->setModel(model);
    m_completer->setCompletionPrefix(partialCode);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (int i = 0; i < model->rowCount(); ++i)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(model->item(i)->text()));
    cr.setWidth(maxWidth + 30 + 22);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

QPixmap Editor::renderEmojiIcon(const QString &emojiStr) const
{
    if (auto it = m_emojiIconCache.find(emojiStr); it != m_emojiIconCache.end())
        return *it;

    QPixmap pix = renderEmojiPixmap(emojiStr, 18);
    m_emojiIconCache[emojiStr] = pix;
    return pix;
}

void Editor::acceptEmojiCompletion(const QString &completion)
{
    if (m_completer)
        m_completer->popup()->hide();

    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int pos = cursor.positionInBlock();

    int colonPos = -1;
    static const QRegularExpression emojiRe(R"(:([a-zA-Z0-9_+-]*)$)");
    auto match = emojiRe.match(line.left(pos));
    if (match.hasMatch())
        colonPos = match.capturedStart(0);
    else
        return;

    int endPos = pos;
    if (pos < line.size() && line[pos] == ':')
        ++endPos;

    int blockStart = cursor.block().position();
    cursor.setPosition(blockStart + colonPos, QTextCursor::MoveAnchor);
    cursor.setPosition(blockStart + endPos, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(completion);
    setTextCursor(cursor);
}

bool Editor::isInsideInlineCode() const
{
    // Backtick parity before the caret on the current line (mirrors the spell
    // highlighter's per-line handling of inline code spans).
    const QString line = textCursor().block().text();
    const int pos = textCursor().positionInBlock();
    int backticks = 0;
    for (int i = 0; i < pos; ++i) {
        if (line[i] == '`' && (i == 0 || line[i - 1] != '\\'))
            ++backticks;
    }
    return backticks % 2 == 1;
}

namespace {
// Code fence languages bundled in resources/highlight.min.js (canonical → aliases).
// Keep in sync with the bundled highlight.js version.
const QHash<QString, QStringList> &codeLanguages()
{
    static const QHash<QString, QStringList> langs = {
        { "bash",        {"sh", "zsh", "shellscript"} },
        { "c",           {} },
        { "cpp",         {"c++", "cxx", "hpp", "hxx", "cc"} },
        { "csharp",      {"cs", "c#"} },
        { "css",         {} },
        { "diff",        {"patch"} },
        { "go",          {"golang"} },
        { "graphql",     {"gql"} },
        { "ini",         {"toml", "cfg", "conf"} },
        { "java",        {} },
        { "javascript",  {"js", "jsx", "mjs", "cjs"} },
        { "json",        {} },
        { "kotlin",      {"kt", "kts"} },
        { "less",        {} },
        { "lua",         {} },
        { "makefile",    {"make", "mk", "mak"} },
        { "markdown",    {"md", "mkdown", "mkd"} },
        { "mermaid",     {"mmd"} },
        { "objectivec",  {"objc", "obj-c", "mm"} },
        { "perl",        {"pl"} },
        { "php",         {} },
        { "plaintext",   {"text", "txt"} },
        { "python",      {"py", "gyp"} },
        { "r",           {} },
        { "ruby",        {"rb", "gemspec", "podspec", "thor", "irb"} },
        { "rust",        {"rs"} },
        { "scss",        {} },
        { "shell",       {"console", "shellsession"} },
        { "sql",         {} },
        { "swift",       {} },
        { "typescript",  {"ts", "tsx"} },
        { "vbnet",       {"vb", "vbs"} },
        { "ec",          {} },
        { "wasm",        {} },
        { "xml",         {"html", "xhtml", "svg", "rss", "atom", "xsl", "plist"} },
        { "yaml",        {"yml"} },
    };
    return langs;
}
}

bool Editor::isInsideLanguageContext(const QTextCursor &cursor, QString &partialLang) const
{
    QString text = cursor.block().text();
    int pos = cursor.positionInBlock();

    static const QRegularExpression fenceRe(R"(^```(\S*)$)");
    auto match = fenceRe.match(text.left(pos));
    if (!match.hasMatch())
        return false;

    // Must be an opening fence: an even number of fence blocks precede it.
    int fenceCount = 0;
    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() < cursor.blockNumber()) {
        if (block.text().trimmed().startsWith("```"))
            ++fenceCount;
        block = block.next();
    }
    if (fenceCount % 2 != 0)
        return false;

    partialLang = match.captured(1);
    return true;
}

bool Editor::showLanguageCompletion(const QString &partialLang)
{
    if (partialLang.isEmpty() ||
        !QSettings().value(Preferences::LanguageAutoComplete, true).toBool())
        return false;

    QStringList matches;
    const QHash<QString, QStringList> &langs = codeLanguages();
    for (auto it = langs.cbegin(); it != langs.cend(); ++it) {
        const QString &name = it.key();
        if (name.contains(partialLang, Qt::CaseInsensitive)) {
            matches.append(name);
            continue;
        }
        for (const QString &alias : it.value()) {
            if (alias.contains(partialLang, Qt::CaseInsensitive)) {
                matches.append(name);
                break;
            }
        }
    }
    if (matches.isEmpty()) {
        if (m_completer)
            m_completer->popup()->hide();
        return false;
    }

    std::sort(matches.begin(), matches.end(),
        [&partialLang](const QString &a, const QString &b) {
            bool aPrefix = a.startsWith(partialLang, Qt::CaseInsensitive);
            bool bPrefix = b.startsWith(partialLang, Qt::CaseInsensitive);
            if (aPrefix != bPrefix)
                return aPrefix;
            return a < b;
        });

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStringListModel *model = new QStringListModel(matches, m_completer);
    m_completer->setModel(model);
    m_completer->setCompletionPrefix(partialLang);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : matches)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

bool Editor::isInsideHtmlCommentContext(const QTextCursor &cursor) const
{
    if (isCursorInFencedCodeBlock() || isInsideInlineCode())
        return false;
    static const QRegularExpression commentRe(R"(^\s*<!--$)");
    return commentRe.match(cursor.block().text().left(cursor.positionInBlock())).hasMatch();
}

bool Editor::showHtmlCommentCompletion()
{
    if (!QSettings().value(Preferences::CommentAutoComplete, true).toBool())
        return false;

    const QStringList entries = {
        QStringLiteral("<!-- keep -->"),
        QStringLiteral("<!-- break -->"),
    };

    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setFilterMode(Qt::MatchStartsWith);
        connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
                this, &Editor::acceptCompletion);
    }

    QStringListModel *model = new QStringListModel(entries, m_completer);
    m_completer->setModel(model);
    m_completer->setCompletionPrefix(QStringLiteral("<!--"));

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : entries)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
    positionCompletionPopup(cr);
    return true;
}

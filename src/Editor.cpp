#include "Editor.h"
#include "Preferences.h"
#include "StaticHelpers.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName("scriba-editor");
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
    setFrameShape(QFrame::NoFrame);

    loadEmojiShortcodes();

    QTextBlockFormat fmt;
    fmt.setLineHeight(240, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
}

void Editor::setCurrentFile(const QString &path)
{
    m_currentFile = path;
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_completer && m_completer->popup()->isVisible()) {
            m_completer->popup()->hide();
            return;
        }
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_completer && m_completer->popup()->isVisible()) {
            QAbstractItemModel *model = m_completer->completionModel();
            QModelIndex idx = m_completer->popup()->currentIndex();
            if (!idx.isValid() && model && model->rowCount() > 0)
                idx = model->index(0, 0);
            if (idx.isValid()) {
                acceptCompletion(model->data(idx).toString());
                return;
            }
        }

        QTextCursor cursor = textCursor();
        QString line = cursor.block().text();
        static const QChar clearSentinel(0x2412);
        QString result = handleListReturn(line);
        if (!result.isEmpty()) {
            if (result == QString(clearSentinel)) {
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                QPlainTextEdit::keyPressEvent(event);
                return;
            }
            QPlainTextEdit::keyPressEvent(event);
            insertPlainText(result);
            return;
        }

        QTextBlock prevBlock = cursor.block().previous();
        result = handleTableReturn(line, prevBlock.isValid() ? prevBlock.text() : QString());
        if (!result.isEmpty()) {
            if (result == QString(clearSentinel)) {
                if (line.contains("<tr>") && line.contains("<td>")) {
                    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                    QTextCursor search = document()->find("</table>", cursor);
                    if (!search.isNull()) {
                        search.movePosition(QTextCursor::EndOfLine);
                        setTextCursor(search);
                        insertPlainText("\n\n");
                        return;
                    }
                }
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                QPlainTextEdit::keyPressEvent(event);
                return;
            }

            // Header row: skip to first data row below separator
            QTextBlock nextBlock = cursor.block().next();
            if (nextBlock.isValid() && nextBlock.text().contains("---")) {
                QTextBlock block = nextBlock.next();
                while (block.isValid()) {
                    QString t = block.text();
                    if (t.startsWith('|') && !t.contains("---")) {
                        QTextCursor tc = textCursor();
                        tc.setPosition(block.position() + 2, QTextCursor::MoveAnchor);
                        setTextCursor(tc);
                        return;
                    }
                    block = block.next();
                }
            }

            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
            cursor.insertText("\n" + result);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            int cellPos = result.startsWith("<tr>") ? result.indexOf("<td>") + 4 : 2;
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, cellPos);
            setTextCursor(cursor);
            return;
        }
    }

    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        bool shift = event->modifiers() & Qt::ShiftModifier;

        if (m_completer && m_completer->popup()->isVisible()) {
            QAbstractItemView *pv = m_completer->popup();
            QAbstractItemModel *model = m_completer->completionModel();
            int rows = model->rowCount();
            if (rows > 0) {
                QModelIndex cur = pv->currentIndex().isValid()
                    ? pv->currentIndex() : model->index(0, 0);
                int next = shift ? cur.row() - 1 : cur.row() + 1;
                if (next < 0) next = rows - 1;
                if (next >= rows) next = 0;
                pv->setCurrentIndex(model->index(next, 0));
            }
            return;
        }

        QTextCursor cursor = textCursor();

        // Block indent/dedent for multi-line selections
        if (cursor.hasSelection()) {
            int startPos = cursor.selectionStart();
            int endPos = cursor.selectionEnd();
            QTextBlock startBlock = document()->findBlock(startPos);
            QTextBlock endBlock = document()->findBlock(endPos);
            if (endBlock.position() == endPos && endBlock.blockNumber() > startBlock.blockNumber())
                endBlock = endBlock.previous();

            int startNum = startBlock.blockNumber();
            int endNum = endBlock.blockNumber();

            bool dedent = shift || event->key() == Qt::Key_Backtab;

            if (dedent) {
                for (int i = endNum; i >= startNum; --i) {
                    QTextBlock block = document()->findBlockByNumber(i);
                    QString text = block.text();
                    int toRemove = 0;
                    while (toRemove < 4 && toRemove < text.size() && text[toRemove] == ' ')
                        ++toRemove;
                    if (toRemove > 0) {
                        QTextCursor tc = textCursor();
                        tc.setPosition(block.position());
                        tc.setPosition(block.position() + toRemove, QTextCursor::KeepAnchor);
                        tc.removeSelectedText();
                    }
                }
            } else {
                for (int i = endNum; i >= startNum; --i) {
                    QTextBlock block = document()->findBlockByNumber(i);
                    QTextCursor tc = textCursor();
                    tc.setPosition(block.position());
                    tc.insertText("    ");
                }
            }

            // Reselect the modified range
            QTextBlock newStart = document()->findBlockByNumber(startNum);
            QTextBlock newEnd = document()->findBlockByNumber(endNum);
            cursor.setPosition(newStart.position());
            cursor.setPosition(newEnd.position() + newEnd.length() - 1, QTextCursor::KeepAnchor);
            setTextCursor(cursor);
            return;
        }

        QString line = cursor.block().text();

        // Table cell navigation
        if (line.startsWith('|') && !line.contains("---")) {
            int pos = cursor.positionInBlock();
            int cellPos = tableNavCell(line, pos, !shift);
            if (cellPos >= 0) {
                cursor.setPosition(cursor.block().position() + cellPos, QTextCursor::MoveAnchor);
                setTextCursor(cursor);
                return;
            }
            if (!shift) {
                QTextBlock block = cursor.block().next();
                while (block.isValid()) {
                    QString t = block.text();
                    if (t.startsWith('|') && !t.contains("---")) {
                        int p = t.indexOf('|', 1) + 1;
                        if (p < t.size() && t[p] == ' ') ++p;
                        cursor.setPosition(block.position() + p, QTextCursor::MoveAnchor);
                        setTextCursor(cursor);
                        return;
                    }
                    block = block.next();
                }
            } else {
                QTextBlock block = cursor.block().previous();
                while (block.isValid()) {
                    QString t = block.text();
                    if (t.startsWith('|') && !t.contains("---")) {
                        QList<int> pipes;
                        for (int i = 0; i < t.size(); ++i)
                            if (t[i] == '|') pipes.append(i);
                        if (pipes.size() >= 2) {
                            int p = pipes[pipes.size() - 2] + 1;
                            if (p < t.size() && t[p] == ' ') ++p;
                            cursor.setPosition(block.position() + p, QTextCursor::MoveAnchor);
                            setTextCursor(cursor);
                            return;
                        }
                    }
                    block = block.previous();
                }
            }
        }

        // HTML table cell navigation
        if (line.contains("<tr>") && line.contains("<td>")) {
            int pos = cursor.positionInBlock();
            int cellPos = tableNavHtmlCell(line, pos, !shift);
            if (cellPos >= 0) {
                cursor.setPosition(cursor.block().position() + cellPos, QTextCursor::MoveAnchor);
                setTextCursor(cursor);
                return;
            }
        }

        auto matchUnordered = QRegularExpression(R"(^\s*[-*+]\s?)").match(line);
        auto matchOrdered = QRegularExpression(R"(^\s*\d+\.\s?)").match(line);
        bool isList = matchUnordered.hasMatch() || matchOrdered.hasMatch();

        if (event->key() == Qt::Key_Backtab || shift) {
            if (isList) {
                QString outdented = outdentListLine(line);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                cursor.insertText(outdented);
                return;
            }
        } else {
            QString partialPath;
            if (isInsideLinkContext(cursor, partialPath)) {
                showFileCompletion(partialPath);
                return;
            }
            {
                QString partialCode;
                if (isInsideEmojiContext(cursor, partialCode) && QSettings().value(Preferences::EmojiAutoComplete, true).toBool()) {
                    showEmojiCompletion(partialCode);
                    return;
                }
            }
            if (isList) {
                QString indented = indentListLine(line);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                cursor.insertText(indented);
                return;
            }
            insertPlainText("    ");
            return;
        }
    }

    if (event->key() == Qt::Key_Down) {
        if (!m_completer || !m_completer->popup()->isVisible()) {
            QTextCursor cursor = textCursor();
            QTextCursor probe = cursor;
            if (!probe.movePosition(QTextCursor::Down)) {
                cursor.movePosition(QTextCursor::EndOfBlock);
                setTextCursor(cursor);
                return;
            }
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        if (m_completer && m_completer->popup()->isVisible()) {
            QString partialCode;
            if (isInsideEmojiContext(textCursor(), partialCode) && QSettings().value(Preferences::EmojiAutoComplete, true).toBool()) {
                if (partialCode.isEmpty())
                    m_completer->popup()->hide();
                else
                    showEmojiCompletion(partialCode);
            } else {
                QString partialPath;
                if (isInsideLinkContext(textCursor(), partialPath)) {
                    if (partialPath.isEmpty())
                        m_completer->popup()->hide();
                    else
                        showFileCompletion(partialPath);
                } else {
                    m_completer->popup()->hide();
                }
            }
        }
    } else if (!event->text().isEmpty()) {
        QChar c = event->text()[0];
        if (c.isLetterOrNumber() || c == '_' || c == ':') {
            QString partialPath;
            if (isInsideLinkContext(textCursor(), partialPath))
                showFileCompletion(partialPath);
            QString partialCode;
            if (isInsideEmojiContext(textCursor(), partialCode) && QSettings().value(Preferences::EmojiAutoComplete, true).toBool())
                showEmojiCompletion(partialCode);
        }
    }
}

bool Editor::isInsideLinkContext(const QTextCursor &cursor, QString &partialPath) const
{
    QString text = cursor.block().text();
    int pos = cursor.positionInBlock();

    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(text.left(pos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return false;

    QString between = text.mid(parenPos, pos - parenPos);
    if (between.contains(')'))
        return false;

    partialPath = between;
    return true;
}

void Editor::showFileCompletion(const QString &partialPath)
{
    if (m_currentFile.isEmpty())
        return;

    QFileInfo fi(m_currentFile);
    QDir dir = fi.absoluteDir();

    int lastSlash = partialPath.lastIndexOf('/');
    QString dirPart = lastSlash >= 0 ? partialPath.left(lastSlash + 1) : QString();
    QString filePart = lastSlash >= 0 ? partialPath.mid(lastSlash + 1) : partialPath;

    QString searchDir = dir.absoluteFilePath(dirPart.isEmpty() ? "." : dirPart);
    QDir search(searchDir);
    if (!search.exists())
        return;

    QStringList entries;
    QStringList all = search.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    for (const QString &entry : all) {
        if (entry.startsWith('.') && !filePart.startsWith('.'))
            continue;
        if (filePart.isEmpty() || entry.startsWith(filePart, Qt::CaseInsensitive)) {
            if (QFileInfo(search, entry).isDir())
                entries.append(entry + "/");
            else
                entries.append(entry);
        }
    }

    int maxResults = QSettings().value(Preferences::FileCompletionLimit, 20).toInt();
    if (entries.size() > maxResults)
        entries = entries.mid(0, maxResults);

    if (entries.isEmpty() || filePart.isEmpty())
        return;

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
    m_completer->setCompletionPrefix(filePart);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : entries)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);

    QTextBlock block = textCursor().block();
    int lineH = fontMetrics().height() * block.blockFormat().lineHeight() / 100;
    cr.moveTop(cr.y() + lineH + 18);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
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

    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(pos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos < 0)
        return;

    QString between = line.mid(parenPos, pos - parenPos);
    int lastSlash = between.lastIndexOf('/');
    int replaceStart = lastSlash >= 0 ? parenPos + lastSlash + 1 : parenPos;

    int blockStart = cursor.block().position();
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
}

void Editor::loadEmojiShortcodes()
{
    QFile file(":/emoji.js");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(file.readAll());
    QRegularExpression re(R"('([^']+)'\s*:\s*'([^']+)')");
    auto it = re.globalMatch(content);
    while (it.hasNext()) {
        auto match = it.next();
        m_emojiShortcodes.append(match.captured(1));
    }
    m_emojiShortcodes.removeDuplicates();
    m_emojiShortcodes.sort();
}

bool Editor::isInsideEmojiContext(const QTextCursor &cursor, QString &partialCode) const
{
    QString text = cursor.block().text();
    int pos = cursor.positionInBlock();
    if (pos < 1)
        return false;

    static const QRegularExpression linkRe(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = linkRe.globalMatch(text.left(pos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos >= 0) {
        QString between = text.mid(parenPos, pos - parenPos);
        if (!between.contains(')'))
            return false;
    }

    for (int i = pos - 1; i >= 0; --i) {
        QChar c = text[i];
        if (c == ':') {
            QString after = text.mid(i + 1, pos - i - 1);
            if (after.contains(':') || after.isEmpty())
                return false;
            partialCode = after;
            return true;
        }
        if (!c.isLetterOrNumber() && c != '_' && c != '-')
            break;
    }
    return false;
}

void Editor::showEmojiCompletion(const QString &partialCode)
{
    QStringList matches;
    for (const QString &sc : m_emojiShortcodes) {
        if (sc.startsWith(partialCode, Qt::CaseInsensitive))
            matches.append(QString(":%1:").arg(sc));
    }

    if (matches.isEmpty())
        return;

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
    m_completer->setCompletionPrefix(partialCode);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &m : matches)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(m));
    cr.setWidth(maxWidth + 30);

    QTextBlock block = textCursor().block();
    int lineH = fontMetrics().height() * block.blockFormat().lineHeight() / 100;
    cr.moveTop(cr.y() + lineH + 18);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
}

void Editor::acceptEmojiCompletion(const QString &completion)
{
    if (m_completer)
        m_completer->popup()->hide();

    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int pos = cursor.positionInBlock();

    int colonPos = -1;
    for (int i = pos - 1; i >= 0; --i) {
        QChar c = line[i];
        if (c == ':') {
            colonPos = i;
            break;
        }
        if (!c.isLetterOrNumber() && c != '_' && c != '-')
            break;
    }
    if (colonPos < 0)
        return;

    int blockStart = cursor.block().position();
    cursor.setPosition(blockStart + colonPos, QTextCursor::MoveAnchor);
    cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(completion);
    setTextCursor(cursor);
}

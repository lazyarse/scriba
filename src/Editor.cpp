#include "Editor.h"
#include "Preferences.h"
#include "StaticHelpers.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QSvgRenderer>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

Editor::Editor(QWidget *parent)
    : QTextEdit(parent)
{
    setObjectName("scriba-editor");
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
    setFrameShape(QFrame::NoFrame);

    loadEmojiShortcodes();
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
                QTextEdit::keyPressEvent(event);
                return;
            }
            QTextEdit::keyPressEvent(event);
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
                QTextEdit::keyPressEvent(event);
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
                QString htmlPath;
                if (isInsideHtmlPathContext(cursor, htmlPath)) {
                    showFileCompletion(htmlPath);
                    return;
                }
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

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_D) {
        QTextCursor cursor = textCursor();
        QTextDocument *doc = document();
        bool hasSel = cursor.hasSelection();

        QTextBlock startBlock = hasSel
            ? doc->findBlock(cursor.selectionStart())
            : cursor.block();
        QTextBlock endBlock = hasSel
            ? doc->findBlock(cursor.selectionEnd())
            : startBlock;

        QStringList lines;
        QTextBlock b = startBlock;
        while (true) {
            lines << b.text();
            if (b == endBlock) break;
            b = b.next();
        }
        QString blockText = lines.join('\n');

        cursor.beginEditBlock();
        if (endBlock.blockNumber() == doc->blockCount() - 1) {
            cursor.movePosition(QTextCursor::End);
            cursor.insertText('\n' + blockText);
        } else {
            QTextBlock next = endBlock.next();
            cursor.setPosition(next.position());
            cursor.insertText(blockText + '\n');
        }
        cursor.endEditBlock();

        event->accept();
        return;
    }

    QTextEdit::keyPressEvent(event);

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
                    QString htmlPath;
                    if (isInsideHtmlPathContext(textCursor(), htmlPath)) {
                        if (htmlPath.isEmpty())
                            m_completer->popup()->hide();
                        else
                            showFileCompletion(htmlPath);
                    } else {
                        m_completer->popup()->hide();
                    }
                }
            }
        }
    } else if (!event->text().isEmpty()) {
        QChar c = event->text()[0];
        if (c.isLetterOrNumber() || c == '_' || c == ':' || c == '+' || c == '-' || c == '.' || c == '/') {
            QString partialPath;
            if (isInsideLinkContext(textCursor(), partialPath))
                showFileCompletion(partialPath);
            else {
                QString htmlPath;
                if (isInsideHtmlPathContext(textCursor(), htmlPath))
                    showFileCompletion(htmlPath);
            }
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

bool Editor::isInsideHtmlPathContext(const QTextCursor &cursor, QString &partialPath) const
{
    QString text = cursor.block().text();
    int pos = cursor.positionInBlock();

    static const QRegularExpression re(R"((?:src|href)\s*=\s*["']([^"']*)$)");
    auto match = re.match(text.left(pos));
    if (!match.hasMatch())
        return false;

    partialPath = match.captured(1);
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
    int blockStart = cursor.block().position();

    // Markdown link context: [text](path) or ![text](path)
    static const QRegularExpression re(R"(\!?\[.*?\]\()");
    QRegularExpressionMatchIterator it = re.globalMatch(line.left(pos));
    int parenPos = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        parenPos = m.capturedEnd();
    }
    if (parenPos >= 0) {
        QString between = line.mid(parenPos, pos - parenPos);
        int lastSlash = between.lastIndexOf('/');
        int replaceStart = lastSlash >= 0 ? parenPos + lastSlash + 1 : parenPos;

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
    static const QRegularExpression htmlRe(R"((?:src|href)\s*=\s*["']([^"']*)$)");
    auto htmlMatch = htmlRe.match(line.left(pos));
    if (htmlMatch.hasMatch()) {
        QString value = htmlMatch.captured(1);
        int valueStart = pos - value.length();
        int lastSlash = value.lastIndexOf('/');
        int replaceStart = lastSlash >= 0 ? valueStart + lastSlash + 1 : valueStart;

        cursor.setPosition(blockStart + replaceStart, QTextCursor::MoveAnchor);
        cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(completion);
        setTextCursor(cursor);
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
        QString name = match.captured(1);
        QString unicode = match.captured(2);
        m_emojiShortcodes.append(name);
        m_emojiUnicode[name] = unicode;
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

    static const QRegularExpression emojiRe(R"(:([a-zA-Z0-9+-][a-zA-Z0-9_+-]*)$)");
    auto match = emojiRe.match(text.left(pos));
    if (match.hasMatch()) {
        partialCode = match.captured(1);
        return true;
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

    QStandardItemModel *model = new QStandardItemModel(m_completer);
    for (const QString &sc : m_emojiShortcodes) {
        if (sc.startsWith(partialCode, Qt::CaseInsensitive)) {
            auto *item = new QStandardItem(QString(":%1:").arg(sc));
            item->setIcon(QIcon(renderEmojiIcon(m_emojiUnicode.value(sc))));
            model->appendRow(item);
        }
    }

    if (model->rowCount() == 0) {
        delete model;
        return;
    }

    m_completer->setModel(model);
    m_completer->setCompletionPrefix(partialCode);

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (int i = 0; i < model->rowCount(); ++i)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(model->item(i)->text()));
    cr.setWidth(maxWidth + 30 + 22);

    QTextBlock block = textCursor().block();
    int lineH = fontMetrics().height() * block.blockFormat().lineHeight() / 100;
    cr.moveTop(cr.y() + lineH + 18);

    m_completer->complete(cr);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));
}

QPixmap Editor::renderEmojiIcon(const QString &emojiStr) const
{
    if (auto it = m_emojiIconCache.find(emojiStr); it != m_emojiIconCache.end())
        return *it;

    QPixmap pix(18, 18);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    QString mode = QSettings().value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    if (mode == "color") {
        QStringList parts;
        for (int i = 0; i < emojiStr.size();) {
            uint code = emojiStr[i].unicode();
            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < emojiStr.size()) {
                uint low = emojiStr[i + 1].unicode();
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            } else {
                i += 1;
            }
            parts.append(QString::number(code, 16));
        }
        QSvgRenderer renderer(QString(":/twemoji/svg/%1.svg").arg(parts.join("-")));
        if (renderer.isValid())
            renderer.render(&painter, QRectF(0, 0, 18, 18));
    } else {
        QFont font("Symbola");
        font.setPixelSize(16);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QRect(0, 0, 18, 18), Qt::AlignCenter, emojiStr);
    }

    painter.end();
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

    int blockStart = cursor.block().position();
    cursor.setPosition(blockStart + colonPos, QTextCursor::MoveAnchor);
    cursor.setPosition(blockStart + pos, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(completion);
    setTextCursor(cursor);
}

void Editor::centerCursor()
{
    QRect cr = cursorRect();
    if (cr.isNull())
        return;
    int vh = viewport()->height();
    int cy = cr.center().y() + verticalScrollBar()->value();
    int target = cy - vh / 2;
    target = qBound(0, target, verticalScrollBar()->maximum());
    verticalScrollBar()->setValue(target);
}

void Editor::setCenterContent(bool enabled, int width)
{
    m_centerContent = enabled;
    m_centerContentWidth = width;
    updateViewportMargins();
}

void Editor::setInsertActions(const QList<QAction *> &actions)
{
    m_insertActions = actions;
}

QString Editor::currentLineText() const
{
    return textCursor().block().text();
}

Editor::CursorContext Editor::detectCursorContext() const
{
    QString line = currentLineText();

    static const QRegularExpression listRe(R"(^\s*[-*+]\s|^\s*\d+\.\s)");
    if (listRe.match(line).hasMatch())
        return CursorContext::ListItem;

    if (line.startsWith('|') && !line.contains("---"))
        return CursorContext::TableRow;

    if (isCursorInFencedCodeBlock())
        return CursorContext::CodeBlock;

    return CursorContext::None;
}

bool Editor::isCursorInFencedCodeBlock() const
{
    QTextCursor cursor = textCursor();
    int blockNum = cursor.blockNumber();
    int fenceCount = 0;
    QTextBlock block = document()->firstBlock();
    while (block.isValid() && block.blockNumber() <= blockNum) {
        QString text = block.text().trimmed();
        if (text.startsWith("```"))
            ++fenceCount;
        block = block.next();
    }
    return (fenceCount % 2) == 1;
}

bool Editor::cursorHasUrlSelection() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return false;
    QString selected = cursor.selectedText();
    static const QRegularExpression urlRe(R"(^https?://\S+$)");
    return urlRe.match(selected).hasMatch();
}

bool Editor::cursorHasImagePathSelection() const
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return false;
    QString selected = cursor.selectedText();
    static const QRegularExpression imgRe(R"(^(\./|\.\.\/|/)?\S+\.(png|jpe?g|gif|svg|webp|bmp)$)", QRegularExpression::CaseInsensitiveOption);
    return imgRe.match(selected).hasMatch();
}

void Editor::toggleCheckbox()
{
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    static const QRegularExpression checkRe(R"(^(\s*[-*+]\s)\[)([ xX])(\])");
    auto match = checkRe.match(line);
    if (!match.hasMatch())
        return;

    QString current = match.captured(2);
    QString toggled = (current == " ") ? "x" : " ";
    int offset = cursor.block().position() + match.capturedStart(2);
    cursor.setPosition(offset, QTextCursor::MoveAnchor);
    cursor.setPosition(offset + 1, QTextCursor::KeepAnchor);
    cursor.insertText(toggled);
    setTextCursor(cursor);
}

void Editor::insertTableRow(bool above)
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!line.startsWith('|'))
        return;

    int pipes = line.count('|');
    QString newRow;
    for (int i = 0; i < pipes; ++i) {
        newRow += "| ";
    }
    if (!newRow.endsWith('\n'))
        newRow += '\n';

    if (above) {
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.insertText(newRow);
    } else {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        cursor.insertText("\n" + newRow);
    }
}

void Editor::insertTableCol(bool left)
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!line.startsWith('|'))
        return;

    int pos = cursor.positionInBlock();
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|') pipes.append(i);

    int insertAfterPipe = -1;
    for (int i = 0; i < pipes.size() - 1; ++i) {
        if (pos >= pipes[i] && pos <= pipes[i + 1]) {
            insertAfterPipe = i;
            break;
        }
    }
    if (insertAfterPipe < 0)
        return;

    int colOffset = pipes[insertAfterPipe] + 1;

    QTextBlock block = cursor.block();
    while (block.isValid() && block.text().startsWith('|')) {
        QString text = block.text();
        QList<int> bPipes;
        for (int i = 0; i < text.size(); ++i)
            if (text[i] == '|') bPipes.append(i);

        if (insertAfterPipe < bPipes.size() - 1) {
            int insertPos = bPipes[insertAfterPipe] + 1;
            bool isSep = text.contains("---");
            QString insert = isSep ? " --- " : "  ";
            QTextCursor tc(document());
            tc.setPosition(block.position() + insertPos);
            tc.insertText(insert);
        }
        block = block.next();
    }
}

void Editor::deleteTableRow()
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!line.startsWith('|'))
        return;

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    if (line.contains("---")) {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    } else {
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
    setTextCursor(cursor);
}

void Editor::deleteTableCol()
{
    QTextCursor cursor = textCursor();
    QString line = currentLineText();
    if (!line.startsWith('|'))
        return;

    int pos = cursor.positionInBlock();
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|') pipes.append(i);

    int colIdx = -1;
    for (int i = 0; i < pipes.size() - 1; ++i) {
        if (pos >= pipes[i] && pos <= pipes[i + 1]) {
            colIdx = i;
            break;
        }
    }
    if (colIdx < 0)
        return;

    QTextBlock b = document()->firstBlock();
    while (b.isValid()) {
        if (b.text().startsWith('|')) {
            QList<int> bPipes;
            QString text = b.text();
            for (int i = 0; i < text.size(); ++i)
                if (text[i] == '|') bPipes.append(i);

            if (colIdx < bPipes.size() - 1) {
                int start = bPipes[colIdx];
                int end = (colIdx + 1 < bPipes.size()) ? bPipes[colIdx + 1] : text.size();
                if (bPipes[colIdx + 1] == bPipes.last() && colIdx + 1 == bPipes.size() - 1) {
                    end = text.size() - 1;
                }
                QTextCursor tc(document());
                tc.setPosition(b.position() + start);
                tc.setPosition(b.position() + end, QTextCursor::KeepAnchor);
                tc.removeSelectedText();
            }
        }
        b = b.next();
    }
}

void Editor::changeCodeLanguage()
{
    QTextCursor cursor = textCursor();
    int blockNum = cursor.blockNumber();

    QTextBlock fenceBlock;
    QTextBlock block = document()->firstBlock();
    bool foundOpen = false;
    while (block.isValid() && block.blockNumber() < blockNum) {
        if (block.text().trimmed().startsWith("```")) {
            if (!foundOpen) {
                fenceBlock = block;
                foundOpen = true;
            } else {
                foundOpen = false;
            }
        }
        block = block.next();
    }

    if (!foundOpen || !fenceBlock.isValid())
        return;

    QString fenceText = fenceBlock.text().trimmed();
    QString currentLang = fenceText.mid(3).trimmed();

    bool ok;
    QString lang = QInputDialog::getText(this, tr("Change Language"),
        tr("Language:"), QLineEdit::Normal, currentLang, &ok);
    if (!ok)
        return;

    QTextCursor tc(document());
    tc.setPosition(fenceBlock.position() + 3);
    tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
    if (!lang.isEmpty())
        tc.insertText(lang);
    setTextCursor(cursor);
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    for (QAction *action : m_insertActions)
        menu.addAction(action);

    CursorContext ctx = detectCursorContext();

    if (ctx == CursorContext::ListItem) {
        menu.addSeparator();
        QAction *indentAction = menu.addAction("Increase Indent");
        connect(indentAction, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString line = currentLineText();
            QString indented = indentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(indented);
        });

        QAction *dedentAction = menu.addAction("Decrease Indent");
        connect(dedentAction, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString line = currentLineText();
            QString outdented = outdentListLine(line);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.insertText(outdented);
        });

        static const QRegularExpression taskRe(R"(^\s*[-*+]\s\[)");
        if (taskRe.match(currentLineText()).hasMatch()) {
            QAction *toggleAction = menu.addAction("Toggle Checkbox");
            connect(toggleAction, &QAction::triggered, this, &Editor::toggleCheckbox);
        }
    }

    if (ctx == CursorContext::TableRow) {
        menu.addSeparator();
        QAction *above = menu.addAction("Insert Row Above");
        connect(above, &QAction::triggered, this, [this]() { insertTableRow(true); });

        QAction *below = menu.addAction("Insert Row Below");
        connect(below, &QAction::triggered, this, [this]() { insertTableRow(false); });

        menu.addSeparator();

        QAction *colLeft = menu.addAction("Insert Column Left");
        connect(colLeft, &QAction::triggered, this, [this]() { insertTableCol(true); });

        QAction *colRight = menu.addAction("Insert Column Right");
        connect(colRight, &QAction::triggered, this, [this]() { insertTableCol(false); });

        menu.addSeparator();

        QAction *delRow = menu.addAction("Delete Row");
        connect(delRow, &QAction::triggered, this, &Editor::deleteTableRow);

        QAction *delCol = menu.addAction("Delete Column");
        connect(delCol, &QAction::triggered, this, &Editor::deleteTableCol);
    }

    if (ctx == CursorContext::CodeBlock) {
        menu.addSeparator();
        QAction *langAction = menu.addAction("Change Language...");
        connect(langAction, &QAction::triggered, this, &Editor::changeCodeLanguage);
    }

    if (cursorHasUrlSelection()) {
        menu.addSeparator();
        QAction *makeLink = menu.addAction("Make Link");
        connect(makeLink, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString url = cursor.selectedText();
            cursor.insertText("[](" + url + ")");
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, url.size() + 3);
            setTextCursor(cursor);
        });
    }

    if (cursorHasImagePathSelection()) {
        menu.addSeparator();
        QAction *insertImg = menu.addAction("Insert Image");
        connect(insertImg, &QAction::triggered, this, [this]() {
            QTextCursor cursor = textCursor();
            QString path = cursor.selectedText();
            cursor.insertText("![](" + path + ")");
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, path.size() + 4);
            setTextCursor(cursor);
        });
    }

    if (!menu.isEmpty())
        menu.exec(event->globalPos());
}

void Editor::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    if (m_centerContent && !m_inResize) {
        m_inResize = true;
        updateViewportMargins();
        m_inResize = false;
    }
}

void Editor::updateViewportMargins()
{
    if (!m_centerContent) {
        setViewportMargins(0, 0, 0, 0);
        return;
    }
    int editorWidth = viewport()->width();
    int scrollbarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
    int available = editorWidth - scrollbarWidth;
    int margin = qMax(0, (available - m_centerContentWidth) / 2);
    setViewportMargins(margin, 0, margin, 0);
}

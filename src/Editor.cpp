#include "Editor.h"
#include "StaticHelpers.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
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
            QModelIndex idx = m_completer->popup()->currentIndex();
            if (idx.isValid()) {
                acceptCompletion(m_completer->completionModel()->data(idx).toString());
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
        QString line = cursor.block().text();
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
            if (isList) {
                QString indented = indentListLine(line);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                cursor.insertText(indented);
                return;
            }
            QString partialPath;
            if (isInsideLinkContext(cursor, partialPath)) {
                showFileCompletion(partialPath);
                return;
            }
            insertPlainText("    ");
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
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

    if (entries.isEmpty())
        return;

    if (filePart.isEmpty())
        return;

    if (entries.size() == 1) {
        acceptCompletion(entries.first());
        return;
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

    QStringListModel *model = new QStringListModel(entries, m_completer);
    m_completer->setModel(model);
    m_completer->setCompletionPrefix(filePart);
    m_completer->popup()->setCurrentIndex(model->index(0, 0));

    QRect cr = cursorRect();
    QFontMetrics fm(font());
    int maxWidth = 0;
    for (const QString &entry : entries)
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(entry));
    cr.setWidth(maxWidth + 30);
    m_completer->complete(cr);
}

void Editor::acceptCompletion(const QString &completion)
{
    if (m_completer)
        m_completer->popup()->hide();

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
}

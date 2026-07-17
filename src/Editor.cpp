#include "Editor.h"
#include "StaticHelpers.h"
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QScrollBar>
#include <QTextBlock>

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

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab) {
        insertPlainText("    ");
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
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

    QPlainTextEdit::keyPressEvent(event);
}

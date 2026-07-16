#include "Editor.h"
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QScrollBar>

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName("scriba-editor");
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
    setFrameShape(QFrame::NoFrame);

    QTextBlockFormat fmt;
    fmt.setLineHeight(190, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
}

void Editor::applyStylesheet(const QString &css)
{
    setStyleSheet(css);
    verticalScrollBar()->setFixedWidth(12);
    horizontalScrollBar()->setFixedHeight(12);
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab) {
        insertPlainText("    ");
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

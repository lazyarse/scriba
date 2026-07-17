#ifndef STATICHELPERS_H
#define STATICHELPERS_H

#include <QString>
#include <QTextCursor>

QString escapeJsString(const QString &s);
int extractContentWidth(const QString &css);
QString handleListReturn(const QString &line);
QString indentListLine(const QString &line);
QString outdentListLine(const QString &line);
QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column);

#endif

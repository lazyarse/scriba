#ifndef STATICHELPERS_H
#define STATICHELPERS_H

#include <QString>
#include <QTextCursor>
#include <QIcon>

QString escapeJsString(const QString &s);
int extractContentWidth(const QString &css);
QString handleListReturn(const QString &line);
QString handleTableReturn(const QString &line, const QString &prevLine);
QString makeEmptyTableRow(int cols);
QString makeEmptyHtmlTableRow(int cols);
int tableNavCell(const QString &line, int cursorPos, bool forward);
int tableNavHtmlCell(const QString &line, int cursorPos, bool forward);
QString indentListLine(const QString &line);
QString outdentListLine(const QString &line);
QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column);

QString firstFontFamily(const QString &cssFontStack);
int countSentences(const QString &text);
int estimateSyllables(const QString &word);
double fleschKincaidGrade(int words, int sentences, int syllables);

QIcon themedIcon(const QString &svgPath, const QColor &color, int size = 28);

#endif

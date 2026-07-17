#ifndef STATICHELPERS_H
#define STATICHELPERS_H

#include <QString>

QString escapeJsString(const QString &s);
int extractContentWidth(const QString &css);
QString handleListReturn(const QString &line);
QString indentListLine(const QString &line);
QString outdentListLine(const QString &line);

#endif

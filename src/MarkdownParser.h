#ifndef MARKDOWNPARSER_H
#define MARKDOWNPARSER_H

#include <QString>

class MarkdownParser
{
public:
    static QString toHtml(const QString &markdown);
};

#endif

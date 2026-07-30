#pragma once

#include <QString>

class MarkdownParser
{
public:
    static QString toHtml(const QString &markdown, bool noHtml = false);
};


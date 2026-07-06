#include "MarkdownParser.h"
#include "MdRenderer.h"
#include <md4c.h>

QString MarkdownParser::toHtml(const QString &markdown)
{
    QByteArray utf8 = markdown.toUtf8();

    unsigned long parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH
                              | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEURLAUTOLINKS
                              | MD_FLAG_ADMONITIONS;

    MdRenderer renderer;
    return renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), parserFlags);
}

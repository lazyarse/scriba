#include "MarkdownParser.h"
#include <md4c.h>
#include <md4c-html.h>

struct OutputBuffer {
    QString result;
};

static void processOutput(const MD_CHAR *data, MD_SIZE size, void *userdata)
{
    auto *buffer = static_cast<OutputBuffer*>(userdata);
    buffer->result.append(QString::fromUtf8(data, size));
}

QString MarkdownParser::toHtml(const QString &markdown)
{
    QByteArray utf8 = markdown.toUtf8();
    OutputBuffer buffer;

    unsigned long parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH
                              | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEURLAUTOLINKS;
    unsigned long renderFlags = 0;

    md_html(utf8.constData(), static_cast<MD_SIZE>(utf8.size()),
            processOutput, &buffer, parserFlags, renderFlags);

    return buffer.result;
}

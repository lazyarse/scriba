#include "MdRenderer.h"

MdRenderer::MdRenderer()
{
}

QString MdRenderer::render(const char *input, MD_SIZE size, unsigned parserFlags)
{
    m_output.clear();
    m_currentLine = 1;

    MD_PARSER parser = {};
    parser.abi_version = 0;
    parser.flags = parserFlags;
    parser.enter_block = enterBlock;
    parser.leave_block = leaveBlock;
    parser.enter_span = enterSpan;
    parser.leave_span = leaveSpan;
    parser.text = text;
    parser.debug_log = nullptr;
    parser.syntax = nullptr;

    md_parse(input, size, &parser, this);
    return m_output;
}

int MdRenderer::enterBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    auto *self = static_cast<MdRenderer*>(userdata);

    switch (type) {
    case MD_BLOCK_DOC:
        break;
    case MD_BLOCK_P:
        self->writeHtml(QString("<p data-line=\"%1\">").arg(self->m_currentLine));
        break;
    case MD_BLOCK_H: {
        auto *d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        self->writeHtml(QString("<h%1 data-line=\"%2\">").arg(d->level).arg(self->m_currentLine));
        break;
    }
    case MD_BLOCK_CODE: {
        auto *d = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
        QString lang;
        if (d->lang.text && d->lang.size > 0)
            lang = QString::fromUtf8(d->lang.text, d->lang.size);
        if (d->fence_char) {
            self->writeHtml(QString("<pre data-line=\"%1\"><code class=\"language-%2\">")
                .arg(self->m_currentLine).arg(lang));
        } else {
            self->writeHtml(QString("<pre data-line=\"%1\"><code>")
                .arg(self->m_currentLine));
        }
        break;
    }
    case MD_BLOCK_UL:
        self->writeHtml("<ul>");
        break;
    case MD_BLOCK_OL:
        self->writeHtml("<ol>");
        break;
    case MD_BLOCK_LI:
        self->writeHtml(QString("<li data-line=\"%1\">").arg(self->m_currentLine));
        break;
    case MD_BLOCK_HR:
        self->writeHtml(QString("<hr data-line=\"%1\">").arg(self->m_currentLine));
        break;
    case MD_BLOCK_HTML:
        break;
    case MD_BLOCK_QUOTE:
        self->writeHtml("<blockquote>");
        break;
    case MD_BLOCK_ADMONITION: {
        auto *d = static_cast<MD_BLOCK_ADMONITION_DETAIL*>(detail);
        QString type;
        if (d->type.text && d->type.size > 0)
            type = QString::fromUtf8(d->type.text, d->type.size);
        QString title = type.left(1).toUpper() + type.mid(1);
        self->writeHtml(QString("<div class=\"admonition %1\" data-line=\"%2\">"
            "<p class=\"admonition-title\">%3</p>")
            .arg(type, QString::number(self->m_currentLine), title));
        break;
    }
    case MD_BLOCK_TABLE:
        self->writeHtml("<table>");
        break;
    case MD_BLOCK_THEAD:
        self->writeHtml("<thead>");
        break;
    case MD_BLOCK_TBODY:
        self->writeHtml("<tbody>");
        break;
    case MD_BLOCK_TR:
        self->writeHtml("<tr>");
        break;
    case MD_BLOCK_TH: {
        auto *d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
        QString align;
        switch (d->align) {
        case MD_ALIGN_LEFT:    align = "left"; break;
        case MD_ALIGN_CENTER:  align = "center"; break;
        case MD_ALIGN_RIGHT:   align = "right"; break;
        default:               break;
        }
        if (align.isEmpty())
            self->writeHtml("<th>");
        else
            self->writeHtml(QString("<th style=\"text-align: %1\">").arg(align));
        break;
    }
    case MD_BLOCK_TD: {
        auto *d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
        QString align;
        switch (d->align) {
        case MD_ALIGN_LEFT:    align = "left"; break;
        case MD_ALIGN_CENTER:  align = "center"; break;
        case MD_ALIGN_RIGHT:   align = "right"; break;
        default:               break;
        }
        if (align.isEmpty())
            self->writeHtml("<td>");
        else
            self->writeHtml(QString("<td style=\"text-align: %1\">").arg(align));
        break;
    }
    default:
        break;
    }
    return 0;
}

int MdRenderer::leaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    auto *self = static_cast<MdRenderer*>(userdata);

    switch (type) {
    case MD_BLOCK_DOC:
        break;
    case MD_BLOCK_P:
        self->writeHtml("</p>");
        break;
    case MD_BLOCK_H: {
        auto *d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        self->writeHtml(QString("</h%1>").arg(d->level));
        break;
    }
    case MD_BLOCK_CODE:
        self->writeHtml("</code></pre>");
        break;
    case MD_BLOCK_UL:
        self->writeHtml("</ul>");
        break;
    case MD_BLOCK_OL:
        self->writeHtml("</ol>");
        break;
    case MD_BLOCK_LI:
        self->writeHtml("</li>");
        break;
    case MD_BLOCK_HR:
        break;
    case MD_BLOCK_HTML:
        break;
    case MD_BLOCK_QUOTE:
        self->writeHtml("</blockquote>");
        break;
    case MD_BLOCK_ADMONITION:
        self->writeHtml("</div>");
        break;
    case MD_BLOCK_TABLE:
        self->writeHtml("</table>");
        break;
    case MD_BLOCK_THEAD:
        self->writeHtml("</thead>");
        break;
    case MD_BLOCK_TBODY:
        self->writeHtml("</tbody>");
        break;
    case MD_BLOCK_TR:
        self->writeHtml("</tr>");
        break;
    case MD_BLOCK_TH:
        self->writeHtml("</th>");
        break;
    case MD_BLOCK_TD:
        self->writeHtml("</td>");
        break;
    default:
        break;
    }
    return 0;
}

int MdRenderer::enterSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    auto *self = static_cast<MdRenderer*>(userdata);

    switch (type) {
    case MD_SPAN_EM:
        self->writeHtml("<em>");
        break;
    case MD_SPAN_STRONG:
        self->writeHtml("<strong>");
        break;
    case MD_SPAN_A: {
        auto *d = static_cast<MD_SPAN_A_DETAIL*>(detail);
        QString href;
        if (d->href.text && d->href.size > 0)
            href = QString::fromUtf8(d->href.text, d->href.size);
        QString title;
        if (d->title.text && d->title.size > 0)
            title = QString::fromUtf8(d->title.text, d->title.size);
        if (title.isEmpty())
            self->writeHtml(QString("<a href=\"%1\">").arg(escapeAttr(href)));
        else
            self->writeHtml(QString("<a href=\"%1\" title=\"%2\">")
                .arg(escapeAttr(href), escapeAttr(title)));
        break;
    }
    case MD_SPAN_IMG: {
        auto *d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
        QString src;
        if (d->src.text && d->src.size > 0)
            src = QString::fromUtf8(d->src.text, d->src.size);
        QString title;
        if (d->title.text && d->title.size > 0)
            title = QString::fromUtf8(d->title.text, d->title.size);
        if (title.isEmpty())
            self->writeHtml(QString("<img src=\"%1\" alt=\"\">").arg(escapeAttr(src)));
        else
            self->writeHtml(QString("<img src=\"%1\" title=\"%2\" alt=\"\">")
                .arg(escapeAttr(src), escapeAttr(title)));
        break;
    }
    case MD_SPAN_CODE:
        self->writeHtml("<code>");
        break;
    case MD_SPAN_DEL:
        self->writeHtml("<del>");
        break;
    case MD_SPAN_U:
        self->writeHtml("<u>");
        break;
    default:
        break;
    }
    return 0;
}

int MdRenderer::leaveSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    auto *self = static_cast<MdRenderer*>(userdata);

    switch (type) {
    case MD_SPAN_EM:
        self->writeHtml("</em>");
        break;
    case MD_SPAN_STRONG:
        self->writeHtml("</strong>");
        break;
    case MD_SPAN_A:
        self->writeHtml("</a>");
        break;
    case MD_SPAN_IMG:
        break;
    case MD_SPAN_CODE:
        self->writeHtml("</code>");
        break;
    case MD_SPAN_DEL:
        self->writeHtml("</del>");
        break;
    case MD_SPAN_U:
        self->writeHtml("</u>");
        break;
    default:
        break;
    }
    return 0;
}

int MdRenderer::text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    auto *self = static_cast<MdRenderer*>(userdata);

    switch (type) {
    case MD_TEXT_NORMAL: {
        for (MD_SIZE i = 0; i < size; i++) {
            if (text[i] == '\n')
                self->m_currentLine++;
        }
        self->writeHtml(QString::fromUtf8(text, size));
        break;
    }
    case MD_TEXT_BR:
        self->m_currentLine++;
        self->writeHtml("<br>");
        break;
    case MD_TEXT_SOFTBR:
        self->m_currentLine++;
        self->writeHtml("\n");
        break;
    case MD_TEXT_CODE:
        self->writeHtml(escapeHtml(QString::fromUtf8(text, size)));
        break;
    case MD_TEXT_HTML:
        self->writeHtml(QString::fromUtf8(text, size));
        break;
    case MD_TEXT_ENTITY:
        self->writeHtml(QString::fromUtf8(text, size));
        break;
    default:
        break;
    }
    return 0;
}

void MdRenderer::writeHtml(const char *data, MD_SIZE size)
{
    m_output.append(QString::fromUtf8(data, size));
}

void MdRenderer::writeHtml(const QString &str)
{
    m_output.append(str);
}

QString MdRenderer::escapeHtml(const QString &str)
{
    QString result = str;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    return result;
}

QString MdRenderer::escapeAttr(const QString &str)
{
    QString result = str;
    result.replace("&", "&amp;");
    result.replace("\"", "&quot;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    return result;
}

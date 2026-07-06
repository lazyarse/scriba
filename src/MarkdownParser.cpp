#include "MarkdownParser.h"
#include <md4c.h>

struct HtmlRenderer {
    QString result;
    bool inImage = false;
    bool inParagraph = false;
    bool paragraphHasText = false;
    bool paragraphOnlyImage = false;
};

static int enterBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    Q_UNUSED(detail)
    auto *r = static_cast<HtmlRenderer*>(userdata);
    switch (type) {
        case MD_BLOCK_DOC: r->result += ""; break;
        case MD_BLOCK_QUOTE: r->result += "<blockquote>"; break;
        case MD_BLOCK_UL: r->result += "<ul>"; break;
        case MD_BLOCK_OL: r->result += "<ol>"; break;
        case MD_BLOCK_LI: r->result += "<li>"; break;
        case MD_BLOCK_HR: r->result += "<hr>"; break;
        case MD_BLOCK_H: {
            auto *d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
            r->result += QString("<h%1>").arg(d->level);
            break;
        }
        case MD_BLOCK_CODE: r->result += "<pre><code>"; break;
        case MD_BLOCK_P:
            r->inParagraph = true;
            r->paragraphHasText = false;
            r->paragraphOnlyImage = false;
            r->result += "<!--P_START-->";
            break;
        case MD_BLOCK_HTML: break;
        case MD_BLOCK_TABLE: r->result += "<table>"; break;
        case MD_BLOCK_THEAD: r->result += "<thead>"; break;
        case MD_BLOCK_TBODY: r->result += "<tbody>"; break;
        case MD_BLOCK_TR: r->result += "<tr>"; break;
        case MD_BLOCK_TH: r->result += "<th>"; break;
        case MD_BLOCK_TD: r->result += "<td>"; break;
        default: break;
    }
    return 0;
}

static int leaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    Q_UNUSED(detail)
    auto *r = static_cast<HtmlRenderer*>(userdata);
    switch (type) {
        case MD_BLOCK_DOC: break;
        case MD_BLOCK_QUOTE: r->result += "</blockquote>"; break;
        case MD_BLOCK_UL: r->result += "</ul>"; break;
        case MD_BLOCK_OL: r->result += "</ol>"; break;
        case MD_BLOCK_LI: r->result += "</li>"; break;
        case MD_BLOCK_HR: break;
        case MD_BLOCK_H: {
            auto *d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
            r->result += QString("</h%1>").arg(d->level);
            break;
        }
        case MD_BLOCK_CODE: r->result += "</code></pre>"; break;
        case MD_BLOCK_P: {
            r->inParagraph = false;
            QString marker = "<!--P_START-->";
            int pos = r->result.lastIndexOf(marker);
            if (r->paragraphOnlyImage && !r->paragraphHasText) {
                r->result.replace(pos, marker.length(), "");
            } else {
                r->result.replace(pos, marker.length(), "<p>");
                r->result += "</p>";
            }
            break;
        }
        case MD_BLOCK_HTML: break;
        case MD_BLOCK_TABLE: r->result += "</table>"; break;
        case MD_BLOCK_THEAD: r->result += "</thead>"; break;
        case MD_BLOCK_TBODY: r->result += "</tbody>"; break;
        case MD_BLOCK_TR: r->result += "</tr>"; break;
        case MD_BLOCK_TH: r->result += "</th>"; break;
        case MD_BLOCK_TD: r->result += "</td>"; break;
        default: break;
    }
    return 0;
}

static int enterSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    Q_UNUSED(detail)
    auto *r = static_cast<HtmlRenderer*>(userdata);
    switch (type) {
        case MD_SPAN_EM: r->result += "<em>"; break;
        case MD_SPAN_STRONG: r->result += "<strong>"; break;
        case MD_SPAN_A: {
            auto *d = static_cast<MD_SPAN_A_DETAIL*>(detail);
            r->result += "<a href=\"";
            r->result += QString::fromUtf8(d->href.text, d->href.size);
            r->result += "\">";
            break;
        }
        case MD_SPAN_IMG: {
            auto *d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
            r->inImage = true;
            if (r->inParagraph) r->paragraphOnlyImage = true;
            r->result += "<img src=\"";
            r->result += QString::fromUtf8(d->src.text, d->src.size);
            r->result += "\">";
            break;
        }
        case MD_SPAN_CODE: r->result += "<code>"; break;
        case MD_SPAN_DEL: r->result += "<del>"; break;
        case MD_SPAN_LATEXMATH: r->result += "<code class=\"latexmath\">"; break;
        case MD_SPAN_WIKILINK: r->result += "<a>"; break;
        case MD_SPAN_U: r->result += "<u>"; break;
        case MD_SPAN_MARK: r->result += "<mark>"; break;
        case MD_SPAN_SPOILER: r->result += "<span class=\"spoiler\">"; break;
        case MD_SPAN_SUPERSCRIPT: r->result += "<sup>"; break;
        case MD_SPAN_SUBSCRIPT: r->result += "<sub>"; break;
        default: break;
    }
    return 0;
}

static int leaveSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    Q_UNUSED(detail)
    auto *r = static_cast<HtmlRenderer*>(userdata);
    switch (type) {
        case MD_SPAN_EM: r->result += "</em>"; break;
        case MD_SPAN_STRONG: r->result += "</strong>"; break;
        case MD_SPAN_A: r->result += "</a>"; break;
        case MD_SPAN_IMG:
            r->inImage = false;
            break;
        case MD_SPAN_CODE: r->result += "</code>"; break;
        case MD_SPAN_DEL: r->result += "</del>"; break;
        case MD_SPAN_LATEXMATH: r->result += "</code>"; break;
        case MD_SPAN_WIKILINK: r->result += "</a>"; break;
        case MD_SPAN_U: r->result += "</u>"; break;
        case MD_SPAN_MARK: r->result += "</mark>"; break;
        case MD_SPAN_SPOILER: r->result += "</span>"; break;
        case MD_SPAN_SUPERSCRIPT: r->result += "</sup>"; break;
        case MD_SPAN_SUBSCRIPT: r->result += "</sub>"; break;
        default: break;
    }
    return 0;
}

static int textCallback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    Q_UNUSED(type)
    auto *r = static_cast<HtmlRenderer*>(userdata);
    if (r->inImage) return 0;
    if (r->inParagraph && !r->paragraphHasText) {
        for (MD_SIZE i = 0; i < size; i++) {
            if (text[i] != ' ' && text[i] != '\n' && text[i] != '\r' && text[i] != '\t') {
                r->paragraphHasText = true;
                break;
            }
        }
    }
    r->result += QString::fromUtf8(text, size);
    return 0;
}

QString MarkdownParser::toHtml(const QString &markdown)
{
    QByteArray utf8 = markdown.toUtf8();
    HtmlRenderer renderer;

    MD_PARSER parser = {};
    parser.abi_version = 0;
    parser.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEURLAUTOLINKS;
    parser.enter_block = enterBlock;
    parser.leave_block = leaveBlock;
    parser.enter_span = enterSpan;
    parser.leave_span = leaveSpan;
    parser.text = textCallback;
    parser.debug_log = nullptr;
    parser.syntax = nullptr;

    md_parse(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), &parser, &renderer);

    return renderer.result;
}

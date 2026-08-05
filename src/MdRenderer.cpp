// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "MdRenderer.h"
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcMd4c, "scriba.md4c")

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
    parser.debug_log = [](const char *msg, void *userdata) {
        Q_UNUSED(userdata);
        qCDebug(lcMd4c) << msg;
    };
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
        self->m_typoState.lastChar = QChar(' ');
        self->writeHtml(QString("<p data-line=\"%1\">").arg(self->m_currentLine));
        break;
    case MD_BLOCK_H: {
        auto *d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        self->m_typoState.lastChar = QChar(' ');
        self->writeHtml(QString("<h%1 data-line=\"%2\">").arg(d->level).arg(self->m_currentLine));
        break;
    }
    case MD_BLOCK_CODE:
        self->enterCodeBlock(detail);
        break;
    case MD_BLOCK_UL:
        self->writeHtml("<ul>");
        break;
    case MD_BLOCK_OL:
        self->writeHtml("<ol>");
        break;
    case MD_BLOCK_LI:
        self->m_typoState.lastChar = QChar(' ');
        self->enterListItem(detail);
        break;
    case MD_BLOCK_HR:
        self->writeHtml(QString("<hr data-line=\"%1\">").arg(self->m_currentLine));
        break;
    case MD_BLOCK_HTML:
        break;
    case MD_BLOCK_QUOTE:
        self->m_typoState.lastChar = QChar(' ');
        self->writeHtml("<blockquote>");
        break;
    case MD_BLOCK_ADMONITION:
        self->enterAdmonition(detail);
        break;
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
    case MD_BLOCK_TH:
        self->m_typoState.lastChar = QChar(' ');
        self->enterAlignedCell(detail, "th");
        break;
    case MD_BLOCK_TD:
        self->m_typoState.lastChar = QChar(' ');
        self->enterAlignedCell(detail, "td");
        break;
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
            title = href;
        self->writeHtml(QString("<a href=\"%1\" title=\"%2\">")
            .arg(escapeAttr(href), escapeAttr(title)));
        break;
    }
    case MD_SPAN_IMG: {
        auto *d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
        self->m_img.inside = true;
        self->m_img.alt.clear();
        self->m_img.src.clear();
        self->m_img.title.clear();
        if (d->src.text && d->src.size > 0)
            self->m_img.src = QString::fromUtf8(d->src.text, d->src.size);
        if (d->title.text && d->title.size > 0)
            self->m_img.title = QString::fromUtf8(d->title.text, d->title.size);
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
        if (!self->m_img.inside)
            break;
        /* Suppress other spans inside image alt text */
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
        if (self->m_img.inside) break;
        self->writeHtml("</a>");
        break;
    case MD_SPAN_IMG: {
        QString cleanSrc;
        int width = -1, height = -1;
        parseDimensions(self->m_img.src, cleanSrc, width, height);

        QString tag = QString("<img src=\"%1\" alt=\"%2\"")
            .arg(escapeAttr(cleanSrc), escapeAttr(self->m_img.alt));
        if (!self->m_img.title.isEmpty())
            tag += QString(" title=\"%1\"").arg(escapeAttr(self->m_img.title));
        QStringList dimStyles;
        if (width >= 0)
            dimStyles += QString("max-width: %1px").arg(width);
        if (height >= 0)
            dimStyles += QString("max-height: %1px").arg(height);
        if (!dimStyles.isEmpty())
            tag += " style=\"" + dimStyles.join("; ") + "\"";
        tag += ">";
        self->writeHtml(tag);
        self->m_img.inside = false;
        break;
    }
    case MD_SPAN_CODE:
        if (self->m_img.inside) break;
        self->writeHtml("</code>");
        break;
    case MD_SPAN_DEL:
        if (self->m_img.inside) break;
        self->writeHtml("</del>");
        break;
    case MD_SPAN_U:
        if (self->m_img.inside) break;
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
        if (self->m_img.inside) {
            self->m_img.alt += QString::fromUtf8(text, size);
        } else {
            QString raw = QString::fromUtf8(text, size);
            if (self->m_typography)
                raw = Typography::apply(raw, self->m_typography, self->m_typoState);
            self->writeHtml(escapeHtml(raw));
        }
        break;
    }
    case MD_TEXT_BR:
        self->m_currentLine++;
        if (!self->m_img.inside) {
            self->m_typoState.lastChar = QChar(' ');
            self->writeHtml("<br>");
        }
        break;
    case MD_TEXT_SOFTBR:
        self->m_currentLine++;
        if (!self->m_img.inside) {
            self->m_typoState.lastChar = QChar(' ');
            self->writeHtml("\n");
        }
        break;
    case MD_TEXT_CODE:
        if (self->m_img.inside) {
            self->m_img.alt += QString::fromUtf8(text, size);
        } else {
            self->writeHtml(escapeHtml(QString::fromUtf8(text, size)));
        }
        break;
    case MD_TEXT_HTML:
        if (!self->m_img.inside)
            self->writeHtml(QString::fromUtf8(text, size));
        break;
    case MD_TEXT_ENTITY:
        if (self->m_img.inside) {
            self->m_img.alt += QString::fromUtf8(text, size);
        } else {
            self->writeHtml(QString::fromUtf8(text, size));
        }
        break;
    default:
        break;
    }
    return 0;
}

void MdRenderer::enterCodeBlock(void *detail)
{
    auto *d = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
    QString lang;
    if (d->lang.text && d->lang.size > 0)
        lang = QString::fromUtf8(d->lang.text, d->lang.size);
    if (d->fence_char) {
        if (lang.isEmpty()) {
            writeHtml(QString("<pre data-line=\"%1\"><code class=\"language-\">")
                .arg(m_currentLine));
        } else {
            writeHtml(QString("<pre data-line=\"%1\" data-lang=\"%2\"><code class=\"language-%2\">")
                .arg(m_currentLine).arg(lang));
        }
    } else {
        writeHtml(QString("<pre data-line=\"%1\"><code>").arg(m_currentLine));
    }
}

void MdRenderer::enterListItem(void *detail)
{
    auto *d = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
    if (d->is_task) {
        writeHtml("<li class=\"task-list-item\" data-line=\"" +
                  QString::number(m_currentLine) + "\">"
                  "<input type=\"checkbox\" class=\"task-list-item-checkbox\" disabled");
        if (d->task_mark == 'x' || d->task_mark == 'X')
            writeHtml(" checked");
        writeHtml(">");
    } else {
        writeHtml(QString("<li data-line=\"%1\">").arg(m_currentLine));
    }
}

void MdRenderer::enterAdmonition(void *detail)
{
    auto *d = static_cast<MD_BLOCK_ADMONITION_DETAIL*>(detail);
    QString type;
    if (d->type.text && d->type.size > 0)
        type = QString::fromUtf8(d->type.text, d->type.size);
    QString title;
    if (d->title.text && d->title.size > 0)
        title = QString::fromUtf8(d->title.text, d->title.size);
    else
        title = type.left(1).toUpper() + type.mid(1);
    writeHtml(QString("<div class=\"admonition %1\" data-line=\"%2\">"
        "<p class=\"admonition-title\">%3</p>")
        .arg(type, QString::number(m_currentLine), title));
}

void MdRenderer::enterAlignedCell(void *detail, const char *tag)
{
    auto *d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
    QString align = alignmentStyle(d->align);
    if (align.isEmpty())
        writeHtml(QString("<%1>").arg(tag));
    else
        writeHtml(QString("<%1 style=\"text-align: %2\">").arg(tag, align));
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
    return escapeHtml(str);
}

QString MdRenderer::alignmentStyle(MD_ALIGN align)
{
    switch (align) {
    case MD_ALIGN_LEFT:    return "left";
    case MD_ALIGN_CENTER:  return "center";
    case MD_ALIGN_RIGHT:   return "right";
    default:               return {};
    }
}

void MdRenderer::parseDimensions(const QString &src, QString &cleanSrc, int &width, int &height)
{
    width = -1;
    height = -1;
    cleanSrc = src;

    static const QRegularExpression re(QStringLiteral(R"(^(.*?)#(\d*)x(\d*)$)"));
    auto match = re.match(src);
    if (match.hasMatch()) {
        cleanSrc = match.captured(1);
        if (!match.captured(2).isEmpty())
            width = match.captured(2).toInt();
        if (!match.captured(3).isEmpty())
            height = match.captured(3).toInt();
    }
}

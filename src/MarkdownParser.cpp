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
#include "MarkdownParser.h"
#include "MdRenderer.h"
#include "Preferences.h"
#include "Typography.h"
#include <md4c.h>
#include <QSettings>

QString MarkdownParser::toHtml(const QString &markdown, bool noHtml)
{
    QByteArray utf8 = markdown.toUtf8();

    unsigned long parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH
                              | MD_FLAG_TASKLISTS | MD_FLAG_PERMISSIVEAUTOLINKS
                              | MD_FLAG_ADMONITIONS | MD_FLAG_HIGHLIGHT
                              | MD_FLAG_SUPERSCRIPTS | MD_FLAG_SUBSCRIPTS
                              | MD_FLAG_SPOILERS | MD_FLAG_FOOTNOTES;

    QSettings settings;
    if (settings.value(Preferences::HardSoftBreaks, false).toBool())
        parserFlags |= MD_FLAG_HARD_SOFT_BREAKS;

    if (noHtml)
        parserFlags |= MD_FLAG_NOHTML;

    MdRenderer renderer;
    renderer.setTypography(Typography::optionsFromSettings());
    return renderer.render(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), parserFlags);
}

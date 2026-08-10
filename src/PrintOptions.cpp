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
#include "PrintOptions.h"
#include "Preferences.h"
#include <QSettings>
#include <QStringList>

namespace PrintOptions {

Options fromSettings()
{
    QSettings settings;
    Options o;

    const QString split = settings.value(Preferences::PrintCodeSplit,
        QStringLiteral("never")).toString();
    if (split == QLatin1String("small"))
        o.codeSplit = CodeSplit::SplitSmall;
    else if (split == QLatin1String("large"))
        o.codeSplit = CodeSplit::SplitLarge;
    else
        o.codeSplit = CodeSplit::NeverSplit;

    o.keepTables = settings.value(Preferences::PrintKeepTables, true).toBool();
    o.keepHeadings = settings.value(Preferences::PrintKeepHeadings, true).toBool();
    o.keepFigures = settings.value(Preferences::PrintKeepFigures, true).toBool();
    o.orphanControl = settings.value(Preferences::PrintOrphanControl, true).toBool();
    o.pageMargin = settings.value(Preferences::PrintPageMargin, QString()).toString();
    o.pageSize = settings.value(Preferences::PrintPageSize, QString()).toString();
    return o;
}

void toSettings(const Options &opts)
{
    QSettings settings;
    switch (opts.codeSplit) {
    case CodeSplit::SplitSmall: settings.setValue(Preferences::PrintCodeSplit, "small"); break;
    case CodeSplit::SplitLarge: settings.setValue(Preferences::PrintCodeSplit, "large"); break;
    case CodeSplit::NeverSplit: settings.setValue(Preferences::PrintCodeSplit, "never"); break;
    }
    settings.setValue(Preferences::PrintKeepTables, opts.keepTables);
    settings.setValue(Preferences::PrintKeepHeadings, opts.keepHeadings);
    settings.setValue(Preferences::PrintKeepFigures, opts.keepFigures);
    settings.setValue(Preferences::PrintOrphanControl, opts.orphanControl);
    settings.setValue(Preferences::PrintPageMargin, opts.pageMargin);
    settings.setValue(Preferences::PrintPageSize, opts.pageSize);
}

QString buildCss(const Options &opts)
{
    QStringList frags;
    switch (opts.codeSplit) {
    case CodeSplit::SplitSmall:
        frags << "pre.scriba-split-small{break-inside:auto;page-break-inside:auto}";
        break;
    case CodeSplit::SplitLarge:
        frags << "pre.scriba-split-large{break-inside:auto;page-break-inside:auto}";
        break;
    case CodeSplit::NeverSplit:
        break;
    }
    if (!opts.keepTables)
        frags << "table{break-inside:auto;page-break-inside:auto}";
    if (!opts.keepHeadings)
        frags << "h1,h2,h3,h4,h5,h6{break-after:auto;page-break-after:auto}";
    if (!opts.keepFigures)
        frags << ".mermaid,.katex-display,.admonition,pre{break-inside:auto;page-break-inside:auto}";
    if (!opts.orphanControl)
        frags << "p{orphans:1;widows:1}";
    return frags.join(QLatin1Char('\n'));
}

QString buildPageOverrideCss(const Options &opts)
{
    if (opts.pageSize.isEmpty() && opts.pageMargin.isEmpty())
        return {};

    QStringList props;
    if (!opts.pageSize.isEmpty())
        props << QStringLiteral("size:") + opts.pageSize;
    if (!opts.pageMargin.isEmpty())
        props << QStringLiteral("margin:") + opts.pageMargin;
    return QStringLiteral("@page{") + props.join(QLatin1Char(';')) + QLatin1Char('}');
}

}

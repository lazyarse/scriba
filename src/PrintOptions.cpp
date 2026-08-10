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
#include <QPageSize>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>

namespace PrintOptions {

enum class CssUnit { Mm, Cm, In, Pt, Px, Pc };

static CssUnit parseCssUnit(const QString &u)
{
    if (u == QStringLiteral("mm")) return CssUnit::Mm;
    if (u == QStringLiteral("cm")) return CssUnit::Cm;
    if (u == QStringLiteral("in")) return CssUnit::In;
    if (u == QStringLiteral("pt")) return CssUnit::Pt;
    if (u == QStringLiteral("px")) return CssUnit::Px;
    if (u == QStringLiteral("pc")) return CssUnit::Pc;
    return CssUnit::Px;
}

static qreal cssLengthToPt(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^(-?\\d+(?:\\.\\d+)?)\\s*(mm|cm|in|pt|px|pc)?$"));
    auto m = re.match(s.trimmed());
    if (!m.hasMatch()) return 0;
    qreal v = m.captured(1).toDouble();
    QString u = m.captured(2);
    switch (parseCssUnit(u)) {
        case CssUnit::Mm: return v * 72.0 / 25.4;
        case CssUnit::Cm: return v * 72.0 / 2.54;
        case CssUnit::In: return v * 72.0;
        case CssUnit::Pt: return v;
        case CssUnit::Pc: return v * 12.0;
        case CssUnit::Px: return v * 0.75;
    }
    return v * 0.75;
}

enum class PageOrientation { Portrait, Landscape };

enum class NamedPageSize { A3, A4, A5, Letter, Legal, Tabloid, Ledger, B4, B5 };

static QSizeF namedPageSizeToSize(const QString &name)
{
    QString n = name.trimmed().toLower();
    if (n == QLatin1String("a3")) return QPageSize(QPageSize::A3).size(QPageSize::Point);
    if (n == QLatin1String("a4")) return QPageSize(QPageSize::A4).size(QPageSize::Point);
    if (n == QLatin1String("a5")) return QPageSize(QPageSize::A5).size(QPageSize::Point);
    if (n == QLatin1String("letter")) return QPageSize(QPageSize::Letter).size(QPageSize::Point);
    if (n == QLatin1String("legal")) return QPageSize(QPageSize::Legal).size(QPageSize::Point);
    if (n == QLatin1String("tabloid")) return QPageSize(QPageSize::Tabloid).size(QPageSize::Point);
    if (n == QLatin1String("ledger")) return QPageSize(QPageSize::Tabloid).size(QPageSize::Point);
    if (n == QLatin1String("b4")) return QPageSize(QPageSize::B4).size(QPageSize::Point);
    if (n == QLatin1String("b5")) return QPageSize(QPageSize::B5).size(QPageSize::Point);
    return {};
}

static QString lastAtPageBlock(const QString &css)
{
    // Take the LAST @page block: the PrintOptions override is appended after
    // print-base.css's own `@page { margin: 15mm; }`, and the last one wins.
    static const QRegularExpression pageRe(QStringLiteral("@page\\s*\\{([^}]*)\\}"),
                                           QRegularExpression::CaseInsensitiveOption);
    QString block;
    auto it = pageRe.globalMatch(css);
    while (it.hasNext()) {
        const QRegularExpressionMatch &m = it.next();
        block = m.captured(1);
    }
    return block;
}

QSizeF parsePageSize(const QString &css)
{
    QString block = lastAtPageBlock(css);
    if (block.isEmpty())
        return QSizeF(595.0, 842.0);
    QRegularExpression sizeRe(QStringLiteral("size\\s*:\\s*([^;}]+)"),
                              QRegularExpression::CaseInsensitiveOption);
    auto sizeMatch = sizeRe.match(block);
    if (!sizeMatch.hasMatch())
        return QSizeF(595.0, 842.0);

    QString raw = sizeMatch.captured(1).trimmed();

    PageOrientation orientation = PageOrientation::Portrait;
    QString cleaned = raw;
    if (cleaned.contains(QStringLiteral("landscape"), Qt::CaseInsensitive)) {
        orientation = PageOrientation::Landscape;
        cleaned.remove(QStringLiteral("landscape"), Qt::CaseInsensitive);
    } else if (cleaned.contains(QStringLiteral("portrait"), Qt::CaseInsensitive)) {
        cleaned.remove(QStringLiteral("portrait"), Qt::CaseInsensitive);
    }
    cleaned = cleaned.trimmed();

    QSizeF sz = namedPageSizeToSize(cleaned);
    if (sz.isValid()) {
        if (orientation == PageOrientation::Landscape)
            sz.transpose();
        return sz;
    }

    QStringList parts = cleaned.split(QRegularExpression(QStringLiteral("\\s+")),
                                      Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        double w = cssLengthToPt(parts[0]);
        double h = cssLengthToPt(parts[1]);
        if (w > 0 && h > 0) {
            if (orientation == PageOrientation::Landscape)
                qSwap(w, h);
            return QSizeF(w, h);
        }
    }

    return QSizeF(595.0, 842.0);
}

QMarginsF parsePageMargins(const QString &css)
{
    QString block = lastAtPageBlock(css);
    if (block.isEmpty())
        return QMarginsF();

    // margin may be the last declaration without a trailing ';' (e.g. the
    // generated `@page{size:A4;margin:18mm}`).
    QRegularExpression marginRe(QStringLiteral("margin\\s*:\\s*([^;}]+)"),
                                QRegularExpression::CaseInsensitiveOption);
    auto marginMatch = marginRe.match(block);
    if (!marginMatch.hasMatch())
        return QMarginsF();

    QStringList parts = marginMatch.captured(1).trimmed().split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

    double t, r, b, l;
    if (parts.size() == 1) { t = r = b = l = cssLengthToPt(parts[0]); }
    else if (parts.size() == 2) { t = b = cssLengthToPt(parts[0]); l = r = cssLengthToPt(parts[1]); }
    else if (parts.size() == 3) { t = cssLengthToPt(parts[0]); l = r = cssLengthToPt(parts[1]); b = cssLengthToPt(parts[2]); }
    else if (parts.size() >= 4) { t = cssLengthToPt(parts[0]); r = cssLengthToPt(parts[1]); b = cssLengthToPt(parts[2]); l = cssLengthToPt(parts[3]); }
    else return QMarginsF();
    return QMarginsF(l, t, r, b);
}

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
        frags << ".mermaid,.katex-display,.admonition,blockquote,pre{break-inside:auto;page-break-inside:auto}";
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

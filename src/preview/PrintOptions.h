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
#pragma once

#include <QString>
#include <QSizeF>
#include <QMarginsF>

namespace PrintOptions {

enum class CodeSplit { NeverSplit, SplitSmall, SplitLarge };

struct Options {
    // DR-2: every default preserves today's output — code blocks never split
    // and every keep option is "on" (base print CSS already carries the
    // page-break-inside: avoid / page-break-after: avoid rules), so buildCss()
    // emits nothing for the all-defaults combination.
    CodeSplit codeSplit{CodeSplit::NeverSplit};
    bool keepTables{true};
    bool keepHeadings{true};
    bool keepFigures{true};
    bool orphanControl{true};
    QString pageMargin{};   // e.g. "18mm"; empty = base CSS default (15mm)
    QString pageSize{};     // e.g. "A4" or "200mm 100mm"; empty = default
};

Options fromSettings();
void toSettings(const Options &opts);

// Returns the override CSS fragments for every option that differs from its
// default, newline-joined (empty when all options are at their defaults).
QString buildCss(const Options &opts);

// Returns a single `@page{...}` override block (size + margin), or empty when
// neither is set. The caller must append it LAST so it wins the cascade over
// print-base.css's `@page { margin: 15mm; }` (DR-4).
QString buildPageOverrideCss(const Options &opts);

// Parse the LAST `@page` block in `css` (the merged print CSS — the override is
// appended after print-base.css's own rule, and the last one wins) for its page
// size and margins, in points. Falls back to A4 (595x842pt) / zero margins.
// Shared by the export path and the preview's page-break mode so both agree on
// the page box.
QSizeF parsePageSize(const QString &css);
QMarginsF parsePageMargins(const QString &css);

}

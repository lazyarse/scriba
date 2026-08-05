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

#include <QChar>
#include <QFlags>
#include <QString>
#include <QStringView>

// Streaming smart-typography conversion applied to *rendered* text only
// (preview + exports), never to the Markdown source. MdRenderer feeds each
// normal-text run through Typography::apply() and keeps a Typography::State
// between calls so quote direction and $...$ math regions work across runs.
// Code spans, fenced code blocks, raw HTML and entities are never routed here
// (MdRenderer handles those as separate callback types).
class Typography
{
public:
    enum class Option {
        Quotes              = 0x1,  // " ' -> " " ' '
        Dashes              = 0x2,  // - -- --- -> hyphen, en dash, em dash
        Ellipsis            = 0x4,  // ... -> …
        Multiplication      = 0x8,  // 3x4 -> 3×4
        DegreeFractionPrime = 0x10, // 90oF, 1/2, 5'10 -> 90°F, ½, 5′10
        NonBreakingSpace    = 0x20, // a word, 10 kg -> non-breaking space
        Symbols             = 0x40, // (c) (r) (tm) (p) (sm) -> © ® ™ ℗ ℠
    };
    Q_DECLARE_FLAGS(Options, Option)

    // Carried between text() runs by the caller. lastChar is the last emitted
    // text character (whitespace carried); inMath/inDisplayMath track $...$
    // and $$...$$ regions so typography never touches KaTeX math. The caller
    // resets lastChar to a space at block boundaries.
    struct State {
        QChar lastChar;
        bool inMath = false;
        bool inDisplayMath = false;
    };

    static QString apply(QStringView text, Options opts, State &state);

    // Reads the enabled conversions from the user's QSettings (all default
    // off). Single source of truth shared by the renderer and the dialog.
    static Options optionsFromSettings();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Typography::Options)

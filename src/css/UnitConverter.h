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

namespace UnitConverter {

// px → TWIP (1 inch = 96 CSS px = 1440 TWIP → 1 px = 15 TWIP)
inline int pxToTwip(double px) { return qRound(px * 15.0); }

// pt → TWIP (1 pt = 20 TWIP)
inline int ptToTwip(double pt) { return qRound(pt * 20.0); }

// cm → TWIP (1 inch = 2.54 cm = 1440 TWIP → 1 cm ≈ 567 TWIP)
inline int cmToTwip(double cm) { return qRound(cm * 1440.0 / 2.54); }

// inch → TWIP (1 in = 1440 TWIP)
inline int inToTwip(double in) { return qRound(in * 1440.0); }

// pt → EIP (eighths of a point)
inline int ptToEip(double pt) { return qRound(pt * 8.0); }

// px → EIP (1 px ≈ 0.75 pt → 0.75 * 8 = 6 EIP)
inline int pxToEip(double px) { return qRound(px * 6.0); }

// pt → HIP (half-points)
inline int ptToHip(double pt) { return qRound(pt * 2.0); }

// px → HIP (1 px ≈ 0.75 pt → 0.75 * 2 = 1.5 HIP)
inline int pxToHip(double px) { return qRound(px * 1.5); }

// Parse a CSS value string with unit → TWIP. Returns -1 if unparseable.
int cssToTwip(const QString &value);

// Parse a CSS value string with unit → EIP. Returns -1 if unparseable.
int cssToEip(const QString &value);

// Parse a CSS value string with unit → HIP. Returns -1 if unparseable.
int cssToHip(const QString &value);

} // namespace UnitConverter

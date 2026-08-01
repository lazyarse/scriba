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
#include "UnitConverter.h"
#include <QRegularExpression>

namespace UnitConverter {

static const QRegularExpression pxRe(R"(([\d.]+)\s*px)", QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression ptRe(R"(([\d.]+)\s*pt)", QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression cmRe(R"(([\d.]+)\s*cm)", QRegularExpression::CaseInsensitiveOption);
static const QRegularExpression inRe(R"(([\d.]+)\s*in)", QRegularExpression::CaseInsensitiveOption);

static double extractValue(const QString &value, const QRegularExpression &re)
{
    auto m = re.match(value);
    if (m.hasMatch())
        return m.captured(1).toDouble();
    // bare number (no unit) — assume px for pixel values, pt for point values
    bool ok = false;
    double v = value.toDouble(&ok);
    if (ok) return v;
    return -1;
}

int cssToTwip(const QString &value)
{
    if (pxRe.match(value).hasMatch())
        return pxToTwip(extractValue(value, pxRe));
    if (ptRe.match(value).hasMatch())
        return ptToTwip(extractValue(value, ptRe));
    if (cmRe.match(value).hasMatch())
        return cmToTwip(extractValue(value, cmRe));
    if (inRe.match(value).hasMatch())
        return inToTwip(extractValue(value, inRe));
    return -1;
}

int cssToEip(const QString &value)
{
    if (pxRe.match(value).hasMatch())
        return pxToEip(extractValue(value, pxRe));
    if (ptRe.match(value).hasMatch())
        return ptToEip(extractValue(value, ptRe));
    if (cmRe.match(value).hasMatch())
        return ptToEip(extractValue(value, cmRe) / 20.0 * 8.0); // cm→TWIP→pt→EIP
    if (inRe.match(value).hasMatch())
        return ptToEip(extractValue(value, inRe) * 72.0); // in→pt→EIP
    return -1;
}

int cssToHip(const QString &value)
{
    if (pxRe.match(value).hasMatch())
        return pxToHip(extractValue(value, pxRe));
    if (ptRe.match(value).hasMatch())
        return ptToHip(extractValue(value, ptRe));
    if (cmRe.match(value).hasMatch())
        return ptToHip(extractValue(value, cmRe) / 2.54 * 72.0); // cm→in→pt→HIP
    if (inRe.match(value).hasMatch())
        return ptToHip(extractValue(value, inRe) * 72.0); // in→pt→HIP
    return -1;
}

} // namespace UnitConverter

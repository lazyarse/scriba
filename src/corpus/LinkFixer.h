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
#include <QStringList>

// Rewrites markdown link destinations after a corpus file is renamed/moved.
// Only destinations that resolve to the old absolute path are touched; the
// new destination is spelled relative to the editing document's directory
// (docDir). Angle-bracket autolinks stay angle-bracketed.
class LinkFixer
{
public:
    // Raw link destination strings (for tests / counting).
    static QStringList linkTargets(const QString &source);
    // Absolute, cleaned paths of every destination referenced by source,
    // resolved against docDir (fragments and remote/data schemes dropped).
    static QStringList resolvedLinkTargets(const QString &source, const QString &docDir);
    // Rewrites destinations resolving to oldAbs into the relative spelling of
    // newAbs (from docDir). Angle-bracket autolinks stay angle-bracketed.
    static QString rewrite(const QString &source, const QString &docDir,
                           const QString &oldAbs, const QString &newAbs);
};

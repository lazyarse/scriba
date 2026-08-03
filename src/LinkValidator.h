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

#include <QSet>
#include <QString>

// Classifies markdown link/image targets and raw URLs as valid, broken file
// references, or malformed URLs. Pure and stateless: file targets are checked
// with QFileInfo::exists() against a base directory (or the current working
// directory when none is given); http(s)/ftp targets get a syntactic sanity
// check. No network access — the app must work fully offline.
class LinkValidator
{
public:
    enum class Status {
        Valid,        // existing file, or a well-formed http(s)/ftp URL
        FileNotFound, // a file-style target that does not resolve to a file
        MalformedUrl, // an http(s)/ftp target that is not well-formed
    };

    // Classifies one link target: the text inside `](...)` or `[ref]: <target>`,
    // or the text of a raw `https://...`/`www.` occurrence in the document.
    // `baseDir` is the directory the document lives in; an empty baseDir
    // resolves relative paths against the current working directory. Anchors
    // (`#...`), empty targets and non-web schemes (mailto:, tel:, data:, ...)
    // are Valid — they are out of scope for broken-link checking.
    static Status validateTarget(const QString &target,
                                 const QString &baseDir = QString());

    // True if `url` is a well-formed http/https/ftp URL: the scheme must be
    // followed by `//`, the host must be non-empty and contain a dot, be
    // "localhost", or be a literal IP address, and there must be no whitespace.
    static bool isValidHttpUrl(const QString &url);

    // The file-style path of a target: trims, unwraps `<...>` (the
    // reference-definition form) and strips a trailing `#fragment`/`?query`.
    static QString fileTargetPath(const QString &target);

    // GitHub-style heading slug, mirroring the preview's JS generator exactly:
    // lowercase, drop every character outside [A-Za-z0-9_\s-] (ASCII, like JS
    // `\w`), collapse each whitespace run to a single '-', trim leading and
    // trailing '-'. Used to validate `#anchor` link fragments against the
    // heading ids the preview assigns.
    static QString headingSlug(const QString &headingText);

    // Slugs for one heading, after duplicate handling: the first occurrence of
    // a slug is kept as-is, later ones become `slug-1`, `slug-2`, ... (the
    // same scheme the preview's generateHeadingIds() uses). Appends every
    // variant to `out` so a link to any of them is valid.
    static void addHeadingSlugs(QSet<QString> &out, const QString &headingText);
};

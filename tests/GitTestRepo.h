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

// Builds a disposable git repository (using the system `git` CLI) with a
// deterministic history and fixed commit dates, for GitGraphBuilder tests.
// All timestamps are 2026-01-<day> 10:00:00 UTC.
//
//   main:    c1 (day 1) c2 (day 2)
//   feature: branched from c2, c4 (day 4) c5 (day 5)
//   main:    c3 (day 3), then merge feature --no-ff -> c6 (day 6)
namespace GitTestRepo {

// Runs `git` with `args` in `repoDir`, optionally overriding the author and
// committer dates. Returns true when git exits successfully.
bool git(const QString &repoDir, const QStringList &args,
         const QString &date = QString());

// Creates the fixture repository at `dir`. Returns true on success.
bool create(const QString &dir);

} // namespace GitTestRepo

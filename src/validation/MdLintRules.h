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
#include <QVariant>
#include <QVector>

struct MdLintRule {
    QString id;
    QString alias;
    QString description;
    QStringList tags;
    bool aggressive = false; // true = disabled in MdLintConfig::defaults()
};

struct MdLintParam { const char *name; QVariant def; };

namespace MdLintRules {
    const QVector<MdLintRule> &all();
    QStringList allTags();
    const MdLintRule *byKey(const QString &key); // id, alias or tag (case-insensitive)
    QStringList rulesForTag(const QString &tag);
    QVector<MdLintParam> paramsFor(const QString &id);
}

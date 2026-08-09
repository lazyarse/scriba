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

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

class ZipReader
{
public:
    explicit ZipReader(const QString &filePath);
    bool open(QString *error = nullptr);
    QStringList entryNames() const;
    bool hasEntry(const QString &name) const;
    QByteArray readEntry(const QString &name) const;

private:
    QString m_path;
    struct Entry {
        quint32 method, crc32, compSize, uncompSize, localOffset;
    };
    QHash<QString, Entry> m_entries;
    mutable QHash<QString, QByteArray> m_cache;
};

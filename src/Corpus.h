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

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

struct CorpusDocument {
    QString path;                 // stored path (corpus-root-relative); "" => embedded
    QString content;              // embedded content when path == ""
    QString name;                 // tab-label hint for embedded entries
    int cursorBlock = 0;
    int cursorCol = 0;
    int scroll = 0;
    QList<int> folds;
};

struct CorpusDictionary {
    QString language;             // "en_US"/"en_GB"/"" (= follow dialect setting)
    QString dialect;              // e.g. "British"
    QStringList customWords;
    QStringList ignoredWords;
};

class Corpus
{
public:
    QString filePath;             // absolute .scriba path; "" until saved
    QString name;                 // display name
    int active = 0;               // index into documents() of the active tab
    bool monitor = true;          // live-directory-monitoring flag (root "monitor")
    CorpusDictionary dictionary;
    QVector<CorpusDocument> documents;

    QString rootDir() const;                             // = dirname(filePath)
    static QString storedPath(const QString &rootDir, const QString &absPath);
    static QString absolutePath(const QString &rootDir, const QString &storedPath);

    QJsonObject toJson() const;
    static Corpus fromJson(const QJsonObject &json, const QString &filePath);
    bool save(QString *error = nullptr) const;
    static bool loadFile(const QString &filePath, Corpus *out, QString *error = nullptr);
};
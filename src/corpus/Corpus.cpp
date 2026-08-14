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
#include "Corpus.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

QString Corpus::rootDir() const
{
    return QFileInfo(filePath).absolutePath();
}

QString Corpus::storedPath(const QString &rootDir_, const QString &absPath)
{
    if (rootDir_.isEmpty() || absPath.isEmpty())
        return absPath;
    const QString rel = QDir(rootDir_).relativeFilePath(absPath);
    if (rel.isEmpty() || rel.startsWith(QLatin1String("..")))
        return absPath;
    return rel;
}

QString Corpus::absolutePath(const QString &rootDir_, const QString &storedPath_)
{
    if (storedPath_.isEmpty())
        return QString();
    const QFileInfo fi(storedPath_);
    if (fi.isAbsolute())
        return fi.absoluteFilePath();
    return QDir(rootDir_).filePath(storedPath_);
}

static QStringList readStringList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &e : v.toArray())
        out.append(e.toString());
    return out;
}

QJsonObject Corpus::toJson() const
{
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    if (!name.isEmpty())
        root[QStringLiteral("name")] = name;
    root[QStringLiteral("active")] = active;
    if (!monitor)
        root[QStringLiteral("monitor")] = false;

    QJsonObject dict;
    dict[QStringLiteral("language")] = dictionary.language;
    dict[QStringLiteral("dialect")] = dictionary.dialect;
    dict[QStringLiteral("customWords")] = QJsonArray::fromStringList(dictionary.customWords);
    dict[QStringLiteral("ignoredWords")] = QJsonArray::fromStringList(dictionary.ignoredWords);
    root[QStringLiteral("dictionary")] = dict;

    QJsonArray docs;
    for (const CorpusDocument &d : documents) {
        QJsonObject o;
        if (!d.path.isEmpty())
            o[QStringLiteral("path")] = d.path;
        if (!d.content.isEmpty())
            o[QStringLiteral("content")] = d.content;
        if (!d.name.isEmpty())
            o[QStringLiteral("name")] = d.name;
        QJsonObject st;
        st[QStringLiteral("block")] = d.cursorBlock;
        st[QStringLiteral("col")] = d.cursorCol;
        o[QStringLiteral("cursor")] = st;
        o[QStringLiteral("scroll")] = d.scroll;
        QJsonArray folds;
        for (int f : d.folds)
            folds.append(f);
        o[QStringLiteral("folds")] = folds;
        docs.append(o);
    }
    root[QStringLiteral("documents")] = docs;
    return root;
}

Corpus Corpus::fromJson(const QJsonObject &json, const QString &filePath_)
{
    Corpus c;
    c.filePath = filePath_;
    c.name = json[QStringLiteral("name")].toString();
    c.active = json[QStringLiteral("active")].toInt(0);
    c.monitor = json.contains(QLatin1String("monitor"))
        ? json[QStringLiteral("monitor")].toBool(true) : true;

    const QJsonObject dict = json[QStringLiteral("dictionary")].toObject();
    c.dictionary.language = dict[QStringLiteral("language")].toString();
    c.dictionary.dialect = dict[QStringLiteral("dialect")].toString();
    c.dictionary.customWords = readStringList(dict[QStringLiteral("customWords")]);
    c.dictionary.ignoredWords = readStringList(dict[QStringLiteral("ignoredWords")]);

    for (const QJsonValue &v : json[QStringLiteral("documents")].toArray()) {
        const QJsonObject o = v.toObject();
        CorpusDocument d;
        d.path = o[QStringLiteral("path")].toString();
        d.content = o[QStringLiteral("content")].toString();
        d.name = o[QStringLiteral("name")].toString();
        const QJsonObject st = o[QStringLiteral("cursor")].toObject();
        d.cursorBlock = st[QStringLiteral("block")].toInt();
        d.cursorCol = st[QStringLiteral("col")].toInt();
        d.scroll = o[QStringLiteral("scroll")].toInt(0);
        for (const QJsonValue &f : o[QStringLiteral("folds")].toArray())
            d.folds.append(f.toInt());
        c.documents.append(d);
    }
    return c;
}

bool Corpus::save(QString *error) const
{
    if (filePath.isEmpty()) {
        if (error) *error = QStringLiteral("no corpus path");
        return false;
    }
    const QByteArray data = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(data);
    f.close();
    return true;
}

bool Corpus::loadFile(const QString &filePath_, Corpus *out, QString *error)
{
    QFile f(filePath_);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull() || !doc.isObject() || doc.object()[QStringLiteral("version")].toInt() != 1) {
        if (error) *error = QStringLiteral("not a supported corpus file");
        return false;
    }
    *out = Corpus::fromJson(doc.object(), QFileInfo(filePath_).absoluteFilePath());
    return true;
}
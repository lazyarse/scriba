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
#include "LinkFixer.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

static QString resolvedTarget(const QString &target, const QString &docDir)
{
    if (target.isEmpty() || target.startsWith(QLatin1Char('#')))
        return {};
    const QUrl u(target);
    const QString scheme = u.scheme();
    if (scheme.size() > 1 && scheme != QLatin1String("file"))
        return {};                       // http(s)/custom schemes: never rewritten
    if (u.isRelative())
        return QDir(docDir).filePath(QUrl::fromPercentEncoding(target.toUtf8()));
    return QFileInfo(QUrl::fromPercentEncoding(target.toUtf8())).absoluteFilePath();
}

static bool matchesPath(const QString &target, const QString &docDir, const QString &oldAbs)
{
    const QString t = resolvedTarget(target, docDir);
    if (t.isEmpty())
        return false;
    const QFileInfo targetInfo(t);
    const QFileInfo oldInfo(oldAbs);
    const QString targetCanon = targetInfo.canonicalFilePath();
    const QString oldCanon = oldInfo.canonicalFilePath();
    // Canonical comparison is only meaningful when both paths exist; a
    // nonexistent target must never fall through to a spurious "match" via
    // two empty canonical paths (the renamed file no longer exists on disk).
    if (!targetCanon.isEmpty() && !oldCanon.isEmpty())
        return targetCanon == oldCanon;
    return targetInfo.absoluteFilePath() == oldInfo.absoluteFilePath();
}

QStringList LinkFixer::linkTargets(const QString &source)
{
    QStringList out;
    static const QRegularExpression inlineLink(
        R"((?m)(?<!\!)\[[^\]]*\]\s*\(\s*([^\s)\]]+))");
    static const QRegularExpression imageLink(
        R"((?m)!\[[^\]]*\]\s*\(\s*([^\s)\]]+))");
    static const QRegularExpression refDef(
        R"((?m)^\[[^\]]*\]:\s*([^\s<]+))");
    static const QRegularExpression angleLink(
        R"(<((?:[^">\s]*\.md|\.md[^">\s]*))>)");
    for (const auto &re : {inlineLink, imageLink, refDef, angleLink})
        for (const QRegularExpressionMatch &m : re.globalMatch(source))
            out.append(m.captured(1));
    return out;
}

QString LinkFixer::rewrite(const QString &source, const QString &docDir,
                           const QString &oldAbs, const QString &newAbs)
{
    struct Hit { qsizetype pos; qsizetype length; QString replacement; };
    QList<Hit> hits;

    const QString newRel = QDir(docDir).relativeFilePath(QFileInfo(newAbs).absoluteFilePath());
    static const QRegularExpression inlineLink(
        R"((?m)(?<!\!)\[([^\]]*)\]\s*\(\s*([^\s)\]]+))");
    static const QRegularExpression imageLink(
        R"((?m)!\[([^\]]*)\]\s*\(\s*([^\s)\]]+))");
    static const QRegularExpression refDef(
        R"((?m)^(\[[^\]]*\]\s*:\s*)([^\s<]+))");
    static const QRegularExpression angleLink(
        R"(<((?:[^">\s]*\.md|\.md[^">\s]*))>)");

    for (const auto &re : {inlineLink, imageLink, refDef}) {
        for (const QRegularExpressionMatch &m : re.globalMatch(source)) {
            const QString dest = m.captured(2);
            if (!matchesPath(dest, docDir, oldAbs))
                continue;
            hits.append({m.capturedStart(2), m.capturedLength(2), newRel});
        }
    }
    for (const QRegularExpressionMatch &m : angleLink.globalMatch(source)) {
        if (!matchesPath(m.captured(1), docDir, oldAbs))
            continue;
        // captured(1) is the inner text (brackets stay in the source), so the
        // replacement is the plain relative path.
        hits.append({m.capturedStart(1), m.capturedLength(1), newRel});
    }

    // Backward iteration: earlier offsets stay valid while replacing.
    std::sort(hits.begin(), hits.end(),
              [](const Hit &a, const Hit &b) { return a.pos > b.pos; });
    QString out = source;
    for (const Hit &h : hits)
        out.replace(h.pos, h.length, h.replacement);
    return out;
}

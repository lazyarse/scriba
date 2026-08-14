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
#include "CorpusIndex.h"

#include "Corpus.h"
#include "validation/LinkValidator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace {

QString readDocSource(const QString &abs)
{
    QFile file(abs);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

QString escapeMd(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('['), QStringLiteral("\\["));
    out.replace(QLatin1Char(']'), QStringLiteral("\\]"));
    return out;
}

} // namespace

QList<CorpusIndex::Heading> CorpusIndex::extractHeadings(const QString &markdown)
{
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    static const QRegularExpression atx(R"(^(#{1,6})[ \t]+(.+?)[ \t]*#*[ \t]*$)");

    QList<Heading> headings;
    bool inFence = false;
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("```"))
            || line.startsWith(QLatin1String("~~~"))) {
            inFence = !inFence;
            continue;
        }
        if (inFence)
            continue;
        const QRegularExpressionMatch m = atx.match(line);
        if (!m.hasMatch())
            continue;
        headings.append({ static_cast<int>(m.captured(1).size()),
                          m.captured(2).trimmed() });
    }
    return headings;
}

QString CorpusIndex::renderToc(const Corpus &corpus,
                               const QHash<QString, QString> &pageLinkByAbs)
{
    QString md = QStringLiteral("# Table of Contents\n\n");

    const QString name = corpus.name.isEmpty()
        ? QFileInfo(corpus.filePath).completeBaseName()
        : corpus.name;
    if (!name.isEmpty())
        md += QStringLiteral("Corpus: **") + name + QStringLiteral("**\n\n");

    const QString root = corpus.rootDir();

    for (const CorpusDocument &d : corpus.documents) {
        if (d.path.isEmpty() || QFileInfo(d.path).isAbsolute())
            continue;   // embedded docs have no file; out-of-root docs go to the external section
        const QString abs = Corpus::absolutePath(root, d.path);
        const QString linkBase = pageLinkByAbs.value(abs);
        if (linkBase.isEmpty())
            continue;
        const QString label = QFileInfo(d.path).fileName();
        md += QStringLiteral("- [%1](%2)\n").arg(label, linkBase);
        for (const Heading &h : extractHeadings(readDocSource(abs))) {
            md += QStringLiteral("  %1- [%2](%3#%4)\n")
                      .arg(QString(h.level > 1 ? (h.level - 1) : 0, QLatin1Char(' ')),
                           escapeMd(h.title), linkBase, LinkValidator::headingSlug(h.title));
        }
    }

    QString extSection;
    for (const CorpusDocument &d : corpus.documents) {
        if (d.path.isEmpty() || !QFileInfo(d.path).isAbsolute())
            continue;
        const QString abs = Corpus::absolutePath(root, d.path);
        const QString linkBase = pageLinkByAbs.value(abs);
        if (linkBase.isEmpty())
            continue;
        extSection += QStringLiteral("- [%1](%2)\n")
                          .arg(QFileInfo(d.path).fileName(), linkBase);
    }
    if (!extSection.isEmpty())
        md += QStringLiteral("\n## External documents\n\n") + extSection;

    return md;
}

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

    md += renderTocLinks(corpus, pageLinkByAbs);
    return md;
}

QString CorpusIndex::renderTocLinks(const Corpus &corpus,
                                    const QHash<QString, QString> &pageLinkByAbs)
{
    QString md;
    const QString root = corpus.rootDir();
    for (const CorpusDocument &d : corpus.documents) {
        if (d.path.isEmpty() || QFileInfo(d.path).isAbsolute())
            continue;
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

QString CorpusIndex::tocStartMarker()
{
    return QStringLiteral("<!--toc:start-->");
}

QString CorpusIndex::tocEndMarker()
{
    return QStringLiteral("<!--toc:end-->");
}

QString CorpusIndex::defaultTocTemplate()
{
    return QStringLiteral("# Table of Contents\n\n<!--toc:start-->\n<!--toc:end-->\n");
}

QString CorpusIndex::replaceTocBlock(const QString &fullText, const QString &linksMd)
{
    const QString start = tocStartMarker();
    const QString end = tocEndMarker();
    const int startLine = fullText.indexOf(start);
    const int endLine = fullText.indexOf(end);

    QString block = start + QLatin1Char('\n') + linksMd;
    if (!linksMd.endsWith(QLatin1Char('\n')))
        block += QLatin1Char('\n');
    block += end;

    if (startLine < 0 && endLine < 0)
        return fullText.endsWith(QLatin1Char('\n'))
            ? fullText + block + QLatin1Char('\n')
            : fullText + QLatin1Char('\n') + block + QLatin1Char('\n');

    // Region runs from the start-marker line through the end-marker line.
    const int regionBegin = startLine >= 0 ? fullText.lastIndexOf(QLatin1Char('\n'), startLine) + 1
                                           : 0;
    const int regionEnd = endLine >= 0 ? fullText.indexOf(QLatin1Char('\n'), endLine) : fullText.size();
    const int endPos = (endLine >= 0 && regionEnd >= 0) ? regionEnd : fullText.size();
    const int beginPos = startLine >= 0 ? regionBegin : fullText.size();

    QString out = fullText.left(beginPos) + block;
    if (endLine >= 0 && endPos < fullText.size())
        out += fullText.mid(endPos);   // preserve text after the end marker
    else if (startLine >= 0 && endLine < 0)
        out += QLatin1Char('\n');       // start-only: block ends the file
    if (out == fullText)
        return fullText;
    return out;
}

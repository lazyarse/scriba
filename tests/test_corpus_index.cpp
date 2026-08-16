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
#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QTemporaryDir>
#include <QStringList>
#include <QUrl>

#include <memory>

#include "corpus/Corpus.h"
#include "corpus/CorpusIndex.h"
#include "prefs/Preferences.h"
#include "validation/LinkValidator.h"

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size();
}

TEST(CorpusIndexTest, ExtractHeadingsSkipsFencedBlocks)
{
    const QString markdown = QStringLiteral(
        "# Real\n"
        "\n"
        "```\n"
        "# fake\n"
        "## fake too\n"
        "```\n"
        "\n"
        "## After\n");
    const QList<CorpusIndex::Heading> headings = CorpusIndex::extractHeadings(markdown);

    ASSERT_EQ(headings.size(), 2);
    EXPECT_EQ(headings[0].level, 1);
    EXPECT_EQ(headings[0].title, QStringLiteral("Real"));
    EXPECT_EQ(headings[1].level, 2);
    EXPECT_EQ(headings[1].title, QStringLiteral("After"));
}

TEST(CorpusIndexTest, ExtractHeadingsSkipsTildeFence)
{
    const QString markdown = QStringLiteral(
        "# Tilde\n"
        "\n"
        "~~~\n"
        "# hidden\n"
        "~~~\n"
        "\n"
        "## Visible\n");
    const QList<CorpusIndex::Heading> headings = CorpusIndex::extractHeadings(markdown);

    ASSERT_EQ(headings.size(), 2);
    EXPECT_EQ(headings[0].level, 1);
    EXPECT_EQ(headings[0].title, QStringLiteral("Tilde"));
    EXPECT_EQ(headings[1].level, 2);
    EXPECT_EQ(headings[1].title, QStringLiteral("Visible"));
}

TEST(CorpusIndexTest, ExtractHeadingsSkipsFrontmatter)
{
    const QString markdown = QStringLiteral(
        "---\n"
        "title: My Doc\n"
        "# not a heading\n"
        "toc-description: desc\n"
        "---\n"
        "# Real\n"
        "## After\n");
    const QList<CorpusIndex::Heading> headings = CorpusIndex::extractHeadings(markdown);

    ASSERT_EQ(headings.size(), 2);
    EXPECT_EQ(headings[0].level, 1);
    EXPECT_EQ(headings[0].title, QStringLiteral("Real"));
    EXPECT_EQ(headings[1].level, 2);
    EXPECT_EQ(headings[1].title, QStringLiteral("After"));
}

TEST(CorpusIndexTest, ExtractHeadingsIgnoresNoSpaceHash)
{
    const QList<CorpusIndex::Heading> headings =
        CorpusIndex::extractHeadings(QStringLiteral("#heading\n##Heading\n# Good\n"));

    ASSERT_EQ(headings.size(), 1);
    EXPECT_EQ(headings[0].level, 1);
    EXPECT_EQ(headings[0].title, QStringLiteral("Good"));
}

TEST(CorpusIndexTest, ExtractHeadingsStripsTrailingHashesAndTrims)
{
    const QList<CorpusIndex::Heading> headings =
        CorpusIndex::extractHeadings(QStringLiteral("# Title ##  \n"));

    ASSERT_EQ(headings.size(), 1);
    EXPECT_EQ(headings[0].level, 1);
    EXPECT_EQ(headings[0].title, QStringLiteral("Title"));
}

TEST(CorpusIndexTest, ExtractHeadingsHandlesLevels)
{
    const QList<CorpusIndex::Heading> headings =
        CorpusIndex::extractHeadings(QStringLiteral("###### Deep\n### Mid\n"));

    ASSERT_EQ(headings.size(), 2);
    EXPECT_EQ(headings[0].level, 6);
    EXPECT_EQ(headings[0].title, QStringLiteral("Deep"));
    EXPECT_EQ(headings[1].level, 3);
    EXPECT_EQ(headings[1].title, QStringLiteral("Mid"));
}

TEST(CorpusIndexTest, RenderTocNestsHeadingsWithMatchingSlugs)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath("docs"));
    ASSERT_TRUE(writeFile(dir.path() + "/docs/a.md",
                          "# Alpha\n\n## Sub A\n\n### Deep A\n"));
    ASSERT_TRUE(writeFile(dir.path() + "/docs/b.md", "# Beta\n"));

    Corpus corpus;
    corpus.filePath = dir.path() + "/my.scriba";
    corpus.name = QStringLiteral("My Corpus");
    corpus.documents = {
        CorpusDocument{ .path = QStringLiteral("docs/a.md") },
        CorpusDocument{ .path = QStringLiteral("docs/b.md") },
    };

    QHash<QString, QString> pageLinkByAbs;
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("docs/a.md")),
                         QStringLiteral("docs/a.md"));
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("docs/b.md")),
                         QStringLiteral("docs/b.md"));

    const QString toc = CorpusIndex::renderToc(corpus, pageLinkByAbs);

    EXPECT_TRUE(toc.startsWith(QStringLiteral("# Table of Contents")));
    EXPECT_TRUE(toc.contains(QStringLiteral("Corpus: **My Corpus**")));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [a.md](docs/a.md)")));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [b.md](docs/b.md)")));
    EXPECT_TRUE(toc.contains(QStringLiteral("  - [Alpha](docs/a.md#alpha)")));
    EXPECT_TRUE(toc.contains(QStringLiteral("   - [Sub A](docs/a.md#sub-a)")));
    EXPECT_TRUE(toc.contains(QStringLiteral("    - [Deep A](docs/a.md#deep-a)")));
    EXPECT_TRUE(toc.contains(QStringLiteral("  - [Beta](docs/b.md#beta)")));
    EXPECT_FALSE(toc.contains(QStringLiteral("External documents")));
}

TEST(CorpusIndexTest, RenderTocPutsOutOfRootDocsInExternalSection)
{
    QTemporaryDir dir;
    QTemporaryDir ext;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(ext.isValid());
    ASSERT_TRUE(QDir(dir.path()).mkpath("docs"));
    ASSERT_TRUE(writeFile(dir.path() + "/docs/a.md", "# Alpha\n"));
    const QString extAbs = ext.path() + "/ext.md";
    ASSERT_TRUE(writeFile(extAbs, "# Ext\n"));

    Corpus corpus;
    corpus.filePath = dir.path() + "/my.scriba";
    corpus.name = QString();
    corpus.documents = {
        CorpusDocument{ .path = QStringLiteral("docs/a.md") },
        CorpusDocument{ .path = extAbs },
    };

    QHash<QString, QString> pageLinkByAbs;
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("docs/a.md")),
                         QStringLiteral("docs/a.md"));
    const QString extLink = QUrl::fromLocalFile(extAbs).toString();
    pageLinkByAbs.insert(extAbs, extLink);

    const QString toc = CorpusIndex::renderToc(corpus, pageLinkByAbs);

    EXPECT_TRUE(toc.contains(QStringLiteral("Corpus: **my**")));

    const int externalPos = toc.indexOf(QStringLiteral("## External documents"));
    ASSERT_GT(externalPos, 0);
    const QString mainSection = toc.left(externalPos);
    EXPECT_TRUE(mainSection.contains(QStringLiteral("- [a.md](docs/a.md)")));
    EXPECT_FALSE(mainSection.contains(QStringLiteral("ext.md")));

    const QString externalSection = toc.mid(externalPos);
    EXPECT_TRUE(externalSection.contains(QStringLiteral("- [ext.md](") + extLink
                                         + QStringLiteral(")")));
    EXPECT_TRUE(externalSection.contains(QStringLiteral("- [ext.md](file:///")));
}

TEST(CorpusIndexTest, RenderTocSkipsEmbeddedAndUnmappedDocuments)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.path() + "/a.md", "# Alpha\n"));
    ASSERT_TRUE(writeFile(dir.path() + "/b.md", "# Beta\n"));
    ASSERT_TRUE(writeFile(dir.path() + "/c.md", "# Gamma\n"));

    Corpus corpus;
    corpus.filePath = dir.path() + "/my.scriba";
    corpus.name = QStringLiteral("My Corpus");
    corpus.documents = {
        CorpusDocument{ .path = QString(), .content = QStringLiteral("# Embedded\n") },
        CorpusDocument{ .path = QStringLiteral("a.md") },
        CorpusDocument{ .path = QStringLiteral("b.md") },
        CorpusDocument{ .path = QStringLiteral("c.md") },
    };

    QHash<QString, QString> pageLinkByAbs;
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("a.md")),
                         QStringLiteral("a.md"));
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("c.md")),
                         QStringLiteral("c.md"));

    const QString toc = CorpusIndex::renderToc(corpus, pageLinkByAbs);

    EXPECT_TRUE(toc.contains(QStringLiteral("- [a.md](a.md)")));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [c.md](c.md)")));
    EXPECT_FALSE(toc.contains(QStringLiteral("Embedded")));
    EXPECT_FALSE(toc.contains(QStringLiteral("b.md")));
    EXPECT_FALSE(toc.contains(QStringLiteral("Beta")));
    EXPECT_FALSE(toc.contains(QStringLiteral("External documents")));
}

TEST(CorpusIndexTest, RenderTocLinksOmitsHeaderAndName)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.path() + "/a.md", "# Alpha\n\n## Sub A\n"));
    const QString extAbs = dir.path() + "/ext.md";
    ASSERT_TRUE(writeFile(extAbs, "# Ext\n"));

    Corpus corpus;
    corpus.filePath = dir.path() + "/my.scriba";
    corpus.name = QStringLiteral("My Corpus");
    corpus.documents = {
        CorpusDocument{ .path = QStringLiteral("a.md") },
        CorpusDocument{ .path = extAbs },
    };

    QHash<QString, QString> pageLinkByAbs;
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("a.md")),
                         QStringLiteral("a.md"));
    pageLinkByAbs.insert(extAbs, QUrl::fromLocalFile(extAbs).toString());

    const QString links = CorpusIndex::renderTocLinks(corpus, pageLinkByAbs);

    EXPECT_FALSE(links.contains(QStringLiteral("# Table of Contents")));
    EXPECT_FALSE(links.contains(QStringLiteral("Corpus:")));
    EXPECT_TRUE(links.contains(QStringLiteral("- [a.md](a.md)")));
    EXPECT_TRUE(links.contains(QStringLiteral("  - [Alpha](a.md#alpha)")));
    EXPECT_TRUE(links.contains(QStringLiteral("   - [Sub A](a.md#sub-a)")));
    EXPECT_TRUE(links.contains(QStringLiteral("## External documents")));
    EXPECT_TRUE(links.contains(QStringLiteral("- [ext.md](") + QUrl::fromLocalFile(extAbs).toString()
                                 + QStringLiteral(")")));
}

TEST(CorpusIndexTest, ReplaceTocBlockBothMarkersPreservesSurroundingText)
{
    QString withMarkers = QStringLiteral("# T\n\nIntro\n\n%1\nold links\n%2\n\nNotes\n")
                              .arg(CorpusIndex::tocStartMarker(), CorpusIndex::tocEndMarker());
    const QString after = CorpusIndex::replaceTocBlock(
        withMarkers, QStringLiteral("- [x](x.md)\n"));

    EXPECT_TRUE(after.contains(QStringLiteral("Intro")));
    EXPECT_TRUE(after.contains(QStringLiteral("Notes")));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_FALSE(after.contains(QStringLiteral("old links")));
    EXPECT_TRUE(after.indexOf(CorpusIndex::tocStartMarker())
                < after.indexOf(QStringLiteral("- [x](x.md)")));
    EXPECT_TRUE(after.indexOf(QStringLiteral("- [x](x.md)"))
                < after.indexOf(CorpusIndex::tocEndMarker()));
}

TEST(CorpusIndexTest, ReplaceTocBlockStartOnlyAppendsEndMarker)
{
    const QString withStart = QStringLiteral("# T\n\n%1\nstale\n")
                                  .arg(CorpusIndex::tocStartMarker());
    const QString after = CorpusIndex::replaceTocBlock(
        withStart, QStringLiteral("- [x](x.md)\n"));

    EXPECT_TRUE(after.contains(CorpusIndex::tocEndMarker()));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_FALSE(after.contains(QStringLiteral("stale")));
    EXPECT_TRUE(after.startsWith(QStringLiteral("# T\n\n")));
    EXPECT_TRUE(after.endsWith(CorpusIndex::tocEndMarker() + QLatin1Char('\n')));
}

TEST(CorpusIndexTest, ReplaceTocBlockNoMarkersAppendsBlock)
{
    const QString before = QStringLiteral("# T\n\nSome intro.\n");
    const QString after = CorpusIndex::replaceTocBlock(
        before, QStringLiteral("- [x](x.md)\n"));

    EXPECT_TRUE(after.startsWith(before));
    EXPECT_TRUE(after.contains(CorpusIndex::tocStartMarker()));
    EXPECT_TRUE(after.contains(CorpusIndex::tocEndMarker()));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
}

TEST(CorpusIndexTest, ReplaceTocBlockIdenticalReturnsSame)
{
    const QString start = CorpusIndex::tocStartMarker();
    const QString end = CorpusIndex::tocEndMarker();
    const QString original = QStringLiteral("# T\n\n") + start + QLatin1Char('\n')
        + QStringLiteral("- [x](x.md)\n") + end + QLatin1Char('\n');
    const QString result = CorpusIndex::replaceTocBlock(original, QStringLiteral("- [x](x.md)\n"));
    EXPECT_EQ(result, original);
}

TEST(CorpusIndexTest, ReplaceTocBlockDoubledBlocksCollapsesToSingleBlock)
{
    const QString start = CorpusIndex::tocStartMarker();
    const QString end = CorpusIndex::tocEndMarker();
    const QString input = QStringLiteral("# T\n\n%1\nfirst links\n%2\n\n%1\nsecond links\n%2\n\nNotes\n")
                              .arg(start, end);
    const QString after = CorpusIndex::replaceTocBlock(
        input, QStringLiteral("- [x](x.md)\n"));

    EXPECT_EQ(after.count(start), 1) << "doubled files must collapse to one block";
    EXPECT_EQ(after.count(end), 1);
    EXPECT_FALSE(after.contains(QStringLiteral("first links")));
    EXPECT_FALSE(after.contains(QStringLiteral("second links")));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_TRUE(after.contains(QStringLiteral("Notes")))
        << "user text after the last end marker must be preserved";
}

TEST(CorpusIndexTest, ReplaceTocBlockOrphanTrailingEndMarkerRemoved)
{
    const QString start = CorpusIndex::tocStartMarker();
    const QString end = CorpusIndex::tocEndMarker();
    const QString input = QStringLiteral("# T\n\n%1\nvalid links\n%2\nstale links\n%2")
                              .arg(start, end);
    const QString after = CorpusIndex::replaceTocBlock(
        input, QStringLiteral("- [x](x.md)\n"));

    EXPECT_EQ(after.count(start), 1);
    EXPECT_EQ(after.count(end), 1);
    EXPECT_FALSE(after.contains(QStringLiteral("valid links")));
    EXPECT_FALSE(after.contains(QStringLiteral("stale links")));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_TRUE(after.endsWith(end)) << "the orphan trailing end must not survive";
}

TEST(CorpusIndexTest, ReplaceTocBlockMultipleEndsPreservesTextAfterLastEnd)
{
    const QString start = CorpusIndex::tocStartMarker();
    const QString end = CorpusIndex::tocEndMarker();
    const QString input = QStringLiteral("# T\n\n%1\nlink A\n%2\nmid text\n%2\n%2\n\nNotes\n")
                              .arg(start, end);
    const QString after = CorpusIndex::replaceTocBlock(
        input, QStringLiteral("- [x](x.md)\n"));

    EXPECT_EQ(after.count(start), 1);
    EXPECT_EQ(after.count(end), 1);
    EXPECT_FALSE(after.contains(QStringLiteral("link A")));
    EXPECT_FALSE(after.contains(QStringLiteral("mid text")));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_TRUE(after.contains(QStringLiteral("Notes")))
        << "text after the last end-marker line must be preserved";
    EXPECT_TRUE(after.indexOf(QStringLiteral("Notes")) > after.lastIndexOf(end));
}

TEST(CorpusIndexTest, ReplaceTocBlockSpacedMarkersAreRecognizedAndCanonicalized)
{
    const QString before = QStringLiteral(
        "# T\n\nIntro\n\n<!-- toc:start -->\nold links\n<!-- toc:end -->\n\nNotes\n");
    const QString after = CorpusIndex::replaceTocBlock(
        before, QStringLiteral("- [x](x.md)\n"));

    EXPECT_TRUE(after.contains(QStringLiteral("Intro")));
    EXPECT_TRUE(after.contains(QStringLiteral("Notes")));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_FALSE(after.contains(QStringLiteral("old links")));
    EXPECT_FALSE(after.contains(QStringLiteral("<!-- toc:start -->")));
    EXPECT_FALSE(after.contains(QStringLiteral("<!-- toc:end -->")));
    EXPECT_EQ(after.count(CorpusIndex::tocStartMarker()), 1);
    EXPECT_EQ(after.count(CorpusIndex::tocEndMarker()), 1);
}

TEST(CorpusIndexTest, ReplaceTocBlockMixedSpacingMarkers)
{
    const QString before = QStringLiteral(
        "# T\n\n<!--toc:start -->\nold\n<!-- toc:end-->\n");
    const QString after = CorpusIndex::replaceTocBlock(
        before, QStringLiteral("- [x](x.md)\n"));

    EXPECT_EQ(after.count(CorpusIndex::tocStartMarker()), 1);
    EXPECT_EQ(after.count(CorpusIndex::tocEndMarker()), 1);
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
}

TEST(CorpusIndexTest, ReplaceTocBlockIgnoresMarkersInsideFences)
{
    const QString before = QStringLiteral(
        "# T\n\n"
        "```\n<!-- toc:start -->\nignored\n<!-- toc:end -->\n```\n\n"
        "intro\n");
    const QString after = CorpusIndex::replaceTocBlock(
        before, QStringLiteral("- [x](x.md)\n"));

    // No markers outside fences: a fresh block is appended at EOF and the
    // fenced lines must remain byte-identical.
    EXPECT_TRUE(after.startsWith(before));
    EXPECT_EQ(after.count(QStringLiteral("<!-- toc:start -->")), 1);
    EXPECT_EQ(after.count(QStringLiteral("<!-- toc:end -->")), 1);
    EXPECT_EQ(after.count(CorpusIndex::tocStartMarker()), 1);
    EXPECT_EQ(after.count(CorpusIndex::tocEndMarker()), 1);
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
}

TEST(CorpusIndexTest, ReplaceTocBlockFencedDecoyDoesNotAnchorRegion)
{
    // A fenced marker before the real pair: the region must anchor on the
    // real markers, leaving the fence untouched.
    const QString before = QStringLiteral(
        "# T\n\n"
        "```\n<!-- toc:start -->\n```\n\n"
        "<!--toc:start-->\nold links\n<!--toc:end-->\n\nNotes\n");
    const QString after = CorpusIndex::replaceTocBlock(
        before, QStringLiteral("- [x](x.md)\n"));

    EXPECT_TRUE(after.contains(QStringLiteral("```\n<!-- toc:start -->\n```")));
    EXPECT_EQ(after.count(CorpusIndex::tocStartMarker()), 1);
    EXPECT_EQ(after.count(CorpusIndex::tocEndMarker()), 1);
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_FALSE(after.contains(QStringLiteral("old links")));
    EXPECT_TRUE(after.contains(QStringLiteral("Notes")));
}

TEST(CorpusIndexTest, ReplaceTocBlockFencedEndMarkerIgnored)
{
    // An end marker inside a fence must not count as the last end: the region
    // runs to the real end marker and the fenced text survives.
    const QString before = QStringLiteral(
        "# T\n\n"
        "<!--toc:start-->\nold links\n<!--toc:end-->\n\n"
        "```\n<!-- toc:end -->\n```\n\nNotes\n");
    const QString after = CorpusIndex::replaceTocBlock(
        before, QStringLiteral("- [x](x.md)\n"));

    EXPECT_EQ(after.count(CorpusIndex::tocStartMarker()), 1);
    EXPECT_EQ(after.count(CorpusIndex::tocEndMarker()), 1);
    EXPECT_TRUE(after.contains(QStringLiteral("```\n<!-- toc:end -->\n```")));
    EXPECT_TRUE(after.contains(QStringLiteral("- [x](x.md)")));
    EXPECT_TRUE(after.contains(QStringLiteral("Notes")));
    EXPECT_TRUE(after.indexOf(QStringLiteral("Notes"))
                > after.lastIndexOf(CorpusIndex::tocEndMarker()));
}

TEST(CorpusIndexTest, DefaultTocTemplateHasBothMarkers)
{
    const QString t = CorpusIndex::defaultTocTemplate();
    EXPECT_TRUE(t.contains(CorpusIndex::tocStartMarker()));
    EXPECT_TRUE(t.contains(CorpusIndex::tocEndMarker()));
    EXPECT_TRUE(t.indexOf(CorpusIndex::tocStartMarker())
                < t.indexOf(CorpusIndex::tocEndMarker()));
}

namespace {

// Builds a single-document corpus whose doc contains `content` and renders
// just its links (no header), for description-format assertions.
QString renderSingleDocLinks(const QString &content)
{
    QTemporaryDir dir;
    if (!dir.isValid())
        return QString();
    if (!QDir(dir.path()).mkpath("docs"))
        return QString();
    if (!writeFile(dir.path() + "/docs/a.md", content.toUtf8()))
        return QString();

    Corpus corpus;
    corpus.filePath = dir.path() + "/my.scriba";
    corpus.name = QStringLiteral("My Corpus");
    corpus.documents = { CorpusDocument{ .path = QStringLiteral("docs/a.md") } };

    QHash<QString, QString> pageLinkByAbs;
    pageLinkByAbs.insert(Corpus::absolutePath(dir.path(), QStringLiteral("docs/a.md")),
                         QStringLiteral("docs/a.md"));
    return CorpusIndex::renderTocLinks(corpus, pageLinkByAbs);
}

} // namespace

TEST(CorpusIndexTest, RenderTocIncludesDescriptionEmDash)
{
    QSettings().setValue(Preferences::CorpusTocDescriptionFormat, QStringLiteral("emDash"));
    const QString toc = renderSingleDocLinks(QStringLiteral(
        "---\ntoc-description: \"A short description\"\n---\n# Alpha\n"));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [a.md](docs/a.md) \u2014 A short description")));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [Alpha](docs/a.md#alpha)")));
}

TEST(CorpusIndexTest, RenderTocIncludesDescriptionColon)
{
    QSettings().setValue(Preferences::CorpusTocDescriptionFormat, QStringLiteral("colon"));
    const QString toc = renderSingleDocLinks(QStringLiteral(
        "---\ntoc-description: \"A short description\"\n---\n# Alpha\n"));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [a.md](docs/a.md): A short description")));
}

TEST(CorpusIndexTest, RenderTocIncludesDescriptionIndented)
{
    QSettings().setValue(Preferences::CorpusTocDescriptionFormat, QStringLiteral("indented"));
    const QString toc = renderSingleDocLinks(QStringLiteral(
        "---\ntoc-description: \"A short description\"\n---\n# Alpha\n"));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [a.md](docs/a.md)\n  A short description")));
}

TEST(CorpusIndexTest, RenderTocDefaultFormatIsEmDash)
{
    QSettings().remove(Preferences::CorpusTocDescriptionFormat);
    const QString toc = renderSingleDocLinks(QStringLiteral(
        "---\ntoc-description: \"A short description\"\n---\n# Alpha\n"));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [a.md](docs/a.md) \u2014 A short description")));
}

TEST(CorpusIndexTest, RenderTocExternalSectionIncludesDescription)
{
    QSettings().setValue(Preferences::CorpusTocDescriptionFormat, QStringLiteral("emDash"));
    QTemporaryDir dir;
    QTemporaryDir ext;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(ext.isValid());
    const QString extAbs = ext.path() + "/ext.md";
    ASSERT_TRUE(writeFile(extAbs, "---\ntoc-description: External desc\n---\n# Ext\n"));

    Corpus corpus;
    corpus.filePath = dir.path() + "/my.scriba";
    corpus.name = QStringLiteral("My Corpus");
    corpus.documents = { CorpusDocument{ .path = extAbs } };

    QHash<QString, QString> pageLinkByAbs;
    pageLinkByAbs.insert(extAbs, QUrl::fromLocalFile(extAbs).toString());

    const QString toc = CorpusIndex::renderTocLinks(corpus, pageLinkByAbs);

    EXPECT_TRUE(toc.contains(QStringLiteral("## External documents")));
    EXPECT_TRUE(toc.contains(QStringLiteral("- [ext.md](") + QUrl::fromLocalFile(extAbs).toString()
                                 + QStringLiteral(") \u2014 External desc")));
}

} // namespace

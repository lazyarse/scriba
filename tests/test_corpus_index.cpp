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
#include <QTemporaryDir>
#include <QStringList>
#include <QUrl>

#include <memory>

#include "Corpus.h"
#include "CorpusIndex.h"
#include "LinkValidator.h"

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

} // namespace

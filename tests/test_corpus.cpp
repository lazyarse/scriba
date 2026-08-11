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
#include <QJsonDocument>
#include <QTemporaryDir>

#include "Corpus.h"

namespace {

QTemporaryDir makeRoot()
{
    QTemporaryDir dir;
    EXPECT_TRUE(dir.isValid());
    return dir;
}

TEST(Corpus, StoredPathIsRelativeInsideRoot)
{
    QTemporaryDir d = makeRoot();
    const QString root = d.path();
    QDir(root).mkpath(QStringLiteral("chapters"));
    EXPECT_EQ(Corpus::storedPath(root, root + "/chapters/intro.md"),
              QStringLiteral("chapters/intro.md"));
    EXPECT_EQ(Corpus::storedPath(root, root + "/top.md"), QStringLiteral("top.md"));
}

TEST(Corpus, StoredPathFallsBackToAbsoluteOutsideRoot)
{
    QTemporaryDir d = makeRoot();
    const QString root = d.path();
    const QString outside = QDir::tempPath() + "/scriba-corpus-test-outside/x.md";
    EXPECT_EQ(Corpus::storedPath(root, outside), outside);
}

TEST(Corpus, AbsolutePathResolvesAgainstRoot)
{
    QTemporaryDir d = makeRoot();
    const QString root = d.path();
    EXPECT_EQ(Corpus::absolutePath(root, QStringLiteral("chapters/intro.md")),
              root + "/chapters/intro.md");
    EXPECT_EQ(Corpus::absolutePath(root, "/abs/path/x.md"), QStringLiteral("/abs/path/x.md"));
}

TEST(Corpus, RoundTripPreservesEverything)
{
    QTemporaryDir d = makeRoot();
    const QString root = d.path();
    QDir(root).mkpath(QStringLiteral("chapters"));
    const QString corpusPath = root + "/mybook.scriba";

    Corpus c;
    c.filePath = corpusPath;
    c.name = QStringLiteral("mybook");
    c.active = 1;
    c.monitor = false;
    c.dictionary.language = QStringLiteral("en_GB");
    c.dictionary.dialect = QStringLiteral("British");
    c.dictionary.customWords = {QStringLiteral("lazyarse")};
    c.dictionary.ignoredWords = {QStringLiteral("flibbertigibbet")};

    CorpusDocument d0;
    d0.path = QStringLiteral("chapters/intro.md");
    d0.cursorBlock = 12; d0.cursorCol = 4; d0.scroll = 340; d0.folds = {2, 7};
    CorpusDocument d1;
    d1.content = QStringLiteral("unsaved text"); d1.name = QStringLiteral("notes");
    d1.cursorBlock = 0; d1.cursorCol = 5;
    c.documents = {d0, d1};

    ASSERT_TRUE(c.save());
    Corpus out;
    ASSERT_TRUE(Corpus::loadFile(corpusPath, &out));
    EXPECT_EQ(out.name, c.name);
    EXPECT_EQ(out.active, c.active);
    EXPECT_FALSE(out.monitor);
    EXPECT_EQ(out.dictionary.language, QStringLiteral("en_GB"));
    EXPECT_EQ(out.dictionary.dialect, QStringLiteral("British"));
    EXPECT_EQ(out.dictionary.customWords, c.dictionary.customWords);
    EXPECT_EQ(out.dictionary.ignoredWords, c.dictionary.ignoredWords);
    ASSERT_EQ(out.documents.size(), 2);
    EXPECT_EQ(out.documents[0].path, QStringLiteral("chapters/intro.md"));
    EXPECT_EQ(out.documents[0].cursorBlock, 12);
    EXPECT_EQ(out.documents[0].folds, QList<int>({2, 7}));
    EXPECT_EQ(out.documents[1].content, QStringLiteral("unsaved text"));
    EXPECT_EQ(out.documents[1].name, QStringLiteral("notes"));
}

TEST(Corpus, MonitorDefaultsToTrueWhenAbsent)
{
    const QByteArray json = R"({ "version": 1, "documents": [] })";
    const Corpus c = Corpus::fromJson(QJsonDocument::fromJson(json).object(), QString());
    EXPECT_TRUE(c.monitor);
}

TEST(Corpus, SaveWritesWithMonitorAbsentWhenTrue)
{
    QTemporaryDir d = makeRoot();
    Corpus c;
    c.filePath = d.path() + "/m.scriba";
    c.documents = {};
    ASSERT_TRUE(c.save());
    QFile f(d.path() + "/m.scriba");
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    const QJsonObject saved = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    EXPECT_FALSE(saved.contains("monitor"));
}

TEST(Corpus, LoadOfMissingFileFails)
{
    Corpus out;
    QString error;
    EXPECT_FALSE(Corpus::loadFile("/nonexistent/path/x.scriba", &out, &error));
    EXPECT_FALSE(error.isEmpty());
}

} // namespace
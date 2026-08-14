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
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "corpus/CorpusWatcher.h"

namespace {

void writeFile(const QString &path, const QString &content)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(content.toUtf8());
    f.close();
}

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s;
}

class CorpusWatcherTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_dir.reset(new QTemporaryDir);
        ASSERT_TRUE(m_dir->isValid());
        m_dirPath = m_dir->path();
    }

    QString file(const QString &name) const { return m_dirPath + "/" + name; }

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_dirPath;
};

TEST_F(CorpusWatcherTest, EditEmitsEditedSignal)
{
    const QString a = file("a.md");
    writeFile(a, "hello");
    CorpusWatcher watcher;
    QSignalSpy edited(&watcher, &CorpusWatcher::edited);
    watcher.setMonitoredFiles({a});

    QTest::qWait(50);
    writeFile(a, "hello world");
    EXPECT_TRUE(edited.wait(2000));
    EXPECT_EQ(edited.count(), 1);
    EXPECT_EQ(edited.first().at(0).toString(), QFileInfo(a).absoluteFilePath());
}

TEST_F(CorpusWatcherTest, RenameEmitsRenamedSignal)
{
    const QString a = file("a.md");
    writeFile(a, "same content here");
    CorpusWatcher watcher;
    QSignalSpy renamed(&watcher, &CorpusWatcher::renamed);
    watcher.setMonitoredFiles({a});

    QTest::qWait(50);
    const QString c = file("c.md");
    ASSERT_TRUE(QFile::rename(a, c));       // content preserved
    EXPECT_TRUE(renamed.wait(2000));
    ASSERT_EQ(renamed.count(), 1);
    EXPECT_EQ(renamed.first().at(0).toString(), QFileInfo(a).absoluteFilePath());
    EXPECT_EQ(renamed.first().at(1).toString(), QFileInfo(c).absoluteFilePath());
}

TEST_F(CorpusWatcherTest, CrosswiseRenameDisambiguatedByHash)
{
    const QString a = file("a.md");
    const QString b = file("b.md");
    writeFile(a, "aaaa");
    writeFile(b, "bbbb");
    CorpusWatcher watcher;
    QSignalSpy renamed(&watcher, &CorpusWatcher::renamed);
    watcher.setMonitoredFiles({a, b});

    QTest::qWait(50);
    // Swap: a's content lands in d, b's content in c.
    QFile::remove(a);
    QFile::remove(b);
    writeFile(file("c.md"), "bbbb");
    writeFile(file("d.md"), "aaaa");

    EXPECT_TRUE(renamed.wait(2000));
    ASSERT_EQ(renamed.count(), 2);
    QSet<QPair<QString, QString>> pairs;
    for (const auto &args : renamed) {
        pairs.insert({args.at(0).toString(), args.at(1).toString()});
    }
    EXPECT_TRUE(pairs.contains({QFileInfo(a).absoluteFilePath(), QFileInfo(file("d.md")).absoluteFilePath()}));
    EXPECT_TRUE(pairs.contains({QFileInfo(b).absoluteFilePath(), QFileInfo(file("c.md")).absoluteFilePath()}));
}

TEST_F(CorpusWatcherTest, DeleteEmitsDeletedSignal)
{
    const QString a = file("a.md");
    const QString b = file("b.md");
    writeFile(a, "aaa");
    writeFile(b, "bbb");
    CorpusWatcher watcher;
    QSignalSpy deleted(&watcher, &CorpusWatcher::deleted);
    watcher.setMonitoredFiles({a, b});

    QTest::qWait(50);
    ASSERT_TRUE(QFile::remove(a));
    EXPECT_TRUE(deleted.wait(2000));
    ASSERT_EQ(deleted.count(), 1);
    EXPECT_EQ(deleted.first().at(0).toString(), QFileInfo(a).absoluteFilePath());
}

TEST_F(CorpusWatcherTest, NewUnmatchedFileEmitsNoSignal)
{
    const QString a = file("a.md");
    writeFile(a, "aaa");
    CorpusWatcher watcher;
    QSignalSpy edited(&watcher, &CorpusWatcher::edited);
    QSignalSpy renamed(&watcher, &CorpusWatcher::renamed);
    QSignalSpy deleted(&watcher, &CorpusWatcher::deleted);
    watcher.setMonitoredFiles({a});

    QTest::qWait(50);
    writeFile(file("brand-new.md"), "unmatched fresh file");
    QTest::qWait(1500);                      // > debounce
    EXPECT_EQ(edited.count(), 0);
    EXPECT_EQ(renamed.count(), 0);
    EXPECT_EQ(deleted.count(), 0);
    EXPECT_EQ(readFile(file("brand-new.md")), QStringLiteral("unmatched fresh file"));
}

TEST_F(CorpusWatcherTest, EditDoesNotRefireOnUnrelatedChange)
{
    const QString a = file("a.md");
    writeFile(a, "hello");
    CorpusWatcher watcher;
    QSignalSpy edited(&watcher, &CorpusWatcher::edited);
    watcher.setMonitoredFiles({a});

    QTest::qWait(50);
    writeFile(a, "hello edited");
    ASSERT_TRUE(edited.wait(2000));
    EXPECT_EQ(edited.count(), 1);

    // An unrelated directory event (a fresh unpaired file) must not re-report
    // a as changed: its content hash was refreshed when the edit was classified.
    writeFile(file("brand-new.md"), "unrelated");
    QTest::qWait(1500);                      // > debounce
    EXPECT_EQ(edited.count(), 1);
}

TEST_F(CorpusWatcherTest, DeleteDoesNotRefireOnUnrelatedChange)
{
    const QString a = file("a.md");
    const QString b = file("b.md");
    writeFile(a, "aaa");
    writeFile(b, "bbb");
    CorpusWatcher watcher;
    QSignalSpy deleted(&watcher, &CorpusWatcher::deleted);
    watcher.setMonitoredFiles({a, b});

    QTest::qWait(50);
    ASSERT_TRUE(QFile::remove(a));
    ASSERT_TRUE(deleted.wait(2000));
    EXPECT_EQ(deleted.count(), 1);

    // A later unrelated event must not re-report the purge as a delete: the
    // entry was dropped from the watched set when it was classified.
    writeFile(b, "bbb edited");
    QTest::qWait(1500);                      // > debounce
    EXPECT_EQ(deleted.count(), 1);
}

TEST_F(CorpusWatcherTest, DeletePlusNewFileInSameDirNotRename)
{
    const QString a = file("a.md");
    writeFile(a, "aaa");
    CorpusWatcher watcher;
    QSignalSpy renamed(&watcher, &CorpusWatcher::renamed);
    QSignalSpy deleted(&watcher, &CorpusWatcher::deleted);
    watcher.setMonitoredFiles({a});

    QTest::qWait(50);
    // Delete a.md and create an unrelated file in the same directory. Despite
    // being the only fresh file in the dir, it must not be paired as a rename:
    // rename detection requires identical content.
    ASSERT_TRUE(QFile::remove(a));
    writeFile(file("notes.md"), "unrelated notes");

    EXPECT_TRUE(deleted.wait(2000));
    ASSERT_EQ(deleted.count(), 1);
    EXPECT_EQ(deleted.first().at(0).toString(), QFileInfo(a).absoluteFilePath());
    EXPECT_EQ(renamed.count(), 0);
}

TEST_F(CorpusWatcherTest, RenameWithEditReportedAsDelete)
{
    const QString a = file("a.md");
    writeFile(a, "original content");
    CorpusWatcher watcher;
    QSignalSpy renamed(&watcher, &CorpusWatcher::renamed);
    QSignalSpy deleted(&watcher, &CorpusWatcher::deleted);
    watcher.setMonitoredFiles({a});

    QTest::qWait(50);
    // Rename while also editing the content: the hash no longer matches, so
    // this surfaces as a delete (the fresh file is left unpaired).
    ASSERT_TRUE(QFile::rename(a, file("renamed.md")));
    writeFile(file("renamed.md"), "content edited during rename");

    EXPECT_TRUE(deleted.wait(2000));
    ASSERT_EQ(deleted.count(), 1);
    EXPECT_EQ(deleted.first().at(0).toString(), QFileInfo(a).absoluteFilePath());
    EXPECT_EQ(renamed.count(), 0);
}

} // namespace

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
// Widget-level tests for the Corpus Files sidecar panel: listing, exclusion,
// recursion, activation emission and the file-system-watcher-driven live
// updates. No WebEngine.
#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

#include <functional>
#include <memory>

#include "corpus/CorpusFilesPanel.h"

namespace {

bool waitFor(const std::function<bool()> &cond, int timeoutMs = 5000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        QApplication::processEvents();
        QTest::qWait(50);
        if (cond())
            return true;
    }
    QApplication::processEvents();
    return cond();
}

class CorpusFilesPanelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_dir.reset(new QTemporaryDir);
        ASSERT_TRUE(m_dir->isValid());
        m_root = m_dir->path();
    }

    void write(const QString &rel, const QString &content = QString())
    {
        QDir().mkpath(QFileInfo(m_root + "/" + rel).absolutePath());
        QFile f(m_root + "/" + rel);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        f.write(content.toUtf8());
        f.close();
    }

    QTreeView *tree() const { return m_panel.findChild<QTreeView *>(); }
    QSortFilterProxyModel *proxy() const
    {
        return qobject_cast<QSortFilterProxyModel *>(tree()->model());
    }
    QModelIndex rootIndex() const { return tree()->rootIndex(); }

    // Names of the rows the view shows at the top level.
    QStringList rootNames() const
    {
        QStringList names;
        const int count = proxy()->rowCount(rootIndex());
        for (int i = 0; i < count; ++i) {
            const QModelIndex idx = proxy()->index(i, 0, rootIndex());
            if (idx.isValid())
                names.append(idx.data().toString());
        }
        return names;
    }

    // Is `name` (a fileName or dir name under the rooted directory) visible in
    // the listing? Path-based and non-recursive: QFileSystemModel nodes are
    // persistent, so holding source indices across async directory fetches is
    // safe — unlike proxy indices, which the QSortFilterProxyModel rebuilds
    // whenever the source model inserts/fetches rows. The exclusion proxy is
    // honoured via mapFromSource, which only resolves to a valid index once
    // the model's watcher has told it about the row (so this genuinely checks
    // the watcher-driven listing, not just "the file exists on disk").
    bool visible(const QString &name) const
    {
        QFileSystemModel *fs = m_panel.findChild<QFileSystemModel *>();
        if (!fs)
            return false;
        const QModelIndex src = fs->index(m_root + "/" + name);
        if (!src.isValid())
            return false;
        return proxy()->mapFromSource(src).isValid();
    }

    // Drain the event loop so the file-system model can catch up with
    // population/fetches before we query it.
    void settle(int ms = 200)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms) {
            QApplication::processEvents();
            QTest::qWait(10);
        }
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_root;
    CorpusFilesPanel m_panel;          // member so it outlives the test
};

TEST_F(CorpusFilesPanelTest, ListsFilesDirectoriesAndSubdir)
{
    write("a.md", "a");
    write("b.pdf", "pdf");
    write("img.png", "png");
    write("sub/c.md", "c");
    write(".hidden.md", "h");
    write("corpus.scriba", "{}");

    m_panel.setRootDir(m_root);
    m_panel.setExcludedPath(m_root + "/corpus.scriba");
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    settle();

    EXPECT_TRUE(visible("a.md"));
    EXPECT_TRUE(visible("b.pdf"));
    EXPECT_TRUE(visible("img.png"));
    EXPECT_TRUE(visible("sub"));
    const QStringList names = rootNames();
    EXPECT_FALSE(names.contains(".hidden.md")) << "hidden dotfiles are excluded by the model filter";
    EXPECT_FALSE(names.contains("corpus.scriba")) << "the corpus's own .scriba is excluded by the proxy";

    // Expand the sub directory and confirm recursion into it (the listing for
    // sub/ is fetched lazily, so give the model time before checking).
    const int count = proxy()->rowCount(rootIndex());
    QModelIndex subIdx;
    for (int i = 0; i < count; ++i) {
        const QModelIndex idx = proxy()->index(i, 0, rootIndex());
        if (idx.data().toString() == QLatin1String("sub")) {
            subIdx = idx;
            break;
        }
    }
    ASSERT_TRUE(subIdx.isValid());
    tree()->expand(subIdx);
    settle();
    EXPECT_TRUE(waitFor([&] { return visible("sub/c.md"); }))
        << "a file inside a subdirectory must appear once its dir is fetched";
}

TEST_F(CorpusFilesPanelTest, ClearResetsListing)
{
    write("a.md", "a");
    m_panel.setRootDir(m_root);
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    settle();
    EXPECT_TRUE(visible("a.md"));

    m_panel.clear();
    EXPECT_EQ(proxy()->rowCount(rootIndex()), 0);
}

TEST_F(CorpusFilesPanelTest, DoubleClickFileEmitsActivated)
{
    write("a.md", "a");
    m_panel.setRootDir(m_root);
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    settle();

    QModelIndex target;
    const int count = proxy()->rowCount(rootIndex());
    for (int i = 0; i < count; ++i) {
        const QModelIndex idx = proxy()->index(i, 0, rootIndex());
        if (idx.data().toString() == QLatin1String("a.md")) {
            target = idx;
            break;
        }
    }
    ASSERT_TRUE(target.isValid());

    QSignalSpy spy(&m_panel, &CorpusFilesPanel::fileActivated);
    QTreeView *v = tree();
    v->scrollTo(target);
    const QRect rect = v->visualRect(target);
    ASSERT_FALSE(rect.isEmpty()) << "a.md row must be visible in the viewport";
    // Click the row once to arm the view, then double-click it: a bare
    // QTest::mouseDClick is not recognized as a double-click until the item
    // has a prior press within doubleClickInterval.
    QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, rect.center());
    QTest::qWait(60);
    QTest::mouseDClick(v->viewport(), Qt::LeftButton, {}, rect.center());
    QApplication::processEvents();

    // Qt emits both `activated` and `doubleClicked` for the double-click; the
    // panel dedupes them, so exactly one fileActivated must arrive.
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toString(),
              QFileInfo(m_root + "/a.md").absoluteFilePath());
}

TEST_F(CorpusFilesPanelTest, DoubleClickDirectoryDoesNotEmit)
{
    write("sub/c.md", "c");
    m_panel.setRootDir(m_root);
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    settle();

    QModelIndex target;
    const int count = proxy()->rowCount(rootIndex());
    for (int i = 0; i < count; ++i) {
        const QModelIndex idx = proxy()->index(i, 0, rootIndex());
        if (idx.data().toString() == QLatin1String("sub")) {
            target = idx;
            break;
        }
    }
    ASSERT_TRUE(target.isValid());

    QSignalSpy spy(&m_panel, &CorpusFilesPanel::fileActivated);
    QTreeView *v = tree();
    v->scrollTo(target);
    const QRect rect = v->visualRect(target);
    ASSERT_FALSE(rect.isEmpty());
    QTest::mouseClick(v->viewport(), Qt::LeftButton, {}, rect.center());
    QTest::qWait(60);
    QTest::mouseDClick(v->viewport(), Qt::LeftButton, {}, rect.center());
    QApplication::processEvents();

    EXPECT_EQ(spy.count(), 0) << "directories expand, never emit fileActivated";
}

TEST_F(CorpusFilesPanelTest, WatcherListsNewlyCreatedFile)
{
    write("a.md", "a");
    m_panel.setRootDir(m_root);
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    EXPECT_TRUE(visible("a.md"));

    write("new.md", "n");
    EXPECT_TRUE(waitFor([&] { return visible("new.md"); }))
        << "a file created after rooting must appear in the listing";
}

TEST_F(CorpusFilesPanelTest, WatcherDropsDeletedFile)
{
    write("a.md", "a");
    m_panel.setRootDir(m_root);
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    EXPECT_TRUE(visible("a.md"));

    ASSERT_TRUE(QFile::remove(m_root + "/a.md"));
    EXPECT_TRUE(waitFor([&] { return !visible("a.md"); }))
        << "a deleted file must disappear from the listing";
}

TEST_F(CorpusFilesPanelTest, WatcherRenameShowsNewName)
{
    write("a.md", "a");
    m_panel.setRootDir(m_root);
    m_panel.resize(400, 400);
    m_panel.show();
    (void)QTest::qWaitForWindowExposed(&m_panel);
    EXPECT_TRUE(visible("a.md"));

    ASSERT_TRUE(QFile::rename(m_root + "/a.md", m_root + "/renamed.md"));
    EXPECT_TRUE(waitFor([&] { return visible("renamed.md") && !visible("a.md"); }));
}

} // namespace
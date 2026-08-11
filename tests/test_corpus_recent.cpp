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
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QSettings>
#include <QSet>
#include <QTemporaryDir>

#include "MainWindow.h"
#include "Preferences.h"
#include "TestConfig.h"

namespace {

QString writeTempCorpus(const QString &dir, const QString &name)
{
    const QString path = dir + "/" + name;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write("{\"version\":1,\"documents\":[]}");
    f.close();
    return path;
}

class CorpusRecentTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        s_window = new MainWindow(nullptr, /*skipCorpusRestore=*/true);
        QApplication::processEvents();
    }

    static void TearDownTestSuite()
    {
        delete s_window;
        s_window = nullptr;
    }

    void SetUp() override
    {
        QSettings().clear();
    }

    static QMenu *recentMenu()
    {
        for (QMenu *m : s_window->findChildren<QMenu *>())
            if (m->title().contains(QStringLiteral("Recent")))
                return m;
        return nullptr;
    }

    static MainWindow *s_window;
};

MainWindow *CorpusRecentTest::s_window = nullptr;

TEST_F(CorpusRecentTest, MenuPrunesStaleAndShowsRecents)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString a = writeTempCorpus(dir.path(), QStringLiteral("a.scriba"));
    const QString b = writeTempCorpus(dir.path(), QStringLiteral("b.scriba"));

    QSettings s;
    s.setValue(Preferences::RecentCorpora,
               QStringList{b, a, QStringLiteral("/nonexistent/stale.scriba")});

    s_window->updateRecentCorporaMenu();

    const QStringList recents = s.value(Preferences::RecentCorpora).toStringList();
    EXPECT_FALSE(recents.contains(QStringLiteral("/nonexistent/stale.scriba")));
    ASSERT_EQ(recents.size(), 2);
    EXPECT_EQ(recents[0], b);
    EXPECT_EQ(recents[1], a);

    QMenu *menu = recentMenu();
    ASSERT_TRUE(menu);
    EXPECT_EQ(menu->actions().size(), 2);
}

TEST_F(CorpusRecentTest, AddRecentPromotesToFrontAndCapsAtFive)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QStringList list;
    for (int i = 1; i <= 5; ++i)
        list.append(writeTempCorpus(dir.path(), QStringLiteral("f%1.scriba").arg(i)));

    QSettings s;
    s.setValue(Preferences::RecentCorpora, list);

    // Reopening an existing corpus promotes it to the front (no duplicate).
    s_window->addRecentCorpus(list[2]);
    QStringList recents = s.value(Preferences::RecentCorpora).toStringList();
    ASSERT_EQ(recents.size(), Preferences::MaxRecentCorpora);
    EXPECT_EQ(recents[0], list[2]);
    EXPECT_EQ(QSet<QString>(recents.begin(), recents.end()).size(), recents.size());   // no duplicates

    // Adding a 6th drops the least-recent entry.
    const QString sixth = writeTempCorpus(dir.path(), QStringLiteral("f6.scriba"));
    s_window->addRecentCorpus(sixth);
    recents = s.value(Preferences::RecentCorpora).toStringList();
    ASSERT_EQ(recents.size(), Preferences::MaxRecentCorpora);
    EXPECT_EQ(recents[0], sixth);
    EXPECT_FALSE(recents.contains(list.back()));
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
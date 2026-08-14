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
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QTemporaryDir>
#include <memory>
#include "mermaid/GitGraphBuilder.h"
#include "GitTestRepo.h"

static int g_argc = 1;
static char g_arg0[] = "test_gitgraph";
static char *g_argv[] = {g_arg0, nullptr};

namespace {

int countSubstring(const QString &hay, const QString &needle)
{
    int n = 0;
    int idx = 0;
    while ((idx = hay.indexOf(needle, idx)) >= 0) {
        ++n;
        idx += needle.size();
    }
    return n;
}

} // namespace

class GitGraphBuilderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QCoreApplication::instance())
            new QCoreApplication(g_argc, g_argv);
    }

    void SetUp() override
    {
        m_repo = std::make_unique<QTemporaryDir>();
        ASSERT_TRUE(m_repo->isValid());
        ASSERT_TRUE(GitTestRepo::create(m_repo->path()));
    }

    QString build(GitGraphBuilder::Options opts)
    {
        QString error;
        QString out = GitGraphBuilder::build(m_repo->path(), opts, &error);
        m_lastError = error;
        return out;
    }

    std::unique_ptr<QTemporaryDir> m_repo;
    QString m_lastError;
};

TEST_F(GitGraphBuilderTest, RepoInfoReportsBranchesAndDates)
{
    QStringList branches;
    QString current;
    QDateTime first, last;
    QString error;
    ASSERT_TRUE(GitGraphBuilder::repoInfo(m_repo->path(), &branches, &current,
                                          &first, &last, &error));
    EXPECT_TRUE(branches.contains(QStringLiteral("main")));
    EXPECT_TRUE(branches.contains(QStringLiteral("feature")));
    EXPECT_EQ(current, QStringLiteral("main"));
    EXPECT_EQ(first, QDateTime(QDate(2026, 1, 1), QTime(10, 0), Qt::UTC));
    EXPECT_EQ(last, QDateTime(QDate(2026, 1, 6), QTime(10, 0), Qt::UTC));
}

TEST_F(GitGraphBuilderTest, AllCommitsIncludeMergeAndFeature)
{
    GitGraphBuilder::Options opts;
    opts.maxCommits = 0;
    QString out = build(opts);
    ASSERT_FALSE(out.isEmpty());
    EXPECT_TRUE(out.startsWith(QStringLiteral("gitGraph")));
    EXPECT_EQ(countSubstring(out, QStringLiteral("commit id:")), 5);
    EXPECT_EQ(countSubstring(out, QStringLiteral("merge")), 1);
    EXPECT_TRUE(out.contains(QStringLiteral("branch feature")));
    EXPECT_TRUE(out.contains(QStringLiteral("merge feature")));
}

TEST_F(GitGraphBuilderTest, BranchFilterLimitsToThatBranch)
{
    GitGraphBuilder::Options opts;
    opts.limit = GitGraphBuilder::Options::Limit::Branch;
    opts.branch = QStringLiteral("feature");
    opts.maxCommits = 0;
    QString out = build(opts);
    ASSERT_FALSE(out.isEmpty());
    EXPECT_EQ(countSubstring(out, QStringLiteral("commit id:")), 4);
    EXPECT_FALSE(out.contains(QStringLiteral("merge")));
}

TEST_F(GitGraphBuilderTest, DateRangeFiltersCommits)
{
    GitGraphBuilder::Options opts;
    opts.limit = GitGraphBuilder::Options::Limit::Dates;
    opts.from = QDateTime(QDate(2026, 1, 4), QTime(0, 0), Qt::UTC);
    opts.to = QDateTime(QDate(2026, 1, 6), QTime(23, 59, 59), Qt::UTC);
    opts.maxCommits = 0;
    QString out = build(opts);
    ASSERT_FALSE(out.isEmpty());
    EXPECT_EQ(countSubstring(out, QStringLiteral("commit id:")), 2); // c4, c5
    EXPECT_TRUE(out.contains(QStringLiteral("merge feature")));      // c6
}

TEST_F(GitGraphBuilderTest, BranchAndDateRange)
{
    GitGraphBuilder::Options opts;
    opts.limit = GitGraphBuilder::Options::Limit::BranchAndDates;
    opts.branch = QStringLiteral("feature");
    opts.from = QDateTime(QDate(2026, 1, 1), QTime(0, 0), Qt::UTC);
    opts.to = QDateTime(QDate(2026, 1, 5), QTime(23, 59, 59), Qt::UTC);
    opts.maxCommits = 0;
    QString out = build(opts);
    ASSERT_FALSE(out.isEmpty());
    EXPECT_EQ(countSubstring(out, QStringLiteral("commit id:")), 4); // c1, c2, c4, c5
    EXPECT_FALSE(out.contains(QStringLiteral("merge")));
}

TEST_F(GitGraphBuilderTest, MaxCommitsCapsOutput)
{
    GitGraphBuilder::Options opts;
    opts.maxCommits = 2;
    QString out = build(opts);
    ASSERT_FALSE(out.isEmpty());
    // Newest commits are the merge (c6) and c5; the merge is a `merge` line.
    EXPECT_EQ(countSubstring(out, QStringLiteral("commit id:")), 1);
    EXPECT_TRUE(out.contains(QStringLiteral("merge feature")));
}

TEST_F(GitGraphBuilderTest, InvalidPathReturnsError)
{
    QString error;
    GitGraphBuilder::Options opts;
    QString out = GitGraphBuilder::build(QStringLiteral("/nonexistent/path"),
                                         opts, &error);
    EXPECT_TRUE(out.isEmpty());
    EXPECT_FALSE(error.isEmpty());
}

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
#include "GitGraphBuilder.h"

#include <git2.h>

#include <algorithm>
#include <memory>

#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

namespace {

// RAII for libgit2 handles
struct RepoDeleter { void operator()(git_repository *r) const { git_repository_free(r); } };
struct RevwalkDeleter { void operator()(git_revwalk *w) const { git_revwalk_free(w); } };

using RepoPtr = std::unique_ptr<git_repository, RepoDeleter>;
using RevwalkPtr = std::unique_ptr<git_revwalk, RevwalkDeleter>;

QString errorMessage(int code)
{
    const git_error *e = git_error_last();
    if (e && e->message)
        return QString::fromUtf8(e->message);
    return QStringLiteral("libgit2 error %1").arg(code);
}

QString fullOid(const git_oid *oid)
{
    char buf[GIT_OID_MAX_HEXSIZE + 1];
    git_oid_tostr(buf, sizeof(buf), oid);
    return QString::fromLatin1(buf);
}

QString shortId(const git_oid *oid)
{
    return fullOid(oid).left(7);
}

// Branch names appear verbatim in the mermaid source; replace characters the
// gitGraph parser does not accept with '_'.
QString sanitizeBranch(const QString &name)
{
    QString out = name;
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")),
                QStringLiteral("_"));
    return out.isEmpty() ? QStringLiteral("branch") : out;
}

bool openRepo(const QString &path, RepoPtr *out, QString *error)
{
    git_repository *repo = nullptr;
    int rc = git_repository_open_ext(&repo, path.toUtf8().constData(), 0, nullptr);
    if (rc != 0) {
        if (error)
            *error = errorMessage(rc);
        return false;
    }
    out->reset(repo);
    return true;
}

QString currentBranchName(git_repository *repo)
{
    git_reference *head = nullptr;
    if (git_repository_head(&head, repo) != 0)
        return {};
    const char *name = nullptr;
    QString result;
    if (git_branch_name(&name, head) == 0)
        result = QString::fromUtf8(name);
    git_reference_free(head);
    return result;
}

QStringList localBranches(git_repository *repo)
{
    QStringList result;
    git_branch_iterator *it = nullptr;
    git_branch_t type;
    git_reference *ref = nullptr;
    if (git_branch_iterator_new(&it, repo, GIT_BRANCH_LOCAL) != 0)
        return result;
    while (git_branch_next(&ref, &type, it) == 0) {
        const char *name = nullptr;
        if (git_branch_name(&name, ref) == 0)
            result << QString::fromUtf8(name);
        git_reference_free(ref);
        ref = nullptr;
    }
    git_branch_iterator_free(it);
    return result;
}

struct CommitInfo {
    QString id;                 // short id (7 hex)
    qint64 time = 0;            // committer epoch seconds
    QString branch;             // assigned branch name
    QStringList parentOids;     // full oids of parents
    bool isMerge = false;
    QString mergedBranch;       // for merges: branch brought in
};

} // namespace

bool GitGraphBuilder::repoInfo(const QString &repoPath, QStringList *branches,
                               QString *currentBranch, QDateTime *firstCommit,
                               QDateTime *lastCommit, QString *error)
{
    git_libgit2_init();
    RepoPtr repo;
    if (!openRepo(repoPath, &repo, error)) {
        git_libgit2_shutdown();
        return false;
    }

    if (branches)
        *branches = localBranches(repo.get());
    if (currentBranch)
        *currentBranch = currentBranchName(repo.get());

    if (firstCommit || lastCommit) {
        RevwalkPtr walk;
        git_revwalk *w = nullptr;
        if (git_revwalk_new(&w, repo.get()) == 0) {
            walk.reset(w);
            git_revwalk_sorting(w, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
            git_revwalk_push_head(w);
            git_oid oid;
            qint64 min = 0, max = 0;
            bool any = false;
            while (git_revwalk_next(&oid, w) == 0) {
                git_commit *commit = nullptr;
                if (git_commit_lookup(&commit, repo.get(), &oid) != 0)
                    continue;
                const git_signature *sig = git_commit_committer(commit);
                qint64 t = static_cast<qint64>(sig ? sig->when.time : 0);
                if (!any) {
                    min = max = t;
                    any = true;
                } else {
                    if (t < min) min = t;
                    if (t > max) max = t;
                }
                git_commit_free(commit);
            }
            if (any) {
                if (firstCommit)
                    *firstCommit = QDateTime::fromSecsSinceEpoch(min, Qt::UTC);
                if (lastCommit)
                    *lastCommit = QDateTime::fromSecsSinceEpoch(max, Qt::UTC);
            }
        }
    }

    git_libgit2_shutdown();
    return true;
}

QString GitGraphBuilder::build(const QString &repoPath, const Options &options,
                               QString *error)
{
    git_libgit2_init();
    RepoPtr repo;
    if (!openRepo(repoPath, &repo, error)) {
        git_libgit2_shutdown();
        return {};
    }

    const bool branchFilter =
        options.limit == Options::Limit::Branch
        || options.limit == Options::Limit::BranchAndDates;
    const bool dateFilter =
        options.limit == Options::Limit::Dates
        || options.limit == Options::Limit::BranchAndDates;

    QString mainBranch = currentBranchName(repo.get());
    if (mainBranch.isEmpty())
        mainBranch = QStringLiteral("main");

    // Branch tips to walk: the selected branch or every local branch.
    QStringList tips;
    if (branchFilter && !options.branch.isEmpty()) {
        tips << options.branch;
    } else {
        tips = localBranches(repo.get());
        if (tips.isEmpty())
            tips << mainBranch;
    }
    if (tips.isEmpty()) {
        if (error)
            *error = QStringLiteral("No branches to graph.");
        git_libgit2_shutdown();
        return {};
    }

    qint64 fromSecs = 0, toSecs = 0; // 0 = unbounded
    if (dateFilter) {
        if (options.from.isValid())
            fromSecs = options.from.date().startOfDay().toSecsSinceEpoch();
        if (options.to.isValid())
            toSecs = options.to.date().endOfDay().toSecsSinceEpoch();
    }

    // Walk the DAG (newest-first), applying filters and the commit cap.
    QMap<QString, CommitInfo> commits;
    QStringList order; // full oids, newest-first discovery order
    git_revwalk *w = nullptr;
    if (git_revwalk_new(&w, repo.get()) != 0) {
        if (error)
            *error = QStringLiteral("Could not walk the repository.");
        git_libgit2_shutdown();
        return {};
    }
    RevwalkPtr walk(w);
    git_revwalk_sorting(w, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
    for (const QString &b : tips) {
        git_reference *ref = nullptr;
        if (git_branch_lookup(&ref, repo.get(), b.toUtf8().constData(),
                              GIT_BRANCH_LOCAL) != 0)
            continue;
        git_object *obj = nullptr;
        if (git_reference_peel(&obj, ref, GIT_OBJECT_COMMIT) == 0) {
            git_revwalk_push(w, git_object_id(obj));
            git_object_free(obj);
        }
        git_reference_free(ref);
    }

    git_oid oid;
    while (git_revwalk_next(&oid, w) == 0) {
        const QString key = fullOid(&oid);
        if (commits.contains(key))
            continue;
        git_commit *commit = nullptr;
        if (git_commit_lookup(&commit, repo.get(), &oid) != 0)
            continue;
        const git_signature *sig = git_commit_committer(commit);
        const qint64 t = static_cast<qint64>(sig ? sig->when.time : 0);
        if (dateFilter && (t < fromSecs || (toSecs > 0 && t > toSecs))) {
            git_commit_free(commit);
            continue;
        }
        CommitInfo info;
        info.id = shortId(&oid);
        info.time = t;
        const unsigned n = git_commit_parentcount(commit);
        for (unsigned p = 0; p < n; ++p) {
            const git_oid *poid = git_commit_parent_id(commit, p);
            if (poid)
                info.parentOids << fullOid(poid);
        }
        info.isMerge = n > 1;
        commits.insert(key, info);
        order.append(key);
        git_commit_free(commit);
        if (options.maxCommits > 0 && order.size() >= options.maxCommits)
            break;
    }

    // Assign every commit to a branch via first-parent chains, prioritising
    // the current (main) branch so merges stay on main.
    QStringList branches = localBranches(repo.get());
    branches.removeAll(mainBranch);
    branches.prepend(mainBranch);
    for (const QString &b : branches) {
        if (b.isEmpty())
            continue;
        git_reference *ref = nullptr;
        if (git_branch_lookup(&ref, repo.get(), b.toUtf8().constData(),
                              GIT_BRANCH_LOCAL) != 0)
            continue;
        git_object *obj = nullptr;
        if (git_reference_peel(&obj, ref, GIT_OBJECT_COMMIT) != 0) {
            git_reference_free(ref);
            continue;
        }
        git_reference_free(ref);
        QString cur = fullOid(git_object_id(obj));
        git_object_free(obj);
        while (commits.contains(cur)) {
            CommitInfo &ci = commits[cur];
            if (ci.branch.isEmpty())
                ci.branch = b;
            if (ci.parentOids.isEmpty())
                break;
            cur = ci.parentOids.first();
        }
    }
    const QStringList keys = commits.keys();
    for (const QString &key : keys) {
        if (commits[key].branch.isEmpty())
            commits[key].branch = mainBranch;
    }

    // For merge commits, find the branch that was brought in (the first
    // non-first parent living on a different branch).
    for (const QString &key : keys) {
        CommitInfo &ci = commits[key];
        if (!ci.isMerge)
            continue;
        for (int p = 1; p < ci.parentOids.size(); ++p) {
            const QString &po = ci.parentOids.at(p);
            if (commits.contains(po) && commits[po].branch != ci.branch) {
                ci.mergedBranch = commits[po].branch;
                break;
            }
        }
    }

    // Emit oldest-first so the graph reads left (old) to right (new).
    QStringList emitOrder = keys;
    std::sort(emitOrder.begin(), emitOrder.end(),
              [&commits, &order](const QString &a, const QString &b) {
                  const qint64 ta = commits[a].time, tb = commits[b].time;
                  if (ta != tb)
                      return ta < tb;
                  return order.indexOf(a) > order.indexOf(b);
              });

    if (emitOrder.isEmpty()) {
        if (error)
            *error = QStringLiteral("No commits match the selected limits.");
        git_libgit2_shutdown();
        return {};
    }

    QStringList out;
    out << QStringLiteral("gitGraph");
    QString current = mainBranch;
    QSet<QString> created;
    for (const QString &key : emitOrder) {
        const CommitInfo &ci = commits[key];
        QString target = ci.branch.isEmpty() ? mainBranch : ci.branch;
        if (target != current) {
            if (target != mainBranch && !created.contains(target)) {
                out << QStringLiteral("    branch %1").arg(sanitizeBranch(target));
                created.insert(target);
            }
            out << QStringLiteral("    checkout %1").arg(sanitizeBranch(target));
            current = target;
        }
        if (ci.isMerge) {
            const QString merged =
                ci.mergedBranch.isEmpty() ? current : ci.mergedBranch;
            out << QStringLiteral("    merge %1 id: \"%2\"")
                       .arg(sanitizeBranch(merged), ci.id);
        } else {
            out << QStringLiteral("    commit id: \"%1\"").arg(ci.id);
        }
    }

    git_libgit2_shutdown();
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

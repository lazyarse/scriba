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
#include "GitTestRepo.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>

namespace {

QString timeArg(int day)
{
    return QStringLiteral("2026-01-%1T10:00:00Z").arg(day, 2, 10, QLatin1Char('0'));
}

} // namespace

namespace GitTestRepo {

bool git(const QString &repoDir, const QStringList &args, const QString &date)
{
    QProcess p;
    if (!date.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("GIT_AUTHOR_DATE"), date);
        env.insert(QStringLiteral("GIT_COMMITTER_DATE"), date);
        p.setProcessEnvironment(env);
    }
    p.setWorkingDirectory(repoDir);
    p.start(QStringLiteral("git"), args);
    if (!p.waitForFinished(30000))
        return false;
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

bool create(const QString &dir)
{
    if (!QDir(dir).mkpath(QStringLiteral(".")))
        return false;

    if (!git(dir, {QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")})) {
        if (!git(dir, {QStringLiteral("init")}))
            return false;
        git(dir, {QStringLiteral("checkout"), QStringLiteral("-b"), QStringLiteral("main")});
    }

    git(dir, {QStringLiteral("config"), QStringLiteral("user.name"),
              QStringLiteral("Test User")});
    git(dir, {QStringLiteral("config"), QStringLiteral("user.email"),
              QStringLiteral("test@example.com")});
    git(dir, {QStringLiteral("config"), QStringLiteral("commit.gpgsign"),
              QStringLiteral("false")});

    auto commit = [&dir](const QString &file, const QString &msg, int day) {
        QFile f(dir + QLatin1Char('/') + file);
        if (!f.open(QIODevice::Append))
            return false;
        f.write((msg + QLatin1Char('\n')).toUtf8());
        f.close();
        if (!git(dir, {QStringLiteral("add"), file}))
            return false;
        return git(dir, {QStringLiteral("commit"), QStringLiteral("-m"), msg},
                   timeArg(day));
    };

    if (!commit(QStringLiteral("file.txt"), QStringLiteral("c1"), 1))
        return false;
    if (!commit(QStringLiteral("file.txt"), QStringLiteral("c2"), 2))
        return false;
    if (!git(dir, {QStringLiteral("checkout"), QStringLiteral("-b"),
                   QStringLiteral("feature")}))
        return false;
    if (!commit(QStringLiteral("feature.txt"), QStringLiteral("c4"), 4))
        return false;
    if (!commit(QStringLiteral("feature.txt"), QStringLiteral("c5"), 5))
        return false;
    if (!git(dir, {QStringLiteral("checkout"), QStringLiteral("main")}))
        return false;
    if (!commit(QStringLiteral("main.txt"), QStringLiteral("c3"), 3))
        return false;
    return git(dir, {QStringLiteral("merge"), QStringLiteral("feature"),
                     QStringLiteral("--no-ff"), QStringLiteral("--no-edit")},
               timeArg(6));
}

} // namespace GitTestRepo

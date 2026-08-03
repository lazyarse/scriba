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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "LinkValidator.h"

namespace {

using Status = LinkValidator::Status;

TEST(LinkValidator, ExistingFileInBaseDirIsValid)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile f(dir.filePath(QStringLiteral("note.md")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    QDir(dir.path()).mkpath(QStringLiteral("sub"));

    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("note.md"), dir.path()));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("./note.md"), dir.path()));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("sub/../note.md"), dir.path()));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("sub/.././note.md"), dir.path()));
}

TEST(LinkValidator, MissingFileInBaseDirIsFileNotFound)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(QStringLiteral("missing.md"), dir.path()));
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(QStringLiteral("sub/deep/missing.md"), dir.path()));
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(QStringLiteral("../missing.md"), dir.path()));
}

TEST(LinkValidator, AbsolutePath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile f(dir.filePath(QStringLiteral("abs.md")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(f.fileName()));
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(dir.filePath(QStringLiteral("nope.md"))));
}

TEST(LinkValidator, EmptyBaseDirFallsBackToCwd)
{
    QTemporaryFile probe(QDir::current().filePath(
        QStringLiteral("scriba-link-validator-probe-%1.md")
            .arg(QCoreApplication::applicationPid())));
    ASSERT_TRUE(probe.open());
    const QString name = QFileInfo(probe.fileName()).fileName();
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(name));
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(QStringLiteral("scriba-definitely-missing-probe.md")));
    probe.remove();
}

TEST(LinkValidator, FragmentAndQueryAreStrippedFromFileTargets)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile f(dir.filePath(QStringLiteral("page.md")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("page.md#section"), dir.path()));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("page.md?raw=1"), dir.path()));
}

TEST(LinkValidator, TildeExpansion)
{
    QTemporaryFile probe(QDir::homePath() + QStringLiteral("/scriba-tilde-probe-XXXXXX"));
    ASSERT_TRUE(probe.open());
    const QString name = QFileInfo(probe.fileName()).fileName();
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("~/") + name));
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(QStringLiteral("~/scriba-tilde-missing-probe.md")));
    probe.remove();
}

TEST(LinkValidator, WellFormedHttpUrls)
{
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("https://example.com")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("http://example.com/page?a=1#frag")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("http://localhost:8080")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("http://192.168.1.1/admin")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("ftp://example.com/file.txt")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("http://[::1]:8000/")));
}

TEST(LinkValidator, MalformedHttpUrls)
{
    EXPECT_EQ(Status::MalformedUrl, LinkValidator::validateTarget(QStringLiteral("http://")));
    EXPECT_EQ(Status::MalformedUrl, LinkValidator::validateTarget(QStringLiteral("https://")));
    EXPECT_EQ(Status::MalformedUrl, LinkValidator::validateTarget(QStringLiteral("http:/missing-slashes")));
    EXPECT_EQ(Status::MalformedUrl, LinkValidator::validateTarget(QStringLiteral("http://no-dot-host")));
    EXPECT_EQ(Status::MalformedUrl, LinkValidator::validateTarget(QStringLiteral("http://exa mple.com")));
}

TEST(LinkValidator, WwwTargetsAreTreatedAsUrls)
{
    // www.-prefixed targets must be URL-validated, not treated as relative
    // files (CommonMark would, but users mean a website).
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("www.example.com")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("www.nohostdot")));
    EXPECT_NE(Status::FileNotFound, LinkValidator::validateTarget(QStringLiteral("www.anything")));
}

TEST(LinkValidator, OutOfScopeTargetsAreValid)
{
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("#section")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("mailto:user@example.com")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("tel:+15551234567")));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("data:image/png;base64,AAAA")));
}

TEST(LinkValidator, AngleBracketTargetsAreUnwrapped)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile f(dir.filePath(QStringLiteral("angle.md")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("<angle.md>"), dir.path()));
    EXPECT_EQ(Status::FileNotFound,
              LinkValidator::validateTarget(QStringLiteral("<missing.md>"), dir.path()));
    EXPECT_EQ(Status::Valid, LinkValidator::validateTarget(QStringLiteral("<https://example.com>")));
}

TEST(LinkValidator, FileTargetPathStripsFragmentsAndAngles)
{
    EXPECT_EQ(QStringLiteral("a/b.md"), LinkValidator::fileTargetPath(QStringLiteral("a/b.md#sec")));
    EXPECT_EQ(QStringLiteral("c.png"), LinkValidator::fileTargetPath(QStringLiteral("c.png?w=100")));
    EXPECT_EQ(QStringLiteral("x.md"), LinkValidator::fileTargetPath(QStringLiteral("<x.md>")));
    EXPECT_EQ(QStringLiteral("y.md"), LinkValidator::fileTargetPath(QStringLiteral(" y.md ")));
}

} // namespace

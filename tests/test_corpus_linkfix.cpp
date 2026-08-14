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
#include <QTemporaryDir>

#include "corpus/LinkFixer.h"

namespace {

// Pure string transforms: the files never need to exist on disk, but the
// paths must stay platform-neutral (QDir::tempPath(), not a hardcoded /tmp).
class LinkFixerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_dir.reset(new QTemporaryDir);
        ASSERT_TRUE(m_dir->isValid());
        m_base = m_dir->path();
        m_oldAbs = m_base + "/old.md";
        m_newAbs = m_base + "/renamed.md";
    }

    QString m_base;
    QString m_oldAbs;
    QString m_newAbs;
    std::unique_ptr<QTemporaryDir> m_dir;
};

TEST_F(LinkFixerTest, InlineLinkRewritten)
{
    const QString src = QStringLiteral("See [notes](old.md) here.");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("See [notes](renamed.md) here."));
}

TEST_F(LinkFixerTest, ImageLinkRewritten)
{
    const QString src = QStringLiteral("![diagram](old.md)");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("![diagram](renamed.md)"));
}

TEST_F(LinkFixerTest, ReferenceDefinitionRewritten)
{
    const QString src = QStringLiteral("See [label].\n[label]: old.md\n");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("See [label].\n[label]: renamed.md\n"));
}

TEST_F(LinkFixerTest, ReferenceDefinitionAnchorOnlyKept)
{
    const QString src = QStringLiteral("[label]: #anchor\n");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, src);
}

TEST_F(LinkFixerTest, RemoteAndDataSchemesNeverRewritten)
{
    const QString src = QStringLiteral(
        "[web](https://example.com/old.md) [img](data:image/png;base64,AAAA)\n");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, src);
}

TEST_F(LinkFixerTest, CanonicalResolutionThroughDocDir)
{
    // Editing doc lives in a subdir; a ../old.md reference resolves up to the
    // renamed file and is spelled relative to the subdir after the rewrite.
    const QString sub = m_base + "/sub";
    const QString out = LinkFixer::rewrite(QStringLiteral("[x](../old.md)"),
                                           sub, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("[x](../renamed.md)"));
}

TEST_F(LinkFixerTest, TwoReferencesRewrittenBackwardSafely)
{
    const QString src = QStringLiteral("[a](old.md) and [b](old.md)");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("[a](renamed.md) and [b](renamed.md)"));
}

TEST_F(LinkFixerTest, NoMatchIsNoOp)
{
    const QString src = QStringLiteral("[a](other.md) and [b](unrelated.md)");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, src);
}

TEST_F(LinkFixerTest, UnrelatedNonexistentTargetNotRewritten)
{
    // Regression: the renamed file no longer exists on disk; a *different*
    // nonexistent target must not spuriously match via two empty canonical
    // paths.
    const QString src = QStringLiteral("[a](missing.md) and [b](old.md)");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("[a](missing.md) and [b](renamed.md)"));
}

TEST_F(LinkFixerTest, AngleBracketAutolinkRewrittenBracketed)
{
    const QString src = QStringLiteral("See <old.md>.");
    const QString out = LinkFixer::rewrite(src, m_base, m_oldAbs, m_newAbs);
    EXPECT_EQ(out, QStringLiteral("See <renamed.md>."));
}

TEST_F(LinkFixerTest, LinkTargetsCollectsRawDestinations)
{
    const QString src = QStringLiteral(
        "See [a](target.md) and ![i](image.png).\n"
        "[ref]: def.md\n"
        "<autolink.md> also <https://x.md>\n");
    EXPECT_EQ(LinkFixer::linkTargets(src),
              QStringList({"target.md", "image.png", "def.md", "autolink.md", "https://x.md"}));
}

TEST_F(LinkFixerTest, ResolvedLinkTargetsCollectsAbsolutePaths)
{
    const QString src = QStringLiteral(
        "See [a](target.md) and ![i](image.png).\n"
        "[ref]: def.md\n"
        "<autolink.md> also <https://x.md>\n");
    EXPECT_EQ(LinkFixer::resolvedLinkTargets(src, m_base),
              QStringList({m_base + "/target.md", m_base + "/image.png",
                           m_base + "/def.md", m_base + "/autolink.md"}));
}

TEST_F(LinkFixerTest, ResolvedLinkTargetsSkipsFragmentsSchemesAndAnchors)
{
    const QString src = QStringLiteral(
        "Same [file](old.md#heading) anchor and [jump](#local).\n"
        "[web](https://example.com/old.md) [data](data:text/plain,hi)\n");
    const QStringList out = LinkFixer::resolvedLinkTargets(src, m_base);
    EXPECT_EQ(out, QStringList({m_base + "/old.md"}));
}

TEST_F(LinkFixerTest, ResolvedLinkTargetsNormalizesDotDot)
{
    const QString src = QStringLiteral("[up](../shared.md)");
    EXPECT_EQ(LinkFixer::resolvedLinkTargets(src, m_base + "/sub"),
              QStringList({m_base + "/shared.md"}));
}

} // namespace

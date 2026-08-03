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
#include <QSignalSpy>
#include <QTextEdit>
#include <QTemporaryDir>

#include "SpellHighlighter.h"
#include "TestConfig.h"

namespace {

// Drives a real QTextEdit + SpellHighlighter with only broken-link checking
// enabled (no spell checker installed, so the link scan is the only active
// pass). setPlainText() on a QTextEdit emits contentsChange with a full
// replace, which runs the check synchronously — no timers to wait for.
class BrokenLinksTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        QFile exists(m_dir.filePath(QStringLiteral("exists.md")));
        ASSERT_TRUE(exists.open(QIODevice::WriteOnly));
        QDir(m_dir.path()).mkpath(QStringLiteral("sub"));
        QFile other(m_dir.filePath(QStringLiteral("sub/other.md")));
        ASSERT_TRUE(other.open(QIODevice::WriteOnly));

        m_editor = new QTextEdit;
        m_hl = new SpellHighlighter(m_editor->document(), m_editor);
        m_hl->setCurrentFile(m_dir.filePath(QStringLiteral("doc.md")));
    }

    void TearDown() override
    {
        delete m_editor;
    }

    QVector<SpellHighlighter::GrammarHit> hitsInBlock(int blockNumber) const
    {
        return m_hl->linkIssuesInBlock(blockNumber);
    }

    QVector<SpellHighlighter::GrammarHit> hitsInLine0() const
    {
        return hitsInBlock(0);
    }

    QTemporaryDir m_dir;
    QTextEdit *m_editor = nullptr;
    SpellHighlighter *m_hl = nullptr;
};

TEST_F(BrokenLinksTest, MissingFileTargetIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("[text](missing.md)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(7, hits[0].start);
    EXPECT_EQ(10, hits[0].length);
    EXPECT_EQ(QStringLiteral("File not found: missing.md"), hits[0].message);
}

TEST_F(BrokenLinksTest, ExistingFileTargetIsNotFlagged)
{
    m_editor->setPlainText(QStringLiteral("[text](exists.md) and [text](sub/other.md)"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, ImageLinkTargetIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("![alt](missing.png)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(7, hits[0].start);
    EXPECT_EQ(11, hits[0].length);
}

TEST_F(BrokenLinksTest, MalformedUrlTargetIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("[text](http://)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(QStringLiteral("Malformed URL: http://"), hits[0].message);
}

TEST_F(BrokenLinksTest, WellFormedUrlTargetIsNotFlagged)
{
    m_editor->setPlainText(QStringLiteral(
        "[text](https://example.com) [text](http://localhost:8080) [text](www.example.com)"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, RawMalformedUrlInTextIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("see http://nodothere for details"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(4, hits[0].start);
    EXPECT_EQ(QStringLiteral("Malformed URL: http://nodothere"), hits[0].message);
}

TEST_F(BrokenLinksTest, RawWellFormedUrlInTextIsNotFlagged)
{
    m_editor->setPlainText(QStringLiteral("see https://example.com and www.example.org"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, UrlInsideLinkTargetIsNotDoubleFlagged)
{
    m_editor->setPlainText(QStringLiteral("[text](http://nodothere)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(7, hits[0].start);
}

TEST_F(BrokenLinksTest, ReferenceUsageWithoutDefinitionIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("see [docs][guide] here"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(11, hits[0].start); // the `guide` inside ][guide]
    EXPECT_EQ(5, hits[0].length);
    EXPECT_EQ(QStringLiteral("Missing reference definition: [guide]"), hits[0].message);
}

TEST_F(BrokenLinksTest, ReferenceUsageWithDefinitionIsNotFlagged)
{
    m_editor->setPlainText(
        QStringLiteral("see [docs][guide] here\n[guide]: exists.md"));
    EXPECT_TRUE(hitsInBlock(0).isEmpty());
    EXPECT_TRUE(hitsInBlock(1).isEmpty());
}

TEST_F(BrokenLinksTest, ReferenceDefinitionWithMissingFileIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("[guide]: missing.md"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(9, hits[0].start);
    EXPECT_EQ(10, hits[0].length);
    EXPECT_EQ(QStringLiteral("File not found: missing.md"), hits[0].message);
}

TEST_F(BrokenLinksTest, AddingDefinitionClearsUsageFlag)
{
    m_editor->setPlainText(QStringLiteral("see [docs][guide]"));
    ASSERT_FALSE(hitsInLine0().isEmpty());

    m_editor->setPlainText(QStringLiteral("see [docs][guide]\n[guide]: exists.md"));
    EXPECT_TRUE(hitsInBlock(0).isEmpty());
}

TEST_F(BrokenLinksTest, RemovingDefinitionFlagsUsageAgain)
{
    m_editor->setPlainText(QStringLiteral("see [docs][guide]\n[guide]: exists.md"));
    ASSERT_TRUE(hitsInBlock(0).isEmpty());

    m_editor->setPlainText(QStringLiteral("see [docs][guide]"));
    ASSERT_FALSE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, IncompleteTargetIsNotFlaggedWhileTyping)
{
    m_editor->setPlainText(QStringLiteral("[text](miss"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, FencedCodeIsNotFlagged)
{
    m_editor->setPlainText(QStringLiteral("```\n[text](missing.md)\n```"));
    EXPECT_TRUE(hitsInBlock(0).isEmpty());
    EXPECT_TRUE(hitsInBlock(1).isEmpty());
    EXPECT_TRUE(hitsInBlock(2).isEmpty());
}

TEST_F(BrokenLinksTest, InlineCodeIsNotFlagged)
{
    m_editor->setPlainText(QStringLiteral("`[text](missing.md)`"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, OutOfScopeTargetsAreNotFlagged)
{
    m_editor->setPlainText(QStringLiteral("[text]() [text](#) [text](mailto:a@b.c)"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, ChangingCurrentFileRechecksRelativeTargets)
{
    // other.md exists only inside the doc dir's `sub/` — from the doc dir
    // root the target is broken.
    m_editor->setPlainText(QStringLiteral("[text](other.md)"));
    ASSERT_FALSE(hitsInLine0().isEmpty());

    QTemporaryDir otherDir;
    ASSERT_TRUE(otherDir.isValid());
    QFile other(otherDir.filePath(QStringLiteral("other.md")));
    ASSERT_TRUE(other.open(QIODevice::WriteOnly));
    m_hl->setCurrentFile(otherDir.filePath(QStringLiteral("doc.md")));
    EXPECT_TRUE(hitsInLine0().isEmpty());
}

TEST_F(BrokenLinksTest, DisablingClearsHitsAndReenablingRestoresThem)
{
    m_editor->setPlainText(QStringLiteral("[text](missing.md)"));
    ASSERT_EQ(1, hitsInLine0().size());

    m_hl->setLinkCheckingEnabled(false);
    EXPECT_TRUE(hitsInLine0().isEmpty());

    m_hl->setLinkCheckingEnabled(true);
    EXPECT_EQ(1, hitsInLine0().size());
}

TEST_F(BrokenLinksTest, SpellingAndLinkCachesAreIndependent)
{
    m_editor->setPlainText(QStringLiteral("[text](missing.md)"));
    EXPECT_EQ(1, hitsInLine0().size());
    EXPECT_TRUE(m_hl->spellHitsInBlock(0).isEmpty())
        << "no spell checker installed: spell cache stays empty";
}

TEST_F(BrokenLinksTest, CheckEmitsSpellHitsChangedForOverlayRepaint)
{
    QSignalSpy spy(m_hl, &SpellHighlighter::spellHitsChanged);
    m_editor->setPlainText(QStringLiteral("[text](missing.md)"));
    EXPECT_GT(spy.count(), 0);
}

TEST_F(BrokenLinksTest, SameDocumentFragmentWithMatchingHeadingIsNotFlagged)
{
    m_editor->setPlainText(QStringLiteral("# Intro\n[text](#intro) and [text](#INtro)"));
    EXPECT_TRUE(hitsInBlock(0).isEmpty());
    EXPECT_TRUE(hitsInBlock(1).isEmpty());
}

TEST_F(BrokenLinksTest, SameDocumentFragmentWithMissingHeadingIsFlagged)
{
    m_editor->setPlainText(QStringLiteral("# Intro\n[text](#missing)"));
    const auto hits = hitsInBlock(1);
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(7, hits[0].start); // the `#missing` inside ](#missing)
    EXPECT_EQ(8, hits[0].length);
    EXPECT_EQ(QStringLiteral("Heading not found: #missing"), hits[0].message);
}

TEST_F(BrokenLinksTest, HeadingSlugsIgnoreFencesAndCountDuplicates)
{
    m_editor->setPlainText(
        QStringLiteral("```\n# Fenced\n```\n# Title\n# Title\n[text](#title) [text](#title-1)\n[text](#title-2) [text](#fenced)"));
    EXPECT_TRUE(hitsInBlock(3).isEmpty());
    EXPECT_TRUE(hitsInBlock(4).isEmpty());
    EXPECT_TRUE(hitsInBlock(5).isEmpty());
    const auto hits = hitsInBlock(6);
    ASSERT_EQ(2, hits.size());
    EXPECT_EQ(QStringLiteral("Heading not found: #title-2"), hits[0].message);
    EXPECT_EQ(QStringLiteral("Heading not found: #fenced"), hits[1].message);
}

TEST_F(BrokenLinksTest, SetextHeadingProduceSlugs)
{
    m_editor->setPlainText(QStringLiteral("Chapter Seven\n=====\nFollow the [text](#chapter-seven)"));
    EXPECT_TRUE(hitsInBlock(0).isEmpty());
    EXPECT_TRUE(hitsInBlock(1).isEmpty());
    EXPECT_TRUE(hitsInBlock(2).isEmpty());
}

TEST_F(BrokenLinksTest, CrossDocFragmentAgainstExistingFile)
{
    QFile f(m_dir.filePath(QStringLiteral("exists.md")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("## Sub\n\nSome content.\n");
    f.close();

    m_editor->setPlainText(QStringLiteral("[text](exists.md#sub) and [text](exists.md#nope)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(QStringLiteral("Heading not found: #nope"), hits[0].message);
    // The hit covers only the fragment, not the file part.
    EXPECT_EQ(42, hits[0].start);
    EXPECT_EQ(5, hits[0].length);
}

TEST_F(BrokenLinksTest, CrossDocAnchorIsFileCheckedFirst)
{
    m_editor->setPlainText(QStringLiteral("[text](missing.md#frag)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(QStringLiteral("File not found: missing.md"), hits[0].message);
    EXPECT_EQ(7, hits[0].start);
    EXPECT_EQ(15, hits[0].length);
}

TEST_F(BrokenLinksTest, CrossDocAnchorCacheInvalidatesOnFileChange)
{
    QFile f(m_dir.filePath(QStringLiteral("exists.md")));
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("# Title\n");
    f.close();

    m_editor->setPlainText(QStringLiteral("[text](exists.md#title)"));
    EXPECT_TRUE(hitsInLine0().isEmpty()) << "fresh doc with the heading";

    // Overwrite the target without a heading: the cache must notice the
    // change (size + mtime) and re-scan, flagging the now-dangling anchor.
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("No headings here.\n");
    f.close();

    m_editor->setPlainText(QStringLiteral("[text](exists.md#title)"));
    const auto hits = hitsInLine0();
    ASSERT_EQ(1, hits.size());
    EXPECT_EQ(QStringLiteral("Heading not found: #title"), hits[0].message);
}

TEST_F(BrokenLinksTest, AnchorInReferenceDefinitionTarget)
{
    m_editor->setPlainText(QStringLiteral("[guide]: exists.md#sub\n[text][guide]"));
    EXPECT_TRUE(hitsInLine0().isEmpty());
    EXPECT_TRUE(hitsInBlock(1).isEmpty());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

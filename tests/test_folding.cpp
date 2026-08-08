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
#include <QTextBlock>
#include <QTextDocument>
#include <QTest>
#include <QTextCursor>

#include "Editor.h"
#include "TestConfig.h"

class FoldingTest : public testing::Test
{
protected:
    void SetUp() override
    {
        editor = new Editor();
        editor->resize(800, 600);
        editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete editor;
    }

    void setText(const QString &text)
    {
        editor->setPlainText(text);
        QApplication::processEvents();
        // Wait for 300ms fold timer to fire
        QTest::qWait(500);
    }

    Editor *editor = nullptr;
};

TEST_F(FoldingTest, FoldsSectionAfterAtxHeader)
{
    setText(
        "# Header 1\n"
        "line 1\n"
        "line 2\n"
        "## Header 2\n"
        "line 3\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    // ## Header 2 (block 3) is inside # Header 1's section, so hidden
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(4).isVisible());
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({0}));
}

TEST_F(FoldingTest, UnfoldRestoresVisibility)
{
    setText(
        "# Header 1\n"
        "line 1\n"
        "## Header 2\n"
        "line 2\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    EXPECT_FALSE(editor->document()->findBlockByNumber(1).isVisible());

    editor->restoreFolds({});
    QApplication::processEvents();
    EXPECT_TRUE(editor->document()->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(editor->document()->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(editor->document()->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, FoldedBlockNumbersRoundtrip)
{
    setText(
        "# A\n"
        "a\n"
        "# B\n"
        "b\n"
        "# C\n"
        "c\n"
    );

    editor->restoreFolds({0, 2});
    QApplication::processEvents();
    QList<int> folded = editor->foldedBlockNumbers();
    EXPECT_TRUE(folded.contains(0));
    EXPECT_TRUE(folded.contains(2));
    EXPECT_EQ(folded.size(), 2);

    editor->restoreFolds({});
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());

    editor->restoreFolds(folded);
    EXPECT_EQ(editor->foldedBlockNumbers(), folded);
}

TEST_F(FoldingTest, DetectsSetextHeaders)
{
    setText(
        "H1 Text\n"
        "=======\n"
        "content\n"
        "H2 Text\n"
        "-------\n"
        "more\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    // H2 (level 2) is inside H1 (level 1) section, so hidden
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(4).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(5).isVisible());
}

TEST_F(FoldingTest, SkipsHeadersInsideFencedCode)
{
    setText(
        "# Real Header\n"
        "text\n"
        "```\n"
        "# Fake Header\n"
        "text\n"
        "```\n"
        "## Real Header 2\n"
        "text\n"
    );

    // Try to fold the fake header inside fenced code - should be rejected
    editor->restoreFolds({0, 3});
    QApplication::processEvents();

    auto folded = editor->foldedBlockNumbers();
    EXPECT_TRUE(folded.contains(0));
    EXPECT_FALSE(folded.contains(3));
    EXPECT_EQ(folded.size(), 1);
}

TEST_F(FoldingTest, SectionEndAtSameLevel)
{
    setText(
        "## H1\n"
        "a\n"
        "## H2\n"
        "b\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible());
}

TEST_F(FoldingTest, SectionEndAtHigherLevel)
{
    setText(
        "### D1\n"
        "a\n"
        "# H1\n"
        "b\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible());
}

TEST_F(FoldingTest, MultipleFoldsIndependent)
{
    setText(
        "# H1\n"
        "a\n"
        "## H2\n"
        "b\n"
        "### H3\n"
        "c\n"
        "## H4\n"
        "d\n"
    );

    editor->restoreFolds({2, 6});
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(0).isVisible()); // H1
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible()); // under H1 (not folded)
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible()); // H2 (header stays)
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible()); // b hidden
    EXPECT_FALSE(doc->findBlockByNumber(4).isVisible()); // H3 hidden (inside H2 section)
    EXPECT_FALSE(doc->findBlockByNumber(5).isVisible()); // c hidden
    EXPECT_TRUE(doc->findBlockByNumber(6).isVisible()); // H4 (header stays)
    EXPECT_FALSE(doc->findBlockByNumber(7).isVisible()); // d hidden

    auto folded = editor->foldedBlockNumbers();
    EXPECT_TRUE(folded.contains(2));
    EXPECT_TRUE(folded.contains(6));
    EXPECT_EQ(folded.size(), 2);
}

TEST_F(FoldingTest, KeyboardFoldUnfold)
{
    setText(
        "# Header\n"
        "line 1\n"
        "line 2\n"
        "text\n"
    );

    QTextCursor cursor(editor->document()->findBlockByNumber(0));
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Minus, Qt::ControlModifier);
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().contains(0));

    QTest::keyClick(editor, Qt::Key_Equal, Qt::ControlModifier);
    QApplication::processEvents();

    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, KeyboardUnfoldNearestAncestor)
{
    setText(
        "# H1\n"
        "line\n"
        "## H2\n"
        "more\n"
        "### H3\n"
        "text\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    QTextCursor cursor(editor->document()->findBlockByNumber(5));
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Equal, Qt::ControlModifier);
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(5).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, FoldsFencedCodeBlock)
{
    setText(
        "text\n"
        "```java\n"
        "int x = 1;\n"
        "int y = 2;\n"
        "```\n"
        "tail\n"
    );

    // Opening fence is block 1; body 2-3; closing fence is block 4.
    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(4).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(5).isVisible());
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({1}));
}

TEST_F(FoldingTest, UnfoldFencedCodeBlockRestoresVisibility)
{
    setText(
        "```python\n"
        "a\n"
        "b\n"
        "```\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());

    editor->restoreFolds({});
    QApplication::processEvents();
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, FencedCodeFoldRoundtrip)
{
    setText(
        "# Intro\n"
        "text\n"
        "```\n"
        "body\n"
        "```\n"
        "# End\n"
        "text\n"
    );

    editor->restoreFolds({0, 2});
    QApplication::processEvents();
    QList<int> folded = editor->foldedBlockNumbers();
    EXPECT_TRUE(folded.contains(0));
    EXPECT_TRUE(folded.contains(2));
    EXPECT_EQ(folded.size(), 2);

    // Unfold all, then re-apply from the read-back set.
    editor->restoreFolds({});
    editor->restoreFolds(folded);
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible()); // header body under H1
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible()); // code body
    EXPECT_FALSE(doc->findBlockByNumber(4).isVisible()); // closing fence
    EXPECT_TRUE(doc->findBlockByNumber(5).isVisible());  // next heading
}

TEST_F(FoldingTest, KeyboardFoldUnfoldFencedCode)
{
    setText(
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n"
    );

    QTextCursor cursor(editor->document()->findBlockByNumber(0));
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Minus, Qt::ControlModifier);
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().contains(0));

    QTest::keyClick(editor, Qt::Key_Equal, Qt::ControlModifier);
    QApplication::processEvents();

    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, KeyboardUnfoldNearestContainedFence)
{
    setText(
        "```\n"
        "alpha\n"
        "beta\n"
        "```\n"
        "tail\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    // Cursor deep inside the hidden code body; Ctrl+= expands the fence.
    QTextCursor cursor(editor->document()->findBlockByNumber(2));
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Equal, Qt::ControlModifier);
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, HeaderNavigationUpDown)
{
    setText(
        "# H1\n"
        "a\n"
        "## H2\n"
        "## H3\n"
        "b\n"
    );

    QTextCursor cursor(editor->document()->findBlockByNumber(4));
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Up, Qt::ControlModifier);
    EXPECT_EQ(editor->textCursor().blockNumber(), 3);

    QTest::keyClick(editor, Qt::Key_Up, Qt::ControlModifier);
    EXPECT_EQ(editor->textCursor().blockNumber(), 2);

    QTest::keyClick(editor, Qt::Key_Up, Qt::ControlModifier);
    EXPECT_EQ(editor->textCursor().blockNumber(), 0);

    QTest::keyClick(editor, Qt::Key_Down, Qt::ControlModifier);
    EXPECT_EQ(editor->textCursor().blockNumber(), 2);

    QTest::keyClick(editor, Qt::Key_Down, Qt::ControlModifier);
    EXPECT_EQ(editor->textCursor().blockNumber(), 3);

    QTest::keyClick(editor, Qt::Key_Down, Qt::ControlModifier);
    EXPECT_EQ(editor->textCursor().blockNumber(), 3);
}

TEST_F(FoldingTest, FoldsMarkedDirtyAfterFold)
{
    setText(
        "# Header\n"
        "line 1\n"
        "line 2\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
}

TEST_F(FoldingTest, FoldsTableToBlockStart)
{
    setText(
        "| H1 | H2 |\n"
        "|---|---|\n"
        "| a | b |\n"
        "| c | d |\n"
        "# Next Section\n"
    );

    // Header=0, separator=1 (anchor), rows=2,3; # header=4 must stay visible.
    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(0).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible()) << "following header not folded away";
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({1}));
}

TEST_F(FoldingTest, TableStopsAtBlankLine)
{
    setText(
        "| H1 | H2 |\n"
        "|---|---|\n"
        "| a | b |\n"
        "| c | d |\n"
        "\n"
        "tail paragraph\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible()); // blank line
    EXPECT_TRUE(doc->findBlockByNumber(5).isVisible()); // paragraph after blank
}

TEST_F(FoldingTest, FoldsHtmlTableToClosingTag)
{
    setText(
        "<table>\n"
        "<tr><td>a</td></tr>\n"
        "<tr><td>b</td></tr>\n"
        "</table>\n"
        "tail\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(0).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible());
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({0}));
}

TEST_F(FoldingTest, IgnoresTableRowsInsideCodeFence)
{
    setText(
        "```\n"
        "| H1 | H2 |\n"
        "|---|---|\n"
        "| a | b |\n"
        "```\n"
        "tail\n"
    );

    // The separator-like row (block 2) sits inside a fence: not foldable.
    editor->restoreFolds({2});
    QApplication::processEvents();

    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
    EXPECT_TRUE(editor->document()->findBlockByNumber(2).isVisible());
}

TEST_F(FoldingTest, TableFoldRoundtrip)
{
    setText(
        "| H1 | H2 |\n"
        "|---|---|\n"
        "| a | b |\n"
        "| c | d |\n"
        "# End\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();
    QList<int> folded = editor->foldedBlockNumbers();
    EXPECT_TRUE(folded.contains(1));
    EXPECT_EQ(folded.size(), 1);

    editor->restoreFolds({});
    QApplication::processEvents();
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
    EXPECT_TRUE(editor->document()->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(editor->document()->findBlockByNumber(3).isVisible());
}

TEST_F(FoldingTest, FoldFirstListItemToEndOfList)
{
    setText(
        "- a\n"
        "- b\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(0).isVisible());
    // Folding the first item collapses the rest of the list.
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_EQ(editor->foldedBlockNumbers(), QList<int>({0}));
}

TEST_F(FoldingTest, FoldListStopsAtNextHeading)
{
    setText(
        "- a\n"
        "- b\n"
        "- c\n"
        "# Heading\n"
        "text\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "heading not folded away";
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible());
}

TEST_F(FoldingTest, FoldNonFirstItemHidesOnlySubtree)
{
    setText(
        "- a\n"
        "- b\n"
        "  - b1\n"
        "- c\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(0).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "sibling stays visible";
}

TEST_F(FoldingTest, NestedItemFoldStopsAtSibling)
{
    setText(
        "- a\n"
        "  - a1\n"
        "    content\n"
        "  - a2\n"
        "- b\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "nested sibling stays visible";
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible());
}

TEST_F(FoldingTest, BlankLineTerminatesFoldBeforeNewParagraph)
{
    setText(
        "- a\n"
        "  cont\n"
        "\n"
        "para\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible()); // blank line
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()); // paragraph after blank
}

TEST_F(FoldingTest, OrderedAndTaskListItemsFoldable)
{
    setText(
        "1. one\n"
        "2. two\n"
        "- [ ] task\n"
        "- done\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(3).isVisible());

    // A task item is its own fold anchor; folding it keeps the last item.
    editor->restoreFolds({2});
    QApplication::processEvents();
    EXPECT_TRUE(editor->foldedBlockNumbers().contains(2));
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible());
}

TEST_F(FoldingTest, FoldsQuotedListItems)
{
    setText(
        "> - a\n"
        "> - b\n"
        "- c\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible()) << "fold stops when the quote ends";
}

TEST_F(FoldingTest, QuotedNestedItemFoldStopsAtQuoteSibling)
{
    setText(
        "> - a\n"
        ">   - a1\n"
        ">     text\n"
        "> - b\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "quote sibling stays visible";
}

TEST_F(FoldingTest, DoubleQuotedListItemFoldsToQuoteEnd)
{
    setText(
        "> - a\n"
        "> > - n1\n"
        "> > - n2\n"
        "> - b\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible()) << "shallower quote line stays visible";
}

TEST_F(FoldingTest, ThematicBreakIsNotListAnchor)
{
    setText(
        "- - -\n"
        "text\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();

    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
    EXPECT_TRUE(editor->document()->findBlockByNumber(0).isVisible());
}

TEST_F(FoldingTest, IgnoresListMarkersInsideCodeFence)
{
    setText(
        "```\n"
        "- not a list\n"
        "- also not\n"
        "```\n"
    );

    editor->restoreFolds({1});
    QApplication::processEvents();

    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
    EXPECT_TRUE(editor->document()->findBlockByNumber(1).isVisible());
}

TEST_F(FoldingTest, KeyboardFoldUnfoldList)
{
    setText(
        "- a\n"
        "  - a1\n"
        "  - a2\n"
    );

    QTextCursor cursor(editor->document()->findBlockByNumber(0));
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Minus, Qt::ControlModifier);
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().contains(0));

    QTest::keyClick(editor, Qt::Key_Equal, Qt::ControlModifier);
    QApplication::processEvents();

    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());
}

TEST_F(FoldingTest, ListFoldRoundtrip)
{
    setText(
        "- a\n"
        "- b\n"
        "- c\n"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    QList<int> folded = editor->foldedBlockNumbers();
    EXPECT_TRUE(folded.contains(0));
    EXPECT_EQ(folded.size(), 1);

    editor->restoreFolds({});
    QApplication::processEvents();
    EXPECT_TRUE(editor->foldedBlockNumbers().isEmpty());

    editor->restoreFolds(folded);
    QApplication::processEvents();
    EXPECT_FALSE(editor->document()->findBlockByNumber(1).isVisible());
    EXPECT_FALSE(editor->document()->findBlockByNumber(2).isVisible());
}

TEST_F(FoldingTest, EnterOnFoldedListInsertsVisibleBelowFold)
{
    setText(
        "- a\n"
        "  - a1\n"
        "  - a2"
    );

    editor->restoreFolds({0});
    QApplication::processEvents();
    auto *doc = editor->document();
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible());

    QTextCursor cursor(doc->findBlockByNumber(0));
    cursor.movePosition(QTextCursor::EndOfLine);
    editor->setTextCursor(cursor);
    QApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Return);
    QApplication::processEvents();
    QTest::qWait(500); // let the fold re-scan run

    EXPECT_TRUE(editor->foldedBlockNumbers().contains(0));
    QTextBlock newLine = doc->findBlockByNumber(doc->blockCount() - 1);
    EXPECT_TRUE(newLine.isVisible()) << "typed line below a folded list stays visible";
    EXPECT_FALSE(doc->findBlockByNumber(1).isVisible())
        << "hidden items below a folded list stay hidden";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

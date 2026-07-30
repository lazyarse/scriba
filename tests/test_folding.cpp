#include <gtest/gtest.h>
#include <QApplication>
#include <QTextBlock>
#include <QTextDocument>
#include <QTest>
#include <QTextCursor>

#include "Editor.h"

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

    QTest::keyClick(editor, Qt::Key_Equal, Qt::ControlModifier);
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

    QTest::keyClick(editor, Qt::Key_Minus, Qt::ControlModifier);
    QApplication::processEvents();

    auto *doc = editor->document();
    EXPECT_TRUE(doc->findBlockByNumber(1).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(2).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(3).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(4).isVisible());
    EXPECT_TRUE(doc->findBlockByNumber(5).isVisible());
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

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

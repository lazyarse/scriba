#include <gtest/gtest.h>
#include <QApplication>
#include <QCompleter>
#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "Editor.h"

class EditorCompletionUITest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tmpDir.isValid());

        currentFilePath = tmpDir.path() + "/doc.md";
        {
            QFile f(currentFilePath);
            (void)f.open(QIODevice::WriteOnly);
            f.write("# Test\n");
        }

        auto touch = [&](const QString &name) {
            QFile f(tmpDir.path() + "/" + name);
            (void)f.open(QIODevice::WriteOnly);
            f.close();
        };
        QDir d(tmpDir.path());
        d.mkdir("resources");
        d.mkdir("resources/icons");
        touch("main.css");
        touch("README.md");
        touch("resources/icons/logo.svg");
        touch("resources/icons/favicon.ico");

        editor = new Editor();
        editor->resize(800, 600);
        editor->setCurrentFile(currentFilePath);
        editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete editor;
    }

    void typeText(const QString &text)
    {
        QTest::keyClicks(editor, text);
        QApplication::processEvents();
    }

    void pressTab()
    {
        QTest::keyClick(editor, Qt::Key_Tab);
        QApplication::processEvents();
    }

    void pressEnter()
    {
        QTest::keyClick(editor, Qt::Key_Return);
        QApplication::processEvents();
    }

    void pressEscape()
    {
        QTest::keyClick(editor, Qt::Key_Escape);
        QApplication::processEvents();
    }

    void pressBackspace()
    {
        QTest::keyClick(editor, Qt::Key_Backspace);
        QApplication::processEvents();
    }

    int popupRowCount() const
    {
        if (!editor->completer() || !editor->completer()->popup()->isVisible())
            return -1;
        return editor->completer()->completionModel()->rowCount();
    }

    QTemporaryDir tmpDir;
    QString currentFilePath;
    Editor *editor = nullptr;
};

TEST_F(EditorCompletionUITest, EmptyLinkDoesNotCrash)
{
    typeText("![](");
    EXPECT_EQ(editor->toPlainText(), "![](");

    pressTab();
    EXPECT_EQ(editor->toPlainText(), "![](");
}

TEST_F(EditorCompletionUITest, SingleMatchCompletes)
{
    typeText("![](resou");
    pressTab();
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "![](resources/");
}

TEST_F(EditorCompletionUITest, ChainedCompletion)
{
    typeText("![](resou");
    pressTab();
    pressEnter();
    ASSERT_EQ(editor->toPlainText(), "![](resources/");

    typeText("ico");
    pressTab();
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "![](resources/icons/");
}

TEST_F(EditorCompletionUITest, ListLinkDoesNotIndent)
{
    typeText("- [link](resou");
    pressTab();
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "- [link](resources/");
}

TEST_F(EditorCompletionUITest, ListWithoutLinkIndents)
{
    typeText("- item");
    pressTab();
    EXPECT_EQ(editor->toPlainText(), "  - item");
}

TEST_F(EditorCompletionUITest, PopupCyclesAndAccepts)
{
    typeText("![](r");

    // Cycle to second item (resources/), then accept
    pressTab();
    pressEnter();

    EXPECT_EQ(editor->toPlainText(), "![](resources/");
}

TEST_F(EditorCompletionUITest, EmojiSingleMatchShowsPopupAndEnterAccepts)
{
    typeText(":asto");
    EXPECT_EQ(editor->toPlainText(), ":asto");
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), ":astonished:");
}

TEST_F(EditorCompletionUITest, EmojiMultiMatchThenEscape)
{
    typeText(":smil");
    pressEscape();
    EXPECT_EQ(editor->toPlainText(), ":smil");
}

TEST_F(EditorCompletionUITest, EmojiEnterAcceptsTopItem)
{
    typeText(":s");
    pressEnter();
    QString result = editor->toPlainText();
    EXPECT_TRUE(result.startsWith(':'));
    EXPECT_TRUE(result.endsWith(':'));
    EXPECT_GT(result.length(), 3);
}

TEST_F(EditorCompletionUITest, FileCompletionLimitsResults)
{
    // Create 25 files matching prefix "zzfile"
    for (int i = 0; i < 25; ++i) {
        QFile f(tmpDir.path() + QString("/zzfile%1.md").arg(i, 2, 10, QChar('0')));
        (void)f.open(QIODevice::WriteOnly);
        f.close();
    }

    typeText("![](zzfile");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible());
    EXPECT_EQ(popupRowCount(), 20);
}

TEST_F(EditorCompletionUITest, EmojiBackspaceUpdatesPopup)
{
    typeText(":smi");
    QApplication::processEvents();
    int before = popupRowCount();
    ASSERT_GT(before, 0);

    pressBackspace();
    int after = popupRowCount();
    ASSERT_GT(after, 0) << "popup should remain visible after backspace";
    EXPECT_GT(after, before) << "shorter prefix should produce more matches";
    EXPECT_EQ(editor->toPlainText(), QString(":sm"));
}

TEST_F(EditorCompletionUITest, BackspaceOnClosingColonHidesPopup)
{
    typeText(":sm");
    QApplication::processEvents();
    ASSERT_GT(popupRowCount(), 0);

    // Backspace to ':s' — popup should still be visible
    pressBackspace();
    ASSERT_GT(popupRowCount(), 0) << "popup visible after backspace to ':s'";

    // Backspace to ':' — popup should hide (no partial code)
    pressBackspace();
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when only ':' remains";
    EXPECT_EQ(editor->toPlainText(), QString(":"));
}

TEST_F(EditorCompletionUITest, EmojiPopupSitsBelowCursorLine)
{
    typeText(":s");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    QAbstractItemView *popup = editor->completer()->popup();
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isVisible());

    QRect cursor = editor->cursorRect();
    QPoint cursorBottomGlobal = editor->viewport()->mapToGlobal(
        QPoint(cursor.x(), cursor.y() + cursor.height()));

    EXPECT_GE(popup->geometry().y(), cursorBottomGlobal.y())
        << "popup top should be at or below the bottom of the cursor line";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

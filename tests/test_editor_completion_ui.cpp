#include <gtest/gtest.h>
#include <QApplication>
#include <QCompleter>
#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include "Editor.h"
#include "Preferences.h"

class EditorCompletionUITest : public testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().setValue(Preferences::FileAutoComplete, true);
        QSettings().setValue(Preferences::EmojiAutoComplete, true);
        QSettings().setValue(Preferences::LanguageAutoComplete, true);

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

TEST_F(EditorCompletionUITest, CodeFenceLanguageCompletes)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0) << "popup should appear while typing language after ```";
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "```python");
}

TEST_F(EditorCompletionUITest, CodeFenceMermaidCompletes)
{
    editor->clear();
    typeText("```mer");
    ASSERT_GT(popupRowCount(), 0);
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "```mermaid");
}

TEST_F(EditorCompletionUITest, CodeFenceJsSuggestsJson)
{
    editor->clear();
    typeText("```js");
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "```json");
}

TEST_F(EditorCompletionUITest, CodeFenceAliasSuggestsJavascript)
{
    editor->clear();
    typeText("```javas");
    pressEnter();
    EXPECT_EQ(editor->toPlainText(), "```javascript");
}

TEST_F(EditorCompletionUITest, CodeFencePrefixSortedFirst)
{
    editor->clear();
    typeText("```c");
    ASSERT_GT(popupRowCount(), 0);
    QString first = editor->completer()->completionModel()->index(0, 0).data().toString();
    EXPECT_EQ(first, "c");
}

TEST_F(EditorCompletionUITest, CodeFenceEscapeKeepsText)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0);
    pressEscape();
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden after escape";
    EXPECT_EQ(editor->toPlainText(), "```py");
}

TEST_F(EditorCompletionUITest, CodeFenceBackspaceUpdatesPopup)
{
    editor->clear();
    typeText("```pyth");
    ASSERT_GT(popupRowCount(), 0);

    pressBackspace();
    ASSERT_GT(popupRowCount(), 0) << "popup should remain visible after backspace";
    EXPECT_EQ(editor->toPlainText(), "```pyt");

    pressBackspace();
    pressBackspace();
    pressBackspace();
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when only ``` remains";
    EXPECT_EQ(editor->toPlainText(), "```");
}

TEST_F(EditorCompletionUITest, NoCompletionOnClosingFence)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0);
    pressEscape();
    pressEnter();

    typeText("```py");
    EXPECT_EQ(popupRowCount(), -1) << "no popup on a closing fence";
    EXPECT_EQ(editor->toPlainText(), "```py\n```py");
}

TEST_F(EditorCompletionUITest, NoCompletionInsideCodeBlock)
{
    editor->clear();
    typeText("```py");
    pressEscape();
    pressEnter();

    typeText("py");
    EXPECT_EQ(popupRowCount(), -1) << "no popup for plain text inside a code block";
}

TEST_F(EditorCompletionUITest, CodeFenceCompletionRespectsPreference)
{
    QSettings().setValue(Preferences::LanguageAutoComplete, false);
    editor->clear();
    typeText("```py");
    EXPECT_EQ(popupRowCount(), -1) << "no popup when language autocomplete is disabled";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

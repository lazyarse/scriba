#include <gtest/gtest.h>
#include <QApplication>
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
            f.open(QIODevice::WriteOnly);
            f.write("# Test\n");
        }

        auto touch = [&](const QString &name) {
            QFile f(tmpDir.path() + "/" + name);
            f.open(QIODevice::WriteOnly);
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
    EXPECT_EQ(editor->toPlainText(), "![](resources/");
}

TEST_F(EditorCompletionUITest, ChainedCompletion)
{
    typeText("![](resou");
    pressTab();
    ASSERT_EQ(editor->toPlainText(), "![](resources/");

    typeText("ico");
    pressTab();
    EXPECT_EQ(editor->toPlainText(), "![](resources/icons/");
}

TEST_F(EditorCompletionUITest, PopupCyclesAndAccepts)
{
    typeText("![](r");
    pressTab();

    // Cycle to second item (resources/), then accept
    pressTab();
    pressEnter();

    EXPECT_EQ(editor->toPlainText(), "![](resources/");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

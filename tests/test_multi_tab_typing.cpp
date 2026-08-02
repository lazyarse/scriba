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
#include <QSettings>
#include <QTest>
#include <QTextCursor>

#include "MainWindow.h"
#include "Editor.h"
#include "Preferences.h"
#include "TestConfig.h"

namespace {

int blockOf(Editor *ed)
{
    return ed->textCursor().blockNumber();
}

int colOf(Editor *ed)
{
    return ed->textCursor().positionInBlock();
}

void pressEnterThroughFocus(MainWindow *window, Editor *ed)
{
    QWidget *focus = QApplication::focusWidget();
    qDebug() << "FOCUS:" << (focus ? focus->metaObject()->className() : "null")
             << "text before:" << ed->toPlainText().replace('\n', '|')
             << "block:" << blockOf(ed) << "col:" << colOf(ed);
    QTest::keyClick(focus ? focus : ed, Qt::Key_Return);
    QApplication::processEvents();
    qDebug() << "  text after:" << ed->toPlainText().replace('\n', '|')
             << "block:" << blockOf(ed) << "col:" << colOf(ed);
}

void createNewTab(MainWindow *window)
{
    QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
    QApplication::processEvents();
}

} // namespace

class MultiTabWindowTest : public testing::Test {
protected:
    void SetUp() override {
        QSettings s;
        s.setValue(Preferences::ReopenLastSession, false);
        s.setValue(Preferences::AutoSaveOnExit, false);
        s.setValue(Preferences::AutoSaveInterval, 0);

        window = new MainWindow();
        window->resize(1200, 800);
        window->show();
        QApplication::processEvents();
    }

    void TearDown() override {
        delete window;
        window = nullptr;
    }

    MainWindow *window = nullptr;
};

TEST_F(MultiTabWindowTest, FirstTabEnterAfterHeadingInsertsNewline)
{
    Editor *ed = window->editor();
    ed->setFocus();
    QApplication::processEvents();

    QTest::keyClicks(ed, "#title");
    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "#title\n");
    EXPECT_EQ(blockOf(ed), 1);

    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "#title\n\n");
    EXPECT_EQ(blockOf(ed), 2);
    EXPECT_EQ(colOf(ed), 0);
}

TEST_F(MultiTabWindowTest, SecondTabEnterAfterHeadingInsertsNewline)
{
    createNewTab(window);
    Editor *ed = window->editor();
    ed->setFocus();
    QApplication::processEvents();

    QTest::keyClicks(ed, "#title");
    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "#title\n");
    EXPECT_EQ(blockOf(ed), 1);

    QTest::qWait(200);
    QApplication::processEvents();
    if (QTest::qWaitForWindowActive(window))
        EXPECT_EQ(QApplication::focusWidget(), ed) << "preview must not steal focus";

    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "#title\n\n");
    EXPECT_EQ(blockOf(ed), 2);
    EXPECT_EQ(colOf(ed), 0);
}

TEST_F(MultiTabWindowTest, SecondTabListContinuationStopsAfterTwoEnters)
{
    createNewTab(window);
    Editor *ed = window->editor();
    ed->setFocus();
    QApplication::processEvents();

    QTest::keyClicks(ed, "- item 1");
    pressEnterThroughFocus(window, ed);
    QTest::keyClicks(ed, "item2");
    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "- item 1\n- item2\n- ");
    EXPECT_EQ(blockOf(ed), 2);
    EXPECT_EQ(colOf(ed), 2);

    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "- item 1\n- item2\n\n");
    EXPECT_EQ(blockOf(ed), 3);
    EXPECT_EQ(colOf(ed), 0);

    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "- item 1\n- item2\n\n\n");
    EXPECT_EQ(blockOf(ed), 4);
    EXPECT_EQ(colOf(ed), 0);
}

TEST_F(MultiTabWindowTest, SecondTabKeepsKeyboardFocusAfterPreviewUpdate)
{
    createNewTab(window);
    Editor *ed = window->editor();
    ed->setFocus();
    QApplication::processEvents();

    QTest::keyClicks(ed, "alpha");
    QTest::qWait(250);
    QApplication::processEvents();
    if (QTest::qWaitForWindowActive(window))
        EXPECT_EQ(QApplication::focusWidget(), ed);

    pressEnterThroughFocus(window, ed);
    EXPECT_EQ(ed->toPlainText(), "alpha\n");
    EXPECT_EQ(blockOf(ed), 1);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

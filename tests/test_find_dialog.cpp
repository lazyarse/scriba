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
#include <QSignalSpy>
#include <QTest>
#include <QTextCursor>
#include <QTextBlock>
#include <QScrollBar>
#include <QPushButton>
#include <QDialog>

#include "dialogs/FindDialog.h"
#include "mainwindow/MainWindow.h"
#include "editor/Editor.h"
#include "TestConfig.h"

class FindDialogTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_find_dialog";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }
};

TEST_F(FindDialogTest, MatchCountLabelDisplay)
{
    FindDialog dialog;
    dialog.show();

    dialog.setMatchCount(0);
    EXPECT_EQ(dialog.matchCountText(), "No matches");

    dialog.setMatchCount(1);
    EXPECT_EQ(dialog.matchCountText(), "1 match");

    dialog.setMatchCount(42);
    EXPECT_EQ(dialog.matchCountText(), "42 matches");
}

TEST_F(FindDialogTest, SearchTextChangedFiredOnTyping)
{
    FindDialog dialog;
    QSignalSpy spy(&dialog, &FindDialog::searchTextChanged);

    dialog.show();
    dialog.focusSearchInput();
    QTest::keyClicks(dialog.focusWidget(), "hello");

    ASSERT_GT(spy.count(), 0);
    auto args = spy.last();
    EXPECT_EQ(args.at(0).toString(), "hello");
}

TEST_F(FindDialogTest, SearchTextChangedFiredOnRegexToggle)
{
    FindDialog dialog;
    dialog.show();

    QSignalSpy spy(&dialog, &FindDialog::searchTextChanged);
    spy.clear();

    auto *regexCb = dialog.findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);

    ASSERT_GE(spy.count(), 1);
    auto args = spy.last();
    EXPECT_TRUE(args.at(1).toBool());
}

TEST_F(FindDialogTest, SearchTextChangedFiredOnCaseToggle)
{
    FindDialog dialog;
    dialog.show();

    auto checkBoxes = dialog.findChildren<QCheckBox *>();
    ASSERT_GE(checkBoxes.size(), 2);
    QCheckBox *caseCb = checkBoxes.at(1);

    QSignalSpy spy(&dialog, &FindDialog::searchTextChanged);
    spy.clear();

    QTest::mouseClick(caseCb, Qt::LeftButton);

    ASSERT_GE(spy.count(), 1);
    auto args = spy.last();
    EXPECT_TRUE(args.at(2).toBool());
}

TEST_F(FindDialogTest, InitialMatchCountZero)
{
    FindDialog dialog;
    dialog.show();

    EXPECT_EQ(dialog.matchCountText(), "No matches");
}

TEST_F(FindDialogTest, EmptySearchDoesNotEmitFindNext)
{
    FindDialog dialog;
    QSignalSpy spy(&dialog, &FindDialog::findNextRequested);

    dialog.show();
    QTest::keyClick(&dialog, Qt::Key_Return);

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(FindDialogTest, EmptySearchDoesNotEmitFindPrev)
{
    FindDialog dialog;
    QSignalSpy spy(&dialog, &FindDialog::findPrevRequested);

    dialog.show();
    QTest::keyClick(dialog.focusWidget(), Qt::Key_Return);

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(FindDialogTest, ReplaceNotEmittedOnEmptySearch)
{
    FindDialog dialog;
    QSignalSpy spy(&dialog, &FindDialog::replaceRequested);

    dialog.show();
    dialog.focusReplaceInput();
    QTest::keyClick(dialog.focusWidget(), Qt::Key_Return);

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(FindDialogTest, ReplaceAllNotEmittedOnEmptySearch)
{
    FindDialog dialog;
    QSignalSpy spy(&dialog, &FindDialog::replaceAllRequested);

    dialog.show();
    auto *replaceAllBtn = dialog.findChildren<QPushButton *>().last();
    QTest::mouseClick(replaceAllBtn, Qt::LeftButton);

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(FindDialogTest, ContentFitsInFixedSizeDialog)
{
    FindDialog dialog;
    dialog.show();
    QApplication::processEvents();

    const QRect clientRect(QPoint(0, 0), dialog.size());
    for (auto *child : dialog.findChildren<QWidget *>()) {
        if (!child->isVisible())
            continue;
        const QRect childRect(child->mapTo(&dialog, QPoint(0, 0)), child->size());
        EXPECT_TRUE(clientRect.contains(childRect))
            << "clipped widget: " << childRect.x() << "," << childRect.y()
            << " " << childRect.width() << "x" << childRect.height()
            << " in dialog " << dialog.width() << "x" << dialog.height();
    }
}

class FindDialogIntegrationTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "test_find_dialog";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
    }

    void SetUp() override {
        window = new MainWindow();
        window->resize(1200, 800);
        window->show();
        QApplication::processEvents();
    }

    void TearDown() override {
        delete window;
    }

    static void clickButton(QWidget *parent, const QString &label)
    {
        const QString wanted = QString(label).remove('&');
        for (auto *btn : parent->findChildren<QPushButton *>()) {
            if (btn->text().remove('&') == wanted) {
                QTest::mouseClick(btn, Qt::LeftButton);
                return;
            }
        }
        FAIL() << "button not found: " << qPrintable(label);
    }

    MainWindow *window = nullptr;
};

TEST_F(FindDialogIntegrationTest, IntegratedLiveCount)
{
    auto *editor = window->editor();
    editor->setPlainText("foo bar foo baz foo");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "foo");
    QApplication::processEvents();

    EXPECT_TRUE(dialog->matchCountText().contains("3"));
}

TEST_F(FindDialogIntegrationTest, CountRespectsCaseSensitivity)
{
    auto *editor = window->editor();
    editor->setPlainText("Foo foo FOO foo");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "foo");
    QApplication::processEvents();

    EXPECT_TRUE(dialog->matchCountText().contains("4"));

    auto checkBoxes = dialog->findChildren<QCheckBox *>();
    ASSERT_GE(checkBoxes.size(), 2);
    QCheckBox *caseCb = checkBoxes.at(1);
    QTest::mouseClick(caseCb, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_TRUE(dialog->matchCountText().contains("2"));
}

TEST_F(FindDialogIntegrationTest, CountRespectsRegex)
{
    auto *editor = window->editor();
    editor->setPlainText("foo bar foobar baz");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "f..");
    QApplication::processEvents();

    EXPECT_EQ(dialog->matchCountText(), "No matches");

    auto *regexCb = dialog->findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_TRUE(dialog->matchCountText().contains("2"));
}

TEST_F(FindDialogIntegrationTest, ClearSearchResetsCount)
{
    auto *editor = window->editor();
    editor->setPlainText("foo bar foo");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "foo");
    QApplication::processEvents();

    EXPECT_TRUE(dialog->matchCountText().contains("2"));

    for (int i = 0; i < 3; ++i)
        QTest::keyClick(dialog->focusWidget(), Qt::Key_Backspace);
    QApplication::processEvents();

    EXPECT_EQ(dialog->matchCountText(), "No matches");
}

TEST_F(FindDialogIntegrationTest, ReplaceAllWithBackreferences)
{
    auto *editor = window->editor();
    editor->setPlainText("2024-01-15 and 2023-12-25");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    auto lineEdits = dialog->findChildren<QLineEdit *>();
    ASSERT_GE(lineEdits.size(), 2);
    lineEdits[0]->setText(R"((\d{4})-(\d{2})-(\d{2}))");
    lineEdits[1]->setText(R"(\3/\2/\1)");
    QApplication::processEvents();

    auto *regexCb = dialog->findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);
    QApplication::processEvents();

    auto buttons = dialog->findChildren<QPushButton *>();
    QPushButton *replaceAllBtn = nullptr;
    for (auto *btn : buttons) {
        if (btn->text().remove('&') == "Replace All") {
            replaceAllBtn = btn;
            break;
        }
    }
    ASSERT_NE(replaceAllBtn, nullptr);
    QTest::mouseClick(replaceAllBtn, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(editor->toPlainText(), "15/01/2024 and 25/12/2023");
}

TEST_F(FindDialogIntegrationTest, ReplaceWithBackreferences)
{
    auto *editor = window->editor();
    editor->setPlainText("hello world");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    auto lineEdits = dialog->findChildren<QLineEdit *>();
    ASSERT_GE(lineEdits.size(), 2);
    lineEdits[0]->setText(R"((\w+) (\w+))");
    lineEdits[1]->setText(R"(\2 \1)");
    QApplication::processEvents();

    auto *regexCb = dialog->findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);
    QApplication::processEvents();

    auto buttons = dialog->findChildren<QPushButton *>();
    QPushButton *replaceBtn = nullptr;
    for (auto *btn : buttons) {
        if (btn->text().remove('&') == "Replace") {
            replaceBtn = btn;
            break;
        }
    }
    ASSERT_NE(replaceBtn, nullptr);

    for (int i = 0; i < 2; ++i) {
        QTest::mouseClick(replaceBtn, Qt::LeftButton);
        QApplication::processEvents();
    }

    EXPECT_EQ(editor->toPlainText(), "world hello");
}

TEST_F(FindDialogIntegrationTest, RegexReplaceAllWithoutBackreference)
{
    auto *editor = window->editor();
    editor->setPlainText("foo 42 bar 7");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    auto lineEdits = dialog->findChildren<QLineEdit *>();
    ASSERT_GE(lineEdits.size(), 2);
    lineEdits[0]->setText(R"(\d+)");
    lineEdits[1]->setText("NUM");
    QApplication::processEvents();

    auto *regexCb = dialog->findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);
    QApplication::processEvents();

    auto buttons = dialog->findChildren<QPushButton *>();
    QPushButton *replaceAllBtn = nullptr;
    for (auto *btn : buttons) {
        if (btn->text().remove('&') == "Replace All") {
            replaceAllBtn = btn;
            break;
        }
    }
    ASSERT_NE(replaceAllBtn, nullptr);
    QTest::mouseClick(replaceAllBtn, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(editor->toPlainText(), "foo NUM bar NUM");
}

TEST_F(FindDialogIntegrationTest, RegexReplaceAllRespectsCaseSensitivity)
{
    auto *editor = window->editor();
    editor->setPlainText("Foo foo FOO foo");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    auto lineEdits = dialog->findChildren<QLineEdit *>();
    ASSERT_GE(lineEdits.size(), 2);
    lineEdits[0]->setText("foo");
    lineEdits[1]->setText("bar");
    QApplication::processEvents();

    auto *regexCb = dialog->findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);
    QApplication::processEvents();

    auto checkBoxes = dialog->findChildren<QCheckBox *>();
    ASSERT_GE(checkBoxes.size(), 2);
    QCheckBox *caseCb = checkBoxes.at(1);
    QTest::mouseClick(caseCb, Qt::LeftButton);
    QApplication::processEvents();

    auto buttons = dialog->findChildren<QPushButton *>();
    QPushButton *replaceAllBtn = nullptr;
    for (auto *btn : buttons) {
        if (btn->text().remove('&') == "Replace All") {
            replaceAllBtn = btn;
            break;
        }
    }
    ASSERT_NE(replaceAllBtn, nullptr);
    QTest::mouseClick(replaceAllBtn, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(editor->toPlainText(), "Foo bar FOO bar");
}

TEST_F(FindDialogIntegrationTest, RegexReplaceWithNoMatch)
{
    auto *editor = window->editor();
    editor->setPlainText("hello world");
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);

    auto lineEdits = dialog->findChildren<QLineEdit *>();
    ASSERT_GE(lineEdits.size(), 2);
    lineEdits[0]->setText("zzz");
    lineEdits[1]->setText("bar");
    QApplication::processEvents();

    auto *regexCb = dialog->findChild<QCheckBox *>();
    ASSERT_NE(regexCb, nullptr);
    QTest::mouseClick(regexCb, Qt::LeftButton);
    QApplication::processEvents();

    auto buttons = dialog->findChildren<QPushButton *>();
    QPushButton *replaceAllBtn = nullptr;
    for (auto *btn : buttons) {
        if (btn->text().remove('&') == "Replace All") {
            replaceAllBtn = btn;
            break;
        }
    }
    ASSERT_NE(replaceAllBtn, nullptr);
    QTest::mouseClick(replaceAllBtn, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(editor->toPlainText(), "hello world");
}

TEST_F(FindDialogIntegrationTest, FindDoesNotScrollVisibleMatch)
{
    auto *editor = window->editor();
    QString text;
    for (int i = 0; i < 300; ++i)
        text += "line " + QString::number(i) + "\n";
    editor->setPlainText(text);
    QApplication::processEvents();

    QTextCursor c(editor->document()->findBlockByNumber(150));
    editor->setTextCursor(c);
    QApplication::processEvents();
    auto *sb = editor->verticalScrollBar();
    int absY = editor->cursorRect().top() + sb->value();
    sb->setValue(qMax(0, absY - editor->viewport()->height() / 3));
    QApplication::processEvents();
    const int scrollBefore = sb->value();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);
    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "line 152");
    clickButton(dialog, "&Next >");
    QApplication::processEvents();

    EXPECT_EQ(editor->verticalScrollBar()->value(), scrollBefore);
    QTextCursor found = editor->textCursor();
    EXPECT_TRUE(found.hasSelection());
    EXPECT_EQ(found.selectionStart(),
              editor->document()->findBlockByNumber(152).position());
}

TEST_F(FindDialogIntegrationTest, FindCentersMatchBelowViewport)
{
    auto *editor = window->editor();
    QString text;
    for (int i = 0; i < 150; ++i)
        text += "line " + QString::number(i) + "\n";
    text += "needle\n";
    for (int i = 151; i < 300; ++i)
        text += "line " + QString::number(i) + "\n";
    editor->setPlainText(text);
    QApplication::processEvents();

    QTextCursor c(editor->document()->findBlockByNumber(10));
    editor->setTextCursor(c);
    QApplication::processEvents();
    editor->verticalScrollBar()->setValue(0);
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);
    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "needle");
    clickButton(dialog, "&Next >");
    QApplication::processEvents();

    const int vh = editor->viewport()->height();
    const QRect cr = editor->cursorRect();
    EXPECT_GT(cr.center().y(), vh * 0.25);
    EXPECT_LT(cr.center().y(), vh * 0.75);
}

TEST_F(FindDialogIntegrationTest, FindCentersWrappedMatch)
{
    auto *editor = window->editor();
    QString text;
    for (int i = 0; i < 40; ++i)
        text += "line " + QString::number(i) + "\n";
    text += "needle\n";
    for (int i = 41; i < 300; ++i)
        text += "line " + QString::number(i) + "\n";
    editor->setPlainText(text);
    QApplication::processEvents();

    QTextCursor c(editor->document()->findBlockByNumber(260));
    editor->setTextCursor(c);
    QApplication::processEvents();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);
    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "needle");
clickButton(dialog, "&Next >");
    QApplication::processEvents();

    const int vh = editor->viewport()->height();
    const QRect cr = editor->cursorRect();
    EXPECT_GT(cr.center().y(), vh * 0.25);
    EXPECT_LT(cr.center().y(), vh * 0.75);
    EXPECT_EQ(editor->textCursor().selectionStart(),
              editor->document()->findBlockByNumber(40).position());
}

TEST_F(FindDialogIntegrationTest, NoMatchKeepsCaretAndScrollPosition)
{
    auto *editor = window->editor();
    QString text;
    for (int i = 0; i < 300; ++i)
        text += "line " + QString::number(i) + "\n";
    editor->setPlainText(text);
    QApplication::processEvents();

    QTextCursor c(editor->document()->findBlockByNumber(150));
    editor->setTextCursor(c);
    QApplication::processEvents();
    auto *sb = editor->verticalScrollBar();
    int absY = editor->cursorRect().top() + sb->value();
    sb->setValue(qMax(0, absY - editor->viewport()->height() / 3));
    QApplication::processEvents();
    const int posBefore = editor->textCursor().position();
    const int scrollBefore = sb->value();

    window->toggleFindDialog();
    QApplication::processEvents();

    auto *dialog = window->findChild<FindDialog *>();
    ASSERT_NE(dialog, nullptr);
    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "no-such-term-zzz");
    clickButton(dialog, "&Next >");
    QApplication::processEvents();

    EXPECT_EQ(editor->textCursor().position(), posBefore);
    EXPECT_EQ(editor->verticalScrollBar()->value(), scrollBefore);
}

TEST_F(FindDialogIntegrationTest, ReturnInSearchInputDoesNotFirePrev)
{
    window->toggleFindDialog();
    QApplication::processEvents();
    auto *dialog = window->findChild<FindDialog *>();
    QSignalSpy spy(dialog, &FindDialog::findPrevRequested);
    dialog->focusSearchInput();
    QTest::keyClicks(dialog->focusWidget(), "foo");
    QTest::keyClick(dialog->focusWidget(), Qt::Key_Return);
    QApplication::processEvents();
    EXPECT_EQ(spy.count(), 0);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

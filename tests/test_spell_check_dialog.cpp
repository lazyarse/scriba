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
//
// Check Spelling (M10): the "ignored always" list is persisted separately
// from the custom dictionary and matched case-insensitively, and the dialog
// drives next/prev, change, ignore-once, ignore-always and add-to-dictionary
// against a real Editor.
#include <gtest/gtest.h>
#include "editor/Editor.h"
#include "spell/SpellCheckDialog.h"
#include "spell/SpellChecker.h"
#include "spell/SpellHighlighter.h"
#include "TestConfig.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QSettings>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>

namespace {

QStringList issueWords(const QVector<SpellHighlighter::SpellIssue> &issues)
{
    QStringList words;
    for (const auto &issue : issues)
        words << issue.word;
    return words;
}

class SpellCheckerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        QDir().mkpath(SpellChecker::configDictDir());
        m_checker.loadLanguage("en_US");
    }

    void TearDown() override
    {
        QSettings().clear();
    }

    SpellChecker m_checker;
};

TEST_F(SpellCheckerTest, IgnoredWordStopsFlaggingAndPersists)
{
    EXPECT_FALSE(m_checker.checkWord("helo"));
    m_checker.addToIgnored("helo");
    EXPECT_TRUE(m_checker.checkWord("helo"));
    EXPECT_TRUE(m_checker.ignoredWords().contains("helo"));
    // Not merged into the custom dictionary.
    EXPECT_FALSE(m_checker.userWords().contains("helo"));

    // A fresh instance picks the ignored list up too.
    SpellChecker fresh;
    ASSERT_TRUE(fresh.loadLanguage("en_US"));
    EXPECT_TRUE(fresh.checkWord("helo"));

    m_checker.removeFromIgnored("helo");
    EXPECT_FALSE(m_checker.checkWord("helo"));
    SpellChecker fresh2;
    ASSERT_TRUE(fresh2.loadLanguage("en_US"));
    EXPECT_FALSE(fresh2.checkWord("helo"));
}

TEST_F(SpellCheckerTest, IgnoredMatchingIsCaseInsensitive)
{
    m_checker.addToIgnored("helo");
    EXPECT_TRUE(m_checker.checkWord("Helo"));
    EXPECT_TRUE(m_checker.checkWord("HELO"));
    EXPECT_TRUE(m_checker.checkWord("helo"));
}

TEST_F(SpellCheckerTest, IgnoredWordsFileRoundTrip)
{
    QFile::remove(SpellChecker::configDictDir() + "/ignored.dic");
    EXPECT_TRUE(SpellChecker::readIgnoredWords().isEmpty());
    SpellChecker::writeIgnoredWords({"zeta", "alpha", "alpha"});
    EXPECT_EQ(SpellChecker::readIgnoredWords(), QStringList({"alpha", "zeta"}));
}

class SpellCheckDialogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        // The persistent user/ignored dictionaries must not leak between
        // tests — remove both so each test starts from a clean slate.
        QDir().mkpath(SpellChecker::configDictDir());
        QFile::remove(SpellChecker::configDictDir() + "/user.dic");
        QFile::remove(SpellChecker::configDictDir() + "/ignored.dic");
        m_editor = new Editor();
        m_editor->resize(800, 600);
        m_editor->show();
    }

    void TearDown() override
    {
        delete m_editor;
        QSettings().clear();
    }

    Editor *m_editor = nullptr;
};

TEST_F(SpellCheckDialogTest, OpensOnFirstError)
{
    m_editor->setPlainText(QStringLiteral("helo speling\n"));
    SpellCheckDialog dlg(m_editor);
    ASSERT_EQ(dlg.issues().size(), 2);
    EXPECT_EQ(dlg.currentIndex(), 0);
    EXPECT_EQ(dlg.issues().at(0).word, "helo");
    EXPECT_EQ(dlg.changeToEdit()->text(), "helo");
}

TEST_F(SpellCheckDialogTest, NextPrevNavigationClampsAtEnds)
{
    m_editor->setPlainText(QStringLiteral("helo speling\n"));
    SpellCheckDialog dlg(m_editor);
    dlg.nextError();
    EXPECT_EQ(dlg.currentIndex(), 1);
    dlg.nextError();
    EXPECT_EQ(dlg.currentIndex(), 1); // clamped at the end
    dlg.prevError();
    EXPECT_EQ(dlg.currentIndex(), 0);
    dlg.prevError();
    EXPECT_EQ(dlg.currentIndex(), 0); // clamped at the start
}

TEST_F(SpellCheckDialogTest, IgnoreOnceSkipsOnlyThatOccurrence)
{
    m_editor->setPlainText(QStringLiteral("helo helo speling\n"));
    SpellCheckDialog dlg(m_editor);
    ASSERT_EQ(dlg.issues().size(), 3);
    dlg.ignoreOnceCurrent();
    ASSERT_EQ(dlg.issues().size(), 2);
    EXPECT_EQ(dlg.currentIndex(), 0);
    const QStringList words = issueWords(dlg.issues());
    EXPECT_TRUE(words.contains("helo"));
    EXPECT_TRUE(words.contains("speling"));
    // The document is untouched.
    EXPECT_EQ(m_editor->toPlainText(), QStringLiteral("helo helo speling\n"));
}

TEST_F(SpellCheckDialogTest, ChangeReplacesWordAndAdvances)
{
    m_editor->setPlainText(QStringLiteral("helo world\n"));
    SpellCheckDialog dlg(m_editor);
    dlg.changeToEdit()->setText(QStringLiteral("hello"));
    dlg.changeCurrent();
    EXPECT_EQ(m_editor->toPlainText(), QStringLiteral("hello world\n"));
    EXPECT_TRUE(dlg.issues().isEmpty());
    // The fix came from a dialog action, so the panel reports check complete.
    EXPECT_EQ(dlg.statusText(), QStringLiteral("Spelling check complete."));
    EXPECT_EQ(dlg.currentIndex(), -1);
}

TEST_F(SpellCheckDialogTest, ChangeSkipsEmptyOrUnchangedText)
{
    m_editor->setPlainText(QStringLiteral("helo\n"));
    SpellCheckDialog dlg(m_editor);
    dlg.changeToEdit()->setText(QStringLiteral("helo"));
    dlg.changeCurrent();
    EXPECT_EQ(dlg.issues().size(), 1); // no-op
    EXPECT_EQ(m_editor->toPlainText(), QStringLiteral("helo\n"));
    dlg.changeToEdit()->setText(QString());
    dlg.changeCurrent();
    EXPECT_EQ(dlg.issues().size(), 1); // still no-op
}

TEST_F(SpellCheckDialogTest, AddToDictionaryClearsWordAndPersists)
{
    m_editor->setPlainText(QStringLiteral("helo helo\n"));
    SpellCheckDialog dlg(m_editor);
    dlg.addToDictionaryCurrent();
    EXPECT_TRUE(dlg.issues().isEmpty());
    EXPECT_EQ(m_editor->toPlainText(), QStringLiteral("helo helo\n")); // doc untouched
    EXPECT_TRUE(SpellChecker::readUserDictionaryWords().contains("helo"));
    EXPECT_FALSE(SpellChecker::readIgnoredWords().contains("helo"));
}

TEST_F(SpellCheckDialogTest, IgnoreAlwaysClearsWordAndPersistsSeparately)
{
    m_editor->setPlainText(QStringLiteral("helo helo speling\n"));
    SpellCheckDialog dlg(m_editor);
    dlg.ignoreAlwaysCurrent();
    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.issues().at(0).word, "speling");
    EXPECT_TRUE(SpellChecker::readIgnoredWords().contains("helo"));
    EXPECT_FALSE(SpellChecker::readUserDictionaryWords().contains("helo"));
}

TEST_F(SpellCheckDialogTest, WorkingThroughAllErrorsEndsInDoneState)
{
    m_editor->setPlainText(QStringLiteral("helo speling\n"));
    SpellCheckDialog dlg(m_editor);
    dlg.changeToEdit()->setText(QStringLiteral("hello"));
    dlg.changeCurrent(); // fixes "helo", advances to "speling"
    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.issues().at(0).word, "speling");
    dlg.ignoreOnceCurrent(); // skips the last one
    EXPECT_TRUE(dlg.issues().isEmpty());
    EXPECT_EQ(dlg.statusText(), QStringLiteral("Spelling check complete."));
}

TEST_F(SpellCheckDialogTest, NoErrorsShowsFoundNoneState)
{
    m_editor->setPlainText(QStringLiteral("hello world\n"));
    SpellCheckDialog dlg(m_editor);
    EXPECT_TRUE(dlg.issues().isEmpty());
    EXPECT_EQ(dlg.statusText(), QStringLiteral("No spelling errors found."));
}

// The panel is modeless: edits in the editor flow back through the
// highlighter's incremental rescan into the issue list.
TEST_F(SpellCheckDialogTest, EditingDocumentRescansTheList)
{
    m_editor->setPlainText(QStringLiteral("hello\n"));
    SpellCheckDialog dlg(m_editor);
    EXPECT_TRUE(dlg.issues().isEmpty());

    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::End);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral(" helo"));
    QTest::qWait(700); // debounce (400 ms) fires

    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.issues().at(0).word, QStringLiteral("helo"));
}

TEST_F(SpellCheckDialogTest, EditingOutTheCurrentErrorDropsItFromList)
{
    m_editor->setPlainText(QStringLiteral("helo\n"));
    SpellCheckDialog dlg(m_editor);
    ASSERT_EQ(dlg.issues().size(), 1);

    // Select the word in the editor and retype it: the fix falls out of the list.
    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral("hello"));
    QTest::qWait(700);

    EXPECT_TRUE(dlg.issues().isEmpty());
    // The fix came from editing the document (not a dialog action), so the
    // panel reports the (now clean) document state.
    EXPECT_EQ(dlg.statusText(), QStringLiteral("No spelling errors found."));
}

TEST_F(SpellCheckDialogTest, PanelReactivatesWhenNewErrorsAppear)
{
    m_editor->setPlainText(QStringLiteral("hello\n"));
    SpellCheckDialog dlg(m_editor);
    EXPECT_TRUE(dlg.issues().isEmpty());
    EXPECT_EQ(dlg.statusText(), QStringLiteral("No spelling errors found."));

    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::End);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral(" helo"));
    QTest::qWait(700);

    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.currentIndex(), 0);
    EXPECT_NE(dlg.statusText(), QStringLiteral("No spelling errors found."));
}

// "Ignore once" is a session filter keyed (block, word): it survives the
// rescans reparsing the document, but an edit to the word itself re-flags it.
TEST_F(SpellCheckDialogTest, IgnoreOnceSurvivesRescanUntilWordEdited)
{
    m_editor->setPlainText(QStringLiteral("helo helo\n"));
    SpellCheckDialog dlg(m_editor);
    ASSERT_EQ(dlg.issues().size(), 2);
    dlg.ignoreOnceCurrent();
    ASSERT_EQ(dlg.issues().size(), 1);

    // An edit elsewhere rescans the document; the suppressed occurrence stays out.
    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::End);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral(" world"));
    QTest::qWait(700);
    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.issues().at(0).word, QStringLiteral("helo"));

    // Editing that very word changes its text, so it is no longer suppressed.
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral("hllo"));
    QTest::qWait(700);
    ASSERT_EQ(dlg.issues().size(), 2);
    EXPECT_EQ(dlg.issues().at(0).word, QStringLiteral("hllo"));
    EXPECT_EQ(dlg.issues().at(1).word, QStringLiteral("helo"));
}

TEST_F(SpellCheckDialogTest, RetargetSwitchesEditors)
{
    m_editor->setPlainText(QStringLiteral("helo\n"));
    SpellCheckDialog dlg(m_editor);
    ASSERT_EQ(dlg.issues().size(), 1);

    Editor second;
    second.resize(800, 600);
    second.show();
    second.setPlainText(QStringLiteral("speling\n"));
    dlg.retarget(&second);
    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.issues().at(0).word, QStringLiteral("speling"));

    // The original editor no longer drives the panel.
    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::End);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral(" more"));
    QTest::qWait(700);
    ASSERT_EQ(dlg.issues().size(), 1);
    EXPECT_EQ(dlg.issues().at(0).word, QStringLiteral("speling"));

    second.close();
}

TEST_F(SpellCheckDialogTest, EditingClearsTheBackgroundHighlight)
{
    m_editor->setPlainText(QStringLiteral("helo speling\n"));
    SpellCheckDialog dlg(m_editor);
    ASSERT_EQ(dlg.issues().size(), 2);
    EXPECT_FALSE(m_editor->extraSelections().isEmpty()); // pointed at the current error

    // Any edit drops the stale pointer so it never sits over shifted text.
    QTextCursor cursor(m_editor->document());
    cursor.movePosition(QTextCursor::End);
    m_editor->setTextCursor(cursor);
    QTest::keyClicks(m_editor, QStringLiteral(" "));
    EXPECT_TRUE(m_editor->extraSelections().isEmpty());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

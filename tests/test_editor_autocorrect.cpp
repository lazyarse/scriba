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
#include <QTextBlock>

#include "Editor.h"
#include "EditorTestHarness.h"
#include "Preferences.h"
#include "StaticHelpers.h"
#include "TestConfig.h"

namespace {

const QStringList kPairs = {
    QStringLiteral("teh=the"),
    QStringLiteral("adn=and"),
    QStringLiteral("alot=a lot"),
    QStringLiteral("im=I'm"),
};

// Fixture that enables autocorrect with a known pair set for every test.
class AutoCorrectHarness : public EditorTestHarness
{
public:
    AutoCorrectHarness()
        : EditorTestHarness(CompletionPrefs{})
    {
    }

protected:
    void SetUp() override
    {
        EditorTestHarness::SetUp();
        setPairs(kPairs);
        QSettings().setValue(Preferences::AutoCorrectEnabled, true);
    }

    void setPairs(const QStringList &pairs)
    {
        QSettings().setValue(Preferences::AutoCorrectPairs, pairs);
    }
};

} // anonymous namespace

// --- Pure autoCorrectWord() tests -------------------------------------------

TEST(AutoCorrectWord, MatchesWordAtEnd)
{
    int start = -1, len = 0;
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh"), 3, kPairs, false, &start, &len), "the");
    EXPECT_EQ(start, 0);
    EXPECT_EQ(len, 3);
}

TEST(AutoCorrectWord, RequiresSeparatorWhenTypingSeparator)
{
    // separatorTyped=true needs a trailing non-letter so mid-word keystrokes never fire
    EXPECT_EQ(autoCorrectWord(QStringLiteral("tehx"), 4, kPairs, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh "), 4, kPairs, true), "the");
}

TEST(AutoCorrectWord, WordEndOnlyMatchesWithSeparator)
{
    // A finished word with no trailing separator only matches when separatorTyped=false
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh"), 3, kPairs, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh."), 4, kPairs, true), "the");
}

TEST(AutoCorrectWord, PreservesCase)
{
    EXPECT_EQ(autoCorrectWord(QStringLiteral("Teh "), 4, kPairs, true), "The");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("TEH "), 4, kPairs, true), "THE");
}

TEST(AutoCorrectWord, ReplacementMayContainSpacesAndApostrophes)
{
    EXPECT_EQ(autoCorrectWord(QStringLiteral("alot "), 5, kPairs, true), "a lot");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("im "), 3, kPairs, true), "I'm");
}

TEST(AutoCorrectWord, UnknownWordNotReplaced)
{
    EXPECT_EQ(autoCorrectWord(QStringLiteral("xyz "), 4, kPairs, true), "");
}

TEST(AutoCorrectWord, SkipsTokensGluedToUrlsAndEmails)
{
    EXPECT_EQ(autoCorrectWord(QStringLiteral("www.teh.com "), 11, kPairs, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("foo@teh "), 8, kPairs, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("a/teh "), 6, kPairs, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("foo_teh "), 8, kPairs, true), "");
}

TEST(AutoCorrectWord, HandlesEmptyAndBadPairs)
{
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh "), 4, QStringList{}, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh "), 4, QStringList{"=the"}, true), "");
    EXPECT_EQ(autoCorrectWord(QStringLiteral("teh "), 4, QStringList{"teh="}, true), "");
}

// --- Typing integration tests -----------------------------------------------

TEST_F(AutoCorrectHarness, CorrectsTypoOnSpace)
{
    typeText("teh ");
    EXPECT_EQ(text(), "the ");
    assertCursor(0, 4);
}

TEST_F(AutoCorrectHarness, CorrectsTypoOnPunctuation)
{
    typeText("teh.");
    EXPECT_EQ(text(), "the.");
    assertCursor(0, 4);
}

TEST_F(AutoCorrectHarness, EnterCompletesAndCorrects)
{
    typeLine("teh");
    EXPECT_EQ(text(), "the\n");
    assertCursor(1, 0);
}

TEST_F(AutoCorrectHarness, NoCorrectionMidWord)
{
    typeText("tehx ");
    EXPECT_EQ(text(), "tehx ");
}

TEST_F(AutoCorrectHarness, PreservesCapitalisation)
{
    typeText("Teh ");
    EXPECT_EQ(text(), "The ");

    setContent("");
    typeText("TEH ");
    EXPECT_EQ(text(), "THE ");
}

TEST_F(AutoCorrectHarness, DoesNotMangleUrl)
{
    typeText("www.teh.com ");
    EXPECT_EQ(text(), "www.teh.com ");
}

TEST_F(AutoCorrectHarness, SkipsFencedCode)
{
    setContent("```\nteh\n```");
    placeCursor(1, 3);
    typeText(" ");
    EXPECT_EQ(text(), "```\nteh \n```");
}

TEST_F(AutoCorrectHarness, SkipsInlineCode)
{
    setContent("`teh`");
    placeCursor(0, 4);
    typeText(" ");
    EXPECT_EQ(text(), "`teh `");
}

TEST_F(AutoCorrectHarness, SkipsLinkPath)
{
    setContent("[x](teh)");
    placeCursor(0, 7);
    typeText(" ");
    EXPECT_EQ(text(), "[x](teh )");
}

TEST_F(AutoCorrectHarness, DisabledSettingLeavesText)
{
    QSettings().setValue(Preferences::AutoCorrectEnabled, false);
    typeText("teh ");
    EXPECT_EQ(text(), "teh ");
}

TEST_F(AutoCorrectHarness, EmptyPairsLeaveText)
{
    setPairs({});
    typeText("teh ");
    EXPECT_EQ(text(), "teh ");
}

TEST_F(AutoCorrectHarness, UndoRestoresTypo)
{
    typeText("teh ");
    EXPECT_EQ(text(), "the ");
    editor->undo();
    EXPECT_EQ(text(), "teh ");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

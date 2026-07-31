#include <gtest/gtest.h>
#include "SpellChecker.h"
#include "Preferences.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

class SpellCheckerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        QDir().mkpath(SpellChecker::configDictDir());
    }

    void TearDown() override
    {
        QSettings().clear();
    }
};

} // namespace

TEST_F(SpellCheckerTest, AvailableLanguagesIncludesBundled)
{
    QStringList langs = SpellChecker::availableLanguages();
    EXPECT_TRUE(langs.contains("en_US"));
    EXPECT_TRUE(langs.contains("en_GB"));
}

TEST_F(SpellCheckerTest, LoadBundledLanguage)
{
    SpellChecker checker;
    EXPECT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_TRUE(checker.isLoaded());
    EXPECT_EQ(checker.language(), "en_US");
}

TEST_F(SpellCheckerTest, UnknownLanguageFailsGracefully)
{
    SpellChecker checker;
    EXPECT_FALSE(checker.loadLanguage("xx_XX"));
    EXPECT_FALSE(checker.isLoaded());
    // Not loaded means everything is "correct"
    EXPECT_TRUE(checker.checkWord("anything"));
}

TEST_F(SpellCheckerTest, CorrectWordPasses)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_TRUE(checker.checkWord("hello"));
    EXPECT_TRUE(checker.checkWord("dictionary"));
}

TEST_F(SpellCheckerTest, MisspelledWordFails)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_FALSE(checker.checkWord("helo"));
    EXPECT_FALSE(checker.checkWord("speling"));
}

TEST_F(SpellCheckerTest, SuggestionsProvided)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    QStringList sugg = checker.suggestions("helo");
    ASSERT_FALSE(sugg.isEmpty());
    EXPECT_TRUE(sugg.contains("hello"));
}

TEST_F(SpellCheckerTest, AddToUserDictionaryStopsFlagging)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QString word = "scribamarkdown";

    QFile f(SpellChecker::configDictDir() + "/user.dic");
    f.remove();

    EXPECT_FALSE(checker.checkWord(word));
    checker.addToUserDictionary(word);
    EXPECT_TRUE(checker.checkWord(word));
    EXPECT_TRUE(checker.userWords().contains(word));

    // Persisted: a fresh instance picks it up too
    SpellChecker checker2;
    ASSERT_TRUE(checker2.loadLanguage("en_US"));
    EXPECT_TRUE(checker2.checkWord(word));
}

TEST_F(SpellCheckerTest, RemoveFromUserDictionary)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QString word = "scribamarkdown";

    checker.addToUserDictionary(word);
    EXPECT_TRUE(checker.checkWord(word));

    checker.removeFromUserDictionary(word);
    EXPECT_FALSE(checker.checkWord(word));

    SpellChecker checker2;
    ASSERT_TRUE(checker2.loadLanguage("en_US"));
    EXPECT_FALSE(checker2.checkWord(word));
}

TEST_F(SpellCheckerTest, IgnoreAllPersists)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QString word = "helo";

    EXPECT_FALSE(checker.checkWord(word));
    checker.ignoreAll(word);
    EXPECT_TRUE(checker.checkWord(word));

    SpellChecker checker2;
    ASSERT_TRUE(checker2.loadLanguage("en_US"));
    EXPECT_TRUE(checker2.checkWord(word));
}

TEST_F(SpellCheckerTest, SessionIgnoreOnly)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QString word = "helo";

    checker.ignoreWord(word);
    EXPECT_TRUE(checker.checkWord(word));

    SpellChecker checker2;
    ASSERT_TRUE(checker2.loadLanguage("en_US"));
    EXPECT_FALSE(checker2.checkWord(word));
}

TEST_F(SpellCheckerTest, ReadUserDictionaryWordsMissingFileIsEmpty)
{
    QFile::remove(SpellChecker::configDictDir() + "/user.dic");
    EXPECT_TRUE(SpellChecker::readUserDictionaryWords().isEmpty());
}

TEST_F(SpellCheckerTest, WriteThenReadUserDictionaryRoundTrip)
{
    SpellChecker::writeUserDictionaryWords({"zeta", "alpha", "alpha", "mid"});
    QStringList words = SpellChecker::readUserDictionaryWords();
    EXPECT_EQ(words, QStringList({"alpha", "mid", "zeta"}));
}

TEST_F(SpellCheckerTest, UserDictionaryRoundTripSurvivesReload)
{
    const QString word = "scribamarkdown";
    SpellChecker::writeUserDictionaryWords({"alpha", word});

    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_TRUE(checker.checkWord(word));

    SpellChecker::writeUserDictionaryWords({"alpha"});
    SpellChecker checker2;
    ASSERT_TRUE(checker2.loadLanguage("en_US"));
    EXPECT_FALSE(checker2.checkWord(word));
}

TEST_F(SpellCheckerTest, WriteThenReadIgnoreListRoundTrip)
{
    SpellChecker::writeIgnoreList({"helo", "wrold", "helo"});
    EXPECT_EQ(SpellChecker::readIgnoreList(), QStringList({"helo", "wrold"}));

    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_TRUE(checker.checkWord("helo"));
    EXPECT_TRUE(checker.checkWord("wrold"));
    EXPECT_TRUE(checker.ignoredWords().contains("helo"));
}

TEST_F(SpellCheckerTest, ClearIgnoreList)
{
    SpellChecker::writeIgnoreList({"helo"});
    SpellChecker::writeIgnoreList({});
    EXPECT_TRUE(SpellChecker::readIgnoreList().isEmpty());

    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_FALSE(checker.isIgnored("helo"));
    EXPECT_FALSE(checker.checkWord("helo"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("scribaTest");
    QCoreApplication::setApplicationName("scribaTest");
    QStandardPaths::setTestModeEnabled(true);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

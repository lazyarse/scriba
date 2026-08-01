#include <gtest/gtest.h>
#include "SpellChecker.h"
#include "Preferences.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "TestConfig.h"

namespace {

// Copy the bundled en_US dictionary pair from the qrc bundle into dirPath
// under a new base name, returning the path of the .aff file.
QString copyBundledAs(const QString &dirPath, const QString &base)
{
    for (const char *ext : {".aff", ".dic"}) {
        QFile src(QStringLiteral(":/dictionaries/en_US") + QLatin1String(ext));
        if (!src.open(QIODevice::ReadOnly))
            return {};
        QFile dst(dirPath + "/" + base + QLatin1String(ext));
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return {};
        dst.write(src.readAll());
    }
    return dirPath + "/" + base + ".aff";
}

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

TEST_F(SpellCheckerTest, BundledLanguageDetection)
{
    EXPECT_TRUE(SpellChecker::isBundledLanguage("en_US"));
    EXPECT_TRUE(SpellChecker::isBundledLanguage("en_GB"));
    EXPECT_FALSE(SpellChecker::isBundledLanguage("de_DE"));
    EXPECT_FALSE(SpellChecker::isBundledLanguage(QString()));
}

TEST_F(SpellCheckerTest, RemoveDictionaryRefusesBundled)
{
    EXPECT_FALSE(SpellChecker::removeDictionary("en_US"));
    EXPECT_FALSE(SpellChecker::removeDictionary("en_GB"));
    EXPECT_TRUE(SpellChecker::availableLanguages().contains("en_US"));
    EXPECT_TRUE(SpellChecker::availableLanguages().contains("en_GB"));
}

TEST_F(SpellCheckerTest, RemoveDictionaryRefusesReservedBase)
{
    EXPECT_FALSE(SpellChecker::removeDictionary("user"));
    EXPECT_FALSE(SpellChecker::removeDictionary("bundled"));
}

TEST_F(SpellCheckerTest, InstallThenRemoveUserDictionary)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_FALSE(copyBundledAs(dir.path(), "zz_ZZ").isEmpty());

    const QString lang = SpellChecker::installDictionary(dir.path() + "/zz_ZZ.aff");
    ASSERT_EQ(lang, "zz_ZZ");
    EXPECT_TRUE(SpellChecker::availableLanguages().contains("zz_ZZ"));

    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("zz_ZZ"));
    EXPECT_EQ(checker.language(), "zz_ZZ");
    EXPECT_TRUE(checker.checkWord("hello"));
    EXPECT_FALSE(checker.checkWord("helo"));

    EXPECT_TRUE(SpellChecker::removeDictionary("zz_ZZ"));
    EXPECT_FALSE(SpellChecker::availableLanguages().contains("zz_ZZ"));

    SpellChecker checker2;
    EXPECT_FALSE(checker2.loadLanguage("zz_ZZ"));
    SpellChecker::removeDictionary("zz_ZZ");
}

TEST_F(SpellCheckerTest, InstallDictionaryFromDicFileWorks)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_FALSE(copyBundledAs(dir.path(), "fr_FR").isEmpty());

    const QString lang = SpellChecker::installDictionary(dir.path() + "/fr_FR.dic");
    EXPECT_EQ(lang, "fr_FR");
    EXPECT_TRUE(SpellChecker::availableLanguages().contains("fr_FR"));
    SpellChecker::removeDictionary("fr_FR");
}

TEST_F(SpellCheckerTest, ReinstallExistingDictionaryIsIdempotent)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_FALSE(copyBundledAs(dir.path(), "it_IT").isEmpty());

    EXPECT_EQ(SpellChecker::installDictionary(dir.path() + "/it_IT.aff"), "it_IT");
    EXPECT_EQ(SpellChecker::installDictionary(dir.path() + "/it_IT.aff"), "it_IT");
    SpellChecker::removeDictionary("it_IT");
}

TEST_F(SpellCheckerTest, InstallDictionaryRequiresSiblingFile)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile aff(dir.path() + "/de_DE.aff");
    ASSERT_TRUE(aff.open(QIODevice::WriteOnly | QIODevice::Text));
    aff.write("SET UTF-8\n");
    aff.close();

    EXPECT_TRUE(SpellChecker::installDictionary(dir.path() + "/de_DE.aff").isEmpty());
    EXPECT_FALSE(SpellChecker::availableLanguages().contains("de_DE"));
}

TEST_F(SpellCheckerTest, InstallDictionaryRejectsCorruptPair)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile aff(dir.path() + "/es_ES.aff");
    ASSERT_TRUE(aff.open(QIODevice::WriteOnly | QIODevice::Text));
    aff.write("SET UTF-8\n");
    aff.close();
    QFile dic(dir.path() + "/es_ES.dic");
    ASSERT_TRUE(dic.open(QIODevice::WriteOnly | QIODevice::Text));
    dic.write("not-a-number\nfoo\n");
    dic.close();

    EXPECT_TRUE(SpellChecker::installDictionary(dir.path() + "/es_ES.aff").isEmpty());
    EXPECT_FALSE(SpellChecker::availableLanguages().contains("es_ES"));
    EXPECT_FALSE(QFileInfo::exists(SpellChecker::configDictDir() + "/es_ES.aff"));
    EXPECT_FALSE(QFileInfo::exists(SpellChecker::configDictDir() + "/es_ES.dic"));
}

TEST_F(SpellCheckerTest, InstallDictionaryRejectsUnsafeBase)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile aff(dir.path() + "/user.aff");
    ASSERT_TRUE(aff.open(QIODevice::WriteOnly | QIODevice::Text));
    aff.write("SET UTF-8\n");
    aff.close();

    EXPECT_TRUE(SpellChecker::installDictionary(dir.path() + "/user.aff").isEmpty());
    EXPECT_FALSE(SpellChecker::availableLanguages().contains("user"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

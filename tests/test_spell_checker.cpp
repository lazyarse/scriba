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
// SpellChecker (M10): the bundled dictionaries are stoppard plain word lists
// (en-US/en-GB + Māori/Canadian allowances), imported dictionaries are plain
// .txt word lists that union with the active base dictionary, and the user
// dictionary keeps its count-header format.
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

// Write a plain word list into dirPath under `base`.txt, returning its path.
QString writeWordList(const QString &dirPath, const QString &base,
                      const QStringList &words)
{
    QDir().mkpath(dirPath);
    QFile f(dirPath + "/" + base + ".txt");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return {};
    f.write(words.join('\n').toUtf8());
    f.write("\n");
    return f.fileName();
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

TEST_F(SpellCheckerTest, AvailableLanguagesAreTheBundledTwo)
{
    // availableLanguages() also extracts the bundled dictionaries on first use.
    QStringList langs = SpellChecker::availableLanguages();
    EXPECT_EQ(langs, QStringList({"en_US", "en_GB"}));
    EXPECT_TRUE(QFileInfo::exists(SpellChecker::configDictDir() + "/bundled/en-US.txt"));
    EXPECT_TRUE(QFileInfo::exists(SpellChecker::configDictDir() + "/bundled/en-GB.txt"));
    EXPECT_TRUE(QFileInfo::exists(SpellChecker::configDictDir() + "/bundled/maori-nz.txt"));
    EXPECT_TRUE(QFileInfo::exists(SpellChecker::configDictDir() + "/bundled/canadian-en.txt"));
    EXPECT_TRUE(QFileInfo::exists(SpellChecker::configDictDir() + "/bundled/keyboard-en-GB.txt"));
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
    EXPECT_TRUE(checker.checkWord("cat"));
}

TEST_F(SpellCheckerTest, MisspelledWordFails)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_FALSE(checker.checkWord("helo"));
    EXPECT_FALSE(checker.checkWord("speling"));
}

TEST_F(SpellCheckerTest, TokenPolicySkipsAreNeverFlagged)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_TRUE(checker.checkWord("recieve-this"));  // hyphenated compound
    EXPECT_TRUE(checker.checkWord("2recieve"));      // digit-bearing
    EXPECT_TRUE(checker.checkWord("RECIEVE"));       // all-caps
}

TEST_F(SpellCheckerTest, SuggestionsProvided)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    QStringList sugg = checker.suggestions("helo");
    ASSERT_FALSE(sugg.isEmpty());
    EXPECT_TRUE(sugg.contains("hello"));
}

TEST_F(SpellCheckerTest, CaseMatchedSuggestions)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QStringList sugg = checker.suggestions("Recieve");
    ASSERT_FALSE(sugg.isEmpty());
    EXPECT_EQ(sugg.front(), "Receive");
}

TEST_F(SpellCheckerTest, AddToUserDictionaryStopsFlagging)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QString word = "scribamarkdown";

    QFile::remove(SpellChecker::configDictDir() + "/user.dic");

    EXPECT_FALSE(checker.checkWord(word));
    checker.addToUserDictionary(word);
    EXPECT_TRUE(checker.checkWord(word));
    EXPECT_TRUE(checker.userWords().contains(word));

    // Persisted: a fresh instance picks it up too
    SpellChecker checker2;
    ASSERT_TRUE(checker2.loadLanguage("en_US"));
    EXPECT_TRUE(checker2.checkWord(word));
}

TEST_F(SpellCheckerTest, UserDictionaryMatchesCaseInsensitively)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    const QString word = "scribamarkdown";

    QFile::remove(SpellChecker::configDictDir() + "/user.dic");

    // A capitalized add must satisfy the folded ("scribamarkdown") lookup:
    // engine folds user entries to the same form the dictionary uses.
    checker.addToUserDictionary("Scribamarkdown");
    EXPECT_TRUE(checker.checkWord(word));

    // Leave the user dictionary clean for subsequent tests.
    QFile::remove(SpellChecker::configDictDir() + "/user.dic");
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

TEST_F(SpellCheckerTest, ParseWordListSplitsLinesAndTrims)
{
    EXPECT_EQ(SpellChecker::parseWordList(QStringLiteral("alpha\nbeta\n  gamma \n")),
        QStringList({"alpha", "beta", "gamma"}));
}

TEST_F(SpellCheckerTest, ParseWordListSkipsEmptyLines)
{
    EXPECT_EQ(SpellChecker::parseWordList(QStringLiteral("\n\nalpha\n\nbeta\n\n")),
        QStringList({"alpha", "beta"}));
}

TEST_F(SpellCheckerTest, ParseWordListHandlesCrlf)
{
    EXPECT_EQ(SpellChecker::parseWordList(QStringLiteral("alpha\r\nbeta\r\ngamma\r\n")),
        QStringList({"alpha", "beta", "gamma"}));
}

TEST_F(SpellCheckerTest, ParseWordListSkipsCountHeader)
{
    // user.dic-style file: leading count line must not become a word
    EXPECT_EQ(SpellChecker::parseWordList(QStringLiteral("3\nalpha\nbeta\ngamma\n")),
        QStringList({"alpha", "beta", "gamma"}));
    // a numeric word later in the list is kept
    EXPECT_EQ(SpellChecker::parseWordList(QStringLiteral("alpha\n42\nbeta\n")),
        QStringList({"alpha", "42", "beta"}));
}

TEST_F(SpellCheckerTest, ParseWordListEmptyInput)
{
    EXPECT_TRUE(SpellChecker::parseWordList(QString()).isEmpty());
    EXPECT_TRUE(SpellChecker::parseWordList(QStringLiteral(" \n\n\t\n")).isEmpty());
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
    EXPECT_TRUE(SpellChecker::importedDictionaries().isEmpty());
}

TEST_F(SpellCheckerTest, RemoveDictionaryRefusesReservedBase)
{
    EXPECT_FALSE(SpellChecker::removeDictionary("user"));
    EXPECT_FALSE(SpellChecker::removeDictionary("bundled"));
    // Bundled file names are not removable imported lists either.
    EXPECT_FALSE(SpellChecker::removeDictionary("en-US"));
}

TEST_F(SpellCheckerTest, InstallThenRemoveImportedList)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeWordList(dir.path(), "technical", {"zqzx", "scribamarkdown"});
    ASSERT_FALSE(path.isEmpty());

    const QString base = SpellChecker::installDictionary(path);
    ASSERT_EQ(base, "technical");
    EXPECT_TRUE(SpellChecker::importedDictionaries().contains("technical"));

    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_FALSE(checker.checkWord("zzzzzzzx"));   // not in any list
    EXPECT_TRUE(checker.checkWord("zqzx"));        // imported word passes
    EXPECT_TRUE(checker.checkWord("scribamarkdown"));

    EXPECT_TRUE(SpellChecker::removeDictionary("technical"));
    EXPECT_FALSE(SpellChecker::importedDictionaries().contains("technical"));
}

TEST_F(SpellCheckerTest, ImportedListIsLanguageIndependent)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeWordList(dir.path(), "technical", {"scribamarkdown"});
    ASSERT_FALSE(path.isEmpty());
    ASSERT_EQ(SpellChecker::installDictionary(path), "technical");

    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_GB"));
    EXPECT_TRUE(checker.checkWord("scribamarkdown"));
    SpellChecker::removeDictionary("technical");
}

TEST_F(SpellCheckerTest, ReinstallExistingDictionaryIsIdempotent)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeWordList(dir.path(), "myterms", {"zqzx"});
    ASSERT_FALSE(path.isEmpty());

    EXPECT_EQ(SpellChecker::installDictionary(path), "myterms");
    EXPECT_EQ(SpellChecker::installDictionary(path), "myterms");
    SpellChecker::removeDictionary("myterms");
}

TEST_F(SpellCheckerTest, InstallRequiresTxtSuffix)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeWordList(dir.path(), "de_DE", {"hallo"});
    ASSERT_FALSE(path.isEmpty());
    const QString renamed = dir.path() + "/de_DE.aff";
    ASSERT_TRUE(QFile::copy(path, renamed));

    EXPECT_TRUE(SpellChecker::installDictionary(renamed).isEmpty());
    EXPECT_TRUE(SpellChecker::importedDictionaries().isEmpty());
}

TEST_F(SpellCheckerTest, InstallRejectsEmptyList)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeWordList(dir.path(), "empty", QStringList());
    ASSERT_FALSE(path.isEmpty());

    EXPECT_TRUE(SpellChecker::installDictionary(path).isEmpty());
    EXPECT_FALSE(SpellChecker::importedDictionaries().contains("empty"));
}

TEST_F(SpellCheckerTest, InstallRejectsUnsafeBase)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    for (const char *base : {"user", "bundled", "en-US"}) {
        const QString path = writeWordList(dir.path(), QLatin1String(base), {"zqzx"});
        ASSERT_FALSE(path.isEmpty());
        EXPECT_TRUE(SpellChecker::installDictionary(path).isEmpty())
            << base << " must not be importable";
    }
    EXPECT_TRUE(SpellChecker::importedDictionaries().isEmpty());
}

TEST_F(SpellCheckerTest, DefaultLanguageForDialect)
{
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("American"), "en_US");
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("Canadian"), "en_US");
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("British"), "en_GB");
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("Australian"), "en_GB");
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("Indian"), "en_GB");
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("New Zealand"), "en_GB");
    EXPECT_EQ(SpellChecker::defaultLanguageForDialect("Klingon"), "en_US");   // unknown
}

TEST_F(SpellCheckerTest, DialectAllowances)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    checker.setDialect("Canadian");
    EXPECT_TRUE(checker.checkWord("centre"));    // Canadian allowance
    EXPECT_FALSE(checker.checkWord("favour"));   // en-GB-only, no allowance
    checker.setDialect("American");
    EXPECT_FALSE(checker.checkWord("centre"));

    SpellChecker nz;
    ASSERT_TRUE(nz.loadLanguage("en_GB"));
    nz.setDialect("New Zealand");
    EXPECT_TRUE(nz.checkWord("whanau"));         // Māori exemption
}

TEST_F(SpellCheckerTest, FollowDialectSelectsDictionary)
{
    QSettings().setValue(Preferences::GrammarDialect, "British");
    QSettings().setValue(Preferences::DictionaryLanguage, QString());

    SpellChecker checker;
    checker.setDialect("British");
    ASSERT_TRUE(checker.loadLanguage(SpellChecker::defaultLanguageForDialect("British")));
    EXPECT_EQ(checker.language(), "en_GB");
    EXPECT_TRUE(checker.checkWord("colour"));
    EXPECT_FALSE(checker.checkWord("color"));
}

TEST_F(SpellCheckerTest, ImportedListMarkedDictionaryIsNotLanguage)
{
    // An imported list is a union, not a selectable language.
    SpellChecker checker;
    EXPECT_FALSE(checker.loadLanguage("technical"));
}

TEST_F(SpellCheckerTest, UnchangedConfigurationSkipsRebuild)
{
    // Regression: every Preferences OK reapplied the engine configuration to
    // every tab, reloading the dictionaries from disk each time — a ~5s stall
    // that scaled with the number of open tabs even when nothing had changed.
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));   // first load rebuilds
    EXPECT_EQ(checker.configLoads(), 1);

    // Reapplying identical language and dialect is a no-op.
    EXPECT_TRUE(checker.loadLanguage("en_US"));
    checker.setDialect("American");               // the default, unchanged
    EXPECT_EQ(checker.configLoads(), 1);

    // A real dialect change rebuilds.
    checker.setDialect("British");
    EXPECT_EQ(checker.configLoads(), 2);

    // Reapplying the same dialect again is a no-op.
    checker.setDialect("British");
    EXPECT_EQ(checker.configLoads(), 2);

    // A user-dictionary change rebuilds; an identical re-add is a no-op.
    checker.addToUserDictionary("scribadictword");
    EXPECT_EQ(checker.configLoads(), 3);
    checker.addToUserDictionary("scribadictword");
    EXPECT_EQ(checker.configLoads(), 3);
}

TEST_F(SpellCheckerTest, CorpusOverrideReplacesGlobalUserWords)
{
    // With a corpus active in the default (override) mode the corpus word
    // sets replace the global user.dic/ignored.dic sets entirely.
    SpellChecker::writeUserDictionaryWords({"qqcglobwone"});
    SpellChecker::writeIgnoredWords({"qqcglobigna"});
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));

    EXPECT_TRUE(checker.checkWord("qqcglobwone"));    // global user word applies
    EXPECT_TRUE(checker.checkWord("qqcglobigna"));   // global ignored applies

    checker.setCorpusWords({"qqccorpushwo"});
    EXPECT_TRUE(checker.checkWord("qqccorpushwo"));
    EXPECT_FALSE(checker.checkWord("qqcglobwone"));   // override drops the globals
    EXPECT_FALSE(checker.checkWord("qqcglobigna"));
}

TEST_F(SpellCheckerTest, CorpusMergeUnionsGlobalAndCorpusWords)
{
    SpellChecker::writeUserDictionaryWords({"qqcglobwone"});
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));

    checker.setCorpusMerge(true);
    checker.setCorpusWords({"qqccorpushwo"});
    EXPECT_TRUE(checker.checkWord("qqcglobwone"));    // globals still apply
    EXPECT_TRUE(checker.checkWord("qqccorpushwo"));
}

TEST_F(SpellCheckerTest, CorpusOverrideWithEmptySetsKeepsGlobalWords)
{
    // Regression guard: a plain editor has no corpus words, so even the
    // default override mode must keep the global user dictionary.
    SpellChecker::writeUserDictionaryWords({"qqcglobwone"});
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    EXPECT_TRUE(checker.checkWord("qqcglobwone"));
}

TEST_F(SpellCheckerTest, CorpusWordsRoundTripAndAddRemove)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    checker.addCorpusWord(" qqccorpusalpha ");     // leading/trailing trimmed
    checker.addCorpusWord("qqccorpusbeta");
    checker.removeCorpusWord("qqccorpusbeta");
    checker.removeCorpusWord("missingWord");       // no-op
    EXPECT_EQ(checker.corpusWords(), QStringList({"qqccorpusalpha"}));
    EXPECT_TRUE(checker.checkWord("qqccorpusalpha"));
    EXPECT_FALSE(checker.checkWord("qqccorpusbeta"));
}

TEST_F(SpellCheckerTest, CorpusIgnoredAppliedAndRoundTrip)
{
    SpellChecker checker;
    ASSERT_TRUE(checker.loadLanguage("en_US"));
    checker.setCorpusIgnored({"qqcignoredzn"});
    EXPECT_EQ(checker.corpusIgnored(), QStringList({"qqcignoredzn"}));
    EXPECT_TRUE(checker.checkWord("qqcignoredzn"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
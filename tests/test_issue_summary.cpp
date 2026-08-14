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
#include <QAbstractTextDocumentLayout>
#include <QSettings>
#include <QTextDocument>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTest>
#include <QObject>
#include <atomic>
#include <memory>

#include "editor/Editor.h"
#include "prefs/Preferences.h"
#include "spell/GrammarChecker.h"
#include "spell/SpellChecker.h"
#include "spell/SpellHighlighter.h"
#include "validation/MarkdownChecker.h"
#include "TestConfig.h"

namespace {

// Returns a grammar issue at a deterministic offset. Subclassing the
// interface keeps the test independent of StoppardEngine.
class CountingGrammarChecker : public GrammarChecker
{
public:
    std::atomic<int> checkCount{0};

    QList<Issue> check(const QString &text) override
    {
        ++checkCount;
        QList<Issue> issues;
        if (text.contains(QStringLiteral("grammer error"))) {
            const int start = text.indexOf(QStringLiteral("grammer"));
            issues.append({start, 7, QStringLiteral("'grammer' is not a word")});
        }
        return issues;
    }
};

class IssueSummaryCountsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        QSettings().setValue(Preferences::SpellCheckEnabled, true);
        QSettings().setValue(Preferences::LinkCheckEnabled, true);
        QSettings().setValue(Preferences::MarkdownCheckEnabled, true);
        QSettings().setValue(Preferences::GrammarCheckEnabled, false);
        QSettings().setValue(Preferences::DictionaryLanguage, QStringLiteral("en_US"));
        QSettings().setValue(Preferences::GrammarDialect, QStringLiteral("American"));
        SpellChecker::availableLanguages(); // installs the bundled dicts into the test config dir
        m_tmp = std::make_unique<QTemporaryDir>();
        m_doc = new QTextDocument;
        m_hl = new SpellHighlighter(m_doc);
        m_checker = std::make_unique<SpellChecker>();
        m_checker->setDialect(QStringLiteral("American"));
        m_checker->loadLanguage(QStringLiteral("en_US"));
        m_hl->setChecker(m_checker.get());
        m_hl->setGrammarChecker(m_grammar);
        m_hl->setForceSyncChecks(true);
    }

    void TearDown() override
    {
        delete m_hl;
        delete m_doc;
        m_checker.reset();
        m_grammar.reset();
        m_tmp.reset();
        QSettings().clear();
    }

    std::shared_ptr<CountingGrammarChecker> m_grammar =
        std::make_shared<CountingGrammarChecker>();
    std::unique_ptr<SpellChecker> m_checker;
    std::unique_ptr<QTemporaryDir> m_tmp;
    QTextDocument *m_doc = nullptr;
    SpellHighlighter *m_hl = nullptr;
};

TEST_F(IssueSummaryCountsTest, CountsSumLiveCaches)
{
    // One typo, one broken link, one trailing-whitespace markdown issue,
    // grammar off (0).
    m_doc->setPlainText(QStringLiteral(
        "helo world\n"
        "[missing](no-such-file.md)\n"
        "a line with trailing spaces   \n"));
    m_doc->markContentsDirty(0, m_doc->characterCount());
    m_doc->documentLayout()->documentSize(); // force full layout so highlightBlock runs everywhere
    m_hl->setLinkCheckingEnabled(true);
    m_hl->setMarkdownCheckingEnabled(true);
    m_hl->setMarkdownChecks(MarkdownChecker::defaultChecks());
    m_hl->setCurrentFile(m_tmp->filePath(QStringLiteral("__test__.md")));
    m_hl->refresh();

    const auto counts = m_hl->counts();
    EXPECT_EQ(counts.spelling, 1);
    EXPECT_EQ(counts.links, 1);
    EXPECT_EQ(counts.markdown, 1);
    EXPECT_EQ(counts.grammar, 0);
}

TEST_F(IssueSummaryCountsTest, GrammarCountPopulatesAfterLint)
{
    m_doc->setPlainText(QStringLiteral("this has a grammer error"));
    m_hl->setGrammarCheckingEnabled(true);
    QTest::qWait(700); // debounced lint timer (400 ms) + queued worker round-trip

    const auto counts = m_hl->counts();
    EXPECT_EQ(m_grammar->checkCount.load(), 1);
    EXPECT_EQ(counts.grammar, 1);
    EXPECT_EQ(counts.spelling, 0);
}

TEST_F(IssueSummaryCountsTest, GrammarLintCompletionEmitsSpellHitsChanged)
{
    int emissions = 0;
    QMetaObject::Connection conn = QObject::connect(
        m_hl, &SpellHighlighter::spellHitsChanged, [&emissions]() { ++emissions; });
    m_doc->setPlainText(QStringLiteral("this has a grammer error"));
    m_hl->setGrammarCheckingEnabled(true);
    QTest::qWait(700);
    QObject::disconnect(conn);

    EXPECT_GE(emissions, 1); // the grammar result must repaint consumers
    EXPECT_EQ(m_hl->counts().grammar, 1);
}

TEST_F(IssueSummaryCountsTest, EngineGettersReportToggleState)
{
    EXPECT_TRUE(m_hl->spellCheckingEnabled());
    EXPECT_TRUE(m_hl->linkCheckingEnabled());
    m_hl->setGrammarCheckingEnabled(true);
    EXPECT_TRUE(m_hl->grammarCheckingEnabled());
    m_hl->setMarkdownCheckingEnabled(true);
    EXPECT_TRUE(m_hl->markdownCheckingEnabled());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
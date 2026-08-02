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
#include "HarperEngine.h"
#include <QString>
#include <memory>

namespace {

class GrammarCheckerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_engine = std::make_unique<HarperEngine>();
    }

    std::unique_ptr<HarperEngine> m_engine;
};

TEST_F(GrammarCheckerTest, EngineInitialises)
{
    EXPECT_TRUE(m_engine->isAvailable());
}

TEST_F(GrammarCheckerTest, DetectsGrammarError)
{
    const auto issues = m_engine->check(QStringLiteral("I has a cat."));
    ASSERT_FALSE(issues.isEmpty());
    bool hasMessage = false;
    for (const auto &issue : issues)
        hasMessage = hasMessage || !issue.message.isEmpty();
    EXPECT_TRUE(hasMessage) << "expected at least one issue with a message";
}

TEST_F(GrammarCheckerTest, DoesNotFlagSpelling)
{
    // SpellCheck is disabled inside harper — Hunspell owns spelling.
    const auto issues = m_engine->check(QStringLiteral("This is helo wrking text."));
    for (const auto &issue : issues)
        EXPECT_FALSE(issue.message.contains(QStringLiteral("Did you mean")))
            << issue.message.toStdString();
}

TEST_F(GrammarCheckerTest, RangesWithinBounds)
{
    const QString text = QStringLiteral("I has a cat. Héllo wörld.");
    const auto issues = m_engine->check(text);
    for (const auto &issue : issues) {
        EXPECT_GE(issue.start, 0);
        EXPECT_LE(issue.start + issue.length, text.size());
    }
}

TEST_F(GrammarCheckerTest, CleanTextHasNoIssues)
{
    const auto issues = m_engine->check(QStringLiteral("The cat sat on the mat."));
    EXPECT_TRUE(issues.isEmpty());
}

TEST_F(GrammarCheckerTest, DialectSwitchKeepsEngineUsable)
{
    m_engine->setDialect(QStringLiteral("British"));
    EXPECT_TRUE(m_engine->isAvailable());
    const auto issues = m_engine->check(QStringLiteral("I has a cat."));
    EXPECT_FALSE(issues.isEmpty());
}

TEST_F(GrammarCheckerTest, DialectAffectsRegionalIdioms)
{
    // "in the cards" is an American idiom; British English prefers
    // "on the cards", so harper flags it only under a British dialect.
    const QString text = QStringLiteral("The plan is in the cards.");
    m_engine->setDialect(QStringLiteral("American"));
    const auto american = m_engine->check(text);
    m_engine->setDialect(QStringLiteral("British"));
    const auto british = m_engine->check(text);
    EXPECT_FALSE(british.isEmpty());
    EXPECT_TRUE(american.isEmpty());
}

TEST_F(GrammarCheckerTest, UnknownDialectFallsBackToAmerican)
{
    m_engine->setDialect(QStringLiteral("Klingon"));
    EXPECT_TRUE(m_engine->isAvailable());
    const auto issues = m_engine->check(QStringLiteral("The cat sat on the mat."));
    EXPECT_TRUE(issues.isEmpty());
}

} // namespace

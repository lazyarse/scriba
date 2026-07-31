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

} // namespace

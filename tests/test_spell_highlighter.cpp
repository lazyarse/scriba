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

#include "SpellChecker.h"
#include "SpellHighlighter.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextLayout>
#include <atomic>
#include "TestConfig.h"

namespace {

QStringList wordsOf(const QString &line)
{
    QStringList out;
    const auto hits = SpellHighlighter::scanWords(line);
    for (const auto &hit : hits)
        out << hit.text;
    return out;
}

class SpellHighlighterScanWordsTest : public ::testing::Test
{
};

TEST_F(SpellHighlighterScanWordsTest, SimpleLine)
{
    EXPECT_EQ(wordsOf("hello world"), (QStringList{"hello", "world"}));
}

TEST_F(SpellHighlighterScanWordsTest, InlineCodeSkipped)
{
    EXPECT_EQ(wordsOf("foo `bar` baz"), (QStringList{"foo", "baz"}));
}

TEST_F(SpellHighlighterScanWordsTest, UrlSkipped)
{
    EXPECT_EQ(wordsOf("Visit https://example.com now"), (QStringList{"Visit", "now"}));
}

TEST_F(SpellHighlighterScanWordsTest, LinkLabelKeptTargetSkipped)
{
    EXPECT_EQ(wordsOf("[helo](https://x.com) text"), (QStringList{"helo", "text"}));
}

TEST_F(SpellHighlighterScanWordsTest, HtmlTagsSkipped)
{
    EXPECT_EQ(wordsOf("<div class='x'>text</div>"), (QStringList{"text"}));
}

TEST_F(SpellHighlighterScanWordsTest, EmojiShortcodeSkipped)
{
    EXPECT_EQ(wordsOf(":smile: hello"), (QStringList{"hello"}));
}

TEST_F(SpellHighlighterScanWordsTest, HyphenSplitsWords)
{
    EXPECT_EQ(wordsOf("well-known"), (QStringList{"well", "known"}));
}

TEST_F(SpellHighlighterScanWordsTest, ApostropheKept)
{
    const auto hits = SpellHighlighter::scanWords("don't");
    ASSERT_EQ(hits.size(), 1);
    EXPECT_EQ(hits[0].text, "don't");
}

TEST_F(SpellHighlighterScanWordsTest, HeadingMarkerIgnored)
{
    EXPECT_EQ(wordsOf("# Heading"), (QStringList{"Heading"}));
}

TEST_F(SpellHighlighterScanWordsTest, OffsetsAreLineRelative)
{
    const auto hits = SpellHighlighter::scanWords("[helo](https://x.com) text");
    ASSERT_EQ(hits.size(), 2);
    EXPECT_EQ(hits[0].text, "helo");
    EXPECT_EQ(hits[0].start, 1);
    EXPECT_EQ(hits[0].length, 4);
    EXPECT_EQ(hits[1].text, "text");
    EXPECT_EQ(hits[1].start, 22);
    EXPECT_EQ(hits[1].length, 4);
}

class SpellHighlighterDocumentTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        QDir().mkpath(SpellChecker::configDictDir());
        QFile::remove(SpellChecker::configDictDir() + "/user.dic");
        ASSERT_TRUE(m_checker.loadLanguage("en_US"));
        m_doc = new QTextDocument;
        m_highlighter = new SpellHighlighter(m_doc);
        m_highlighter->setChecker(&m_checker);
    }

    void TearDown() override
    {
        delete m_highlighter;
        delete m_doc;
        QFile::remove(SpellChecker::configDictDir() + "/user.dic");
        QSettings().clear();
    }

    void setText(const QString &text)
    {
        m_doc->setPlainText(text);
        forceRelayout();
    }

    void forceRelayout()
    {
        m_doc->markContentsDirty(0, m_doc->characterCount());
        m_doc->documentLayout()->documentSize();
    }

    bool hasSpellUnderline(int blockNumber, int pos) const
    {
        for (const auto &hit : m_highlighter->spellHitsInBlock(blockNumber)) {
            if (pos >= hit.start && pos < hit.start + hit.length)
                return true;
        }
        return false;
    }

    SpellChecker m_checker;
    QTextDocument *m_doc = nullptr;
    SpellHighlighter *m_highlighter = nullptr;
};

TEST_F(SpellHighlighterDocumentTest, MisspelledWordUnderlined)
{
    setText("helo");
    EXPECT_TRUE(hasSpellUnderline(0, 0));
    EXPECT_TRUE(hasSpellUnderline(0, 3));
}

TEST_F(SpellHighlighterDocumentTest, CorrectWordNotUnderlined)
{
    setText("hello");
    EXPECT_FALSE(hasSpellUnderline(0, 0));
}

TEST_F(SpellHighlighterDocumentTest, FencedCodeBlockNotUnderlined)
{
    setText("```\nhelo world\n```\nhelo again");
    EXPECT_FALSE(hasSpellUnderline(0, 0));
    EXPECT_FALSE(hasSpellUnderline(1, 0));
    EXPECT_FALSE(hasSpellUnderline(2, 0));
    EXPECT_TRUE(hasSpellUnderline(3, 0));
}

TEST_F(SpellHighlighterDocumentTest, InlineCodeNotUnderlined)
{
    setText("helo `helo` helo");
    EXPECT_TRUE(hasSpellUnderline(0, 0));
    EXPECT_FALSE(hasSpellUnderline(0, 7));
    EXPECT_TRUE(hasSpellUnderline(0, 12));
}

TEST_F(SpellHighlighterDocumentTest, FrontMatterNotUnderlined)
{
    setText("---\ntitle: helo\n---\nhelo body");
    EXPECT_FALSE(hasSpellUnderline(0, 0));
    EXPECT_FALSE(hasSpellUnderline(1, 7));
    EXPECT_FALSE(hasSpellUnderline(2, 0));
    EXPECT_TRUE(hasSpellUnderline(3, 0));
}

TEST_F(SpellHighlighterDocumentTest, UserDictionaryRemovesUnderline)
{
    setText("scribamarkdown");
    EXPECT_TRUE(hasSpellUnderline(0, 0));

    m_checker.addToUserDictionary("scribamarkdown");
    m_highlighter->refresh();
    forceRelayout();
    EXPECT_FALSE(hasSpellUnderline(0, 0));
}

class SpellHighlighterHitTest : public SpellHighlighterDocumentTest
{
protected:
    bool covers(int blockNumber, int pos) const
    {
        for (const auto &hit : m_highlighter->spellHitsInBlock(blockNumber)) {
            if (pos >= hit.start && pos < hit.start + hit.length)
                return true;
        }
        return false;
    }
};

TEST_F(SpellHighlighterHitTest, MisspelledWordReported)
{
    setText("helo world");
    EXPECT_TRUE(covers(0, 0));
    EXPECT_TRUE(covers(0, 3));
    EXPECT_FALSE(covers(0, 5));
}

TEST_F(SpellHighlighterHitTest, CorrectWordNotReported)
{
    setText("hello");
    EXPECT_FALSE(covers(0, 0));
}

TEST_F(SpellHighlighterHitTest, FencedCodeBlockNotReported)
{
    setText("```\nhelo world\n```\nhelo again");
    EXPECT_FALSE(covers(0, 0));
    EXPECT_FALSE(covers(1, 0));
    EXPECT_FALSE(covers(2, 0));
    EXPECT_TRUE(covers(3, 0));
}

TEST_F(SpellHighlighterHitTest, InlineCodeNotReported)
{
    setText("helo `helo` helo");
    EXPECT_TRUE(covers(0, 0));
    EXPECT_FALSE(covers(0, 7));
    EXPECT_TRUE(covers(0, 12));
}

TEST_F(SpellHighlighterHitTest, UserDictionaryClearsHits)
{
    setText("helo");
    EXPECT_TRUE(covers(0, 0));

    m_checker.addToUserDictionary("helo");
    m_highlighter->refresh();
    forceRelayout();
    EXPECT_FALSE(covers(0, 0));
}

// Grammar-check scheduling regression: a format-only change (which is what
// rehighlight() performs) must not re-arm the debounced lint timer. Before
// the fix the lint's own rehighlight() fed back through contentsChanged and
// re-armed the timer endlessly, re-linting the whole document every 400 ms
// and re-triggering preview updates.
//
// Drives a real QTextEdit (like the app does): typing fires
// QTextDocument::contentsChange with real removed/added counts, while
// highlight-only work fires it with (0,0) — which the scheduler must ignore.
class CountingGrammarChecker : public GrammarChecker
{
public:
    std::atomic<int> checkCount{0};

    QList<Issue> check(const QString &) override
    {
        ++checkCount;
        return {};
    }
};

class SpellHighlighterLintTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        m_edit = new QTextEdit;
        m_edit->setPlainText("hello world");
        m_highlighter = new SpellHighlighter(m_edit->document());
        m_highlighter->setGrammarChecker(&m_grammar);
    }

    void TearDown() override
    {
        // Destroying the highlighter stops and joins the lint worker thread,
        // so the checker is never used afterwards.
        delete m_highlighter;
        delete m_edit;
        QSettings().clear();
    }

    CountingGrammarChecker m_grammar;
    QTextEdit *m_edit = nullptr;
    SpellHighlighter *m_highlighter = nullptr;
};

TEST_F(SpellHighlighterLintTest, FormatOnlyChangeDoesNotReTriggerLint)
{
    m_highlighter->setGrammarCheckingEnabled(true);
    QTest::qWait(700); // initial lint completes
    ASSERT_EQ(m_grammar.checkCount.load(), 1);

    // Format-only change (e.g. spell-checking toggled, block states updated,
    // or the lint result itself being applied).
    m_highlighter->rehighlight();
    QTest::qWait(700);
    EXPECT_EQ(m_grammar.checkCount.load(), 1);
}

TEST_F(SpellHighlighterLintTest, RealEditStillTriggersLint)
{
    m_highlighter->setGrammarCheckingEnabled(true);
    QTest::qWait(700); // initial lint completes
    ASSERT_EQ(m_grammar.checkCount.load(), 1);

    QTest::keyClicks(m_edit, " more");
    QTest::qWait(700);
    EXPECT_EQ(m_grammar.checkCount.load(), 2);
}

// Spell checking is deferred so that words currently being typed are not
// flagged as typos: an edit makes its block "stale" (underlines cleared)
// until the check runs again, which happens immediately when a separator
// (space, punctuation, newline) is typed, or after a short debounce when the
// user pauses mid-word. These tests drive a real QTextEdit like the app
// does — typing is what fires QTextDocument::contentsChange.
class SpellHighlighterSpellDebounceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().clear();
        QDir().mkpath(SpellChecker::configDictDir());
        QFile::remove(SpellChecker::configDictDir() + "/user.dic");
        ASSERT_TRUE(m_checker.loadLanguage("en_US"));
        m_edit = new QTextEdit;
        m_edit->setPlainText("hello world");
        m_highlighter = new SpellHighlighter(m_edit->document());
        m_highlighter->setChecker(&m_checker);
        m_highlighter->refresh(); // baseline check of "hello world"
    }

    void TearDown() override
    {
        delete m_highlighter;
        delete m_edit;
        QFile::remove(SpellChecker::configDictDir() + "/user.dic");
        QSettings().clear();
    }

    bool covers(int blockNumber, int pos) const
    {
        for (const auto &hit : m_highlighter->spellHitsInBlock(blockNumber))
            if (pos >= hit.start && pos < hit.start + hit.length)
                return true;
        return false;
    }

    SpellChecker m_checker;
    QTextEdit *m_edit = nullptr;
    SpellHighlighter *m_highlighter = nullptr;
};

TEST_F(SpellHighlighterSpellDebounceTest, WordBeingTypedNotFlagged)
{
    m_edit->moveCursor(QTextCursor::End);
    // Appends "helo" to "hello world" → "hello worldhelo". No separator is
    // typed, so the misspelled "worldhelo" must not be flagged mid-typing.
    QTest::keyClicks(m_edit, "helo");
    EXPECT_FALSE(covers(0, 6));
    EXPECT_FALSE(covers(0, 14));
}

TEST_F(SpellHighlighterSpellDebounceTest, SeparatorTriggersImmediateCheck)
{
    m_edit->moveCursor(QTextCursor::End);
    QTest::keyClicks(m_edit, "helo");
    QTest::keyClick(m_edit, Qt::Key_Space);
    EXPECT_TRUE(covers(0, 6));  // "worldhelo" is complete now → flagged
    EXPECT_FALSE(covers(0, 0)); // "hello" is correct
}

TEST_F(SpellHighlighterSpellDebounceTest, PauseTriggersDebouncedCheck)
{
    m_edit->moveCursor(QTextCursor::End);
    QTest::keyClicks(m_edit, "helo");
    EXPECT_FALSE(covers(0, 6));

    QTest::qWait(700); // debounce (400 ms) fires
    EXPECT_TRUE(covers(0, 6));
}

TEST_F(SpellHighlighterSpellDebounceTest, EditingFlaggedWordAgainClearsUnderline)
{
    m_edit->moveCursor(QTextCursor::End);
    QTest::keyClicks(m_edit, "helo");
    QTest::keyClick(m_edit, Qt::Key_Space);
    ASSERT_TRUE(covers(0, 6));

    // Insert a letter in the middle of the flagged word: the edit is now
    // mid-word again, so the underline clears until the next check.
    m_edit->moveCursor(QTextCursor::Left);
    m_edit->moveCursor(QTextCursor::Left);
    QTest::keyClick(m_edit, Qt::Key_X);
    EXPECT_FALSE(covers(0, 6));
}

TEST_F(SpellHighlighterSpellDebounceTest, CheckCompletionEmitsSignal)
{
    QSignalSpy spy(m_highlighter, &SpellHighlighter::spellHitsChanged);

    // Mid-word typing defers the check (and the signal)...
    m_edit->moveCursor(QTextCursor::End);
    QTest::keyClicks(m_edit, "helo");
    EXPECT_EQ(spy.count(), 0);

    // ...a separator checks immediately...
    QTest::keyClick(m_edit, Qt::Key_Space);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(covers(0, 6));

    // ...and a pause lets the debounced check run and signal too.
    QTest::keyClicks(m_edit, "zzz");
    QTest::qWait(700);
    EXPECT_GT(spy.count(), 1);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

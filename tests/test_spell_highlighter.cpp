#include <gtest/gtest.h>

#include "SpellChecker.h"
#include "SpellHighlighter.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextLayout>
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

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

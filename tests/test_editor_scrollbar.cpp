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

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTextBlock>
#include <QTextDocument>
#include <QTemporaryDir>
#include <QTest>
#include <functional>

#include "editor/Editor.h"
#include "editor/EditorScrollBar.h"
#include "prefs/Preferences.h"
#include "spell/SpellChecker.h"
#include "TestConfig.h"

namespace {

constexpr const char *kMisspelled = "helo";

EditorScrollBar *scrollbarOf(Editor *editor)
{
    return static_cast<EditorScrollBar *>(editor->verticalScrollBar());
}

// Pumps the event loop (grammar lint runs on a background worker) until `cond`
// is true or `ms` elapses.
void pumpUntil(std::function<bool()> cond, int ms = 5000)
{
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms) {
        QApplication::processEvents();
        QTest::qWait(10);
    }
}

// Builds the slider (thumb) rect the same way EditorScrollBar::paintEvent does.
QRect thumbRect(QScrollBar *sb)
{
    QStyleOptionSlider opt;
    opt.initFrom(sb);
    opt.orientation = Qt::Vertical;
    opt.minimum = sb->minimum();
    opt.maximum = sb->maximum();
    opt.sliderPosition = sb->sliderPosition();
    opt.sliderValue = sb->value();
    opt.pageStep = sb->pageStep();
    opt.singleStep = sb->singleStep();
    opt.upsideDown = false;
    return sb->style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                       QStyle::SC_ScrollBarSlider, sb);
}

// Predicates matching the four underline colors (d6 40 50 / 00 cc 66 /
// f0 90 00 / 3b 82 f6), tolerant of antialiasing.
bool isRed(const QColor &c)    { return c.red() > 150 && c.green() < 100 && c.blue() < 110; }
bool isAmber(const QColor &c)  { return c.red() > 200 && c.green() > 100 && c.green() < 190 && c.blue() < 60; }
bool isBlue(const QColor &c)   { return c.red() < 100 && c.green() > 100 && c.green() < 180 && c.blue() > 200; }

bool colorNear(const QImage &img, int y, std::function<bool(const QColor &)> pred)
{
    for (int x = 0; x < img.width(); ++x) {
        for (int dy = y; dy <= y + 2 && dy < img.height(); ++dy) {
            if (dy < 0)
                continue;
            if (pred(img.pixelColor(x, dy)))
                return true;
        }
    }
    return false;
}

class EditorScrollbarTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().setValue(Preferences::SpellCheckEnabled, true);
        QSettings().setValue(Preferences::GrammarCheckEnabled, false);
        QSettings().setValue(Preferences::MarkdownCheckEnabled, false);
        QSettings().setValue(Preferences::ErrorScrollbarEnabled, true);
        QSettings().setValue(Preferences::DictionaryLanguage, QStringLiteral("en_US"));
        SpellChecker::availableLanguages(); // installs the bundled dicts into the test config dir

        m_editor = new Editor();
        m_editor->resize(800, 600);
        m_editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete m_editor;
        m_editor = nullptr;
        QSettings().clear();
    }

    void setDoc(const QString &text)
    {
        m_editor->setPlainText(text);
        QTextDocument *doc = m_editor->document();
        doc->markContentsDirty(0, doc->characterCount());
        doc->documentLayout()->documentSize(); // force full layout so highlightBlock runs everywhere
        QApplication::processEvents();
        m_editor->spellHighlighter()->refresh();
        QApplication::processEvents();
    }

    Editor *m_editor = nullptr;
};

} // namespace

TEST_F(EditorScrollbarTest, DocumentFractionClamps)
{
    EXPECT_EQ(EditorScrollBar::documentFraction(0, 100), 0.0);
    EXPECT_EQ(EditorScrollBar::documentFraction(50, 100), 0.5);
    EXPECT_EQ(EditorScrollBar::documentFraction(200, 100), 1.0);
    EXPECT_EQ(EditorScrollBar::documentFraction(10, 0), 0.0);
}

TEST_F(EditorScrollbarTest, CleanDocumentProducesNoEntries)
{
    setDoc(QStringLiteral("a perfectly clean line of text\nanother fine and correct line"));
    auto *sb = scrollbarOf(m_editor);
    sb->grab(); // forces the lazy index rebuild via paintEvent
    EXPECT_TRUE(sb->entries().isEmpty());
}

TEST_F(EditorScrollbarTest, EntriesIndexAllFourErrorTypes)
{
    QSettings().setValue(Preferences::GrammarCheckEnabled, true);
    QSettings().setValue(Preferences::MarkdownCheckEnabled, true);
    m_editor->recheckSpelling();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile exists(dir.filePath(QStringLiteral("exists.md")));
    ASSERT_TRUE(exists.open(QIODevice::WriteOnly));
    m_editor->setCurrentFile(dir.filePath(QStringLiteral("doc.md")));

    setDoc(QStringLiteral("I has a cat.\n"            // block 0: grammar
                          "helo world here\n"          // block 1: spelling
                          "see [text](missing.md)\n"   // block 2: broken link
                          "# Title\n"                  // block 3: clean heading
                          "# Title"));                 // block 4: duplicate heading

    // The grammar lint is whole-document on a background thread; wait for it.
    pumpUntil([this] { return !m_editor->spellHighlighter()->grammarIssuesInBlock(0).isEmpty(); });

    auto *sb = scrollbarOf(m_editor);
    sb->grab(); // forces the lazy index rebuild via paintEvent
    const auto entries = sb->entries();
    auto flagsOf = [&entries](int n) -> quint8 {
        for (const auto &e : entries)
            if (e.blockNumber == n)
                return e.flags;
        return 0;
    };

    EXPECT_TRUE(flagsOf(0) & EditorScrollBar::Flag::Grammar);
    EXPECT_TRUE(flagsOf(1) & EditorScrollBar::Flag::Spell);
    EXPECT_TRUE(flagsOf(2) & EditorScrollBar::Flag::Link);
    EXPECT_TRUE(flagsOf(4) & EditorScrollBar::Flag::Markdown);
    EXPECT_EQ(flagsOf(3), quint8(0)) << "the first heading must stay clean";
}

TEST_F(EditorScrollbarTest, MarkerPaintedOutsideThumbBand)
{
    QStringList lines;
    const int total = 200;
    const int bad = 150;
    for (int i = 0; i < total; ++i)
        lines << (i == bad ? QStringLiteral("helo world and more words")
                           : QStringLiteral("this line is perfectly fine and correct"));
    setDoc(lines.join(QLatin1Char('\n')));

    auto *sb = scrollbarOf(m_editor);
    ASSERT_GT(sb->maximum(), 0) << "document must be scrollable so the scrollbar is visible";

    const QTextBlock block = m_editor->document()->findBlockByNumber(bad);
    const qreal center = m_editor->document()->documentLayout()->blockBoundingRect(block).center().y();
    const qreal docH = m_editor->document()->documentLayout()->documentSize().height();
    const int expectedY = qRound(EditorScrollBar::documentFraction(center, docH) * sb->height());

    const QRect thumb = thumbRect(sb);
    ASSERT_FALSE(expectedY >= thumb.top() && expectedY <= thumb.bottom())
        << "line " << bad << " of " << total << " must sit below the thumb";

    const QImage img = sb->grab().toImage();
    ASSERT_FALSE(img.isNull());
    EXPECT_TRUE(colorNear(img, expectedY, isRed))
        << "a red spell marker must be painted at the block's track position";
}

TEST_F(EditorScrollbarTest, MarkerVisibleUnderThumbBand)
{
    QStringList lines;
    const int total = 200;
    const int bad = 15; // near the top of the document → lands under the thumb
    for (int i = 0; i < total; ++i)
        lines << (i == bad ? QStringLiteral("helo world and more words")
                           : QStringLiteral("this line is perfectly fine and correct"));
    setDoc(lines.join(QLatin1Char('\n')));

    auto *sb = scrollbarOf(m_editor);
    ASSERT_GT(sb->maximum(), 0);

    const QTextBlock block = m_editor->document()->findBlockByNumber(bad);
    const qreal center = m_editor->document()->documentLayout()->blockBoundingRect(block).center().y();
    const qreal docH = m_editor->document()->documentLayout()->documentSize().height();
    const int expectedY = qRound(EditorScrollBar::documentFraction(center, docH) * sb->height());

    const QRect thumb = thumbRect(sb);
    ASSERT_TRUE(expectedY >= thumb.top() && expectedY <= thumb.bottom())
        << "line " << bad << " of " << total << " must sit under the thumb";

    const QImage img = sb->grab().toImage();
    ASSERT_FALSE(img.isNull());
    EXPECT_TRUE(colorNear(img, expectedY, isRed))
        << "the marker must be painted on top of the thumb";
}

TEST_F(EditorScrollbarTest, ToggleOffHidesAllMarkers)
{
    QSettings().setValue(Preferences::ErrorScrollbarEnabled, false);
    m_editor->recheckSpelling();

    QStringList lines;
    for (int i = 0; i < 200; ++i)
        lines << (i == 150 ? QStringLiteral("helo world and more words")
                           : QStringLiteral("this line is perfectly fine and correct"));
    setDoc(lines.join(QLatin1Char('\n')));

    auto *sb = scrollbarOf(m_editor);
    ASSERT_GT(sb->maximum(), 0);
    const QImage img = sb->grab().toImage();
    ASSERT_FALSE(img.isNull());
    for (int y = 0; y < img.height(); ++y) {
        EXPECT_FALSE(colorNear(img, y, isRed));
    }
}

TEST_F(EditorScrollbarTest, BrokenLinkMarkerUsesAmber)
{
    QSettings().setValue(Preferences::SpellCheckEnabled, false);
    m_editor->recheckSpelling();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile exists(dir.filePath(QStringLiteral("exists.md")));
    ASSERT_TRUE(exists.open(QIODevice::WriteOnly));
    m_editor->setCurrentFile(dir.filePath(QStringLiteral("doc.md")));

    QStringList lines;
    const int total = 200;
    const int bad = 150;
    for (int i = 0; i < total; ++i)
        lines << (i == bad ? QStringLiteral("see [text](missing.md) and ok")
                           : QStringLiteral("this line is perfectly fine and correct"));
    setDoc(lines.join(QLatin1Char('\n')));

    auto *hl = m_editor->spellHighlighter();
    ASSERT_FALSE(hl->linkIssuesInBlock(bad).isEmpty());

    auto *sb = scrollbarOf(m_editor);
    const QTextBlock block = m_editor->document()->findBlockByNumber(bad);
    const qreal center = m_editor->document()->documentLayout()->blockBoundingRect(block).center().y();
    const qreal docH = m_editor->document()->documentLayout()->documentSize().height();
    const int expectedY = qRound(EditorScrollBar::documentFraction(center, docH) * sb->height());

    const QImage img = sb->grab().toImage();
    EXPECT_TRUE(colorNear(img, expectedY, isAmber))
        << "a broken link must paint an amber marker";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
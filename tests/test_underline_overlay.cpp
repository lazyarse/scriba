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
#include <QColor>
#include <QFile>
#include <QFontMetrics>
#include <QImage>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTemporaryDir>

#include "Editor.h"
#include "Preferences.h"
#include "SpellChecker.h"
#include "SpellHighlighter.h"
#include "TestConfig.h"

namespace {

constexpr const char *kMisspelled = "helo";
constexpr const char *kOverlayName = "underline-overlay";

QString docWithMisspellingOnLine(int lineOf, int totalLines)
{
    QStringList lines;
    for (int i = 0; i < totalLines; ++i) {
        lines << (i == lineOf
                      ? QStringLiteral("helo world and more words here")
                      : QStringLiteral("this line is perfectly fine and correct"));
    }
    return lines.join(QLatin1Char('\n'));
}

class UnderlineOverlayTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSettings().setValue(Preferences::SpellCheckEnabled, true);
        QSettings().setValue(Preferences::GrammarCheckEnabled, false);
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

    void setDocAndHighlight(int misspelledLine, int totalLines)
    {
        m_editor->setPlainText(docWithMisspellingOnLine(misspelledLine, totalLines));
        QTextDocument *doc = m_editor->document();
        doc->markContentsDirty(0, doc->characterCount());
        doc->documentLayout()->documentSize(); // force full layout so highlightBlock runs everywhere
        QApplication::processEvents();

        auto *hl = m_editor->findChild<SpellHighlighter *>();
        ASSERT_NE(hl, nullptr);
        EXPECT_FALSE(hl->spellHitsInBlock(misspelledLine).isEmpty())
            << "misspelled word must be flagged before scrolling";
    }

    Editor *m_editor = nullptr;
};

} // namespace

TEST_F(UnderlineOverlayTest, OverlayStaysAnchoredToViewportAfterScroll)
{
    const int misspelledLine = 4;
    setDocAndHighlight(misspelledLine, 60);

    QWidget *overlay = m_editor->findChild<QWidget *>(QLatin1String(kOverlayName));
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(overlay->pos(), QPoint(0, 0));

    QScrollBar *sb = m_editor->verticalScrollBar();
    ASSERT_GT(sb->maximum(), 0) << "document must be scrollable";
    const int delta = QFontMetrics(m_editor->font()).height() * 3;
    sb->setValue(qMin(sb->value() + delta, sb->maximum()));
    QApplication::processEvents();

    EXPECT_EQ(overlay->pos(), QPoint(0, 0))
        << "QWidget::scroll() moves viewport children; the underline overlay must be re-anchored";
    EXPECT_EQ(overlay->size(), m_editor->viewport()->size());
}

TEST_F(UnderlineOverlayTest, UnderlineStaysUnderMisspelledWordAfterScroll)
{
    const int misspelledLine = 4;
    setDocAndHighlight(misspelledLine, 60);

    QWidget *overlay = m_editor->findChild<QWidget *>(QLatin1String(kOverlayName));
    ASSERT_NE(overlay, nullptr);

    QTextBlock block = m_editor->document()->findBlockByNumber(misspelledLine);
    const int wordStart = block.text().indexOf(QLatin1String(kMisspelled));
    ASSERT_GE(wordStart, 0);
    QTextCursor left(m_editor->document());
    left.setPosition(block.position() + wordStart);
    QTextCursor right(m_editor->document());
    right.setPosition(block.position() + wordStart + int(std::strlen(kMisspelled)));

    QScrollBar *sb = m_editor->verticalScrollBar();
    const int delta = QFontMetrics(m_editor->font()).height() * 3;
    sb->setValue(qMin(sb->value() + delta, sb->maximum()));
    QApplication::processEvents();

    // The word's current on-screen row (viewport coordinates, after scrolling).
    const QRect leftRect = m_editor->cursorRect(left);
    const QRect rightRect = m_editor->cursorRect(right);
    const QFontMetrics fm(m_editor->font());
    const int underlineY = leftRect.top() + fm.ascent() + fm.underlinePos() + Editor::kUnderlineDropPx;

    // The overlay's local space must coincide with the viewport's; account for
    // any drift via overlay->pos() so the check tracks the real on-screen word.
    const QPoint expected(leftRect.left() - overlay->pos().x(),
                          underlineY - overlay->pos().y());
    const int wordWidth = rightRect.left() - leftRect.left();

    const QImage img = overlay->grab().toImage();
    ASSERT_FALSE(img.isNull());

    bool found = false;
    for (int x = expected.x(); x < expected.x() + wordWidth && !found; ++x) {
        for (int y = expected.y() - 1; y <= expected.y() + 2 && !found; ++y) {
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                continue;
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 180 && c.green() < 120 && c.blue() < 130)
                found = true;
        }
    }
    EXPECT_TRUE(found)
        << "red underline must be painted at the word's on-screen row after scrolling";
}

TEST_F(UnderlineOverlayTest, AmberUnderlinePaintedForBrokenLink)
{
    // Spell check off, broken-link check on (default): only amber underlines
    // can be painted for the probe text.
    QSettings().setValue(Preferences::SpellCheckEnabled, false);
    m_editor->recheckSpelling();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile exists(dir.filePath(QStringLiteral("exists.md")));
    ASSERT_TRUE(exists.open(QIODevice::WriteOnly));
    m_editor->setCurrentFile(dir.filePath(QStringLiteral("doc.md")));
    m_editor->setPlainText(QStringLiteral("see [text](missing-file.md) and [text](exists.md)"));
    QTextDocument *doc = m_editor->document();
    doc->markContentsDirty(0, doc->characterCount());
    doc->documentLayout()->documentSize(); // force full layout so highlightBlock runs everywhere
    QApplication::processEvents();

    auto *hl = m_editor->findChild<SpellHighlighter *>();
    ASSERT_NE(hl, nullptr);
    const auto hits = hl->linkIssuesInBlock(0);
    ASSERT_EQ(1, hits.size()) << "the missing file target must be flagged, the existing one must not";
    ASSERT_EQ(11, hits[0].start);
    ASSERT_EQ(15, hits[0].length);

    QWidget *overlay = m_editor->findChild<QWidget *>(QLatin1String(kOverlayName));
    ASSERT_NE(overlay, nullptr);

    QTextBlock block = doc->findBlockByNumber(0);
    QTextCursor left(doc);
    left.setPosition(block.position() + hits[0].start);
    QTextCursor right(doc);
    right.setPosition(block.position() + hits[0].start + hits[0].length);
    const QRect leftRect = m_editor->cursorRect(left);
    const QRect rightRect = m_editor->cursorRect(right);
    const QFontMetrics fm(m_editor->font());
    const int underlineY = leftRect.top() + fm.ascent() + fm.underlinePos() + Editor::kUnderlineDropPx;

    const QPoint expected(leftRect.left() - overlay->pos().x(),
                          underlineY - overlay->pos().y());
    const int width = rightRect.left() - leftRect.left();

    const QImage img = overlay->grab().toImage();
    ASSERT_FALSE(img.isNull());

    bool found = false;
    for (int x = expected.x(); x < expected.x() + width && !found; ++x) {
        for (int y = expected.y() - 1; y <= expected.y() + 2 && !found; ++y) {
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                continue;
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 200 && c.green() > 100 && c.green() < 190 && c.blue() < 60)
                found = true;
        }
    }
    EXPECT_TRUE(found) << "amber underline must be painted under the broken link target";
}

TEST_F(UnderlineOverlayTest, ExplanationShownForMarkdownDuplicateHeading)
{
    QSettings().setValue(Preferences::MarkdownCheckEnabled, true);
    m_editor->recheckSpelling();
    m_editor->setPlainText(QStringLiteral("# Title\n# Title"));
    m_editor->spellHighlighter()->refresh();

    // Second heading is a duplicate; the first one (and its content) is clean.
    EXPECT_TRUE(m_editor->explanationAt(1, 3).contains(QStringLiteral("Duplicate heading")))
        << "hovering the duplicate heading must yield the checker's message";
    EXPECT_TRUE(m_editor->explanationAt(0, 3).isEmpty())
        << "a clean heading must have no explanation";
}

TEST_F(UnderlineOverlayTest, ExplanationShownForBrokenLink)
{
    QSettings().setValue(Preferences::SpellCheckEnabled, false);
    m_editor->recheckSpelling();

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile exists(dir.filePath(QStringLiteral("exists.md")));
    ASSERT_TRUE(exists.open(QIODevice::WriteOnly));
    m_editor->setCurrentFile(dir.filePath(QStringLiteral("doc.md")));
    m_editor->setPlainText(QStringLiteral("see [text](missing-file.md) and [text](exists.md)"));
    m_editor->spellHighlighter()->refresh();

    auto *hl = m_editor->findChild<SpellHighlighter *>();
    ASSERT_NE(hl, nullptr);
    const auto hits = hl->linkIssuesInBlock(0);
    ASSERT_EQ(1, hits.size());

    const QString tip = m_editor->explanationAt(0, hits[0].start + 1);
    EXPECT_TRUE(tip.startsWith(QStringLiteral("Broken link: ")));
    EXPECT_TRUE(tip.contains(QStringLiteral("missing-file.md")))
        << "the tooltip must name the broken target";
    EXPECT_TRUE(m_editor->explanationAt(0, hits[0].start + hits[0].length + 1).isEmpty())
        << "the adjacent valid link must stay clean";
}

TEST_F(UnderlineOverlayTest, ExplanationShowsMisspelledWord)
{
    setDocAndHighlight(4, 60);
    EXPECT_TRUE(m_editor->explanationAt(4, 1).contains(QStringLiteral("helo")))
        << "hovering the typo must name the misspelled word";
    EXPECT_TRUE(m_editor->explanationAt(0, 1).isEmpty())
        << "a correctly-spelled line must have no explanation";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

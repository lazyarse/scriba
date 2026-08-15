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
#include <QFileInfo>
#include <QLabel>
#include <QSettings>
#include <QTextDocument>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTest>
#include <QObject>
#include <atomic>
#include <memory>

#include "editor/Editor.h"
#include "editor/IssueSummaryPane.h"
#include "prefs/Preferences.h"
#include "spell/GrammarChecker.h"
#include "spell/SpellChecker.h"
#include "spell/SpellHighlighter.h"
#include "validation/MdLintConfig.h"
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
    // One typo, one broken link, one trailing-whitespace markdown issue
    // (MD009 is a style rule: warning severity by default),
    // grammar off (0).
    m_doc->setPlainText(QStringLiteral(
        "helo world\n"
        "[missing](no-such-file.md)\n"
        "a line with trailing spaces   \n"));
    m_doc->markContentsDirty(0, m_doc->characterCount());
    m_doc->documentLayout()->documentSize(); // force full layout so highlightBlock runs everywhere
    m_hl->setLinkCheckingEnabled(true);
    m_hl->setMarkdownCheckingEnabled(true);
    m_hl->setMarkdownConfig(MdLintConfig::defaults());
    m_hl->setCurrentFile(m_tmp->filePath(QStringLiteral("__test__.md")));
    m_hl->refresh();

    const auto counts = m_hl->counts();
    EXPECT_EQ(counts.spelling, 1);
    EXPECT_EQ(counts.links, 1);
    EXPECT_EQ(counts.markdown, 0);
    EXPECT_EQ(counts.markdownWarnings, 1);
    EXPECT_EQ(counts.grammar, 0);
}

TEST_F(IssueSummaryCountsTest, MarkdownWarningSplit)
{
    // MD009 (trailing spaces) configured as a warning, MD040 (fenced code
    // language) as an error: the counts split by severity.
    m_doc->setPlainText(QStringLiteral(
        "trailing   \n"
        "```\ncode\n```\n"));
    m_doc->markContentsDirty(0, m_doc->characterCount());
    m_doc->documentLayout()->documentSize();
    m_hl->setMarkdownCheckingEnabled(true);
    m_hl->setMarkdownConfig(MdLintConfig::fromJson(QStringLiteral(
        R"({"MD009": {"enabled": true, "severity": "warning"}, "MD040": {"enabled": true}})")));
    m_hl->setCurrentFile(m_tmp->filePath(QStringLiteral("__test__.md")));
    m_hl->refresh();

    const auto counts = m_hl->counts();
    EXPECT_EQ(counts.markdown, 1);          // MD040 error
    EXPECT_EQ(counts.markdownWarnings, 1);  // MD009 warning
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

class IssueSummaryPaneTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_pane = new IssueSummaryPane;
        m_pane->setTheme(QColor(QStringLiteral("#1e1e1e")), QColor(QStringLiteral("#d4d4d4")));
    }

    void TearDown() override
    {
        delete m_pane;
        QSettings().clear();
    }

    IssueSummaryPane *m_pane = nullptr;
};

TEST_F(IssueSummaryPaneTest, RowsRenderLabelAndCount)
{
    QVector<IssueSummaryPane::Row> rows = {
        {IssueSummaryPane::Kind::Typos, QStringLiteral("Typos"), 3,
         QColor(QStringLiteral("#d64050"))},
        {IssueSummaryPane::Kind::Links, QStringLiteral("Broken links"), 0,
         QColor(QStringLiteral("#f09000"))},
    };
    m_pane->setRows(rows);
    m_pane->showWithTimeout(0);
    QApplication::processEvents();

    ASSERT_TRUE(m_pane->isVisible());
    QString allText;
    for (QLabel *lbl : m_pane->findChildren<QLabel *>())
        allText += lbl->text();
    EXPECT_TRUE(allText.contains(QStringLiteral("Typos")));
    EXPECT_TRUE(allText.contains(QStringLiteral("3")));
    EXPECT_TRUE(allText.contains(QStringLiteral("Broken links")));
}

TEST_F(IssueSummaryPaneTest, CloseButtonHidesAndEmits)
{
    bool emitted = false;
    QObject::connect(m_pane, &IssueSummaryPane::closeRequested, [&emitted]() { emitted = true; });
    m_pane->setRows({});
    m_pane->showWithTimeout(0);
    QApplication::processEvents();
    ASSERT_TRUE(m_pane->isVisible());

    QToolButton *closeBtn = nullptr;
    for (QToolButton *btn : m_pane->findChildren<QToolButton *>())
        closeBtn = btn;
    ASSERT_NE(closeBtn, nullptr);
    QTest::mouseClick(closeBtn, Qt::LeftButton);

    EXPECT_TRUE(emitted);
    EXPECT_FALSE(m_pane->isVisible());
}

TEST_F(IssueSummaryPaneTest, TimeoutHidesAfterInterval)
{
    m_pane->setRows({});
    m_pane->showWithTimeout(60);
    QApplication::processEvents();
    ASSERT_TRUE(m_pane->isVisible());

    QTest::qWait(250);
    EXPECT_FALSE(m_pane->isVisible());
}

TEST_F(IssueSummaryPaneTest, ZeroTimeoutKeepsVisible)
{
    m_pane->setRows({});
    m_pane->showWithTimeout(0);
    QTest::qWait(250);
    EXPECT_TRUE(m_pane->isVisible());
}

TEST_F(IssueSummaryPaneTest, BackgroundRendersSemiTransparent)
{
    m_pane->setRows({});
    m_pane->resize(200, 100);
    m_pane->show();
    QApplication::processEvents();

    ASSERT_TRUE(m_pane->isVisible());
    const QImage img = m_pane->grab().toImage();
    ASSERT_FALSE(img.isNull());
    const int cx = img.width() / 2;
    const int cy = img.height() - 4; // bottom margin, clear of text
    ASSERT_TRUE(cy > 0 && cy < img.height());
    const QColor px = img.pixelColor(cx, cy);
    EXPECT_GT(px.alpha(), 200) << "the pane background must render semi-transparently";
    EXPECT_LT(px.alpha(), 240) << "the background must not be fully opaque";
}

class IssueSummaryEditorTest : public ::testing::Test
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
        SpellChecker::availableLanguages();
        m_tmp = std::make_unique<QTemporaryDir>();
        m_editor = new Editor;
        // Editor::applySpellSettings() honours SpellCheckEnabled on ctor.
        m_editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete m_editor;
        m_tmp.reset();
        QSettings().clear();
    }

    Editor::IssueSummaryOptions options()
    {
        Editor::IssueSummaryOptions o;
        o.enabled = true;
        o.timeoutEnabled = true;
        o.timeoutSeconds = 2;
        o.categories = {
            IssueSummaryPane::Kind::Typos,
            IssueSummaryPane::Kind::Links,
            IssueSummaryPane::Kind::Lint,
        };
        return o;
    }

    std::unique_ptr<QTemporaryDir> m_tmp;
    Editor *m_editor = nullptr;
};

TEST_F(IssueSummaryEditorTest, PaneShowsForMdFileAndTracksCounts)
{
    m_editor->setCurrentFile(m_tmp->filePath(QStringLiteral("notes.md")));
    m_editor->setIssueSummaryOptions(options(), QColor(QStringLiteral("#ffffff")),
                                     QColor(QStringLiteral("#333333")));
    m_editor->setPlainText(QStringLiteral("helo world\n[missing](no-such-file.md)\n"));
    m_editor->showIssueSummary();
    QTest::qWait(700); // spell/link pass (word-boundary triggered on newline)

    IssueSummaryPane *pane = m_editor->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    ASSERT_TRUE(pane->isVisible());
    QString allText;
    for (QLabel *lbl : pane->findChildren<QLabel *>())
        allText += lbl->text();
    EXPECT_TRUE(allText.contains(QStringLiteral("Typos")));
    EXPECT_TRUE(allText.contains(QStringLiteral("Broken links")));
}

TEST_F(IssueSummaryEditorTest, PaneShowsForUntitledFile)
{
    // No setCurrentFile: a fresh tab is untitled markdown and must show the pane.
    m_editor->setIssueSummaryOptions(options(), QColor(QStringLiteral("#ffffff")),
                                     QColor(QStringLiteral("#333333")));
    m_editor->setPlainText(QStringLiteral("helo world\n[missing](no-such-file.md)\n"));
    m_editor->showIssueSummary();
    QTest::qWait(700); // spell/link pass (word-boundary triggered on newline)

    IssueSummaryPane *pane = m_editor->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    ASSERT_TRUE(pane->isVisible());
    QString allText;
    for (QLabel *lbl : pane->findChildren<QLabel *>())
        allText += lbl->text();
    EXPECT_TRUE(allText.contains(QStringLiteral("Typos")));
    EXPECT_TRUE(allText.contains(QStringLiteral("Broken links")));
}

TEST_F(IssueSummaryEditorTest, PaneHiddenForNonMdFile)
{
    m_editor->setCurrentFile(m_tmp->filePath(QStringLiteral("data.rtf")));
    m_editor->setIssueSummaryOptions(options(), QColor(QStringLiteral("#ffffff")),
                                     QColor(QStringLiteral("#333333")));
    m_editor->setPlainText(QStringLiteral("helo world\n"));
    m_editor->showIssueSummary();
    QTest::qWait(300);

    IssueSummaryPane *pane = m_editor->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    EXPECT_FALSE(pane->isVisible());
}

TEST_F(IssueSummaryEditorTest, PaneHiddenWhenDisabled)
{
    m_editor->setCurrentFile(m_tmp->filePath(QStringLiteral("notes.md")));
    Editor::IssueSummaryOptions o = options();
    o.enabled = false;
    m_editor->setIssueSummaryOptions(o, QColor(QStringLiteral("#ffffff")),
                                     QColor(QStringLiteral("#333333")));
    m_editor->showIssueSummary();
    QTest::qWait(300);

    IssueSummaryPane *pane = m_editor->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    EXPECT_FALSE(pane->isVisible());
}

TEST_F(IssueSummaryEditorTest, TimeoutDismissesPane)
{
    m_editor->setCurrentFile(m_tmp->filePath(QStringLiteral("notes.md")));
    Editor::IssueSummaryOptions o = options();
    o.timeoutEnabled = true;
    o.timeoutSeconds = 1;
    m_editor->setIssueSummaryOptions(o, QColor(QStringLiteral("#ffffff")),
                                     QColor(QStringLiteral("#333333")));
    m_editor->setPlainText(QStringLiteral("helo world\n"));
    m_editor->showIssueSummary();
    QTest::qWait(700); // show debounce (400 ms) + spell pass
    IssueSummaryPane *pane = m_editor->issueSummaryPane();
    ASSERT_NE(pane, nullptr);
    ASSERT_TRUE(pane->isVisible());

    QTest::qWait(1500);
    EXPECT_FALSE(pane->isVisible());
}

TEST_F(IssueSummaryEditorTest, DismissedStaysHiddenUntilExplicitShow)
{
    m_editor->setCurrentFile(m_tmp->filePath(QStringLiteral("notes.md")));
    m_editor->setIssueSummaryOptions(options(), QColor(QStringLiteral("#ffffff")),
                                     QColor(QStringLiteral("#333333")));
    m_editor->setPlainText(QStringLiteral("helo world\n"));
    m_editor->showIssueSummary();
    QTest::qWait(700); // show debounce (400 ms) + spell pass
    IssueSummaryPane *pane = m_editor->issueSummaryPane();
    ASSERT_TRUE(pane->isVisible());

    // [x] dismisses; further count changes must NOT re-show it.
    for (QToolButton *btn : pane->findChildren<QToolButton *>())
        QTest::mouseClick(btn, Qt::LeftButton);
    EXPECT_FALSE(pane->isVisible());

    m_editor->setPlainText(QStringLiteral("helo world and more words here\n"));
    QTest::qWait(400);
    EXPECT_FALSE(pane->isVisible());

    // An explicit trigger (tab switch / file open) re-shows it (after the
    // 400 ms show debounce).
    m_editor->showIssueSummary();
    QTest::qWait(700);
    EXPECT_TRUE(pane->isVisible());
}

TEST(IssueSummaryPrefsTest, SettingsKeysRoundTrip)
{
    QSettings().clear();
    QSettings().setValue(Preferences::IssueSummaryEnabled, true);
    QSettings().setValue(Preferences::IssueSummaryTimeoutEnabled, true);
    QSettings().setValue(Preferences::IssueSummaryTimeoutSeconds, 12);
    QSettings().setValue(Preferences::IssueSummaryShowTypos, true);
    QSettings().setValue(Preferences::IssueSummaryShowGrammar, false);
    QSettings().setValue(Preferences::IssueSummaryShowLint, true);
    QSettings().setValue(Preferences::IssueSummaryShowLinks, false);

    EXPECT_TRUE(QSettings().value(Preferences::IssueSummaryEnabled, false).toBool());
    EXPECT_TRUE(QSettings().value(Preferences::IssueSummaryTimeoutEnabled, false).toBool());
    EXPECT_EQ(QSettings().value(Preferences::IssueSummaryTimeoutSeconds, 5).toInt(), 12);
    EXPECT_TRUE(QSettings().value(Preferences::IssueSummaryShowTypos, true).toBool());
    EXPECT_FALSE(QSettings().value(Preferences::IssueSummaryShowGrammar, true).toBool());
    EXPECT_TRUE(QSettings().value(Preferences::IssueSummaryShowLint, true).toBool());
    EXPECT_FALSE(QSettings().value(Preferences::IssueSummaryShowLinks, true).toBool());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
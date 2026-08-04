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
#include <QCompleter>
#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QMessageLogContext>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include "Editor.h"
#include "EditorTestHarness.h"
#include "Preferences.h"
#include "TestConfig.h"

class EditorCompletionHarness : public EditorTestHarness
{
public:
    EditorCompletionHarness() : EditorTestHarness(CompletionPrefs::all()) {}

protected:
    void SetUp() override
    {
        EditorTestHarness::SetUp();

        ASSERT_TRUE(tmpDir.isValid());

        currentFilePath = tmpDir.path() + "/doc.md";
        {
            QFile f(currentFilePath);
            (void)f.open(QIODevice::WriteOnly);
            f.write("# Test\n");
        }

        auto touch = [&](const QString &name) {
            QFile f(tmpDir.path() + "/" + name);
            (void)f.open(QIODevice::WriteOnly);
            f.close();
        };
        QDir d(tmpDir.path());
        d.mkdir("resources");
        d.mkdir("resources/icons");
        touch("main.css");
        touch("README.md");
        touch("resources/icons/logo.svg");
        touch("resources/icons/favicon.ico");
        touch("resources/icons/scriba.svg");

        editor->setCurrentFile(currentFilePath);
    }

    int popupRowCount() const
    {
        if (!editor->completer() || !editor->completer()->popup()->isVisible())
            return -1;
        return editor->completer()->completionModel()->rowCount();
    }

    QTemporaryDir tmpDir;
    QString currentFilePath;
};

TEST_F(EditorCompletionHarness, SingleMatchCompletes)
{
    typeText("![](resou");
    press(Qt::Key_Tab);
    enter();
    EXPECT_EQ(text(), "![](resources/");
}

TEST_F(EditorCompletionHarness, ChainedCompletion)
{
    typeText("![](resou");
    press(Qt::Key_Tab);
    enter();
    ASSERT_EQ(text(), "![](resources/");

    typeText("ico");
    press(Qt::Key_Tab);
    enter();
    EXPECT_EQ(text(), "![](resources/icons/");
}

TEST_F(EditorCompletionHarness, ListLinkDoesNotIndent)
{
    typeText("- [link](resou");
    press(Qt::Key_Tab);
    enter();
    EXPECT_EQ(text(), "- [link](resources/");
}

TEST_F(EditorCompletionHarness, ListWithoutLinkIndents)
{
    typeText("- item");
    press(Qt::Key_Tab);
    EXPECT_EQ(text(), "  - item");
}

TEST_F(EditorCompletionHarness, PopupCyclesAndAccepts)
{
    typeText("![](r");

    // Cycle to second item (resources/), then accept
    press(Qt::Key_Tab);
    enter();

    EXPECT_EQ(text(), "![](resources/");
}

TEST_F(EditorCompletionHarness, EmojiSingleMatchShowsPopupAndEnterAccepts)
{
    typeText(":asto");
    EXPECT_EQ(text(), ":asto");
    enter();
    EXPECT_EQ(text(), ":astonished:");
}

TEST_F(EditorCompletionHarness, EmojiMultiMatchThenEscape)
{
    typeText(":smil");
    press(Qt::Key_Escape);
    EXPECT_EQ(text(), ":smil");
}

TEST_F(EditorCompletionHarness, EmojiEnterAcceptsTopItem)
{
    typeText(":s");
    enter();
    QString result = text();
    EXPECT_TRUE(result.startsWith(':'));
    EXPECT_TRUE(result.endsWith(':'));
    EXPECT_GT(result.length(), 3);
}

TEST_F(EditorCompletionHarness, FileCompletionLimitsResults)
{
    // Create 25 files matching prefix "zzfile"
    for (int i = 0; i < 25; ++i) {
        QFile f(tmpDir.path() + QString("/zzfile%1.md").arg(i, 2, 10, QChar('0')));
        (void)f.open(QIODevice::WriteOnly);
        f.close();
    }

    typeText("![](zzfile");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible());
    EXPECT_EQ(popupRowCount(), 20);
}

TEST_F(EditorCompletionHarness, FileCompletionHidesWhenNarrowedToNoMatch)
{
    typeText("![](resources/icons/logo");
    QApplication::processEvents();
    ASSERT_GT(popupRowCount(), 0) << "popup visible while 'logo' matches";

    typeText("x");
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when no entry contains the typed fragment";
    EXPECT_EQ(text(), "![](resources/icons/logox");
}

TEST_F(EditorCompletionHarness, FileCompletionSequentialFragment)
{
    // "scrsvg" matches "scriba.svg" sequentially, skipping "iba."
    typeText("![](resources/icons/scrsvg");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible());
    ASSERT_EQ(popupRowCount(), 1);
    EXPECT_EQ(editor->completer()->completionModel()->index(0, 0).data().toString(), "scriba.svg");

    enter();
    EXPECT_EQ(text(), "![](resources/icons/scriba.svg)");
}

TEST_F(EditorCompletionHarness, FileCompletionSequentialSortsDenserFirst)
{
    {
        QFile f(tmpDir.path() + "/resources/icons/fullscreen.svg");
        (void)f.open(QIODevice::WriteOnly);
        f.close();
    }

    // Both match "scrsvg"; "scriba.svg" starts matching earlier (position 0 vs 4),
    // so it must rank above "fullscreen.svg".
    typeText("![](resources/icons/scrsvg");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible());
    ASSERT_EQ(popupRowCount(), 2);
    auto *model = editor->completer()->completionModel();
    EXPECT_EQ(model->index(0, 0).data().toString(), "scriba.svg");
    EXPECT_EQ(model->index(1, 0).data().toString(), "fullscreen.svg");

    enter();
    EXPECT_EQ(text(), "![](resources/icons/scriba.svg)");
}

TEST_F(EditorCompletionHarness, TypingCloseParenHidesFilePopup)
{
    typeText("![](resources/icons/logo");
    QApplication::processEvents();
    ASSERT_GT(popupRowCount(), 0);

    typeText(")");
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden after leaving the link context";
    EXPECT_EQ(text(), "![](resources/icons/logo)");
}

TEST_F(EditorCompletionHarness, EmojiBackspaceUpdatesPopup)
{
    typeText(":smi");
    QApplication::processEvents();
    int before = popupRowCount();
    ASSERT_GT(before, 0);

    press(Qt::Key_Backspace);
    int after = popupRowCount();
    ASSERT_GT(after, 0) << "popup should remain visible after backspace";
    EXPECT_GT(after, before) << "shorter prefix should produce more matches";
    EXPECT_EQ(text(), QString(":sm"));
}

TEST_F(EditorCompletionHarness, BackspaceOnClosingColonHidesPopup)
{
    typeText(":sm");
    QApplication::processEvents();
    ASSERT_GT(popupRowCount(), 0);

    // Backspace to ':s' — popup should still be visible
    press(Qt::Key_Backspace);
    ASSERT_GT(popupRowCount(), 0) << "popup visible after backspace to ':s'";

    // Backspace to ':' — popup should hide (no partial code)
    press(Qt::Key_Backspace);
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when only ':' remains";
    EXPECT_EQ(text(), QString(":"));
}

TEST_F(EditorCompletionHarness, EmojiCompletionHidesWhenNarrowedToNoMatch)
{
    typeText(":smil");
    QApplication::processEvents();
    ASSERT_GT(popupRowCount(), 0);

    typeText("z");
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when no shortcode contains the typed fragment";
    EXPECT_EQ(text(), ":smilz");
}

TEST_F(EditorCompletionHarness, EmojiClosingColonHidesPopup)
{
    typeText(":sm");
    QApplication::processEvents();
    ASSERT_GT(popupRowCount(), 0);

    typeText(":");
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when the shortcode is closed";
    EXPECT_EQ(text(), ":sm:");
}

TEST_F(EditorCompletionHarness, EmojiPopupSitsBelowCursorLine)
{
    // ":" alone does not open the popup (no partial code yet), so capture the
    // cursor's global position before the popup exists. Under xvfb the window
    // can be moved asynchronously while the popup is shown, so comparing
    // against a post-popup cursor position would be flaky.
    typeText(":");
    QRect cursor = editor->cursorRect();
    QPoint cursorBottomGlobal = editor->viewport()->mapToGlobal(
        QPoint(cursor.x(), cursor.y() + cursor.height()));

    typeText("s");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    QAbstractItemView *popup = editor->completer()->popup();
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isVisible());

    EXPECT_GE(popup->geometry().y(), cursorBottomGlobal.y())
        << "popup top should be at or below the bottom of the cursor line";
}

TEST_F(EditorCompletionHarness, CompletionPopupHugsCursorLine)
{
    // The popup must sit just below the active line — a small, bounded gap.
    // Regression guard: a large offset (e.g. an accidental big padding below the
    // caret) used to leave a large blank space between the text and the popup.
    auto gapFor = [&]() -> int {
        if (!editor->completer() || !editor->completer()->popup()->isVisible())
            return -1;
        QAbstractItemView *popup = editor->completer()->popup();
        QRect cursor = editor->cursorRect();
        QPoint bottomG = editor->viewport()->mapToGlobal(
            QPoint(cursor.x(), cursor.y() + cursor.height()));
        return popup->geometry().y() - bottomG.y();
    };

    typeText("![](res");
    QApplication::processEvents();
    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible()) << "file popup should be visible";
    int fileGap = gapFor();
    EXPECT_GE(fileGap, 0) << "file popup must not appear above the caret line";
    EXPECT_LE(fileGap, 10) << "file popup gap below caret is too large: " << fileGap;

    editor->clear();
    typeText(":smil");
    QApplication::processEvents();
    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible()) << "emoji popup should be visible";
    int emojiGap = gapFor();
    EXPECT_GE(emojiGap, 0) << "emoji popup must not appear above the caret line";
    EXPECT_LE(emojiGap, 10) << "emoji popup gap below caret is too large: " << emojiGap;
}

TEST_F(EditorCompletionHarness, CompletionPopupFlipsAboveWhenNearBottom)
{
    // When the caret is at the bottom of the screen there is no room for the
    // popup below, so it must flip and appear above the active line.
    QScreen *screen = QGuiApplication::primaryScreen();
    ASSERT_NE(screen, nullptr);
    QRect avail = screen->availableGeometry();
    editor->move(avail.left(), avail.bottom() - editor->height() + 1);

    QStringList lines;
    for (int i = 0; i < 200; ++i)
        lines << ("line " + QString::number(i));
    setContent(lines.join("\n"));

    placeCursor(199, 0);
    editor->ensureCursorVisible();
    QApplication::processEvents();

    typeText(":smil");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    QAbstractItemView *popup = editor->completer()->popup();
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isVisible()) << "emoji popup should be visible";

    QRect cursor = editor->cursorRect();
    QPoint caretBottom = editor->viewport()->mapToGlobal(
        QPoint(cursor.x(), cursor.y() + cursor.height()));
    QPoint caretTop = editor->viewport()->mapToGlobal(QPoint(cursor.x(), cursor.y()));

    EXPECT_LT(popup->geometry().y(), caretBottom.y())
        << "popup should be placed above the caret line when near the bottom";
    EXPECT_LE(popup->geometry().y() + popup->height(), caretTop.y() + 4)
        << "popup bottom should not extend below the top of the caret line";
}

TEST_F(EditorCompletionHarness, CodeFenceLanguageCompletes)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0) << "popup should appear while typing language after ```";
    enter();
    EXPECT_EQ(text(), "```python");
}

TEST_F(EditorCompletionHarness, CodeFenceMermaidCompletes)
{
    editor->clear();
    typeText("```mer");
    ASSERT_GT(popupRowCount(), 0);
    enter();
    EXPECT_EQ(text(), "```mermaid");
}

TEST_F(EditorCompletionHarness, CodeFenceJsSuggestsJson)
{
    editor->clear();
    typeText("```js");
    enter();
    EXPECT_EQ(text(), "```json");
}

TEST_F(EditorCompletionHarness, CodeFenceAliasSuggestsJavascript)
{
    editor->clear();
    typeText("```javas");
    enter();
    EXPECT_EQ(text(), "```javascript");
}

TEST_F(EditorCompletionHarness, CodeFencePrefixSortedFirst)
{
    editor->clear();
    typeText("```c");
    ASSERT_GT(popupRowCount(), 0);
    QString first = editor->completer()->completionModel()->index(0, 0).data().toString();
    EXPECT_EQ(first, "c");
}

TEST_F(EditorCompletionHarness, CodeFenceEscapeKeepsText)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0);
    press(Qt::Key_Escape);
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden after escape";
    EXPECT_EQ(text(), "```py");
}

TEST_F(EditorCompletionHarness, CodeFenceBackspaceUpdatesPopup)
{
    editor->clear();
    typeText("```pyth");
    ASSERT_GT(popupRowCount(), 0);

    press(Qt::Key_Backspace);
    ASSERT_GT(popupRowCount(), 0) << "popup should remain visible after backspace";
    EXPECT_EQ(text(), "```pyt");

    press(Qt::Key_Backspace);
    press(Qt::Key_Backspace);
    press(Qt::Key_Backspace);
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when only ``` remains";
    EXPECT_EQ(text(), "```");
}

TEST_F(EditorCompletionHarness, CodeFenceHidesWhenNarrowedToNoMatch)
{
    editor->clear();
    typeText("```pytho");
    ASSERT_GT(popupRowCount(), 0);

    typeText("z");
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when no language contains the typed fragment";
    EXPECT_EQ(text(), "```pythoz");
}

TEST_F(EditorCompletionHarness, CodeFenceSpaceHidesPopup)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0);

    typeText(" ");
    EXPECT_EQ(popupRowCount(), -1) << "popup hidden when a space leaves the language context";
    EXPECT_EQ(text(), "```py ");
}

TEST_F(EditorCompletionHarness, NoCompletionOnClosingFence)
{
    editor->clear();
    typeText("```py");
    ASSERT_GT(popupRowCount(), 0);
    press(Qt::Key_Escape);
    enter();

    typeText("```py");
    EXPECT_EQ(popupRowCount(), -1) << "no popup on a closing fence";
    EXPECT_EQ(text(), "```py\n```py");
}

TEST_F(EditorCompletionHarness, NoCompletionInsideCodeBlock)
{
    editor->clear();
    typeText("```py");
    press(Qt::Key_Escape);
    enter();

    typeText("py");
    EXPECT_EQ(popupRowCount(), -1) << "no popup for plain text inside a code block";
}

TEST_F(EditorCompletionHarness, CodeFenceCompletionRespectsPreference)
{
    QSettings().setValue(Preferences::LanguageAutoComplete, false);
    editor->clear();
    typeText("```py");
    EXPECT_EQ(popupRowCount(), -1) << "no popup when language autocomplete is disabled";
}

TEST_F(EditorCompletionHarness, HtmlSrcAttributeCompletes)
{
    typeText("<img src=\"resou");
    press(Qt::Key_Tab);
    enter();
    EXPECT_EQ(text(), "<img src=\"resources/");
}

TEST_F(EditorCompletionHarness, HtmlHrefAttributeCompletes)
{
    typeText("<a href='resou");
    press(Qt::Key_Tab);
    enter();
    EXPECT_EQ(text(), "<a href='resources/");

    typeText("ico");
    press(Qt::Key_Tab);
    enter();
    EXPECT_EQ(text(), "<a href='resources/icons/");
}

TEST_F(EditorCompletionHarness, EmojiCompletionLimitsResults)
{
    QSettings().setValue(Preferences::EmojiCompletionLimit, 10);
    typeText(":s");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    ASSERT_TRUE(editor->completer()->popup()->isVisible());
    EXPECT_EQ(popupRowCount(), 10);
}

TEST_F(EditorCompletionHarness, EmojiSuggestionsContainNoGarbageEntries)
{
    QSettings().setValue(Preferences::EmojiCompletionLimit, 10);
    typeText(":s");
    QApplication::processEvents();

    ASSERT_NE(editor->completer(), nullptr);
    auto *model = editor->completer()->completionModel();
    static const QRegularExpression shortcodeRe("^:[a-z0-9_+\\-]+:$");
    for (int i = 0; i < model->rowCount(); ++i) {
        QString label = model->index(i, 0).data().toString();
        EXPECT_TRUE(shortcodeRe.match(label).hasMatch()) << "unexpected suggestion: " << label.toStdString();
    }
}

namespace {
int g_svgWarningCount = 0;

void svgWarningCounter(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtWarningMsg && msg.startsWith("qt.svg: Cannot open file"))
        ++g_svgWarningCount;
}
}

TEST_F(EditorCompletionHarness, EmojiAutocompleteRendersNoMissingSvgWarnings)
{
    QSettings().setValue(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Color));

    auto *oldHandler = qInstallMessageHandler(svgWarningCounter);
    g_svgWarningCount = 0;

    // User repro: typing an emoji shortcode renders icons for every matched
    // suggestion; every keystroke re-renders the popup. ":fem" and ":pers"
    // additionally hit shortcodes whose twemoji SVG is a ZWJ sequence
    // (kept-fe0f name) or genuinely absent from the bundled twemoji set.
    typeText(":smil");
    QApplication::processEvents();
    press(Qt::Key_Escape);
    typeText(" :rocke");
    QApplication::processEvents();
    press(Qt::Key_Escape);
    typeText(" :fem");
    QApplication::processEvents();
    press(Qt::Key_Escape);
    typeText(" :pers");
    QApplication::processEvents();
    press(Qt::Key_Escape);

    qInstallMessageHandler(oldHandler);
    EXPECT_EQ(g_svgWarningCount, 0) << "autocomplete rendered a twemoji SVG that does not exist";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

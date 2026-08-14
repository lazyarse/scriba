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
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>

#include "editor/Editor.h"
#include "EditorTestHarness.h"
#include "prefs/Preferences.h"
#include "TestConfig.h"

namespace {

QString longProseDoc()
{
    QString content;
    for (int i = 0; i < 2000; ++i)
        content += QStringLiteral("Line number %1 with some filler text to fill the block width.\n").arg(i);
    return content;
}

QTextBlockFormat appLineHeightFormat()
{
    QTextBlockFormat fmt;
    fmt.setLineHeight(QSettings().value(Preferences::EditorLineHeight,
                                         Preferences::DefaultEditorLineHeight).toInt(),
                      QTextBlockFormat::ProportionalHeight);
    return fmt;
}

void applyFormatsToAllBlocks(QTextEdit *edit)
{
    QTextCursor cur(edit->document());
    cur.select(QTextCursor::Document);
    cur.mergeBlockFormat(appLineHeightFormat());
    QApplication::processEvents();
}

int scrollValue(QTextEdit *edit)
{
    QApplication::processEvents();
    return edit->verticalScrollBar()->value();
}

} // namespace

TEST_F(EditorTestHarness, EnterAtEndOfScrolledDocumentKeepsScrollPosition)
{
    setContent(longProseDoc());
    applyFormatsToAllBlocks(editor);
    placeCursorAtEnd();
    int before = scrollValue(editor);
    ASSERT_GT(before, 0) << "test setup: editor must be scrolled down";

    enter();

    int after = scrollValue(editor);
    EXPECT_GE(after, before) << "Enter at the bottom must not collapse the viewport to the top";
    EXPECT_EQ(editor->document()->blockCount(), 2002) << "paragraph separator must be inserted";
}

TEST_F(EditorTestHarness, EnterMidScrolledDocumentKeepsScrollPosition)
{
    setContent(longProseDoc());
    applyFormatsToAllBlocks(editor);
    placeCursor(1000, 0);
    int before = scrollValue(editor);
    ASSERT_GT(before, 0) << "test setup: editor must be scrolled down";

    enter();

    int after = scrollValue(editor);
    EXPECT_GE(after, before) << "Enter mid-document must not collapse the viewport to the top";
    EXPECT_EQ(editor->document()->blockCount(), 2002) << "paragraph separator must be inserted";
}

TEST_F(EditorTestHarness, EnterKeepsLineHeightFormatOnBothBlocks)
{
    setContent(longProseDoc());
    applyFormatsToAllBlocks(editor);
    placeCursorAtEnd();

    enter();

    QTextDocument *doc = editor->document();
    QTextBlock prev = doc->findBlockByNumber(doc->blockCount() - 2);
    QTextBlock current = doc->findBlockByNumber(doc->blockCount() - 1);
    const auto wantHeight = appLineHeightFormat().lineHeight();
    EXPECT_EQ(prev.blockFormat().lineHeight(), wantHeight) << "previous block keeps line height";
    EXPECT_EQ(current.blockFormat().lineHeight(), wantHeight) << "new block gets line height";
}

TEST_F(EditorTestHarness, EnterAtDocumentEndScrollsCaretFullyIntoView)
{
    setContent(longProseDoc());
    applyFormatsToAllBlocks(editor);
    placeCursorAtEnd();
    int before = scrollValue(editor);
    ASSERT_GT(before, 0) << "test setup: editor must be scrolled down";

    enter();

    EXPECT_GE(scrollValue(editor), before) << "Enter at the bottom must not collapse the viewport to the top";
    QRect caret = editor->cursorRect();
    QRect vp = editor->viewport()->rect();
    EXPECT_GE(caret.top(), vp.top()) << "caret top must be inside the viewport";
    EXPECT_LE(caret.bottom(), vp.bottom()) << "caret must be fully visible, not half-clipped below the viewport";
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

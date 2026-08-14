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
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include "editor/Gutter.h"
#include "TestConfig.h"

namespace {

QTextBlock makeSingleLineBlock(int pct)
{
    QTextDocument doc;
    QFont font("DejaVu Sans");
    font.setPointSizeF(14.0);
    doc.setDefaultFont(font);

    QTextBlockFormat fmt;
    fmt.setLineHeight(pct, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(&doc);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
    cursor.setPosition(0);
    cursor.insertText("Hello world");
    doc.documentLayout()->documentSize();

    return doc.firstBlock();
}

} // namespace

TEST(GutterLineHeight, AnchorIsIndependentOfLineHeight)
{
    qreal firstAnchor = Gutter::firstLineTextCenterY(makeSingleLineBlock(100));
    for (int pct : {125, 150, 200, 250}) {
        qreal anchor = Gutter::firstLineTextCenterY(makeSingleLineBlock(pct));
        EXPECT_NEAR(anchor, firstAnchor, 0.5) << "pct=" << pct;
    }
}

TEST(GutterLineHeight, AnchorMatchesLayoutTextCenter)
{
    QTextDocument doc;
    QFont font("DejaVu Sans");
    font.setPointSizeF(14.0);
    doc.setDefaultFont(font);

    QTextBlockFormat fmt;
    fmt.setLineHeight(250, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(&doc);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
    cursor.setPosition(0);
    cursor.insertText("Hello world");
    doc.documentLayout()->documentSize();

    QTextBlock block = doc.firstBlock();
    QTextLayout *layout = block.layout();
    ASSERT_GT(layout->lineCount(), 0);
    QTextLine line0 = layout->lineAt(0);
    qreal expected = layout->position().y() + line0.y()
                   + (line0.ascent() + line0.descent()) / 2.0;

    EXPECT_NEAR(Gutter::firstLineTextCenterY(block), expected, 0.5);
}

TEST(GutterLineHeight, AnchorIsIndependentOfFontSize)
{
    qreal smallAnchor = Gutter::firstLineTextCenterY(makeSingleLineBlock(125));
    QTextDocument doc;
    QFont font("DejaVu Sans");
    font.setPointSizeF(24.0);
    doc.setDefaultFont(font);

    QTextBlockFormat fmt;
    fmt.setLineHeight(125, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(&doc);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
    cursor.setPosition(0);
    cursor.insertText("Hello world");
    doc.documentLayout()->documentSize();

    QTextBlock block = doc.firstBlock();
    QTextLayout *layout = block.layout();
    QTextLine line0 = layout->lineAt(0);
    qreal expected = layout->position().y() + line0.y()
                   + (line0.ascent() + line0.descent()) / 2.0;

    EXPECT_NEAR(Gutter::firstLineTextCenterY(block), expected, 0.5);
    EXPECT_NE(smallAnchor, Gutter::firstLineTextCenterY(block));
}

TEST(GutterLineHeight, WrappedBlockAnchorsToFirstLine)
{
    QTextDocument doc;
    QFont font("DejaVu Sans");
    font.setPointSizeF(14.0);
    doc.setDefaultFont(font);
    doc.setTextWidth(150);

    QTextBlockFormat fmt;
    fmt.setLineHeight(125, QTextBlockFormat::ProportionalHeight);
    QTextCursor cursor(&doc);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(fmt);
    cursor.setPosition(0);
    cursor.insertText("This is a paragraph long enough to wrap over multiple lines in a narrow column.");
    doc.documentLayout()->documentSize();

    QTextBlock block = doc.firstBlock();
    QTextLayout *layout = block.layout();
    ASSERT_GT(layout->lineCount(), 1);

    qreal anchor = Gutter::firstLineTextCenterY(block);
    QTextLine line0 = layout->lineAt(0);
    qreal lineTop = layout->position().y() + line0.y();
    qreal lineBottom = lineTop + line0.height();
    EXPECT_GE(anchor, lineTop);
    EXPECT_LE(anchor, lineBottom);

    qreal blockCenter = layout->position().y() + layout->boundingRect().height() / 2.0;
    EXPECT_LT(anchor, blockCenter - 5.0) << "anchor must sit on the first line, not the block center";
}

TEST(GutterLineHeight, EmptyBlockFallsBackToZero)
{
    QTextDocument doc;
    QFont font("DejaVu Sans");
    font.setPointSizeF(14.0);
    doc.setDefaultFont(font);
    QTextBlock block = doc.firstBlock();
    EXPECT_EQ(Gutter::firstLineTextCenterY(block), 0.0);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

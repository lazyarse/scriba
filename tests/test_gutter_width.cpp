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
#include <QSettings>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>

#include "Editor.h"
#include "Gutter.h"
#include "Preferences.h"
#include "TestConfig.h"

namespace {

void setGutterPreferences(bool lineNumbers)
{
    QSettings s;
    s.setValue(Preferences::ShowLineNumbers, lineNumbers);
    s.setValue(Preferences::GutterColorOverride, false);
    s.sync();
}

void fillDocument(Editor *editor, int blocks)
{
    QString text;
    for (int i = 0; i < blocks; ++i)
        text += QString("line %1\n").arg(i);
    editor->setPlainText(text);
    QApplication::processEvents();
}

// Y at the vertical centre of block `n`, computed the same way the gutter does
// (document-top offset + block bounding rect).
int blockCenterY(Editor *editor, int n)
{
    QTextCursor cursor0(editor->document());
    cursor0.setPosition(0);
    int docTopY = editor->viewport()->mapTo(editor, editor->cursorRect(cursor0).topLeft()).y()
                  - (int)editor->document()->documentMargin();
    QRectF r = editor->document()->documentLayout()
                   ->blockBoundingRect(editor->document()->findBlockByNumber(n));
    return docTopY + (int)r.y() + (int)r.height() / 2;
}

} // namespace

class GutterWidthTest : public testing::Test
{
protected:
    void SetUp() override
    {
        editor = new Editor();
        editor->resize(800, 600);
        editor->show();
        QApplication::processEvents();
    }

    void TearDown() override
    {
        delete editor;
    }

    Editor *editor = nullptr;
};

TEST_F(GutterWidthTest, WidthStableAcrossSettingsReload)
{
    setGutterPreferences(true);
    fillDocument(editor, 150);

    editor->updateGutterSettings();
    QApplication::processEvents();
    QTest::qWait(10); // let deferred updateGutter() fire
    QApplication::processEvents();

    int expected = editor->gutter()->fontMetrics().horizontalAdvance(QLatin1Char('9')) * 3
                   + 16 + 14;
    EXPECT_EQ(editor->gutter()->width(), expected);
}

TEST_F(GutterWidthTest, AutoWidensAsLineCountCrossesDigitBoundary)
{
    setGutterPreferences(true);
    fillDocument(editor, 5);
    QTest::qWait(10);
    QApplication::processEvents();

    int widthTwoDigits = editor->gutter()->fontMetrics().horizontalAdvance(QLatin1Char('9')) * 2
                         + 16 + 14;
    EXPECT_EQ(editor->gutter()->width(),
              editor->gutter()->fontMetrics().horizontalAdvance(QLatin1Char('9')) + 16 + 14);

    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    for (int i = 0; i < 50; ++i)
        cursor.insertText("extra line\n");
    QTest::qWait(10);
    QApplication::processEvents();

    EXPECT_EQ(editor->gutter()->width(), widthTwoDigits);
}

TEST_F(GutterWidthTest, WidthWithoutLineNumbersIsFoldIconsOnly)
{
    setGutterPreferences(false);
    fillDocument(editor, 150);

    editor->updateGutterSettings();
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();

    EXPECT_EQ(editor->gutter()->width(), 14);
}

TEST_F(GutterWidthTest, GutterStylesheetColorReachesPalette)
{
    setGutterPreferences(true);
    editor->setStyleSheet("QTextEdit { background-color: #282a36; color: #f8f8f2; }\n"
                          "#gutter { background-color: #21232d; color: #a0a0a0; }");
    QApplication::processEvents();
    QTest::qWait(10);
    QApplication::processEvents();

    EXPECT_EQ(editor->gutter()->palette().windowText().color(), QColor("#a0a0a0"));
}

TEST_F(GutterWidthTest, GutterIsChildOfEditor)
{
    EXPECT_NE(editor->gutter(), nullptr);
    EXPECT_EQ(editor->gutter()->parentWidget(), editor);
}

TEST_F(GutterWidthTest, ChartPencilClickEmitsEditForMermaidFence)
{
    setGutterPreferences(true);
    // Block 0: fence, block 1: content line, block 2: closing fence.
    editor->setPlainText("```mermaid\npie title Pets\n```");
    QApplication::processEvents();
    QTest::qWait(400); // let the debounced fold/chart scan run
    QApplication::processEvents();

    auto *gutter = editor->gutter();
    Gutter *g = qobject_cast<Gutter *>(gutter);
    ASSERT_NE(g, nullptr);

    // Compute the y of the first content line (block 1) the same way the
    // gutter does: block bounding rect offset by the document top.
    int contentY = blockCenterY(editor, 1);

    EXPECT_EQ(g->chartFenceAtPos(contentY), 0);

    // Mouse press at the content line should emit chartEditRequested(0).
    int received = -1;
    QObject::connect(g, &Gutter::chartEditRequested, [&received](int bn) { received = bn; });
    QTest::mouseClick(g, Qt::LeftButton, Qt::NoModifier,
                      QPoint(g->width() / 2, contentY));
    EXPECT_EQ(received, 0);
}

TEST_F(GutterWidthTest, ChartPencilClickDoesNotFireOnPlainLines)
{
    setGutterPreferences(true);
    editor->setPlainText("plain line\n```mermaid\npie title Pets\n```");
    QApplication::processEvents();
    QTest::qWait(400); // let the debounced fold/chart scan run
    QApplication::processEvents();

    auto *g = qobject_cast<Gutter *>(editor->gutter());
    ASSERT_NE(g, nullptr);

    int line0Y = blockCenterY(editor, 0);

    EXPECT_EQ(g->chartFenceAtPos(line0Y), -1);
}

TEST_F(GutterWidthTest, PointerCursorOverFoldAndChartIcons)
{
    setGutterPreferences(true);
    // Block 0 is a header (foldable), blocks 1-2 form a mermaid chart whose
    // pencil sits on block 2 (its first content line).
    editor->setPlainText("# Heading\n```mermaid\npie title Pets\n```");
    QApplication::processEvents();
    QTest::qWait(400); // let the debounced fold/chart scan run
    QApplication::processEvents();

    auto *g = qobject_cast<Gutter *>(editor->gutter());
    ASSERT_NE(g, nullptr);

    g->setCursor(Qt::ArrowCursor);

    QMouseEvent move1(QEvent::MouseMove, QPointF(g->width() / 2, blockCenterY(editor, 0)),
                      QPointF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(g, &move1);
    EXPECT_EQ(g->cursor().shape(), Qt::PointingHandCursor); // fold triangle

    QMouseEvent move2(QEvent::MouseMove, QPointF(g->width() / 2, blockCenterY(editor, 2)),
                      QPointF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(g, &move2);
    EXPECT_EQ(g->cursor().shape(), Qt::PointingHandCursor); // chart pencil

    QMouseEvent move3(QEvent::MouseMove, QPointF(1, blockCenterY(editor, 3)),
                      QPointF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(g, &move3);
    EXPECT_EQ(g->cursor().shape(), Qt::ArrowCursor); // plain closing fence row
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupTestConfig();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

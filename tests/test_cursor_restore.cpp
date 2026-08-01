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
#include <QTextDocument>
#include "StaticHelpers.h"

TEST(RestoreCursorPosition, StartOfEmptyDocument) {
    QTextDocument doc("");
    QTextCursor c = restoreCursorPosition(&doc, 0, 0);
    EXPECT_EQ(c.blockNumber(), 0);
    EXPECT_EQ(c.columnNumber(), 0);
}

TEST(RestoreCursorPosition, StartOfSingleLine) {
    QTextDocument doc("hello world");
    QTextCursor c = restoreCursorPosition(&doc, 0, 0);
    EXPECT_EQ(c.blockNumber(), 0);
    EXPECT_EQ(c.columnNumber(), 0);
}

TEST(RestoreCursorPosition, MiddleOfFirstLine) {
    QTextDocument doc("hello world");
    QTextCursor c = restoreCursorPosition(&doc, 0, 5);
    EXPECT_EQ(c.blockNumber(), 0);
    EXPECT_EQ(c.columnNumber(), 5);
}

TEST(RestoreCursorPosition, EndOfFirstLine) {
    QTextDocument doc("hello world");
    QTextCursor c = restoreCursorPosition(&doc, 0, 11);
    EXPECT_EQ(c.blockNumber(), 0);
    EXPECT_EQ(c.columnNumber(), 11);
}

TEST(RestoreCursorPosition, SecondLineStart) {
    QTextDocument doc("line one\nline two");
    QTextCursor c = restoreCursorPosition(&doc, 1, 0);
    EXPECT_EQ(c.blockNumber(), 1);
    EXPECT_EQ(c.columnNumber(), 0);
}

TEST(RestoreCursorPosition, SecondLineMiddle) {
    QTextDocument doc("line one\nline two");
    QTextCursor c = restoreCursorPosition(&doc, 1, 4);
    EXPECT_EQ(c.blockNumber(), 1);
    EXPECT_EQ(c.columnNumber(), 4);
}

TEST(RestoreCursorPosition, ThirdLineEnd) {
    QTextDocument doc("aaa\nbbb\nccc");
    QTextCursor c = restoreCursorPosition(&doc, 2, 3);
    EXPECT_EQ(c.blockNumber(), 2);
    EXPECT_EQ(c.columnNumber(), 3);
}

TEST(RestoreCursorPosition, BlockBeyondDocClamps) {
    QTextDocument doc("short");
    QTextCursor c = restoreCursorPosition(&doc, 99, 0);
    EXPECT_EQ(c.blockNumber(), 0);
    EXPECT_EQ(c.columnNumber(), 0);
}

TEST(RestoreCursorPosition, ColumnBeyondLineClamps) {
    QTextDocument doc("abc");
    QTextCursor c = restoreCursorPosition(&doc, 0, 99);
    EXPECT_EQ(c.blockNumber(), 0);
    EXPECT_EQ(c.columnNumber(), 3);
}

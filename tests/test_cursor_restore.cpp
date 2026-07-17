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

#include <gtest/gtest.h>
#include "StaticHelpers.h"

TEST(EscapeJsStringTest, EmptyString) {
    EXPECT_EQ(escapeJsString(""), "");
}

TEST(EscapeJsStringTest, NoSpecialChars) {
    EXPECT_EQ(escapeJsString("hello world"), "hello world");
}

TEST(EscapeJsStringTest, Backslash) {
    EXPECT_EQ(escapeJsString("a\\b"), "a\\\\b");
}

TEST(EscapeJsStringTest, SingleQuote) {
    EXPECT_EQ(escapeJsString("it's"), "it\\'s");
}

TEST(EscapeJsStringTest, Newline) {
    EXPECT_EQ(escapeJsString("a\nb"), "a\\nb");
}

TEST(EscapeJsStringTest, MultipleSpecialChars) {
    EXPECT_EQ(escapeJsString("a\\b'c\nd"), "a\\\\b\\'c\\nd");
}

TEST(EscapeJsStringTest, PathWithBackslashes) {
    EXPECT_EQ(escapeJsString("C:\\Users\\test"), "C:\\\\Users\\\\test");
}

TEST(ExtractContentWidthTest, EmptyCss) {
    EXPECT_EQ(extractContentWidth(""), 840);
}

TEST(ExtractContentWidthTest, MaxWidthOnly) {
    EXPECT_EQ(extractContentWidth("body { max-width: 800px }"), 840);
}

TEST(ExtractContentWidthTest, MaxWidth600) {
    EXPECT_EQ(extractContentWidth("body { max-width: 600px }"), 640);
}

TEST(ExtractContentWidthTest, PaddingLeftAndRight) {
    QString css = "body { max-width: 600px; padding-left: 20px; padding-right: 20px; }";
    EXPECT_EQ(extractContentWidth(css), 640);
}

TEST(ExtractContentWidthTest, PaddingShorthand) {
    QString css = "body { max-width: 600px; padding: 10px; }";
    EXPECT_EQ(extractContentWidth(css), 620);
}

TEST(ExtractContentWidthTest, MaxWidth1024) {
    EXPECT_EQ(extractContentWidth("body { max-width: 1024px }"), 1064);
}

TEST(ExtractContentWidthTest, NoMaxWidthWithPadding) {
    QString css = "body { padding-left: 30px; padding-right: 30px; }";
    EXPECT_EQ(extractContentWidth(css), 860);
}

TEST(ExtractContentWidthTest, PaddingLeftRightOverPadding) {
    QString css = "body { max-width: 600px; padding-left: 10px; padding-right: 10px; padding: 50px; }";
    EXPECT_EQ(extractContentWidth(css), 620);
}

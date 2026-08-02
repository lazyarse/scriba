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
#include "tokenizer.h"
using namespace stoppard;

TEST(Tokenize, EmptyInput) { EXPECT_TRUE(tokenize(u"").empty()); }

TEST(Tokenize, BasicSentence) {
    auto t = tokenize(u"The cat sat.");
    ASSERT_EQ(t.size(), 6u);
    EXPECT_EQ(t[0].kind, TokenKind::Word);  EXPECT_EQ(t[0].start, 0);  EXPECT_EQ(t[0].length, 3);
    EXPECT_EQ(t[1].kind, TokenKind::Whitespace);  EXPECT_EQ(t[1].start, 3);
    EXPECT_EQ(t[2].kind, TokenKind::Word);  EXPECT_EQ(t[2].start, 4);
    EXPECT_EQ(t[4].kind, TokenKind::Word);  EXPECT_EQ(t[4].start, 8);
    EXPECT_EQ(t[5].kind, TokenKind::Punctuation);  EXPECT_EQ(t[5].start, 11);  EXPECT_EQ(t[5].length, 1);
}

TEST(Tokenize, Numbers) {
    auto t = tokenize(u"42 cats");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].kind, TokenKind::Number);  EXPECT_EQ(t[0].length, 2);
    EXPECT_EQ(t[2].kind, TokenKind::Word);
}

TEST(Tokenize, NonBmpIsOneUtf16Token) {
    auto t = tokenize(u"a \U0001F600 b");   // emoji = 2 UTF-16 code units
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[2].kind, TokenKind::Word);  EXPECT_EQ(t[2].start, 2);  EXPECT_EQ(t[2].length, 2);
}

TEST(Tokenize, ApostropheWordIsSingleToken) {
    auto t = tokenize(u"don't");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Word);  EXPECT_EQ(t[0].length, 5);
}

TEST(Tokenize, FencedCodeIsOpaque) {
    auto t = tokenize(u"before\n```cpp\nint x;\n```\nafter");
    ASSERT_EQ(t.size(), 5u);   // word, ws, CODE, ws, word
    EXPECT_EQ(t[2].kind, TokenKind::Code);
    EXPECT_EQ(t[2].length, 17); // "```cpp\nint x;\n```"
}

TEST(Tokenize, InlineCodeIsOpaque) {
    auto t = tokenize(u"Use `rm -rf` now");
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[2].kind, TokenKind::Code);  EXPECT_EQ(t[2].length, 8); // "`rm -rf`"
}

TEST(Tokenize, HeadingMarkerIsPunctuation) {
    auto t = tokenize(u"# Title");
    EXPECT_EQ(t[0].kind, TokenKind::Punctuation);
}

TEST(SplitContraction, Table) {
    EXPECT_EQ(splitContraction(u"won't"), (std::vector<std::u16string>{u"will", u"n't"}));
    EXPECT_EQ(splitContraction(u"can't"), (std::vector<std::u16string>{u"can", u"n't"}));
    EXPECT_EQ(splitContraction(u"don't"), (std::vector<std::u16string>{u"do", u"n't"}));
    EXPECT_EQ(splitContraction(u"she's"), (std::vector<std::u16string>{u"she", u"'s"}));
    EXPECT_EQ(splitContraction(u"I'm"),   (std::vector<std::u16string>{u"I", u"'m"}));
    EXPECT_EQ(splitContraction(u"we'll"), (std::vector<std::u16string>{u"we", u"'ll"}));
    EXPECT_EQ(splitContraction(u"they'd"),(std::vector<std::u16string>{u"they", u"'d"}));
    EXPECT_EQ(splitContraction(u"I've"),  (std::vector<std::u16string>{u"I", u"'ve"}));
    EXPECT_EQ(splitContraction(u"they're"),(std::vector<std::u16string>{u"they", u"'re"}));
    EXPECT_EQ(splitContraction(u"let's"), (std::vector<std::u16string>{u"let", u"'s"}));
    EXPECT_EQ(splitContraction(u"cat"),   std::vector<std::u16string>{});
}

TEST(SplitSentences, Basic) {
    auto s = splitSentences(u"Hello world. This is fun!");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].length, 12);   // "Hello world."
    EXPECT_EQ(s[1].start, 13);    // "This is fun!"
}

TEST(SplitSentences, EllipsisIsOneBoundary) {
    auto s = splitSentences(u"Wait... what?");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].length, 7);    // "Wait..."
}

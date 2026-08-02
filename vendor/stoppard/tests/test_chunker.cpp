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
#include "chunker.h"
using namespace stoppard;

static const Chunk *findChunk(const std::vector<Chunk> &cs, ChunkKind k) {
    for (const auto &c : cs) if (c.kind == k) return &c;
    return nullptr;
}

TEST(Chunker, NPWithUnknownModifiers) {
    auto t = tag(u"The big cat sat.");
    auto cs = chunk(t);
    auto *np = findChunk(cs, ChunkKind::NP);
    ASSERT_NE(np, nullptr);
    EXPECT_EQ(np->startToken, 0); EXPECT_EQ(np->endToken, 2);      // the big cat, head cat
}

TEST(Chunker, PronounIsNP) {
    auto t = tag(u"I can go.");
    auto cs = chunk(t);
    auto *np = findChunk(cs, ChunkKind::NP);
    ASSERT_NE(np, nullptr);
    EXPECT_EQ(np->startToken, 0); EXPECT_EQ(np->endToken, 0);
}

TEST(Chunker, PPIsPrepPlusNP) {
    auto t = tag(u"on the table");
    auto cs = chunk(t);
    auto *pp = findChunk(cs, ChunkKind::PP);
    ASSERT_NE(pp, nullptr);
    EXPECT_EQ(pp->startToken, 0); EXPECT_EQ(pp->endToken, 2);
}

TEST(Chunker, VPChain) {
    auto t = tag(u"has been go");
    auto cs = chunk(t);
    auto *vp = findChunk(cs, ChunkKind::VP);
    ASSERT_NE(vp, nullptr);
    EXPECT_EQ(vp->startToken, 0); EXPECT_EQ(vp->endToken, 2);      // aux aux verb
}

TEST(Chunker, SubjectDetection) {
    auto t = tag(u"The cat sat on the mat.");
    EXPECT_EQ(findSubjectHead(t, 2), 1);                   // cat (sat at 2)
    t = tag(u"I can go.");
    EXPECT_EQ(findSubjectHead(t, 2), 0);                   // I (go at 2)
    t = tag(u"We ran and they walked.");
    EXPECT_EQ(findSubjectHead(t, 1), 0);                   // We (ran at 1)
    EXPECT_EQ(findSubjectHead(t, 4), 3);                   // they (walked at 4)
    t = tag(u"Go away.");
    EXPECT_EQ(findSubjectHead(t, 0), -1);                  // imperative
}

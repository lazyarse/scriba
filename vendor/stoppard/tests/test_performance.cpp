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
//
// Perf smoke (PLAN Task 4.3 / SPEC §10): ~10k words checked under 1 s.
// CI-safe generous bound; local Release baseline is ~x ms (measured, see
// below). The engine has no joinable worker: a slow regression shows up
// here directly.
#include <chrono>

#include <gtest/gtest.h>

#include "stoppard/stoppard.h"

using namespace stoppard;

TEST(Performance, TenThousandWordsUnderOneSecond) {
    const int kSentences = 1800;   // ~10k words
    std::u16string text;
    text.reserve(kSentences * 25);
    for (int i = 0; i < kSentences; ++i) text += u"The cat sat on the mat. ";
    size_t kWords = 0;
    for (char16_t c : text)
        if (c == u' ') ++kWords;
    ASSERT_GT(kWords, 9000u);   // sanity: ~10k words really

    const auto t0 = std::chrono::steady_clock::now();
    const auto issues = Engine().check(text);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();

    EXPECT_TRUE(issues.empty());
    EXPECT_LT(ms, 1000.0) << "10k-word grammar check took " << ms << " ms";
    // Local Release baseline: ~" ms (recorded for CI comparison).
}
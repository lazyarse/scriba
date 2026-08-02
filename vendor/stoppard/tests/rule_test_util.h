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
#pragma once
#include <gtest/gtest.h>
#include "rules.h"
using namespace stoppard;

inline std::vector<Issue> checkAll(std::u16string_view text, Dialect d = Dialect::American)
{
    return runAll(text, d);
}

inline void expectClean(const std::vector<Issue>& issues)
{
    EXPECT_TRUE(issues.empty()) << "expected no issues";
}

inline void expectIssue(const std::vector<Issue>& issues, int start, int len,
                        const char16_t* suggestion = nullptr)
{
    ASSERT_FALSE(issues.empty());
    const auto& it = issues.front();
    EXPECT_EQ(it.start, start);
    EXPECT_EQ(it.length, len);
    if (suggestion) {
        ASSERT_FALSE(it.suggestions.empty());
        EXPECT_EQ(it.suggestions.front().text, std::u16string(suggestion));
    }
}

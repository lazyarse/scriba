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
// Shared helpers for data-file-driven tests (corpus, mutation, rule-case
// harnesses): UTF-8 -> UTF-16 conversion and line reading from TESTS_DATA_DIR.
#pragma once
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

inline std::u16string utf8ToUtf16(const std::string &s)
{
    std::u16string out;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i++]);
        char32_t cp = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 0x6) {
            cp = static_cast<char32_t>(c & 0x1F) << 6 | (s[i++] & 0x3F);
        } else if ((c >> 4) == 0xE) {
            cp = static_cast<char32_t>(c & 0xF) << 12 | (s[i++] & 0x3F) << 6 | (s[i++] & 0x3F);
        } else {
            cp = static_cast<char32_t>(c & 0x7) << 18 | (s[i++] & 0x3F) << 12
               | (s[i++] & 0x3F) << 6 | (s[i++] & 0x3F);
        }
        if (cp < 0x10000) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

inline std::vector<std::string> readLines(const std::string &path)
{
    std::vector<std::string> lines;
    std::ifstream f(path);
    EXPECT_TRUE(f.good()) << path;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}
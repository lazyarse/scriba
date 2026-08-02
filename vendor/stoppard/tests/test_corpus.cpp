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
// Clean-corpus zero-issue bar (SPEC §15 / PLAN Task 4.2): every sentence in
// tests/data/clean_corpus*.txt must produce no issues under its dialect.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "data_util.h"
#include "stoppard/stoppard.h"

using namespace stoppard;

namespace {

std::string name(Dialect d)
{
    switch (d) {
    case Dialect::American: return "american";
    case Dialect::British: return "british";
    case Dialect::Australian: return "australian";
    case Dialect::Indian: return "indian";
    case Dialect::Canadian: return "canadian";
    case Dialect::NewZealand: return "newzealand";
    }
    return "";
}

void expectCleanUnder(Dialect d, const std::string &file)
{
    const std::vector<std::string> lines =
        readLines(std::string(TESTS_DATA_DIR) + "/" + file);
    ASSERT_GT(lines.size(), 10u);
    Engine e(d);
    for (const auto &line : lines) {
        const auto issues = e.check(utf8ToUtf16(line));
        EXPECT_TRUE(issues.empty()) << file << ": " << line;
    }
}

} // namespace

TEST(Corpus, AmericanClean) {
    const std::vector<std::string> lines =
        readLines(std::string(TESTS_DATA_DIR) + "/clean_corpus.txt");
    ASSERT_GT(lines.size(), 100u);
    Engine e(Dialect::American);
    for (const auto &line : lines) {
        const auto issues = e.check(utf8ToUtf16(line));
        EXPECT_TRUE(issues.empty()) << "issue in clean line: " << line;
    }
}

TEST(Corpus, DialectFiles) {
    for (Dialect d : {Dialect::American, Dialect::British, Dialect::Australian,
                      Dialect::Indian, Dialect::Canadian, Dialect::NewZealand})
        expectCleanUnder(d, "clean_corpus_" + name(d) + ".txt");
}

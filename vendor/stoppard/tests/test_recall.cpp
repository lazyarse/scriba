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
// Recall corpus (SPEC §17.6 / §18.5). Unlike the rule-case harness (§15,
// exact span via LCS), Recall asserts **presence only**: the `wrong` side
// must produce at least one issue whose start falls within the LCS-derived
// mismatch span. Reordering errors (R13's "us one" -> "one of us") produce
// insert-only edit runs that no exact-span check could match; presence is
// what makes them testable. The LCS backtrack here prefers inserts on ties
// so the mismatch span covers the deleted wrong tokens (the pronoun in
// R13's swap), not the inserted right tokens.
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rule_test_util.h"

using namespace stoppard;

namespace {

struct Word {
    std::u16string text;
    size_t start;   // offset in the source string
};

std::vector<Word> splitWords(const std::u16string &s)
{
    std::vector<Word> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == u' ') { ++i; continue; }
        const size_t start = i;
        while (i < s.size() && s[i] != u' ') ++i;
        out.push_back({s.substr(start, i - start), start});
    }
    return out;
}

std::u16string utf8ToUtf16(const std::string &s)
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

bool isAsciiAlnum(char16_t c)
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9');
}

size_t strippedLen(const std::u16string &w)
{
    size_t len = w.size();
    while (len > 0 && !isAsciiAlnum(w[len - 1])) --len;
    return len;
}

// Merged mismatch span: from the first deleted wrong token's start to the
// last deleted wrong token's end (insert-first LCS backtrack). Insert-only
// edits yield a zero-length span at the insertion boundary.
struct Span {
    size_t start;
    size_t end;
    bool empty;   // no deleted wrong tokens
};

Span mismatchSpan(const std::vector<Word> &w, const std::vector<std::u16string> &c)
{
    std::vector<std::vector<size_t>> lcs(w.size() + 1,
                                         std::vector<size_t>(c.size() + 1, 0));
    for (size_t i = 1; i <= w.size(); ++i)
        for (size_t j = 1; j <= c.size(); ++j)
            lcs[i][j] = w[i - 1].text == c[j - 1]
                ? lcs[i - 1][j - 1] + 1
                : std::max(lcs[i - 1][j], lcs[i][j - 1]);

    size_t first = w.size(), last = w.size();
    size_t i = w.size(), j = c.size();
    while (i > 0 && j > 0) {
        if (w[i - 1].text == c[j - 1]) { --i; --j; }
        else if (lcs[i][j - 1] >= lcs[i - 1][j]) { --j; }   // insert preferred
        else {
            if (first == w.size()) first = i - 1;
            last = i - 1;
            --i;
        }
    }
    while (i > 0) { if (first == w.size()) first = i - 1; last = i - 1; --i; }

    if (first == w.size())
        return {0, 0, true};   // identical sides
    Span s{0, 0, false};
    s.start = w[first].start;
    s.end = w[last].start + strippedLen(w[last].text);
    return s;
}

std::vector<std::u16string> splitParts(const std::u16string &line)
{
    std::vector<std::u16string> parts;
    size_t p = 0;
    while (p < line.size()) {
        const size_t q = line.find(u" | ", p);
        parts.push_back(line.substr(p, q == std::u16string::npos ? std::u16string::npos : q - p));
        if (q == std::u16string::npos) break;
        p = q + 3;
    }
    return parts;
}

void runLine(const std::u16string &wrong, const std::u16string &right, Dialect d)
{
    const std::vector<Word> ww = splitWords(wrong);
    std::vector<std::u16string> rr;
    for (const Word &word : splitWords(right)) rr.push_back(word.text);
    const Span span = mismatchSpan(ww, rr);
    const std::vector<Issue> issues = checkAll(wrong, d);
    if (span.empty) {
        expectClean(issues);
        return;
    }
    bool found = false;
    for (const Issue &iss : issues) {
        if (iss.start >= static_cast<int>(span.start)
            && iss.start < static_cast<int>(span.end)) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "no issue within [" << span.start << "," << span.end
                       << ") for: " << std::string(wrong.begin(), wrong.end());
}

} // namespace

// SPEC §17.6: real-world error sentences, presence-only assertion.
TEST(RRecall, Corpus)
{
    const std::string path = std::string(TESTS_DATA_DIR) + "/recall_corpus.txt";
    std::ifstream f(path);
    ASSERT_TRUE(f.good()) << path;
    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        SCOPED_TRACE(path + ":" + std::to_string(lineno) + " " + line);
        const std::vector<std::u16string> parts = splitParts(utf8ToUtf16(line));
        ASSERT_EQ(parts.size(), 3u);
        const Dialect d = parts[2] == u"R9" ? Dialect::British : Dialect::American;
        runLine(parts[0], parts[1], d);
    }
}

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
// Data-driven rule-case matrix (PLAN Task 3.7 / SPEC §15). Each line of
// tests/data/rule_cases/R{n}_*.txt is `wrong | right | rule-id`; the
// expected issue for `wrong` is derived from the token-level diff of the
// two sides (LCS alignment): a mismatched run expects one issue spanning
// the deleted words, suggesting the inserted words (or Remove when the
// run only deletes).
#include <algorithm>
#include <cstdint>
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

struct Run {
    size_t wStart;   // first deleted word index
    size_t wEnd;     // last deleted word index
    size_t start;    // span in the wrong text
    size_t length;
    std::u16string suggestion;   // empty => Remove
    bool remove;
};

// Token-level LCS edit script between `w` and `c`; the contiguous delete
// runs become expected issues (delete-only run => Remove suggestion).
std::vector<Run> diffRuns(const std::vector<Word> &w, const std::vector<std::u16string> &c)
{
    std::vector<std::vector<size_t>> lcs(w.size() + 1,
                                         std::vector<size_t>(c.size() + 1, 0));
    for (size_t i = 1; i <= w.size(); ++i)
        for (size_t j = 1; j <= c.size(); ++j)
            lcs[i][j] = w[i - 1].text == c[j - 1]
                ? lcs[i - 1][j - 1] + 1
                : std::max(lcs[i - 1][j], lcs[i][j - 1]);

    // Edit script: keep (0, w-index), delete (1, w-index), insert (2, c-index).
    struct Op { int kind; size_t idx; };
    std::vector<Op> ops;
    size_t i = w.size(), j = c.size();
    while (i > 0 && j > 0) {
        if (w[i - 1].text == c[j - 1]) { ops.push_back({0, i - 1}); --i; --j; }
        else if (lcs[i - 1][j] >= lcs[i][j - 1]) { ops.push_back({1, i - 1}); --i; }
        else { ops.push_back({2, j - 1}); --j; }
    }
    while (i > 0) { ops.push_back({1, i - 1}); --i; }
    while (j > 0) { ops.push_back({2, j - 1}); --j; }
    std::reverse(ops.begin(), ops.end());

    std::vector<Run> runs;
    for (size_t k = 0; k < ops.size();) {
        if (ops[k].kind == 0) { ++k; continue; }
        Run r;
        r.wStart = r.wEnd = w.size();
        r.start = r.length = 0;
        r.remove = true;
        std::u16string sug;
        while (k < ops.size() && ops[k].kind != 0) {
            if (ops[k].kind == 1) {
                if (r.wStart == w.size()) r.wStart = ops[k].idx;
                r.wEnd = ops[k].idx;
            } else {
                if (!sug.empty()) sug += u' ';
                sug += c[ops[k].idx];
            }
            ++k;
        }
        if (r.wStart == w.size()) {   // insert-only edit: not representable
            runs.push_back(r);        // zero-length span => visible mismatch
            continue;
        }
        const Word &first = w[r.wStart];
        const Word &last = w[r.wEnd];
        r.start = first.start;
        r.length = last.start + strippedLen(last.text) - first.start;
        if (!sug.empty()) {
            r.remove = false;
            r.suggestion = sug.substr(0, strippedLen(sug));
        }
        runs.push_back(r);
    }
    return runs;
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
    const std::vector<Run> runs = diffRuns(ww, rr);
    const std::vector<Issue> issues = checkAll(wrong, d);
    if (runs.empty()) {
        expectClean(issues);
        return;
    }
    ASSERT_EQ(issues.size(), runs.size()) << "wrong: " << std::string(wrong.begin(), wrong.end());
    for (const Run &r : runs) {
        bool found = false;
        for (const Issue &iss : issues) {
            if (iss.start == static_cast<int>(r.start)
                && iss.length == static_cast<int>(r.length)) {
                found = true;
                ASSERT_FALSE(iss.suggestions.empty());
                if (r.remove) {
                    EXPECT_EQ(iss.suggestions[0].kind, SuggestionKind::Remove);
                    EXPECT_TRUE(iss.suggestions[0].text.empty());
                } else {
                    EXPECT_EQ(iss.suggestions[0].kind, SuggestionKind::Replace);
                    EXPECT_EQ(iss.suggestions[0].text, r.suggestion);
                }
            }
        }
        EXPECT_TRUE(found) << "no issue at (" << r.start << "," << r.length << ") for: "
                           << std::string(wrong.begin(), wrong.end());
    }
}

void runFile(const char *name, const char *family, Dialect d)
{
    const std::string path = std::string(TESTS_DATA_DIR) + "/rule_cases/" + name;
    std::ifstream f(path);
    ASSERT_TRUE(f.good()) << path;
    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        SCOPED_TRACE(name + std::string(":") + std::to_string(lineno) + " " + line);
        const std::vector<std::u16string> parts = splitParts(utf8ToUtf16(line));
        ASSERT_EQ(parts.size(), 3u);
        EXPECT_EQ(std::string(parts[2].begin(), parts[2].end()), family);
        runLine(parts[0], parts[1], d);
    }
}

} // namespace

TEST(RuleCases, R1) { runFile("R1_modal.txt", "R1", Dialect::American); }
TEST(RuleCases, R2) { runFile("R2_aux.txt", "R2", Dialect::American); }
TEST(RuleCases, R3) { runFile("R3_agreement.txt", "R3", Dialect::American); }
TEST(RuleCases, R4) { runFile("R4_to_infinitive.txt", "R4", Dialect::American); }
TEST(RuleCases, R5) { runFile("R5_determiner.txt", "R5", Dialect::American); }
TEST(RuleCases, R6) { runFile("R6_pronoun_case.txt", "R6", Dialect::American); }
TEST(RuleCases, R7) { runFile("R7_double_modal.txt", "R7", Dialect::American); }
TEST(RuleCases, R8) { runFile("R8_double_negative.txt", "R8", Dialect::American); }
TEST(RuleCases, R9) { runFile("R9_regionalisms.txt", "R9", Dialect::British); }

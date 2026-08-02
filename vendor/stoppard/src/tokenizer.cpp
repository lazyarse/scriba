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
#include "tokenizer.h"

#include <algorithm>
#include <cctype>
#include <array>

namespace stoppard {
namespace {

bool isWordCodeUnit(char16_t c)
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || c == u'\''
        || c >= 0x80;   // non-ASCII: covers Māori kōrero, emoji, etc.
}

bool isDigit(char16_t c) { return c >= u'0' && c <= u'9'; }

bool isWhitespace(char16_t c) { return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n'; }

bool startsWith(std::u16string_view s, size_t at, std::u16string_view prefix)
{
    return s.substr(at, prefix.size()) == prefix;
}

// Fenced block: "```" at line start -> closing "```" at line start, inclusive.
bool fencedBlockAt(std::u16string_view text, size_t at, size_t &end)
{
    if (!startsWith(text, at, u"```"))
        return false;
    const size_t lineStart = at;
    if (lineStart > 0 && text[lineStart - 1] != u'\n')
        return false;
    size_t nl = text.find(u'\n', lineStart + 3);
    if (nl == std::u16string_view::npos) { end = text.size(); return true; }
    for (size_t i = nl + 1; ; ) {
        // i is a line start (or EOF)
        if (i >= text.size()) { end = text.size(); return true; }
        if (startsWith(text, i, u"```")) { end = i + 3; return true; }
        const size_t next = text.find(u'\n', i);
        if (next == std::u16string_view::npos) { end = text.size(); return true; }
        i = next + 1;
    }
}

// Inline code: "`...`" run (backtick-delimited, no newlines).
bool inlineCodeAt(std::u16string_view text, size_t at, size_t &end)
{
    if (at >= text.size() || text[at] != u'`')
        return false;
    const size_t close = text.find(u'`', at + 1);
    if (close == std::u16string_view::npos) {
        end = text.size();
        return true;
    }
    if (text.substr(at, close - at + 1).find(u'\n') != std::u16string_view::npos)
        return false;   // multi-line run: not inline code
    end = close + 1;
    return true;
}

} // namespace

std::vector<Token> tokenize(std::u16string_view text)
{
    std::vector<Token> tokens;
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        // Markdown scan first: fenced blocks and inline code are opaque.
        size_t codeEnd = 0;
        if (fencedBlockAt(text, i, codeEnd) || inlineCodeAt(text, i, codeEnd)) {
            tokens.push_back({TokenKind::Code, static_cast<int>(i), static_cast<int>(codeEnd - i)});
            i = codeEnd;
            continue;
        }
        char16_t c = text[i];
        if (isWhitespace(c)) {
            size_t j = i + 1;
            while (j < n && isWhitespace(text[j])) ++j;
            tokens.push_back({TokenKind::Whitespace, static_cast<int>(i), static_cast<int>(j - i)});
            i = j;
        } else if (isWordCodeUnit(c)) {
            size_t j = i + 1;
            while (j < n && isWordCodeUnit(text[j])) ++j;
            tokens.push_back({TokenKind::Word, static_cast<int>(i), static_cast<int>(j - i)});
            i = j;
        } else if (isDigit(c)) {
            size_t j = i + 1;
            while (j < n && isDigit(text[j])) ++j;
            tokens.push_back({TokenKind::Number, static_cast<int>(i), static_cast<int>(j - i)});
            i = j;
        } else {
            tokens.push_back({TokenKind::Punctuation, static_cast<int>(i), 1});
            ++i;
        }
    }
    return tokens;
}

std::vector<std::u16string> splitContraction(std::u16string_view word)
{
    struct Irregular { std::u16string_view form; std::u16string_view stem; };
    static constexpr std::array<Irregular, 3> irregulars = {
        Irregular{u"won't", u"will"}, Irregular{u"can't", u"can"}, Irregular{u"shan't", u"shall"},
    };
    for (const auto &ir : irregulars) {
        if (word == ir.form)
            return {std::u16string(ir.stem), u"n't"};
    }
    static constexpr std::array<std::u16string_view, 7> suffixes = {
        u"n't", u"'s", u"'ll", u"'d", u"'ve", u"'re", u"'m",
    };
    static constexpr std::array<std::u16string_view, 33> stems = {
        u"I", u"It", u"you", u"he", u"she", u"it", u"we", u"they", u"who", u"there", u"that",
        u"let", u"do", u"does", u"did", u"have", u"has", u"had", u"would", u"should",
        u"could", u"might", u"must", u"will", u"shall", u"are", u"is", u"am", u"was",
        u"were", u"need", u"dare", u"o",
    };
    for (const auto &suffix : suffixes) {
        if (word.size() > suffix.size()
            && word.substr(word.size() - suffix.size()) == suffix) {
            const auto stem = word.substr(0, word.size() - suffix.size());
            if (std::find(stems.begin(), stems.end(), stem) != stems.end())
                return {std::u16string(stem), std::u16string(suffix)};
        }
    }
    return {};
}

std::vector<SentenceSpan> splitSentences(std::u16string_view text)
{
    std::vector<SentenceSpan> spans;
    const size_t n = text.size();
    size_t start = 0;
    size_t i = 0;
    while (i < n) {
        const char16_t c = text[i];
        if (c != u'.' && c != u'!' && c != u'?') { ++i; continue; }
        size_t j = i + 1;
        while (j < n && (text[j] == u'.' || text[j] == u'!' || text[j] == u'?')) ++j;
        // Boundary fires if followed by whitespace then an uppercase ASCII letter.
        // An ellipsis run (3+ boundary puncts) also fires before a lowercase letter
        // ("Wait... what?"). Known limitation: abbreviations like "Dr." split.
        // Sentence splitting only bounds clause context, never gates rules directly.
        const bool ellipsis = (j - i) >= 3;
        size_t k = j;
        if (k < n && isWhitespace(text[k])) {
            while (k < n && isWhitespace(text[k])) ++k;
            const bool upper = k < n && text[k] >= u'A' && text[k] <= u'Z';
            const bool anyLetter = k < n && ((text[k] >= u'a' && text[k] <= u'z') || upper);
            if (k < n && (upper || (ellipsis && anyLetter))) {
                spans.push_back({static_cast<int>(start), static_cast<int>(j - start)});
                start = k;
                i = k;
                continue;
            }
        }
        i = j;
    }
    if (start < n)
        spans.push_back({static_cast<int>(start), static_cast<int>(n - start)});
    return spans;
}

} // namespace stoppard

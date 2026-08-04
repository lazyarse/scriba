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
#include "spellcheck.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <queue>
#include <sstream>

#include "lexicon.h"
#include "morphology.h"
#include "tokenizer.h"

namespace stoppard {

// --- accent folding ---------------------------------------------------------
// Precomposed accented Latin letters -> ASCII base. Covers Latin-1
// supplement (0xC0-0xFF) and Latin Extended-A (0x100-0x17F, incl. the
// macron vowels used by te reo Maori). Lookup folds both the token and the
// dictionary, so "whanau" and "whānau" hit the same entry (SPEC §19.7).

namespace {

char16_t foldLatin1(char16_t c)
{
    // Latin-1 supplement: mostly "remove the accent"; 'x'/'÷'/'ß' stay put.
    static constexpr char16_t kTable[64] = {
        u'a', u'a', u'a', u'a', u'a', u'a', u'a', u'c', u'e', u'e', u'e', u'e', u'i', u'i', u'i', u'i',
        u'd', u'n', u'o', u'o', u'o', u'o', u'o', u'x', u'o', u'u', u'u', u'u', u'u', u'y', u't', u's',
        u'a', u'a', u'a', u'a', u'a', u'a', u'a', u'c', u'e', u'e', u'e', u'e', u'i', u'i', u'i', u'i',
        u'd', u'n', u'o', u'o', u'o', u'o', u'o', u'/', u'o', u'u', u'u', u'u', u'u', u'y', u't', u'y',
    };
    return kTable[c - 0xC0];
}

char16_t foldLatinExtA(char16_t c)
{
    // Latin Extended-A (0x100-0x17F), one entry per code point, 8 per row.
    // 0x100-0x17F = 128 entries; the static_assert guards the count.
    static constexpr char16_t kTable[128] = {
        u'a', u'a', u'a', u'a', u'a', u'a', u'c', u'c',
        u'c', u'c', u'c', u'c', u'd', u'd', u'd', u'd',
        u'd', u'd', u'e', u'e', u'e', u'e', u'e', u'e',
        u'e', u'e', u'e', u'e', u'g', u'g', u'g', u'g',
        u'g', u'g', u'g', u'g', u'h', u'h', u'h', u'h',
        u'i', u'i', u'i', u'i', u'i', u'i', u'i', u'i',
        u'i', u'i', u'i', u'i', u'j', u'j', u'k', u'k',
        u'k', u'l', u'l', u'l', u'l', u'l', u'l', u'l',
        u'l', u'l', u'l', u'l', u'n', u'n', u'n', u'n',
        u'n', u'n', u'n', u'n', u'o', u'o', u'o', u'o',
        u'o', u'o', u'o', u'o', u'r', u'r', u'r', u'r',
        u'r', u'r', u's', u's', u's', u's', u's', u's',
        u's', u's', u't', u't', u't', u't', u't', u't',
        u'u', u'u', u'u', u'u', u'u', u'u', u'u', u'u',
        u'u', u'u', u'u', u'u', u'w', u'w', u'y', u'y',
        u'y', u'z', u'z', u'z', u'z', u'z', u'z', u's',
    };
    static_assert(sizeof(kTable) / sizeof(kTable[0]) == 128);
    return kTable[c - 0x100];
}

char16_t foldChar(char16_t c)
{
    if (c >= u'A' && c <= u'Z')
        return char16_t(c + 0x20);
    if (c >= 0xC0 && c <= 0xFF)
        return foldLatin1(c);
    if (c >= 0x100 && c <= 0x17F)
        return foldLatinExtA(c);
    return c;
}

std::u16string fold(std::u16string_view s)
{
    std::u16string out;
    out.reserve(s.size());
    for (char16_t c : s)
        out.push_back(foldChar(c));
    return out;
}

bool hasDigit(std::u16string_view w)
{
    return std::any_of(w.begin(), w.end(), [](char16_t c) { return c >= u'0' && c <= u'9'; });
}

bool isAllCaps(std::u16string_view w)
{
    bool anyLetter = false;
    for (char16_t c : w) {
        if (c >= u'a' && c <= u'z')
            return false;
        if ((c >= u'A' && c <= u'Z') || c >= 0xC0)
            anyLetter = true;
    }
    return anyLetter;   // "NASA" -> skip; "" -> false
}

} // namespace

// Exported alias of the anonymous-namespace fold(), so Engine::setUserWords
// can normalize user-added words to the same folded form the dictionary and
// spelling lookup use.
std::u16string foldWord(std::u16string_view s) { return fold(s); }

// --- hunspell ngram machinery (SPEC §19.4) ----------------------------------
// Faithful ports of suggestmgr.cxx: ngram() (multi-size substring-anywhere
// matching with early break), leftcommonsubstring(), commoncharacterpositions()
// and the final gscore re-rank with lcslen().

namespace {

constexpr int kNgramLongerWorse = 1;
constexpr int kNgramAnyMismatch = 2;
constexpr int kNgramWeighted = 8;

// hunspell ngram(): for j = 1..n, counts how many j-grams of s1 occur
// anywhere in s2; breaks out when a level finds < 2 matches (unless
// weighted). Optionally penalizes a length mismatch between the two.
int ngram(int n, std::u16string_view s1, std::u16string_view s2, int opt)
{
    if (s2.empty())
        return 0;
    const int l1 = int(s1.size()), l2 = int(s2.size());
    int nscore = 0;
    for (int j = 1; j <= n; ++j) {
        int ns = 0;
        for (int i = 0; i <= l1 - j; ++i) {
            const auto needle = s1.substr(i, j);
            if (s2.find(needle) != std::u16string_view::npos) {
                ++ns;
            } else if (opt & kNgramWeighted) {
                --ns;
                if (i == 0 || i == l1 - j)
                    --ns;   // side weight
            }
        }
        nscore += ns;
        if (ns < 2 && !(opt & kNgramWeighted))
            break;
    }
    int penalty = 0;
    if (opt & kNgramLongerWorse)
        penalty = (l2 - l1) - 2;
    if (opt & kNgramAnyMismatch)
        penalty = std::abs(l2 - l1) - 2;
    return nscore - (penalty > 0 ? penalty : 0);
}

// Length of the common prefix (both strings already folded).
int leftCommonSubstring(std::u16string_view s1, std::u16string_view s2)
{
    size_t i = 0;
    while (i < s1.size() && i < s2.size() && s1[i] == s2[i])
        ++i;
    return int(i);
}

// Count of equal character positions; isSwap set when the only two
// differences are a transposition.
int commonCharacterPositions(std::u16string_view s1, std::u16string_view s2, bool& isSwap)
{
    isSwap = false;
    int num = 0, diff = 0, diffpos[2] = {0, 0};
    const size_t n = std::min(s1.size(), s2.size());
    for (size_t i = 0; i < n; ++i) {
        if (s1[i] == s2[i]) {
            ++num;
        } else if (diff < 2) {
            diffpos[diff] = int(i);
            ++diff;
        } else {
            ++diff;
        }
    }
    if (diff == 2 && s1.size() == s2.size() &&
        s1[diffpos[0]] == s2[diffpos[1]] && s1[diffpos[1]] == s2[diffpos[0]])
        isSwap = true;
    return num;
}

// Longest common subsequence length (plain DP; hunspell's lcslen()).
int lcslen(std::u16string_view a, std::u16string_view b)
{
    const int m = int(a.size()), n = int(b.size());
    if (m == 0 || n == 0)
        return 0;
    std::vector<int> prev(n + 1, 0), cur(n + 1, 0);
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j)
            cur[j] = (a[i - 1] == b[j - 1]) ? prev[j - 1] + 1
                                            : std::max(prev[j], cur[j - 1]);
        prev.swap(cur);
    }
    return prev[n];
}

} // namespace

// --- SpellData --------------------------------------------------------------

namespace {

// Parses a plain word list: one word per line, '#' comments, blank lines
// skipped. Words are folded (lowercase, de-accented) into the set.
std::unordered_set<std::u16string> readWordList(std::string_view path)
{
    std::unordered_set<std::u16string> words;
    std::ifstream in{std::string(path)};
    if (!in.is_open())
        return words;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        words.insert(fold(std::u16string(line.begin(), line.end())));
    }
    return words;
}

} // namespace

std::shared_ptr<const SpellData> SpellData::load(std::string_view enUSPath,
                                                 std::string_view enGBPath,
                                                 std::string_view maoriPath,
                                                 std::string_view canadianPath)
{
    auto data = std::make_shared<SpellData>();
    data->m_us = readWordList(enUSPath);
    data->m_gb = readWordList(enGBPath);
    data->m_maori = readWordList(maoriPath);
    data->m_canadian = readWordList(canadianPath);

    // Length buckets for the ngramsuggest scan (|len(typo) - len(word)| <= 4).
    auto buildBuckets = [](const std::unordered_set<std::u16string>& words) {
        std::vector<std::vector<std::u16string_view>> buckets;
        size_t maxLen = 0;
        for (const auto &w : words)
            maxLen = std::max(maxLen, w.size());
        buckets.resize(maxLen + 1);
        for (const auto &w : words)
            buckets[w.size()].push_back(w);
        return buckets;
    };
    data->m_usByLen = buildBuckets(data->m_us);
    data->m_gbByLen = buildBuckets(data->m_gb);

    // Keyboard tables (SPEC §19.4): TRY string + REP pairs, same format the
    // fetch script writes into data/keyboard-*.txt.
    auto keyboardPath = std::string(enGBPath);
    // keyboard files sit next to the dictionaries; named per dictionary.
    // (The two keyboard files are identical — one shared table per SPEC.)
    const std::string kbPath = [&] {
        const auto slash = keyboardPath.find_last_of("/\\");
        const auto prefix = slash == std::string::npos ? std::string() : keyboardPath.substr(0, slash + 1);
        return prefix + "keyboard-en-GB.txt";
    }();
    std::ifstream in(kbPath);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ss(line);
            std::string key;
            if (!(ss >> key))
                continue;
            if (key == "TRY") {
                std::string letters;
                ss >> letters;
                std::unordered_set<char16_t> seen;
                for (char c : letters) {
                    const char16_t l = char16_t(std::tolower(static_cast<unsigned char>(c)));
                    if (std::isalpha(static_cast<unsigned char>(c)) && seen.insert(l).second)
                        data->m_try.push_back(l);
                }
            } else if (key == "REP") {
                std::string a, b;
                if ((ss >> a) && (ss >> b)) {
                    // hunspell converts '_' to space in REP outstrings at load
                    // (replist.cxx) — "alot a_lot" becomes "alot" -> "a lot".
                    std::replace(b.begin(), b.end(), '_', ' ');
                    data->m_rep.emplace_back(fold(std::u16string(a.begin(), a.end())),
                                             fold(std::u16string(b.begin(), b.end())));
                }
            }
        }
    }
    return data;
}

bool SpellData::isWord(Dialect dialect, Language language, std::u16string_view folded,
                       const std::unordered_set<std::u16string>& userWords) const
{
    const auto &dict = language == Language::American ? m_us : m_gb;
    if (dict.find(std::u16string(folded)) != dict.end())
        return true;
    if (dialect == Dialect::NewZealand && m_maori.find(std::u16string(folded)) != m_maori.end())
        return true;
    // Canadian (SPEC §19.3): American dictionary plus a short allowance list
    // of standard Canadian spellings absent from en-US ("centre", "colour"...).
    if (dialect == Dialect::Canadian && m_canadian.find(std::u16string(folded)) != m_canadian.end())
        return true;
    if (!userWords.empty() && userWords.find(std::u16string(folded)) != userWords.end())
        return true;
    return false;
}

// --- suggestions ------------------------------------------------------------

namespace {

// The suggestion pipeline mirrors hunspell's suggest() (simple pass in
// generator order) + ngsuggest() (dictionary scan) — SPEC §19.4. "good"
// tracks whether the REP pass produced anything; ngramsuggest only runs
// when it did not (hunspell: "try ngram approach since found nothing good").
struct SugContext {
    Dialect dialect;
    Language language;
    const SpellData *data;
    const std::unordered_set<std::u16string> *userWords;
    std::vector<std::u16string> wlst;
    bool good = false;

    bool isWord_(std::u16string_view w) const
    {
        return data->isWord(dialect, language, w, *userWords);
    }

    // hunspell testsug(): dedup + dictionary check + capacity guard.
    void addSug(std::u16string_view cand)
    {
        if (wlst.size() >= 5)
            return;
        if (std::find(wlst.begin(), wlst.end(), cand) != wlst.end())
            return;
        if (!isWord_(cand))
            return;
        wlst.emplace_back(cand);
    }
};

// hunspell replchars(): REP pairs first; each occurrence of the pattern is
// replaced (one at a time) and tested. REP candidates with a space ("alot"
// -> "a lot") are accepted via the post-chunk trick: each chunk after a
// space must itself be a dictionary word, then the full candidate is kept.
void replChars(SugContext &c, std::u16string_view w)
{
    for (const auto &[pat, repl] : c.data->repPairs()) {
        size_t r = 0;
        while ((r = w.find(pat, r)) != std::u16string_view::npos) {
            std::u16string cand(w.substr(0, r));
            cand += repl;
            cand += w.substr(r + pat.size());
            if (cand != w)
                c.addSug(cand);
            size_t sp = cand.find(u' ');
            size_t prev = 0;
            while (sp != std::u16string::npos) {
                const auto prevChunk = std::u16string_view(cand).substr(prev, sp - prev);
                if (c.isWord_(prevChunk)) {
                    const size_t oldns = c.wlst.size();
                    c.addSug(std::u16string_view(cand).substr(sp + 1));
                    if (c.wlst.size() > oldns)
                        c.wlst.back() = cand;
                }
                prev = sp + 1;
                sp = cand.find(u' ', prev);
            }
            ++r;
        }
    }
}

// hunspell swapchar(): adjacent transpositions, then double swaps for
// 4/5-character words.
void swapChar(SugContext &c, std::u16string_view w)
{
    if (w.size() < 2)
        return;
    for (size_t i = 0; i + 1 < w.size(); ++i) {
        std::u16string cand(w);
        std::swap(cand[i], cand[i + 1]);
        c.addSug(cand);
    }
    if (w.size() == 4 || w.size() == 5) {
        std::u16string cand(w);
        cand[0] = w[1];
        cand[1] = w[0];
        cand[2] = w[2];
        cand[cand.size() - 2] = w[w.size() - 1];
        cand[cand.size() - 1] = w[w.size() - 2];
        c.addSug(cand);
        if (w.size() == 5) {
            // second pattern builds on the first: positions 0..2 from the
            // original, the last-two swap from pattern A stays in place
            cand[0] = w[0];
            cand[1] = w[2];
            cand[2] = w[1];
            c.addSug(cand);
        }
    }
}

// hunspell longswapchar(): non-adjacent transpositions within distance 4.
void longSwapChar(SugContext &c, std::u16string_view w)
{
    for (size_t p = 0; p < w.size(); ++p) {
        for (size_t q = 0; q < w.size(); ++q) {
            const int dist = std::abs(int(q) - int(p));
            if (dist > 1 && dist <= 4) {
                std::u16string cand(w);
                std::swap(cand[p], cand[q]);
                c.addSug(cand);
            }
        }
    }
}

// hunspell extrachar(): delete each character, right to left.
void extraChar(SugContext &c, std::u16string_view w)
{
    if (w.size() < 2)
        return;
    for (size_t i = 0; i < w.size(); ++i) {
        const size_t idx = w.size() - 1 - i;
        std::u16string cand(w);
        cand.erase(idx, 1);
        c.addSug(cand);
    }
}

// hunspell forgotchar(): insert each TRY letter, right to left.
void forgotChar(SugContext &c, std::u16string_view w)
{
    for (char16_t ch : c.data->tryString()) {
        for (size_t i = 0; i <= w.size(); ++i) {
            const size_t idx = w.size() - i;
            std::u16string cand(w);
            cand.insert(idx, 1, ch);
            c.addSug(cand);
        }
    }
}

// hunspell movechar(): bubble each character up to 4 positions right, then
// left; adjacent moves (distance 1) are omitted.
void moveChar(SugContext &c, std::u16string_view w)
{
    for (size_t p = 0; p < w.size(); ++p) {
        std::u16string cand(w);
        for (size_t q = p + 1; q < w.size() && q - p <= 4; ++q) {
            std::swap(cand[q], cand[q - 1]);
            if (q - p >= 2)
                c.addSug(cand);
        }
    }
    for (size_t p = w.size(); p-- > 0;) {
        std::u16string cand(w);
        for (size_t q = p; q > 0 && p - (q - 1) <= 4; --q) {
            std::swap(cand[q], cand[q - 1]);
            if (p - (q - 1) >= 2)
                c.addSug(cand);
        }
    }
}

// hunspell badchar(): substitute each TRY letter, positions right to left.
void badChar(SugContext &c, std::u16string_view w)
{
    for (char16_t ch : c.data->tryString()) {
        for (size_t i = w.size(); i-- > 0;) {
            if (w[i] == ch)
                continue;
            std::u16string cand(w);
            cand[i] = ch;
            c.addSug(cand);
        }
    }
}

// hunspell doubletwochars(): "vacation -> vacacation" pattern removal.
void doubleTwoChars(SugContext &c, std::u16string_view w)
{
    if (w.size() < 5)
        return;
    int state = 0;
    for (size_t i = 2; i < w.size(); ++i) {
        if (w[i] == w[i - 2]) {
            ++state;
            if (state == 3 || (state == 2 && i >= 4)) {
                std::u16string cand(w);
                cand.erase(i - 1, 1);
                c.addSug(cand);
                state = 0;
            }
        } else {
            state = 0;
        }
    }
}

// hunspell twowords(): splits after every character. A split whose spaced
// form is a dictionary-listed entry is a "word pair" — it clears the list
// and is inserted at the front (top priority). Otherwise, when no REP hit
// ("good") and capacity allows, splits into two dictionary words are
// appended (space, then dash for languages with dash usage — TRY contains
// 'a', so both fire for English).
void twoWords(SugContext &c, std::u16string_view w)
{
    if (w.size() < 3)
        return;
    const bool dashUsage = c.data->tryString().find(u'a') != std::u16string::npos;
    for (size_t p = 1; p < w.size(); ++p) {
        const auto left = w.substr(0, p);
        const auto right = w.substr(p);
        std::u16string cand(left);
        cand.push_back(u' ');
        cand += right;
        if (c.isWord_(cand)) {
            // dictionary word pair: top priority, drops other suggestions
            if (!c.good) {
                c.good = true;
                c.wlst.clear();
            }
            c.wlst.insert(c.wlst.begin(), cand);
            if (dashUsage) {
                cand[p] = u'-';
                if (c.isWord_(cand)) {
                    if (!c.good) {
                        c.good = true;
                        c.wlst.clear();
                    }
                    c.wlst.insert(c.wlst.begin(), cand);
                }
            }
            // fall through to the plain split (hunspell runs both)
        }
        cand[p] = u' ';   // restore spaced form for the plain split
        if (c.wlst.size() >= 5 || c.good)
            continue;
        if (!c.isWord_(left) || !c.isWord_(right))
            continue;
        if (std::find(c.wlst.begin(), c.wlst.end(), cand) == c.wlst.end())
            c.wlst.emplace_back(cand);
        if (dashUsage && left.size() > 1 && right.size() > 1 && c.wlst.size() < 5) {
            cand[p] = u'-';
            if (std::find(c.wlst.begin(), c.wlst.end(), cand) == c.wlst.end())
                c.wlst.emplace_back(std::move(cand));
        }
    }
}

// hunspell ngsuggest(): scan the dictionary for words within 4 characters
// of the typo, keep the 100 best by ngram(3)+leftcommon, filter by a
// mangled-word threshold, then re-rank with the LCS/position formula.
void ngramSuggest(SugContext &c, std::u16string_view typo)
{
    const int n = int(typo.size());
    const SpellData &d = *c.data;
    const auto &buckets = d.lengthBuckets(c.language);

    // Top-100 roots by ngram(3, typo, w, LONGER_WORSE) + leftcommon.
    struct Scored {
        std::u16string_view word;
        int sc;
    };
    auto scCmp = [](const Scored &a, const Scored &b) { return a.sc > b.sc; };  // min-heap
    std::priority_queue<Scored, std::vector<Scored>, decltype(scCmp)> heap(scCmp);
    const auto scanWord = [&](std::u16string_view w) {
        const int clen = int(w.size());
        if (std::abs(n - clen) > 4)
            return;
        const int sc = ngram(3, typo, w, kNgramLongerWorse) + leftCommonSubstring(typo, w);
        if (heap.size() < 100 || sc > heap.top().sc) {
            heap.push({w, sc});
            if (heap.size() > 100)
                heap.pop();
        }
    };
    const int lo = std::max(0, n - 4);
    const int hi = std::min(int(buckets.size()) - 1, n + 4);
    for (int len = lo; len <= hi; ++len)
        for (const auto &w : buckets[len])
            scanWord(w);
    if (c.dialect == Dialect::NewZealand)
        for (const auto &w : d.maoriWords())
            scanWord(w);
    for (const auto &w : *c.userWords)
        scanWord(w);

    // Threshold: score the typo against three mangled versions of itself.
    int thresh = 0;
    for (int sp = 1; sp < 4; ++sp) {
        std::u16string mw(typo);
        for (int k = sp; k < n; k += 4)
            mw[k] = u'*';
        thresh += ngram(n, typo, mw, kNgramAnyMismatch);
    }
    thresh = thresh / 3 - 1;

    // Guesses: roots re-scored with the full-size ngram, thresholded.
    std::vector<Scored> guesses;
    guesses.reserve(heap.size());
    std::vector<Scored> roots;
    roots.reserve(heap.size());
    while (!heap.empty()) {
        roots.push_back(heap.top());
        heap.pop();
    }
    for (const auto &r : roots) {
        const int sc = ngram(n, typo, r.word, kNgramAnyMismatch) + leftCommonSubstring(typo, r.word);
        if (sc > thresh)
            guesses.push_back({r.word, sc});
    }
    std::stable_sort(guesses.begin(), guesses.end(),
                     [](const Scored &a, const Scored &b) { return a.sc > b.sc; });

    // Final re-rank (suggestmgr.cxx "weight suggestions with a similarity
    // index"): LCS, position matches, weighted 2-grams, side penalties.
    std::vector<int> gscores(guesses.size());
    for (size_t i = 0; i < guesses.size(); ++i) {
        const std::u16string_view w = guesses[i].word;
        const int len = int(w.size());
        const int lcs = lcslen(typo, w);
        if (n == len && n == lcs) {
            gscores[i] = guesses[i].sc + 2000;   // identical word: stop re-ranking
            break;
        }
        const int leftcommon = leftCommonSubstring(typo, w);
        bool isSwap = false;
        const int ccp = commonCharacterPositions(typo, w, isSwap);
        const int ngram4 = ngram(4, typo, w, kNgramAnyMismatch);
        int re = ngram(2, typo, w, kNgramAnyMismatch | kNgramWeighted);
        re += ngram(2, w, typo, kNgramAnyMismatch | kNgramWeighted);
        gscores[i] = 2 * lcs - std::abs(n - len) + leftcommon
                   + (ccp ? 1 : 0) + (isSwap ? 10 : 0)
                   + ngram4 + re
                   + (re < (n + len) ? -1000 : 0);
    }

    // Zip-sort by gscore descending, then append with hunspell's
    // excellent/negative cutoffs and substring-dedup against wlst.
    std::vector<size_t> order(guesses.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return gscores[a] > gscores[b]; });

    const size_t oldns = c.wlst.size();
    int same = 0;
    for (size_t idx : order) {
        if (c.wlst.size() >= oldns + 4 || c.wlst.size() >= 5)
            break;
        const int gscore = gscores[idx];
        if (same && gscore <= 1000)
            continue;
        if (gscore > 1000)
            same = 1;
        else if (gscore < -100) {
            same = 1;
            if (c.wlst.size() > oldns)
                continue;
        }
        const std::u16string_view w = guesses[idx].word;
        bool unique = true;
        for (const auto &j : c.wlst)
            if (w.find(j) != std::u16string::npos) {
                unique = false;
                break;
            }
        // hunspell also re-checks the guess against the dictionary here
        if (unique && !c.isWord_(w))
            unique = false;
        if (unique)
            c.wlst.emplace_back(w);
    }
}

} // namespace

std::vector<std::u16string> SpellData::suggestions(Dialect dialect, Language language,
                                                   std::u16string_view folded,
                                                   const std::unordered_set<std::u16string>& userWords) const
{
    SugContext c{dialect, language, this, &userWords, {}, false};
    const size_t beforeRep = 0;
    replChars(c, folded);
    if (c.wlst.size() > beforeRep)
        c.good = true;   // REP hit: "good" suggestion, closes the ngram gate
    swapChar(c, folded);
    longSwapChar(c, folded);
    extraChar(c, folded);
    forgotChar(c, folded);
    moveChar(c, folded);
    badChar(c, folded);
    doubleTwoChars(c, folded);
    twoWords(c, folded);   // may set good on a dictionary word pair
    if (!c.good)
        ngramSuggest(c, folded);
    return std::move(c.wlst);
}

// --- the pass ---------------------------------------------------------------

namespace {

// One flagged token -> one Issue covering the whole token, suggestions
// case-matched to the source (§19.5).
Issue makeIssue(int start, int length, std::u16string_view source,
                std::vector<std::u16string> suggs)
{
    std::u16string message;
    if (!suggs.empty())
        message = u"Did you mean \"" + suggs.front() + u"\"?";
    else
        message = u"Unknown word";
    Issue it;
    it.start = start;
    it.length = length;
    it.message = std::move(message);
    for (auto &s : suggs)
        it.suggestions.push_back({SuggestionKind::Replace, matchCase(source, std::move(s))});
    return it;
}

} // namespace

std::u16string flaggedBase(std::u16string_view word, Dialect dialect, Language language,
                           const std::unordered_set<std::u16string>& userWords,
                           const SpellData& data)
{
    // Token policy (SPEC §19.5): digits, hyphenated compounds, and all-caps
    // words are skipped.
    if (word.empty() || hasDigit(word) || word.find(u'-') != std::u16string::npos
        || isAllCaps(word))
        return {};

    // Closed-class contractions decompose ("don't" -> do + n't); the
    // fragments are closed-lexicon or punctuation-like.
    const auto parts = splitContraction(word);
    if (!parts.empty()) {
        for (const auto &p : parts) {
            const auto f = fold(p);
            if (f.empty() || f.find(u'\'') != std::u16string::npos)
                continue;
            if (lookupWord(f) != nullptr)
                continue;
            if (!data.isWord(dialect, language, f, userWords))
                return fold(word);   // flag the whole token
        }
        return {};
    }

    // Possessive: strip trailing 's and check the base (the tokenizer does
    // not decompose open-class possessives).
    if (word.size() > 2 && word.ends_with(u"'s")) {
        const auto base = word.substr(0, word.size() - 2);
        if (base.find(u'\'') != std::u16string::npos)
            return {};   // "O'Brien's"-style: skip
        const auto f = fold(base);
        if (lookupWord(f) == nullptr && !data.isWord(dialect, language, f, userWords))
            return f;
        return {};
    }

    // Any remaining internal apostrophe is a proper-noun pattern
    // ("O'Brien") -> skip.
    if (word.find(u'\'') != std::u16string::npos)
        return {};

    const auto f = fold(word);
    if (lookupWord(f) != nullptr)
        return {};   // closed-lexicon immunity
    if (!data.isWord(dialect, language, f, userWords))
        return f;
    return {};
}

std::vector<Issue> runSpelling(std::u16string_view text, Dialect dialect, Language language,
                               const std::vector<std::u16string>& userWords,
                               const SpellData& data)
{
    if (language == Language::None)
        return {};
    std::unordered_set<std::u16string> userSet(userWords.begin(), userWords.end());

    std::vector<Issue> issues;
    for (const Token &tok : tokenize(text)) {
        if (tok.kind != TokenKind::Word || tok.length <= 0)
            continue;
        const std::u16string word(text.substr(tok.start, tok.length));
        const auto base = flaggedBase(word, dialect, language, userSet, data);
        if (base.empty())
            continue;
        issues.push_back(makeIssue(tok.start, tok.length, word,
                                   data.suggestions(dialect, language, base, userSet)));
    }
    return issues;
}
} // namespace stoppard
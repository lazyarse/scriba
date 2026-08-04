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
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "stoppard/stoppard.h"

namespace stoppard {

// Spelling (R14, SPEC §19). Immutable after load; safe to share across
// threads. Loaded from the paths a consumer supplies (plain word lists, one
// word per line — see data/ and scripts/fetch_dictionaries.py).
class SpellData {
public:
    static std::shared_ptr<const SpellData> load(std::string_view enUSPath,
                                                 std::string_view enGBPath,
                                                 std::string_view maoriPath,
                                                 std::string_view canadianPath = {});
    bool isWord(Dialect dialect, Language language, std::u16string_view folded,
                const std::unordered_set<std::u16string>& userWords) const;
    std::vector<std::u16string> suggestions(Dialect dialect, Language language,
                                            std::u16string_view folded,
                                            const std::unordered_set<std::u16string>& userWords) const;
    // Māori exemption list (§19.7) — the ngramsuggest scan unions it in for NZ.
    const std::unordered_set<std::u16string>& maoriWords() const { return m_maori; }
    // TRY letter string and REP pairs (§19.4 keyboard tables).
    const std::u16string& tryString() const { return m_try; }
    const std::vector<std::pair<std::u16string, std::u16string>>& repPairs() const { return m_rep; }
    // Length buckets for the ngramsuggest |len diff| <= 4 scan.
    const std::vector<std::vector<std::u16string_view>>&
    lengthBuckets(Language language) const
    {
        return language == Language::American ? m_usByLen : m_gbByLen;
    }

private:
    std::unordered_set<std::u16string> m_us;
    std::unordered_set<std::u16string> m_gb;
    std::unordered_set<std::u16string> m_maori;
    std::unordered_set<std::u16string> m_canadian;   // §19.3 Canadian spelling allowance
    // Length buckets over the folded sets (ngramsuggest's |len diff| <= 4 scan).
    std::vector<std::vector<std::u16string_view>> m_usByLen;
    std::vector<std::vector<std::u16string_view>> m_gbByLen;
    std::u16string m_try;   // folded keyboard/letter-fallback string
    std::vector<std::pair<std::u16string, std::u16string>> m_rep;
};

// Accent-insensitive, case-insensitive fold applied to dictionary entries
// and to every token before lookup. Exposed so the engine can fold user-word
// entries the same way (otherwise a capitalized add never matches the folded
// lookup). Mirrors dictionary handling in SpellData::load().
std::u16string foldWord(std::u16string_view s);

// Shared single-word spelling check (SPEC §19.5 token policy), used by
// runSpelling and the Engine's per-word API. Returns the folded form
// suggestions should be computed against when the word is flagged
// (contractions: the whole folded token; possessives: the folded base), or
// an empty string when the word is clean (policy skip, closed lexicon,
// accepted contraction, or dictionary hit).
std::u16string flaggedBase(std::u16string_view word, Dialect dialect, Language language,
                           const std::unordered_set<std::u16string>& userWords,
                           const SpellData& data);

// The dictionary pass (SPEC §19.2 trigger set): every Word token that is not
// closed-lexicon and not a contraction fragment, regardless of tagger output.
// Returns issues sorted by start, grammar-priority overlap already resolved
// by the caller (Engine::check).
std::vector<Issue> runSpelling(std::u16string_view text, Dialect dialect, Language language,
                               const std::vector<std::u16string>& userWords,
                               const SpellData& data);

} // namespace stoppard

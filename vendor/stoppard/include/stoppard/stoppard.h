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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace stoppard {

class SpellData;   // spelling dictionaries (§19); see spellcheck.h

enum class SuggestionKind { Replace, Remove, InsertAfter };

struct Suggestion { SuggestionKind kind = SuggestionKind::Replace; std::u16string text; };

struct Issue {
    int start = 0;
    int length = 0;
    std::u16string message;
    std::vector<Suggestion> suggestions;
};

enum class Dialect { American, British, Australian, Indian, Canadian, NewZealand };
enum class Language { None, American, British };   // spelling (§19); None = grammar-only

struct Regionalism {
    std::u16string wrong;       // lowercase phrase
    std::u16string correction;
};

struct DialectProfile {
    bool collectivePluralAgreement = false;
    std::vector<Regionalism> regionalisms;
};
DialectProfile profileFor(Dialect dialect);   // impl lands in M3

class Engine {
public:
    explicit Engine(Dialect dialect = Dialect::American);
    std::vector<Issue> check(std::u16string_view text) const;  // thread-safe; snapshots config at call start
    Dialect dialect() const;
    void setDialect(Dialect d);
    void setLanguage(Language l);   // spelling dictionary (§19); default None
    void setGrammar(bool on);       // grammar rules (default on); off = spelling-only
    void setUserWords(std::vector<std::u16string> words);  // atomic swap (§19.2)
    // Plain word-list dictionaries (one word per line). maoriPath and
    // canadianPath may be empty: the Māori exemption list (§19.7, consulted
    // under Dialect::NewZealand) and the Canadian spelling allowance
    // (§19.3, consulted under Dialect::Canadian). Loads the immutable
    // spelling data eagerly; call before check() for spelling to take
    // effect. Spelling is skipped silently if a path cannot be read.
    void setDictionaryPaths(std::string enUSPath, std::string enGBPath,
                            std::string maoriPath, std::string canadianPath = {});

    // Per-word spelling queries (SPEC §19.5): the same token policy and
    // dictionary checks the spelling pass inside check() applies to each
    // token. Policy-skipped words (digits, hyphenated compounds, all-caps)
    // are never misspelled. spellSuggestions() returns the case-matched
    // suggestion list, empty when the word is clean or spelling is off.
    bool isMisspelled(std::u16string_view word) const;
    std::vector<std::u16string> spellSuggestions(std::u16string_view word) const;

private:
    struct Config {
        Dialect dialect = Dialect::American;
        Language language = Language::None;
        bool grammar = true;
        std::vector<std::u16string> userWords;
    };
    // Snapshot pointers, guarded by a mutex: std::atomic<std::shared_ptr>
    // (P0718R2) is not implemented in libc++, so it fails on macOS.
    std::shared_ptr<const Config> m_config;
    std::shared_ptr<const SpellData> m_spellData;
    mutable std::mutex m_configMutex;
    mutable std::mutex m_spellDataMutex;
    std::shared_ptr<const Config> config() const
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        return m_config;
    }
    void setConfig(std::shared_ptr<const Config> cfg)
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        m_config = std::move(cfg);
    }
    std::shared_ptr<const SpellData> spellData() const
    {
        std::lock_guard<std::mutex> lk(m_spellDataMutex);
        return m_spellData;
    }
    void setSpellData(std::shared_ptr<const SpellData> data)
    {
        std::lock_guard<std::mutex> lk(m_spellDataMutex);
        m_spellData = std::move(data);
    }
};

} // namespace stoppard

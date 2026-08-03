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
#include "stoppard/stoppard.h"

#include <unordered_set>

#include "morphology.h"
#include "rules.h"
#include "spellcheck.h"

namespace stoppard {

Engine::Engine(Dialect dialect)
    : m_config(std::make_shared<const Config>(Config{dialect, Language::None, {}}))
{
}

Dialect Engine::dialect() const { return m_config.load()->dialect; }

void Engine::setDialect(Dialect d)
{
    auto cfg = m_config.load();
    m_config.store(std::make_shared<const Config>(
        Config{d, cfg->language, cfg->userWords}));
}

void Engine::setLanguage(Language l)
{
    auto cfg = m_config.load();
    m_config.store(std::make_shared<const Config>(
        Config{cfg->dialect, l, cfg->userWords}));
}

void Engine::setUserWords(std::vector<std::u16string> words)
{
    auto cfg = m_config.load();
    m_config.store(std::make_shared<const Config>(
        Config{cfg->dialect, cfg->language, std::move(words)}));
}

void Engine::setDictionaryPaths(std::string enUSPath, std::string enGBPath,
                                std::string maoriPath, std::string canadianPath)
{
    m_spellData.store(SpellData::load(enUSPath, enGBPath, maoriPath, canadianPath));
}

bool Engine::isMisspelled(std::u16string_view word) const
{
    auto cfg = m_config.load();
    if (cfg->language == Language::None)
        return false;
    auto data = m_spellData.load();
    if (!data)
        return false;
    std::unordered_set<std::u16string> userSet(cfg->userWords.begin(), cfg->userWords.end());
    return !flaggedBase(word, cfg->dialect, cfg->language, userSet, *data).empty();
}

std::vector<std::u16string> Engine::spellSuggestions(std::u16string_view word) const
{
    auto cfg = m_config.load();
    if (cfg->language == Language::None)
        return {};
    auto data = m_spellData.load();
    if (!data)
        return {};
    std::unordered_set<std::u16string> userSet(cfg->userWords.begin(), cfg->userWords.end());
    const auto base = flaggedBase(word, cfg->dialect, cfg->language, userSet, *data);
    if (base.empty())
        return {};
    auto suggs = data->suggestions(cfg->dialect, cfg->language, base, userSet);
    for (auto &s : suggs)
        s = matchCase(word, std::move(s));
    return suggs;
}

std::vector<Issue> Engine::check(std::u16string_view text) const
{
    // Snapshot the config once; grammar and spelling then see one
    // consistent view, so the no-mutex thread-safety contract holds.
    auto cfg = m_config.load();
    auto issues = runAll(text, cfg->dialect);
    if (cfg->language != Language::None) {
        auto data = m_spellData.load();
        if (data) {
            auto spelling = runSpelling(text, cfg->dialect, cfg->language,
                                        cfg->userWords, *data);
            issues.insert(issues.end(), spelling.begin(), spelling.end());
        }
    }
    return dedupIssues(std::move(issues));
}

} // namespace stoppard

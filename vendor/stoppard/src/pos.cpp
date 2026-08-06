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
#include "pos.h"

#include <algorithm>
#include <array>
#include <vector>
#include "morphology.h"

namespace stoppard {
namespace pos {

namespace {

bool member(std::u16string_view word, const std::vector<std::u16string_view> &table)
{
    return std::find(table.begin(), table.end(), word) != table.end();
}

// Common regular base verbs (irregulars live in morphology's table). Curated,
// v1 precision-first: enough recall to make "too + base verb" practical.
const std::vector<std::u16string_view> kRegularBases{
    u"arrive", u"ask", u"bake", u"call", u"change", u"clean", u"cook", u"dance",
    u"depart", u"help", u"jump", u"learn", u"like", u"live", u"look", u"love",
    u"move", u"need", u"open", u"play", u"rest", u"shop", u"start", u"stay",
    u"stop", u"study", u"talk", u"travel", u"try", u"turn", u"use", u"visit",
    u"wait", u"walk", u"want", u"watch", u"work",
};

const std::vector<std::u16string_view> kInfinitiveTaking{
    u"afford", u"agree", u"appear", u"ask", u"begin", u"choose", u"continue",
    u"decide", u"deserve", u"expect", u"forget", u"hate", u"hesitate",
    u"hope", u"intend", u"learn", u"like", u"love", u"manage", u"mean",
    u"need", u"offer", u"plan", u"pretend", u"promise", u"refuse",
    u"remember", u"seem", u"start", u"struggle", u"tend", u"threaten",
    u"try", u"want", u"wish",
};

// Nouns that are prototypically possessed: body parts, kin, belongings, and
// common abstract possessables. Predicate/idiom heads ("time", "fun", "fact",
// "important", "true") are deliberately absent so "it's time to go." stays
// clean.
const std::vector<std::u16string_view> kPossessedNouns{
    u"absence", u"age", u"aim", u"appearance", u"arm", u"author", u"back",
    u"bag", u"behavior", u"behaviour", u"beginning", u"body", u"book",
    u"brain", u"brother", u"car", u"cat", u"cause", u"child", u"claw",
    u"coat", u"color", u"colour", u"computer", u"condition", u"creator",
    u"death", u"depth", u"design", u"destination", u"direction", u"dog",
    u"door", u"dream", u"ear", u"effect", u"end", u"eye", u"face",
    u"father", u"finger", u"floor", u"food", u"foot", u"friend", u"future",
    u"garden", u"goal", u"hair", u"hand", u"hat", u"head", u"health",
    u"heart", u"height", u"history", u"home", u"horn", u"house", u"idea",
    u"image", u"importance", u"key", u"leader", u"leg", u"length",
    u"level", u"life", u"limit", u"memory", u"method", u"middle", u"mind",
    u"money", u"mother", u"mouth", u"name", u"nature", u"neck", u"nose",
    u"origin", u"owner", u"partner", u"past", u"paw", u"performance",
    u"phone", u"photo", u"picture", u"position", u"present", u"purpose",
    u"quality", u"reason", u"result", u"role", u"roof", u"room", u"shape",
    u"shoulder", u"size", u"skin", u"son", u"song", u"soul", u"sound",
    u"spirit", u"state", u"stomach", u"story", u"successor", u"tail",
    u"team", u"thought", u"title", u"toe", u"tooth", u"voice", u"wall",
    u"wallet", u"weight", u"wife", u"width", u"window", u"wing", u"wish",
    u"word",
};

const std::vector<std::u16string_view> kDegreeWords{u"many", u"much", u"few", u"little"};

// Verbs whose lemma licenses a to + NP prepositional complement. Guards the
// mid-sentence to->too degree pattern: "give to many charities", "connected to
// many servers" stay clean (harper-derived; give/listen/talk/donate are the
// documented false-positive sources).
const std::vector<std::u16string_view> kToTaking{
    u"accede", u"admit", u"announce", u"attach", u"attend", u"belong",
    u"confess", u"conform", u"connect", u"contribute", u"donate", u"give",
    u"lead", u"listen", u"owe", u"point", u"pray", u"refer", u"relate",
    u"reply", u"resort", u"respond", u"revert", u"subscribe", u"surrender",
    u"talk", u"yield",
};

bool endsWith(std::u16string_view s, std::u16string_view suffix)
{
    return s.size() >= suffix.size() &&
           s.substr(s.size() - suffix.size()) == suffix;
}

std::u16string foldLower(std::u16string_view s)
{
    std::u16string out;
    out.reserve(s.size());
    for (char16_t c : s)
        out.push_back(c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c);
    return out;
}

} // namespace

bool isBaseVerb(std::u16string_view word)
{
    const std::u16string key = foldLower(word);
    if (member(key, kRegularBases)) return true;
    return isIrregularVerbBase(key);
}

bool isInfinitiveTakingVerb(std::u16string_view word)
{
    return member(foldLower(word), kInfinitiveTaking);
}

bool isPossessedNoun(std::u16string_view word)
{
    const std::u16string key = foldLower(word);
    // Check the surface form, then plausible singulars: "homes" -> home,
    // "abilities" -> ability, "boxes" -> box. Direct form first so "sadness"
    // (-ness) and "homes" (plural of table word) both match.
    std::array<std::u16string, 4> candidates;
    size_t n = 0;
    candidates[n++] = key;
    if (key.size() > 1 && key.back() == u's') {
        std::u16string s(key, 0, key.size() - 1);
        candidates[n++] = s;                       // homes -> home
        if (endsWith(key, u"ies") && key.size() > 3) {
            std::u16string y(key, 0, key.size() - 3);
            y.push_back(u'y');
            candidates[n++] = y;                   // abilities -> ability
        }
        if (endsWith(key, u"es") && key.size() > 2) {
            std::u16string e(key, 0, key.size() - 2);
            candidates[n++] = e;                   // boxes -> box
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (member(candidates[i], kPossessedNouns)) return true;
        for (std::u16string_view suffix : {u"tion", u"sion", u"ness", u"ment", u"ity"})
            if (endsWith(candidates[i], suffix)) return true;
    }
    return false;
}

bool isDegreeWord(std::u16string_view word)
{
    return member(foldLower(word), kDegreeWords);
}

bool isToTakingVerb(std::u16string_view word)
{
    // Check the lemma so participles ("connected", "gave") resolve to their
    // base; fall back to the surface form for bare non-table words.
    const VerbFormInfo form = classifyVerbForm(word);
    if (form.known) return member(form.lemma, kToTaking);
    return member(foldLower(word), kToTaking);
}

} // namespace pos
} // namespace stoppard

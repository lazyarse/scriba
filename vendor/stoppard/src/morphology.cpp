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
// Design decisions:
// - All inputs ASCII-lowercased for lookup; pluralize/singularize restore source case
//   (match-case: all-caps -> ALL-CAPS, capitalized -> Capitalized, else lower).
//   classifyVerbForm returns the lowercase lemma ("Went" -> "go").
// - Irregular verb table stores {base, third, past, participle}; an empty third
//   means "compute regular -s". Gerund is always computed by regular rules
//   (which handle be->being, die->dying).
// - classifyVerbForm: irregular table exact match first, then suffix rules
//   (-ing -> Gerund, -ed/-ied -> Past, -ies -> Third, -s/-es -> Third candidate;
//   noun ambiguity is the tagger's job). Past participle == past string for
//   regulars; classify reports Past (rules needing participles use lookupVerbForms).
// - Ambiguities inherent to English morphology without a dictionary are accepted
//   and documented where they occur (e.g. "rolled" classifies as lemma "rol").
#include "morphology.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace stoppard {
namespace {

struct VerbEntry { const char16_t* base; const char16_t* third; const char16_t* past; const char16_t* participle; };

constexpr VerbEntry kIrregularVerbs[] = {
    {u"arise", u"", u"arose", u"arisen"},
    {u"awake", u"", u"awoke", u"awoken"},
    {u"be", u"is", u"was", u"been"},
    {u"bear", u"", u"bore", u"borne"},
    {u"beat", u"", u"beat", u"beaten"},
    {u"become", u"", u"became", u"become"},
    {u"begin", u"", u"began", u"begun"},
    {u"bend", u"", u"bent", u"bent"},
    {u"bet", u"", u"bet", u"bet"},
    {u"bind", u"", u"bound", u"bound"},
    {u"bite", u"", u"bit", u"bitten"},
    {u"bleed", u"", u"bled", u"bled"},
    {u"blow", u"", u"blew", u"blown"},
    {u"break", u"", u"broke", u"broken"},
    {u"breed", u"", u"bred", u"bred"},
    {u"bring", u"", u"brought", u"brought"},
    {u"broadcast", u"", u"broadcast", u"broadcast"},
    {u"build", u"", u"built", u"built"},
    {u"burn", u"", u"burnt", u"burnt"},
    {u"burst", u"", u"burst", u"burst"},
    {u"buy", u"", u"bought", u"bought"},
    {u"cast", u"", u"cast", u"cast"},
    {u"catch", u"", u"caught", u"caught"},
    {u"choose", u"", u"chose", u"chosen"},
    {u"cling", u"", u"clung", u"clung"},
    {u"come", u"", u"came", u"come"},
    {u"cost", u"", u"cost", u"cost"},
    {u"creep", u"", u"crept", u"crept"},
    {u"cut", u"", u"cut", u"cut"},
    {u"deal", u"", u"dealt", u"dealt"},
    {u"dig", u"", u"dug", u"dug"},
    {u"do", u"does", u"did", u"done"},
    {u"draw", u"", u"drew", u"drawn"},
    {u"dream", u"", u"dreamt", u"dreamt"},
    {u"drink", u"", u"drank", u"drunk"},
    {u"drive", u"", u"drove", u"driven"},
    {u"dwell", u"", u"dwelt", u"dwelt"},
    {u"eat", u"", u"ate", u"eaten"},
    {u"fall", u"", u"fell", u"fallen"},
    {u"feed", u"", u"fed", u"fed"},
    {u"feel", u"", u"felt", u"felt"},
    {u"fight", u"", u"fought", u"fought"},
    {u"find", u"", u"found", u"found"},
    {u"flee", u"", u"fled", u"fled"},
    {u"fling", u"", u"flung", u"flung"},
    {u"fly", u"", u"flew", u"flown"},
    {u"forbid", u"", u"forbade", u"forbidden"},
    {u"forget", u"", u"forgot", u"forgotten"},
    {u"forgive", u"", u"forgave", u"forgiven"},
    {u"forgo", u"", u"forwent", u"forgone"},
    {u"freeze", u"", u"froze", u"frozen"},
    {u"get", u"", u"got", u"gotten"},
    {u"give", u"", u"gave", u"given"},
    {u"go", u"goes", u"went", u"gone"},
    {u"grind", u"", u"ground", u"ground"},
    {u"grow", u"", u"grew", u"grown"},
    {u"hang", u"", u"hung", u"hung"},
    {u"have", u"has", u"had", u"had"},
    {u"hear", u"", u"heard", u"heard"},
    {u"hide", u"", u"hid", u"hidden"},
    {u"hit", u"", u"hit", u"hit"},
    {u"hold", u"", u"held", u"held"},
    {u"hurt", u"", u"hurt", u"hurt"},
    {u"keep", u"", u"kept", u"kept"},
    {u"kneel", u"", u"knelt", u"knelt"},
    {u"knit", u"", u"knit", u"knit"},
    {u"know", u"", u"knew", u"known"},
    {u"lay", u"", u"laid", u"laid"},
    {u"lead", u"", u"led", u"led"},
    {u"lean", u"", u"leant", u"leant"},
    {u"leap", u"", u"leapt", u"leapt"},
    {u"learn", u"", u"learnt", u"learnt"},
    {u"leave", u"", u"left", u"left"},
    {u"lend", u"", u"lent", u"lent"},
    {u"let", u"", u"let", u"let"},
    {u"lie", u"", u"lay", u"lain"},
    {u"light", u"", u"lit", u"lit"},
    {u"lose", u"", u"lost", u"lost"},
    {u"make", u"", u"made", u"made"},
    {u"mean", u"", u"meant", u"meant"},
    {u"meet", u"", u"met", u"met"},
    {u"mistake", u"", u"mistook", u"mistaken"},
    {u"mow", u"", u"mowed", u"mown"},
    {u"overcome", u"", u"overcame", u"overcome"},
    {u"pay", u"", u"paid", u"paid"},
    {u"put", u"", u"put", u"put"},
    {u"quit", u"", u"quit", u"quit"},
    {u"read", u"", u"read", u"read"},
    {u"ride", u"", u"rode", u"ridden"},
    {u"ring", u"", u"rang", u"rung"},
    {u"rise", u"", u"rose", u"risen"},
    {u"run", u"", u"ran", u"run"},
    {u"saw", u"", u"sawed", u"sawn"},
    {u"say", u"says", u"said", u"said"},
    {u"see", u"", u"saw", u"seen"},
    {u"seek", u"", u"sought", u"sought"},
    {u"sell", u"", u"sold", u"sold"},
    {u"send", u"", u"sent", u"sent"},
    {u"set", u"", u"set", u"set"},
    {u"shake", u"", u"shook", u"shaken"},
    {u"shed", u"", u"shed", u"shed"},
    {u"shine", u"", u"shone", u"shone"},
    {u"shoot", u"", u"shot", u"shot"},
    {u"show", u"", u"showed", u"shown"},
    {u"shrink", u"", u"shrank", u"shrunk"},
    {u"shut", u"", u"shut", u"shut"},
    {u"sing", u"", u"sang", u"sung"},
    {u"sink", u"", u"sank", u"sunk"},
    {u"sit", u"", u"sat", u"sat"},
    {u"sleep", u"", u"slept", u"slept"},
    {u"slide", u"", u"slid", u"slid"},
    {u"slit", u"", u"slit", u"slit"},
    {u"smell", u"", u"smelt", u"smelt"},
    {u"speak", u"", u"spoke", u"spoken"},
    {u"speed", u"", u"sped", u"sped"},
    {u"spell", u"", u"spelt", u"spelt"},
    {u"spend", u"", u"spent", u"spent"},
    {u"spill", u"", u"spilt", u"spilt"},
    {u"spin", u"", u"spun", u"spun"},
    {u"spit", u"", u"spat", u"spat"},
    {u"split", u"", u"split", u"split"},
    {u"spoil", u"", u"spoilt", u"spoilt"},
    {u"spread", u"", u"spread", u"spread"},
    {u"spring", u"", u"sprang", u"sprung"},
    {u"stand", u"", u"stood", u"stood"},
    {u"steal", u"", u"stole", u"stolen"},
    {u"stick", u"", u"stuck", u"stuck"},
    {u"sting", u"", u"stung", u"stung"},
    {u"stink", u"", u"stank", u"stunk"},
    {u"strike", u"", u"struck", u"struck"},
    {u"string", u"", u"strung", u"strung"},
    {u"swear", u"", u"swore", u"sworn"},
    {u"sweep", u"", u"swept", u"swept"},
    {u"swim", u"", u"swam", u"swum"},
    {u"swing", u"", u"swung", u"swung"},
    {u"take", u"", u"took", u"taken"},
    {u"teach", u"", u"taught", u"taught"},
    {u"tear", u"", u"tore", u"torn"},
    {u"tell", u"", u"told", u"told"},
    {u"think", u"", u"thought", u"thought"},
    {u"throw", u"", u"threw", u"thrown"},
    {u"thrust", u"", u"thrust", u"thrust"},
    {u"understand", u"", u"understood", u"understood"},
    {u"wake", u"", u"woke", u"woken"},
    {u"wear", u"", u"wore", u"worn"},
    {u"weave", u"", u"wove", u"woven"},
    {u"weep", u"", u"wept", u"wept"},
    {u"win", u"", u"won", u"won"},
    {u"wind", u"", u"wound", u"wound"},
    {u"withdraw", u"", u"withdrew", u"withdrawn"},
    {u"wring", u"", u"wrung", u"wrung"},
    {u"write", u"", u"wrote", u"written"},
};

constexpr VerbEntry *kBe = nullptr;   // placeholder to keep array indexable; see findVerb below

const VerbEntry* findVerb(std::u16string_view base)
{
    const auto it = std::lower_bound(
        std::begin(kIrregularVerbs), std::end(kIrregularVerbs), base,
        [](const VerbEntry &e, std::u16string_view b) { return std::u16string_view(e.base) < b; });
    if (it == std::end(kIrregularVerbs) || std::u16string_view(it->base) != base)
        return nullptr;
    return it;
}

bool isVowel(char16_t c) { return c == u'a' || c == u'e' || c == u'i' || c == u'o' || c == u'u'; }

bool isCvc(std::u16string_view s)
{
    if (s.size() < 3) return false;
    const auto c0 = s[s.size() - 3], c1 = s[s.size() - 2], c2 = s[s.size() - 1];
    return !isVowel(c0) && isVowel(c1) && !isVowel(c2) && c2 != u'y' && c2 != u'w';
}

std::u16string thirdSingularOf(std::u16string_view base)
{
    if (base.size() >= 2 && (base.back() == u's' || base.back() == u'x' || base.back() == u'z'
                             || base.back() == u'o')) {
        if (base.back() == u'o' && base.size() >= 3 && (base[base.size() - 2] == u'o' || base[base.size() - 2] == u'e'))
            return std::u16string(base) + u"s";
        return std::u16string(base) + u"es";
    }
    if (base.size() >= 2 && base.back() == u'h'
        && (base[base.size() - 2] == u's' || base[base.size() - 2] == u'c'))
        return std::u16string(base) + u"es";
    if (base.size() >= 2 && base.back() == u'y' && !isVowel(base[base.size() - 2]))
        return std::u16string(base.substr(0, base.size() - 1)) + u"ies";
    return std::u16string(base) + u"s";
}

std::u16string pastOf(std::u16string_view base)
{
    if (base == u"be") return u"was";
    if (base.back() == u'e') {
        if (base.size() >= 2 && base[base.size() - 2] == u'e') return std::u16string(base) + u"d";
        return std::u16string(base) + u"d";
    }
    if (base.size() >= 2 && base.back() == u'y' && !isVowel(base[base.size() - 2]))
        return std::u16string(base.substr(0, base.size() - 1)) + u"ied";
    if (isCvc(base))
        return std::u16string(base) + std::u16string(1, base.back()) + u"ed";
    return std::u16string(base) + u"ed";
}

std::u16string gerundOf(std::u16string_view base)
{
    if (base == u"be") return u"being";   // single-letter stem keeps its e
    if (base.size() >= 2 && base.back() == u'e') {
        if (base[base.size() - 2] == u'e') return std::u16string(base) + u"ing";
        if (base.size() >= 2 && base[base.size() - 2] == u'i') return std::u16string(base.substr(0, base.size() - 2)) + u"ying";
        return std::u16string(base.substr(0, base.size() - 1)) + u"ing";
    }
    if (isCvc(base))
        return std::u16string(base) + std::u16string(1, base.back()) + u"ing";
    return std::u16string(base) + u"ing";
}

struct NounEntry { const char16_t* singular; const char16_t* plural; int number; };   // number: -1,+1,0(both)

constexpr NounEntry kIrregularNouns[] = {
    {u"child", u"children", -1},
    {u"deer", u"deer", 0},
    {u"fish", u"fish", 0},
    {u"foot", u"feet", -1},
    {u"goose", u"geese", -1},
    {u"man", u"men", -1},
    {u"mouse", u"mice", -1},
    {u"ox", u"oxen", -1},
    {u"person", u"people", -1},
    {u"series", u"series", 0},
    {u"sheep", u"sheep", 0},
    {u"species", u"species", 0},
    {u"tooth", u"teeth", -1},
    {u"woman", u"women", -1},
};

const NounEntry* findNoun(std::u16string_view word)
{
    for (const auto &e : kIrregularNouns) {
        if (word == e.singular || word == e.plural) return &e;
    }
    return nullptr;
}

// Number of the given word form: +1 for plural forms (children), -1 for singular,
// 0 for invariant (sheep).

// Nouns whose plural is plain -s despite the regular -es/-ves/-ies patterns.
bool isPlainSPlural(std::u16string_view word)
{
    static constexpr std::array<std::u16string_view, 6> kPlainS = {
        u"piano", u"photo", u"solo", u"halo", u"auto", u"kimono",
    };
    for (const auto &w : kPlainS)
        if (word == w) return true;
    return false;
}

std::u16string pluralizeLower(std::u16string_view word)
{
    if (const auto *e = findNoun(word)) {
        if (e->number == 0) return std::u16string(word);
        return e->plural;
    }
    if (isPlainSPlural(word)) return std::u16string(word) + u"s";
    const char16_t last = word.back();
    if (last == u's' || last == u'x' || last == u'z'
        || (last == u'h' && word.size() >= 2 && (word[word.size() - 2] == u's' || word[word.size() - 2] == u'c')))
        return std::u16string(word) + u"es";
    if (last == u'o' && !(word.size() >= 2 && isVowel(word[word.size() - 2])))
        return std::u16string(word) + u"es";
    if (last == u'y' && word.size() >= 2 && !isVowel(word[word.size() - 2]))
        return std::u16string(word.substr(0, word.size() - 1)) + u"ies";
    if (last == u'f')
        return std::u16string(word.substr(0, word.size() - 1)) + u"ves";
    if (last == u'e' && word.size() >= 2 && word[word.size() - 2] == u'f')
        return std::u16string(word.substr(0, word.size() - 2)) + u"ves";
    return std::u16string(word) + u"s";
}

std::u16string singularizeLower(std::u16string_view word)
{
    if (const auto *e = findNoun(word)) {
        if (e->number == 0) return std::u16string(word);
        return e->singular;
    }
    if (word.size() >= 4 && word.ends_with(u"ves")) {   // -ves -> -fe or -f, whichever roundtrips
        const std::u16string stem(word.substr(0, word.size() - 3));   // drop "ves"
        const std::u16string withFe = stem + u"fe";
        if (pluralizeLower(withFe) == word) return withFe;
        const std::u16string withF = stem + u"f";
        if (pluralizeLower(withF) == word) return withF;
    }
    if (word.size() >= 4 && word.ends_with(u"ies")
        && !isVowel(word[word.size() - 4])) {
        return std::u16string(word.substr(0, word.size() - 3)) + u"y";   // babies -> baby
    }
    if (word.size() >= 2 && word.ends_with(u"es")) {
        const std::u16string stem(word.substr(0, word.size() - 2));
        if (pluralizeLower(stem) == word) return stem;
        return std::u16string(word.substr(0, word.size() - 1));   // fall back to -s strip
    }
    if (word.size() >= 2 && word.ends_with(u"s")) {
        const std::u16string stem(word.substr(0, word.size() - 1));
        if (pluralizeLower(stem) == word) return stem;
    }
    return std::u16string(word);
}

enum class Case { Lower, Capitalized, Upper };

Case sourceCase(std::u16string_view word)
{
    if (word.empty()) return Case::Lower;
    bool anyUpper = false, anyLower = false;
    for (char16_t c : word) {
        if (c >= u'A' && c <= u'Z') anyUpper = true;
        else if (c >= u'a' && c <= u'z') anyLower = true;
    }
    if (anyUpper && !anyLower) return Case::Upper;
    if (!anyUpper && anyLower) return Case::Lower;
    if (word[0] >= u'A' && word[0] <= u'Z') return Case::Capitalized;
    return Case::Lower;
}

std::u16string toLower(std::u16string_view word)
{
    std::u16string out;
    out.reserve(word.size());
    for (char16_t c : word)
        out.push_back(c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c);
    return out;
}

} // namespace

bool isIrregularVerbBase(std::u16string_view word)
{
    const std::u16string key = toLower(word);
    return findVerb(key) != nullptr;
}

std::u16string matchCase(std::u16string_view source, std::u16string out)
{
    switch (sourceCase(source)) {
    case Case::Upper:
        for (auto &c : out) c = static_cast<char16_t>(std::toupper(c));
        break;
    case Case::Capitalized:
        if (!out.empty()) out[0] = static_cast<char16_t>(std::toupper(out[0]));
        break;
    case Case::Lower:
        break;
    }
    return out;
}

VerbForms lookupVerbForms(std::u16string_view lemma)
{
    VerbForms v;
    v.base = std::u16string(lemma);
    const std::u16string key = toLower(lemma);
    if (const auto *e = findVerb(key)) {
        v.third = e->third[0] ? std::u16string(e->third) : thirdSingularOf(key);
        v.past = e->past;
        v.participle = e->participle;
    } else {
        v.third = thirdSingularOf(key);
        v.past = pastOf(key);
        v.participle = v.past;
    }
    v.gerund = gerundOf(key);
    v.known = true;
    return v;
}

VerbFormInfo classifyVerbForm(std::u16string_view word)
{
    VerbFormInfo info;
    const std::u16string key = toLower(word);
    // Irregular table exact match (third/past/participle -> lemma).
    for (const auto &e : kIrregularVerbs) {
        if (e.third[0] && key == e.third) { info.lemma = e.base; info.form = VerbForm::ThirdSingular; info.known = true; return info; }
        if (key == e.past) { info.lemma = e.base; info.form = VerbForm::Past; info.known = true; return info; }
        if (key == e.participle) { info.lemma = e.base; info.form = VerbForm::PastParticiple; info.known = true; return info; }
    }
    // be's person-number forms.
    if (key == u"am" || key == u"are") { info.lemma = u"be"; info.form = VerbForm::Base; info.known = true; return info; }
    if (key == u"were") { info.lemma = u"be"; info.form = VerbForm::Past; info.known = true; return info; }
    if (key == u"being") { info.lemma = u"be"; info.form = VerbForm::Gerund; info.known = true; return info; }

    if (key.size() >= 4 && key.ends_with(u"ing")) {
        std::u16string stem(key.substr(0, key.size() - 3));
        if (stem.size() >= 2 && stem.back() == u'y' && stem.size() == 2) {
            info.lemma = stem + u"ie";                    // dying -> die, tying -> tie
        } else if (isCvc(stem)) {
            const std::u16string withE = stem + u"e";
            info.lemma = gerundOf(withE) == key ? withE : stem;   // hoping -> hope
        } else if (stem.size() >= 2 && stem.back() == stem[stem.size() - 2]) {
            stem.pop_back();                              // hopping -> hop
            info.lemma = gerundOf(stem) == key ? stem : stem + u"e";
        } else {
            info.lemma = stem;                            // walking -> walk
        }
        info.form = VerbForm::Gerund;
        info.known = true;
        return info;
    }
    if (key.size() >= 3 && key.ends_with(u"ied")) {
        info.lemma = key.substr(0, key.size() - 3) + u"y";   // carried -> carry
        info.form = VerbForm::Past;
        info.known = true;
        return info;
    }
    if (key.size() >= 3 && key.ends_with(u"ed")) {
        std::u16string stem(key.substr(0, key.size() - 2));
        if (isCvc(stem)) {
            const std::u16string withE = stem + u"e";
            info.lemma = pastOf(withE) == key ? withE : stem;   // hoped -> hope
        } else if (stem.size() >= 2 && stem.back() == stem[stem.size() - 2]) {
            stem.pop_back();                              // hopped -> hop
            info.lemma = pastOf(stem) == key ? stem : stem + u"e";
        } else {
            info.lemma = stem;                            // jumped -> jump
        }
        info.form = VerbForm::Past;
        info.known = true;
        return info;
    }
    if (key.size() >= 4 && key.ends_with(u"ies")) {
        info.lemma = key.substr(0, key.size() - 3) + u"y";   // carries -> carry
        info.form = VerbForm::ThirdSingular;
        info.known = true;
        return info;
    }
    if (key.size() >= 3 && key.ends_with(u"es")) {
        // Round-trip like the -ing/-ed branches: prefer the stem that
        // actually generates this form. "sees" -> "see" (not "se"); "boxes"
        // -> "box" (thirdSingularOf("boxe") is "boxs", not "boxes").
        std::u16string stem(key.substr(0, key.size() - 2));
        if (thirdSingularOf(stem) != key) {
            const std::u16string withE = stem + u"e";
            if (thirdSingularOf(withE) == key) stem = withE;
        }
        info.lemma = stem;
        info.form = VerbForm::ThirdSingular;
        info.known = true;
        return info;
    }
    if (key.size() >= 2 && key.ends_with(u"s") && key[key.size() - 2] != u's') {
        std::u16string stem(key.substr(0, key.size() - 1));
        if (thirdSingularOf(stem) != key) {
            const std::u16string withE = stem + u"e";
            if (thirdSingularOf(withE) == key) stem = withE;   // comes -> come
        }
        info.lemma = stem;
        info.form = VerbForm::ThirdSingular;
        info.known = true;
        return info;
    }
    return info;
}

std::u16string pluralize(std::u16string_view noun)
{
    const std::u16string lower = toLower(noun);
    return matchCase(noun, pluralizeLower(lower));
}

std::u16string singularize(std::u16string_view noun)
{
    const std::u16string lower = toLower(noun);
    return matchCase(noun, singularizeLower(lower));
}

int nounNumber(std::u16string_view noun)
{
    const std::u16string key = toLower(noun);
    if (const auto *e = findNoun(key)) {
        if (e->number == 0) return 0;                  // sheep
        return key == e->plural ? +1 : -1;             // children -> +1, child -> -1
    }
    if (key.empty()) return -1;
    const char16_t last = key.back();
    if (last == u's') {
        // -ss/-us/-as/-is/-os endings stay singular; plural only if it roundtrips.
        if (key.size() >= 2) {
            const char16_t prev = key[key.size() - 2];
            if (prev == u's' || prev == u'u' || prev == u'a' || prev == u'i' || prev == u'o')
                return -1;
        }
        const std::u16string sg = singularizeLower(key);
        return pluralizeLower(sg) == key ? +1 : -1;
    }
    return -1;
}

} // namespace stoppard

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
// Deliberate v1 decisions:
// - Contracted words (can't etc.) are decomposed by the tokenizer BEFORE tagging,
//   so only particles ('ll 'd 've 're 'm 's n't) are lexed here.
// - Ambiguous dual-role words (her = object pronoun or possessive; will = modal or
//   content verb; have = aux or content verb) keep ONE lexicon entry (first-listed
//   role); context disambiguation is an M2 tagger concern.
// - 's -> AuxVerb::Be (M2 disambiguates is/has).
#include "lexicon.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace stoppard {
namespace {

struct Entry { const char16_t* word; Lexeme lexeme; };

// Entries must stay sorted by word (case-folded ASCII) for lower_bound.
constexpr Entry kLexicon[] = {
    {u"'d",   {WordTag::Modal}},
    {u"'ll",  {WordTag::Modal}},
    {u"'m",   {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Base}},
    {u"'re",  {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Base}},
    {u"'s",   {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Base}},
    {u"'ve",  {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Have, AuxForm::Base}},
    {u"a",    {WordTag::Determiner, -1}},
    {u"about", {WordTag::Preposition}},
    {u"above", {WordTag::Preposition}},
    {u"across", {WordTag::Preposition}},
    {u"after", {WordTag::Preposition}},
    {u"against", {WordTag::Preposition}},
    {u"all",  {WordTag::Determiner, 0}},
    {u"along", {WordTag::Preposition}},
    {u"already", {WordTag::Adverb}},
    {u"although", {WordTag::Conjunction}},
    {u"am",   {WordTag::Auxiliary, -1, 1, PronounCase::Subject, AuxVerb::Be, AuxForm::Base}},
    {u"among", {WordTag::Preposition}},
    {u"amongst", {WordTag::Preposition}},
    {u"an",   {WordTag::Determiner, -1}},
    {u"and",  {WordTag::Conjunction}},
    {u"another", {WordTag::Determiner, -1}},
    {u"any",  {WordTag::Determiner, 0}},
    {u"are",  {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Base}},
    {u"around", {WordTag::Preposition}},
    {u"as",   {WordTag::Conjunction}},
    {u"at",   {WordTag::Preposition}},
    {u"bad", {WordTag::Adjective}},
    {u"because", {WordTag::Conjunction}},
    {u"been", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::PastParticiple}},
    {u"before", {WordTag::Preposition}},
    {u"behind", {WordTag::Preposition}},
    {u"being", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Gerund}},
    {u"below", {WordTag::Preposition}},
    {u"beside", {WordTag::Preposition}},
    {u"between", {WordTag::Preposition}},
    {u"beyond", {WordTag::Preposition}},
    {u"big", {WordTag::Adjective}},
    {u"black", {WordTag::Adjective}},
    {u"blue", {WordTag::Adjective}},
    {u"both", {WordTag::Determiner, +1}},
    {u"bright", {WordTag::Adjective}},
    {u"busy", {WordTag::Adjective}},
    {u"but",  {WordTag::Conjunction}},
    {u"by",   {WordTag::Preposition}},
    {u"can",  {WordTag::Modal}},
    {u"cannot", {WordTag::Modal}},
    {u"clean", {WordTag::Adjective}},
    {u"common", {WordTag::Adjective}},
    {u"cool", {WordTag::Adjective}},
    {u"could", {WordTag::Modal}},
    {u"dark", {WordTag::Adjective}},
    {u"deep", {WordTag::Adjective}},
    {u"despite", {WordTag::Preposition}},
    {u"did",  {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Do, AuxForm::Past}},
    {u"dirty", {WordTag::Adjective}},
    {u"do",   {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Do, AuxForm::Base}},
    {u"does", {WordTag::Auxiliary, -1, 3, PronounCase::Subject, AuxVerb::Do, AuxForm::ThirdSingular}},
    {u"doing", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Do, AuxForm::Gerund}},
    {u"done", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Do, AuxForm::PastParticiple}},
    {u"during", {WordTag::Preposition}},
    {u"each", {WordTag::Determiner, -1}},
    {u"early", {WordTag::Adjective}},
    {u"easy", {WordTag::Adjective}},
    {u"either", {WordTag::Determiner, -1}},
    {u"empty", {WordTag::Adjective}},
    {u"every", {WordTag::Determiner, -1}},
    {u"except", {WordTag::Preposition}},
    {u"far", {WordTag::Adjective}},
    {u"fast", {WordTag::Adjective}},
    {u"few",  {WordTag::Determiner, +1}},
    {u"for",  {WordTag::Preposition}},
    {u"free", {WordTag::Adjective}},
    {u"fresh", {WordTag::Adjective}},
    {u"from", {WordTag::Preposition}},
    {u"full", {WordTag::Adjective}},
    {u"good", {WordTag::Adjective}},
    {u"great", {WordTag::Adjective}},
    {u"green", {WordTag::Adjective}},
    {u"had",  {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Have, AuxForm::Past}},
    {u"happy", {WordTag::Adjective}},
    {u"has",  {WordTag::Auxiliary, -1, 3, PronounCase::Subject, AuxVerb::Have, AuxForm::ThirdSingular}},
    {u"have", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Have, AuxForm::Base}},
    {u"having", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Have, AuxForm::Gerund}},
    {u"he",   {WordTag::Pronoun, -1, 3, PronounCase::Subject}},
    {u"heavy", {WordTag::Adjective}},
    {u"her",  {WordTag::Pronoun, -1, 3, PronounCase::Object}},
    {u"hers", {WordTag::Pronoun, -1, 3, PronounCase::Possessive}},
    {u"high", {WordTag::Adjective}},
    {u"him",  {WordTag::Pronoun, -1, 3, PronounCase::Object}},
    {u"his",  {WordTag::Pronoun, -1, 3, PronounCase::PossessiveDeterminer}},
    {u"hot", {WordTag::Adjective}},
    {u"how",  {WordTag::Interrogative}},
    {u"huge", {WordTag::Adjective}},
    {u"i",   {WordTag::Pronoun, -1, 1, PronounCase::Subject}},
    {u"if",   {WordTag::Conjunction}},
    {u"in",   {WordTag::Preposition}},
    {u"including", {WordTag::Preposition}},
    {u"inside", {WordTag::Preposition}},
    {u"into", {WordTag::Preposition}},
    {u"is",   {WordTag::Auxiliary, -1, 3, PronounCase::Subject, AuxVerb::Be, AuxForm::ThirdSingular}},
    {u"it",   {WordTag::Pronoun, -1, 3, PronounCase::Subject}},
    {u"its",  {WordTag::Pronoun, -1, 3, PronounCase::PossessiveDeterminer}},
    {u"large", {WordTag::Adjective}},
    {u"late", {WordTag::Adjective}},
    {u"little", {WordTag::Adjective}},
    {u"long", {WordTag::Adjective}},
    {u"loud", {WordTag::Adjective}},
    {u"low", {WordTag::Adjective}},
    {u"many", {WordTag::Determiner, +1}},
    {u"may",  {WordTag::Modal}},
    {u"me",   {WordTag::Pronoun, -1, 1, PronounCase::Object}},
    {u"might", {WordTag::Modal}},
    {u"mine", {WordTag::Pronoun, -1, 1, PronounCase::Possessive}},
    {u"minus", {WordTag::Preposition}},
    {u"must", {WordTag::Modal}},
    {u"my",   {WordTag::Pronoun, -1, 1, PronounCase::PossessiveDeterminer}},
    {u"n't",  {WordTag::Negator}},
    {u"narrow", {WordTag::Adjective}},
    {u"near", {WordTag::Preposition}},
    {u"near", {WordTag::Adjective}},
    {u"neither", {WordTag::Determiner, -1}},
    {u"never", {WordTag::Negator}},
    {u"new", {WordTag::Adjective}},
    {u"nice", {WordTag::Adjective}},
    {u"no",   {WordTag::Determiner, 0}},
    {u"nobody", {WordTag::Negator}},
    {u"none", {WordTag::Negator}},
    {u"nor",  {WordTag::Conjunction}},
    {u"not",  {WordTag::Negator}},
    {u"nothing", {WordTag::Negator}},
    {u"nowhere", {WordTag::Negator}},
    {u"numerous", {WordTag::Determiner, +1}},
    {u"of",   {WordTag::Preposition}},
    {u"off",  {WordTag::Preposition}},
    {u"old", {WordTag::Adjective}},
    {u"on",   {WordTag::Preposition}},
    {u"onto", {WordTag::Preposition}},
    {u"or",   {WordTag::Conjunction}},
    {u"our",  {WordTag::Pronoun, +1, 1, PronounCase::PossessiveDeterminer}},
    {u"ours", {WordTag::Pronoun, +1, 1, PronounCase::Possessive}},
    {u"out",  {WordTag::Preposition}},
    {u"outside", {WordTag::Preposition}},
    {u"over", {WordTag::Preposition}},
    {u"past", {WordTag::Preposition}},
    {u"per",  {WordTag::Preposition}},
    {u"plus", {WordTag::Preposition}},
    {u"poor", {WordTag::Adjective}},
    {u"pretty", {WordTag::Adjective}},
    {u"quiet", {WordTag::Adjective}},
    {u"rare", {WordTag::Adjective}},
    {u"red", {WordTag::Adjective}},
    {u"regarding", {WordTag::Preposition}},
    {u"rich", {WordTag::Adjective}},
    {u"rough", {WordTag::Adjective}},
    {u"sad", {WordTag::Adjective}},
    {u"several", {WordTag::Determiner, +1}},
    {u"shall", {WordTag::Modal}},
    {u"sharp", {WordTag::Adjective}},
    {u"she",  {WordTag::Pronoun, -1, 3, PronounCase::Subject}},
    {u"short", {WordTag::Adjective}},
    {u"should", {WordTag::Modal}},
    {u"since", {WordTag::Preposition}},
    {u"slow", {WordTag::Adjective}},
    {u"small", {WordTag::Adjective}},
    {u"smooth", {WordTag::Adjective}},
    {u"so",   {WordTag::Conjunction}},
    {u"soft", {WordTag::Adjective}},
    {u"some", {WordTag::Determiner, 0}},
    {u"sour", {WordTag::Adjective}},
    {u"stale", {WordTag::Adjective}},
    {u"strong", {WordTag::Adjective}},
    {u"sweet", {WordTag::Adjective}},
    {u"tall", {WordTag::Adjective}},
    {u"than", {WordTag::Conjunction}},
    {u"that", {WordTag::Determiner, -1}},
    {u"the",  {WordTag::Determiner, 0}},
    {u"their", {WordTag::Pronoun, +1, 3, PronounCase::PossessiveDeterminer}},
    {u"theirs", {WordTag::Pronoun, +1, 3, PronounCase::Possessive}},
    {u"them", {WordTag::Pronoun, +1, 3, PronounCase::Object}},
    {u"these", {WordTag::Determiner, +1}},
    {u"they", {WordTag::Pronoun, +1, 3, PronounCase::Subject}},
    {u"thick", {WordTag::Adjective}},
    {u"thin", {WordTag::Adjective}},
    {u"this", {WordTag::Determiner, -1}},
    {u"those", {WordTag::Determiner, +1}},
    {u"though", {WordTag::Conjunction}},
    {u"through", {WordTag::Preposition}},
    {u"throughout", {WordTag::Preposition}},
    {u"tiny", {WordTag::Adjective}},
    {u"to",   {WordTag::Preposition}},
    {u"toward", {WordTag::Preposition}},
    {u"towards", {WordTag::Preposition}},
    {u"two",  {WordTag::Determiner, +1}},
    {u"under", {WordTag::Preposition}},
    {u"underneath", {WordTag::Preposition}},
    {u"unless", {WordTag::Conjunction}},
    {u"until", {WordTag::Preposition}},
    {u"unto", {WordTag::Preposition}},
    {u"up",   {WordTag::Preposition}},
    {u"upon", {WordTag::Preposition}},
    {u"us",   {WordTag::Pronoun, +1, 1, PronounCase::Object}},
    {u"various", {WordTag::Determiner, +1}},
    {u"via",  {WordTag::Preposition}},
    {u"warm", {WordTag::Adjective}},
    {u"was",  {WordTag::Auxiliary, -1, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Past}},
    {u"we",   {WordTag::Pronoun, +1, 1, PronounCase::Subject}},
    {u"weak", {WordTag::Adjective}},
    {u"were", {WordTag::Auxiliary, 0, 0, PronounCase::Subject, AuxVerb::Be, AuxForm::Past}},
    {u"what", {WordTag::Interrogative}},
    {u"when", {WordTag::Interrogative}},
    {u"where", {WordTag::Interrogative}},
    {u"whereas", {WordTag::Conjunction}},
    {u"which", {WordTag::Interrogative}},
    {u"while", {WordTag::Conjunction}},
    {u"white", {WordTag::Adjective}},
    {u"who",  {WordTag::Interrogative}},
    {u"whom", {WordTag::Interrogative}},
    {u"whose", {WordTag::Interrogative}},
    {u"why",  {WordTag::Interrogative}},
    {u"wide", {WordTag::Adjective}},
    {u"will", {WordTag::Modal}},
    {u"with", {WordTag::Preposition}},
    {u"within", {WordTag::Preposition}},
    {u"without", {WordTag::Preposition}},
    {u"would", {WordTag::Modal}},
    {u"yellow", {WordTag::Adjective}},
    {u"yet",  {WordTag::Conjunction}},
    {u"you",  {WordTag::Pronoun, 0, 2, PronounCase::Subject}},
    {u"young", {WordTag::Adjective}},
    {u"your", {WordTag::Pronoun, 0, 2, PronounCase::PossessiveDeterminer}},
    {u"yours", {WordTag::Pronoun, 0, 2, PronounCase::Possessive}},
};

char16_t fold(char16_t c) { return (c >= u'A' && c <= u'Z') ? static_cast<char16_t>(c - u'A' + u'a') : c; }

std::u16string folded(std::u16string_view word)
{
    std::u16string out;
    out.reserve(word.size());
    for (char16_t c : word) out.push_back(fold(c));
    return out;
}

} // namespace

const Lexeme* lookupWord(std::u16string_view word)
{
    const std::u16string key = folded(word);
    const auto it = std::lower_bound(
        std::begin(kLexicon), std::end(kLexicon), key,
        [](const Entry &e, const std::u16string &w) {
            return std::u16string_view(e.word) < std::u16string_view(w);
        });
    if (it == std::end(kLexicon) || std::u16string_view(it->word) != key)
        return nullptr;
    return &it->lexeme;
}

} // namespace stoppard

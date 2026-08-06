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
// - Contraction decomposition happens inside tag() (before tagging) — sub-tokens
//   keep exact UTF-16 sub-spans (don't -> do(0,2) + n't(2,3)).
// - Closed-class wins: any lexicon hit -> deterministic tag + features, even
//   mid-sentence (a can of beans tags can as Modal — accepted precision trade-off).
// - Auxiliaries are always tagged via lexicon (has -> Auxiliary even after a
//   modal — rules R1/R4 handle "modal + non-base auxiliary" explicitly).
// - Ambiguous content words (-s, -ing) resolve by context; unresolved -> Unknown;
//   rules never fire on Unknown.
// - "no"/"neither" tag as Determiner (they're in both spec lists; R8 matches by
//   text, not POS).
// - Context flags: verbCtx (after subject Pronoun/Modal/Auxiliary; survives
//   Negator/Adverb), nounCtx (after Determiner/Preposition/Number; survives
//   Negator/Adverb/Unknown; kept alive by Noun tags so stacked bare nouns
//   chunk correctly: "the big cat" -> the,big,cat).
//   Negator/Adverb) and nounCtx (after Determiner/Number/Preposition except "to").
//   Content tags clear both; Unknown/Adverb/Noun keep nounCtx alive so modifiers
//   and stacked nouns chunk correctly ("the big cat").
#include "tagger.h"

#include <algorithm>
#include <array>

namespace stoppard {
namespace {

bool isNounSuffix(std::u16string_view word)
{
    // -tion/-sion/-ness/-ment/-ity -> noun. -er is intentionally NOT used:
    // "to store" must stay Unknown (bare after "to" is ambiguous), and -er
    // misfires on comparatives (faster) — precision-first.
    static constexpr std::array<std::u16string_view, 5> kSuffixes = {
        u"tion", u"sion", u"ness", u"ment", u"ity",
    };
    for (const auto &s : kSuffixes)
        if (word.size() > s.size() && word.ends_with(s)) return true;
    return false;
}

bool isLyAdverb(std::u16string_view word)
{
    return word.size() > 2 && word.ends_with(u"ly");
}

// Modal-context rule (Brill-style contextual transformation: VBN/VBD -> VB
// after MD). After a modal the verb is the base form; negators and adverbs
// between them ("can't", "will not", "can quickly") keep the context alive.
bool nextModalCtx(const TaggedToken &t, bool prev)
{
    return t.pos == PosTag::Modal
        || ((t.pos == PosTag::Negator || t.pos == PosTag::Adverb) && prev);
}

bool hasVowel(std::u16string_view s)
{
    return std::any_of(s.begin(), s.end(), [](char16_t c) {
        return c == u'a' || c == u'e' || c == u'i' || c == u'o' || c == u'u';
    });
}

TaggedToken tagLexeme(const Lexeme &lex)
{
    TaggedToken t;
    switch (lex.tag) {
    case WordTag::Determiner: t.pos = PosTag::Determiner; break;
    case WordTag::Pronoun:    t.pos = PosTag::Pronoun; break;
    case WordTag::Modal:      t.pos = PosTag::Modal; break;
    case WordTag::Auxiliary:  t.pos = PosTag::Auxiliary; break;
    case WordTag::Preposition: t.pos = PosTag::Preposition; break;
    case WordTag::Conjunction: t.pos = PosTag::Conjunction; break;
    case WordTag::Negator:    t.pos = PosTag::Negator; break;
    case WordTag::Interrogative: t.pos = PosTag::Interrogative; break;
    case WordTag::Adjective:    t.pos = PosTag::Adjective; break;
    case WordTag::Adverb:       t.pos = PosTag::Adverb; break;
    }
    t.number = lex.number;
    t.person = lex.person;
    t.pcase = lex.pcase;
    t.auxVerb = lex.auxVerb;
    t.auxForm = lex.auxForm;
    return t;
}

void applyContext(TaggedToken &t, bool verbCtx, bool nounCtx, std::u16string_view word)
{
    if (verbCtx) {
        t.pos = PosTag::Verb;
        t.verbForm = VerbForm::Base;
        t.lemma = std::u16string(word);
    } else if (nounCtx) {
        t.pos = PosTag::Noun;
        t.number = nounNumber(word);
        t.lemma = std::u16string(word);
    }
}

void updateContexts(const TaggedToken &t, bool &verbCtx, bool &nounCtx)
{
    switch (t.pos) {
    case PosTag::Modal:
    case PosTag::Auxiliary:
        verbCtx = true;
        nounCtx = false;
        break;
    case PosTag::Pronoun:
        if (t.pcase == PronounCase::Subject) { verbCtx = true; nounCtx = false; }
        else { verbCtx = false; nounCtx = false; }
        break;
    case PosTag::Determiner:
        nounCtx = true;
        verbCtx = false;
        break;
    case PosTag::Adjective:
        nounCtx = true;   // pre-nominal modifier: "big cat"
        verbCtx = false;
        break;
    case PosTag::Preposition:
        nounCtx = true;   // "to" is cleared below by the caller
        verbCtx = false;
        break;
    case PosTag::Number:
        nounCtx = true;
        verbCtx = false;
        break;
    case PosTag::Noun:
        nounCtx = true;      // stacked nouns: "the big cat" -> the,big,cat
        verbCtx = false;
        break;
    case PosTag::Verb:
        nounCtx = false;
        verbCtx = false;
        break;
    case PosTag::Negator:
    case PosTag::Adverb:
    case PosTag::Unknown:
        break;   // survives: "don't walk", "not quickly walk", "the big cat"
    default:
        nounCtx = false;
        verbCtx = false;
        break;
    }
}

} // namespace

std::vector<TaggedToken> tag(std::u16string_view text)
{
    std::vector<TaggedToken> out;
    bool verbCtx = false, nounCtx = false, afterTo = false, modalCtx = false;
    for (const auto &tok : tokenize(text)) {
        TaggedToken t;
        t.token = tok;
        switch (tok.kind) {
        case TokenKind::Whitespace:
            continue;   // dropped: chunker and rules index content tokens only
        case TokenKind::Code:
            t.pos = PosTag::Code;
            nounCtx = false;
            verbCtx = false;
            modalCtx = false;
            out.push_back(t);
            continue;
        case TokenKind::Punctuation:
            t.pos = PosTag::Punctuation;
            nounCtx = false;
            verbCtx = false;
            modalCtx = false;
            out.push_back(t);
            continue;
        case TokenKind::Number:
            t.pos = PosTag::Number;
            nounCtx = true;
            verbCtx = false;
            modalCtx = false;
            out.push_back(t);
            continue;
        case TokenKind::Word:
            break;
        }

        const std::u16string_view word = text.substr(tok.start, tok.length);
        const auto parts = splitContraction(word);
        if (!parts.empty()) {
            int off = 0;
            for (const auto &part : parts) {
                TaggedToken pt;
                if (const auto *lex = lookupWord(part))
                    pt = tagLexeme(*lex);
                pt.token = {TokenKind::Word, tok.start + off, static_cast<int>(part.size())};
                pt.fromLexicon = pt.pos != PosTag::Unknown;
                updateContexts(pt, verbCtx, nounCtx);
                modalCtx = nextModalCtx(pt, modalCtx);
                out.push_back(pt);
                off += static_cast<int>(part.size());
            }
            afterTo = false;
            continue;
        }

        if (const auto *lex = lookupWord(word)) {
            TaggedToken lt = tagLexeme(*lex);
            lt.token = tok;
            lt.fromLexicon = true;
            afterTo = false;
            const bool isTo = lt.pos == PosTag::Preposition && word == u"to";
            if (isTo) {
                verbCtx = false;      // "to" sets no noun context; conservative
                afterTo = true;       // next 3ps/past form tags Verb (R4)
            } else {
                updateContexts(lt, verbCtx, nounCtx);
            }
            modalCtx = nextModalCtx(lt, modalCtx);
            out.push_back(lt);
            continue;
        }

        // Content word.
        const VerbFormInfo vf = classifyVerbForm(word);
        if (vf.known) {
            TaggedToken vt;
            vt.token = tok;
            switch (vf.form) {
            case VerbForm::Gerund:
                // -ing: noun ctx -> Noun, else Verb. Plausibility guard: lemma
                // must contain a vowel ("wrking" -> wrk has none -> Unknown).
                if (hasVowel(vf.lemma)) {
                    vt.pos = nounCtx ? PosTag::Noun : PosTag::Verb;
                    vt.verbForm = VerbForm::Gerund;
                    vt.lemma = vf.lemma;
                    if (vt.pos == PosTag::Noun) vt.number = nounNumber(word);
                }
                break;
            case VerbForm::Past:
            case VerbForm::PastParticiple:
                vt.pos = PosTag::Verb;   // unambiguous forms tag unconditionally
                vt.verbForm = vf.form;
                vt.lemma = vf.lemma;
                // "run"/"cut"/"hit"/"read" share the base and past/participle
                // spellings. After a modal the verb must be the base form
                // (VBN/VBD -> VB after MD); after "to" the infinitive is the
                // base form ("I want to read."). "went"/"gone" are unaffected:
                // their base is "go", so they stay Past/Participle and the
                // rules flag them.
                if ((modalCtx || afterTo) && lookupVerbForms(vf.lemma).base == word)
                    vt.verbForm = VerbForm::Base;
                break;
            case VerbForm::ThirdSingular:
                // trailing -s: context-resolve (3ps verb vs plural noun)
                if (verbCtx || afterTo) {
                    vt.pos = PosTag::Verb;   // after "to": unambiguous 3ps
                    vt.verbForm = VerbForm::ThirdSingular;
                    vt.lemma = vf.lemma;
                } else if (nounCtx) {
                    vt.pos = PosTag::Noun;
                    vt.number = nounNumber(word);
                    vt.lemma = vf.lemma;
                }
                break;
            default:
                break;
            }
            if (vt.pos == PosTag::Unknown) nounCtx = verbCtx = false;
            else updateContexts(vt, verbCtx, nounCtx);
            afterTo = false;
            out.push_back(vt);
            continue;
        }

        if (isNounSuffix(word)) {
            TaggedToken nt;
            nt.token = tok;
            afterTo = false;
            nt.pos = PosTag::Noun;
            nt.number = nounNumber(word);
            nt.lemma = std::u16string(word);
            updateContexts(nt, verbCtx, nounCtx);
            modalCtx = false;
            out.push_back(nt);
            continue;
        }
        if (isLyAdverb(word)) {
            TaggedToken at;
            at.token = tok;
            afterTo = false;
            at.pos = PosTag::Adverb;
            updateContexts(at, verbCtx, nounCtx);
            modalCtx = nextModalCtx(at, modalCtx);
            out.push_back(at);
            continue;
        }

        // Bare word: context-resolve.
        TaggedToken bt;
        bt.token = tok;
        afterTo = false;
        applyContext(bt, verbCtx, nounCtx, word);
        updateContexts(bt, verbCtx, nounCtx);
        modalCtx = false;
        out.push_back(bt);
    }
    return out;
}

} // namespace stoppard

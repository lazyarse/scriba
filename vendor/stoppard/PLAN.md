# Stoppard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Stoppard library (M1-M4: repo, tokenizer/lexicon/morphology, tagger/chunker, rules R1-R9, engine + corpus) per SPEC.md, then integrate into Scriba (M5), all with per-component gtest suites.

**Architecture:** Qt-free C++23 static library, public header only at `include/stoppard/stoppard.h`, UTF-16 spans end-to-end. Milestones: M1 foundation layers (tokenizer, closed-class lexicon, morphology paradigms); M2 tagger + chunker; M3 rule framework + R1-R9; M4 engine + corpus + polish; M5 Phase 2 scriba integration (vendor copy, StoppardEngine, delete harper).

**Tech Stack:** C++23, CMake ≥3.21, GoogleTest (FetchContent), no Qt.

---

## Task 1: Repo scaffold

**Files:**
- Create: `/home/tpa/code/stoppard/{CMakeLists.txt, .gitignore, LICENSE, README.md, include/stoppard/stoppard.h}`

- [ ] **Step 1: `git init`, write scaffold files**

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.21)
project(stoppard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(stoppard STATIC
    src/tokenizer.cpp
    src/lexicon.cpp
    src/morphology.cpp
)
target_include_directories(stoppard PUBLIC include)
target_compile_options(stoppard PRIVATE -Wall -Wextra -Wpedantic)

include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(test_stoppard
    tests/test_tokenizer.cpp
    tests/test_lexicon.cpp
    tests/test_morphology.cpp
    tests/test_smoke.cpp
)
target_link_libraries(test_stoppard PRIVATE stoppard GTest::gtest_main)
gtest_discover_tests(test_stoppard)
```

`.gitignore`:
```
build/
```

`README.md`: project name, one-paragraph description, build/test commands (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4`, `cd build && ctest --output-on-failure -j1`), link to SPEC.md, GPL-3.0 line.

`LICENSE`: full GPL-3.0 text (copy from `/home/tpa/code/scriba/LICENSE`).

`include/stoppard/stoppard.h` (spec §4 + §14.1 — declarations only; `Engine`/`profileFor` implementations land in M3/M4):
```cpp
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

#include <string>
#include <string_view>
#include <vector>

namespace stoppard {

enum class SuggestionKind { Replace, Remove, InsertAfter };

struct Suggestion { SuggestionKind kind = SuggestionKind::Replace; std::u16string text; };

struct Issue {
    int start = 0;
    int length = 0;
    std::u16string message;
    std::vector<Suggestion> suggestions;
};

enum class Dialect { American, British, Australian, Indian, Canadian, NewZealand };

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
    std::vector<Issue> check(std::u16string_view text) const;  // impl lands in M4
    Dialect dialect() const;
    void setDialect(Dialect d);
};

} // namespace stoppard
```
(Unused declared methods don't break linking — no M1 translation unit references them.)

All subsequent new `.cpp`/`.h` files start with the same license header as above.

- [ ] **Step 2: Create empty test files with one placeholder test each**

`tests/test_tokenizer.cpp`, `tests/test_lexicon.cpp`, `tests/test_morphology.cpp`, `tests/test_smoke.cpp` — each with the license header, `#include <gtest/gtest.h>`, and one trivial `TEST(Placeholder, Links)` so the target links.

- [ ] **Step 3: Configure + build**

```bash
cd /home/tpa/code/stoppard && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4
```
Expected: gtest fetched, `libstoppard.a` + `test_stoppard` build, ctest runs 4 passing placeholder tests.

- [ ] **Step 4: Commit** — `git add -A && git commit -m "chore: scaffold stoppard repo (cmake, gtest, license, public header)"`

---

## Task 2: Tokenizer

**Files:**
- Create: `src/tokenizer.h`, `src/tokenizer.cpp`, replace body of `tests/test_tokenizer.cpp` (license header already in place)

- [ ] **Step 1: Write the failing tests** (`tests/test_tokenizer.cpp`)

```cpp
#include <gtest/gtest.h>
#include "stoppard/tokenizer.h"
using namespace stoppard;

TEST(Tokenize, EmptyInput) { EXPECT_TRUE(tokenize(u"").empty()); }

TEST(Tokenize, BasicSentence) {
    auto t = tokenize(u"The cat sat.");
    ASSERT_EQ(t.size(), 7u);
    EXPECT_EQ(t[0].kind, TokenKind::Word);  EXPECT_EQ(t[0].start, 0);  EXPECT_EQ(t[0].length, 3);
    EXPECT_EQ(t[1].kind, TokenKind::Whitespace);  EXPECT_EQ(t[1].start, 3);
    EXPECT_EQ(t[2].kind, TokenKind::Word);  EXPECT_EQ(t[2].start, 4);
    EXPECT_EQ(t[4].kind, TokenKind::Word);  EXPECT_EQ(t[4].start, 8);
    EXPECT_EQ(t[6].kind, TokenKind::Punctuation);  EXPECT_EQ(t[6].start, 11);  EXPECT_EQ(t[6].length, 1);
}

TEST(Tokenize, Numbers) {
    auto t = tokenize(u"42 cats");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].kind, TokenKind::Number);  EXPECT_EQ(t[0].length, 2);
    EXPECT_EQ(t[2].kind, TokenKind::Word);
}

TEST(Tokenize, NonBmpIsOneUtf16Token) {
    auto t = tokenize(u"a \U0001F600 b");   // emoji = 2 UTF-16 code units
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[2].kind, TokenKind::Word);  EXPECT_EQ(t[2].start, 2);  EXPECT_EQ(t[2].length, 2);
}

TEST(Tokenize, ApostropheWordIsSingleToken) {
    auto t = tokenize(u"don't");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].kind, TokenKind::Word);  EXPECT_EQ(t[0].length, 5);
}

TEST(Tokenize, FencedCodeIsOpaque) {
    auto t = tokenize(u"before\n```cpp\nint x;\n```\nafter");
    ASSERT_EQ(t.size(), 5u);   // word, ws, CODE, ws, word
    EXPECT_EQ(t[2].kind, TokenKind::Code);
    EXPECT_EQ(t[2].length, 16); // "```cpp\nint x;\n```"
}

TEST(Tokenize, InlineCodeIsOpaque) {
    auto t = tokenize(u"Use `rm -rf` now");
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[2].kind, TokenKind::Code);  EXPECT_EQ(t[2].length, 8); // "`rm -rf`"
}

TEST(Tokenize, HeadingMarkerIsPunctuation) {
    auto t = tokenize(u"# Title");
    EXPECT_EQ(t[0].kind, TokenKind::Punctuation);
}

TEST(SplitContraction, Table) {
    EXPECT_EQ(splitContraction(u"won't"), (std::vector<std::u16string>{u"will", u"n't"}));
    EXPECT_EQ(splitContraction(u"can't"), (std::vector<std::u16string>{u"can", u"n't"}));
    EXPECT_EQ(splitContraction(u"don't"), (std::vector<std::u16string>{u"do", u"n't"}));
    EXPECT_EQ(splitContraction(u"she's"), (std::vector<std::u16string>{u"she", u"'s"}));
    EXPECT_EQ(splitContraction(u"I'm"),   (std::vector<std::u16string>{u"I", u"'m"}));
    EXPECT_EQ(splitContraction(u"we'll"), (std::vector<std::u16string>{u"we", u"'ll"}));
    EXPECT_EQ(splitContraction(u"they'd"),(std::vector<std::u16string>{u"they", u"'d"}));
    EXPECT_EQ(splitContraction(u"I've"),  (std::vector<std::u16string>{u"I", u"'ve"}));
    EXPECT_EQ(splitContraction(u"they're"),(std::vector<std::u16string>{u"they", u"'re"}));
    EXPECT_EQ(splitContraction(u"let's"), (std::vector<std::u16string>{u"let", u"'s"}));
    EXPECT_EQ(splitContraction(u"cat"),   std::vector<std::u16string>{});
}

TEST(SplitSentences, Basic) {
    auto s = splitSentences(u"Hello world. This is fun!");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].length, 11);   // "Hello world."
    EXPECT_EQ(s[1].start, 12);    // "This is fun!"
}

TEST(SplitSentences, EllipsisIsOneBoundary) {
    auto s = splitSentences(u"Wait... what?");
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].length, 8);    // "Wait..."
}
```

- [ ] **Step 2: Run — expect FAIL** (`cd build && cmake --build . -j4 && ctest --output-on-failure -j1`)

- [ ] **Step 3: Implement** — `src/tokenizer.h`:
```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace stoppard {

enum class TokenKind { Word, Number, Punctuation, Whitespace, Code };

struct Token { TokenKind kind; int start; int length; };   // UTF-16 code units

std::vector<Token> tokenize(std::u16string_view text);

std::vector<std::u16string> splitContraction(std::u16string_view word);

struct SentenceSpan { int start; int length; };
std::vector<SentenceSpan> splitSentences(std::u16string_view text);

} // namespace stoppard
```
`tokenizer.cpp` — design decisions:
- Word chars: ASCII letters, apostrophe, and any non-ASCII code unit (covers Māori `kōrero`; emoji become Word tokens of length 2 and will hit the `Unknown` tag later — never flagged, by construction). Number: ASCII digits. Whitespace: space/tab/CR/LF. Everything else: Punctuation.
- Markdown scan **before** the generic loop: fenced blocks (``` at line start → until closing ``` at line start, inclusive, one `Code` token) and inline `` `...` `` runs.
- `splitContraction`: irregular table (`won't→will+n't`, `can't→can+n't`, `shan't→shall+n't`), else stem + suffix where suffix ∈ {n't, 's, 'll, 'd, 've, 're, 'm} and stem ∈ small hardcoded set {I, you, he, she, it, we, they, who, there, that, let, do, does, did, have, has, had, would, should, could, might, must, will, shall, are, is, am, was, were, need, dare, o} — tokenizer stays lexicon-independent.
- `splitSentences`: scan `.` `!` `?` runs (run = consecutive boundary punct); boundary fires only if followed by whitespace then an uppercase ASCII letter. Known limitation (documented in a comment): abbreviations like `Dr.` split — sentence splitting only bounds clause context, never gates rules directly.

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit** — `git add -A && git commit -m "feat: tokenizer (utf16 spans, contractions, markdown code, sentences)"`

---

## Task 3: Lexicon

**Files:**
- Create: `src/lexicon.h`, `src/lexicon.cpp`, replace body of `tests/test_lexicon.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
#include <gtest/gtest.h>
#include "stoppard/lexicon.h"
using namespace stoppard;

TEST(Lexicon, SpecClosedClasses) {
    // every word in SPEC.md 6.2 must resolve to its tag
    EXPECT_EQ(lookupWord(u"the")->tag, WordTag::Determiner);
    EXPECT_EQ(lookupWord(u"many")->tag, WordTag::Determiner);
    EXPECT_EQ(lookupWord(u"they")->tag, WordTag::Pronoun);
    EXPECT_EQ(lookupWord(u"could")->tag, WordTag::Modal);
    EXPECT_EQ(lookupWord(u"had")->tag, WordTag::Auxiliary);
    EXPECT_EQ(lookupWord(u"between")->tag, WordTag::Preposition);
    EXPECT_EQ(lookupWord(u"although")->tag, WordTag::Conjunction);
    EXPECT_EQ(lookupWord(u"never")->tag, WordTag::Negator);
    EXPECT_EQ(lookupWord(u"whose")->tag, WordTag::Interrogative);
}

TEST(Lexicon, CaseInsensitive) {
    EXPECT_EQ(lookupWord(u"The")->tag, WordTag::Determiner);
    EXPECT_EQ(lookupWord(u"THE")->tag, WordTag::Determiner);
}

TEST(Lexicon, ContentWordIsNull) { EXPECT_EQ(lookupWord(u"cat"), nullptr); }

TEST(Lexicon, DeterminerNumber) {
    EXPECT_EQ(lookupWord(u"this")->number, -1);
    EXPECT_EQ(lookupWord(u"every")->number, -1);
    EXPECT_EQ(lookupWord(u"these")->number, +1);
    EXPECT_EQ(lookupWord(u"few")->number, +1);
    EXPECT_EQ(lookupWord(u"the")->number, 0);
    EXPECT_EQ(lookupWord(u"any")->number, 0);
}

TEST(Lexicon, PronounFeatures) {
    EXPECT_EQ(lookupWord(u"me")->person, 1);  EXPECT_EQ(lookupWord(u"me")->pcase, PronounCase::Object);
    EXPECT_EQ(lookupWord(u"we")->person, 1);  EXPECT_EQ(lookupWord(u"we")->number, +1);
    EXPECT_EQ(lookupWord(u"them")->person, 3); EXPECT_EQ(lookupWord(u"them")->number, +1);
    EXPECT_EQ(lookupWord(u"my")->pcase, PronounCase::PossessiveDeterminer);
    EXPECT_EQ(lookupWord(u"mine")->pcase, PronounCase::Possessive);
}

TEST(Lexicon, DecomposedParticles) {   // splitContraction outputs must be taggable
    EXPECT_EQ(lookupWord(u"'ll")->tag, WordTag::Modal);
    EXPECT_EQ(lookupWord(u"n't")->tag, WordTag::Negator);
}

TEST(Lexicon, AuxiliaryForms) {
    EXPECT_EQ(lookupWord(u"am")->auxVerb, AuxVerb::Be);
    EXPECT_EQ(lookupWord(u"am")->auxForm, AuxForm::Base);       // am = be+base, person 1
    EXPECT_EQ(lookupWord(u"was")->auxForm, AuxForm::Past);
    EXPECT_EQ(lookupWord(u"been")->auxForm, AuxForm::PastParticiple);
    EXPECT_EQ(lookupWord(u"doing")->auxForm, AuxForm::Gerund);
}
```

- [ ] **Step 2: Run — expect FAIL**

- [ ] **Step 3: Implement** — `src/lexicon.h`:
```cpp
#pragma once
#include <string_view>

namespace stoppard {

enum class WordTag { Determiner, Pronoun, Modal, Auxiliary, Preposition, Conjunction, Negator, Interrogative };
enum class PronounCase { Subject, Object, PossessiveDeterminer, Possessive };
enum class AuxVerb { Be, Have, Do };
enum class AuxForm { Base, ThirdSingular, Past, PastParticiple, Gerund };

struct Lexeme {
    WordTag tag;
    int number = 0;          // -1 singular, +1 plural, 0 both/unknown (determiners, pronouns)
    int person = 0;          // pronouns: 1, 2, 3
    PronounCase pcase = PronounCase::Subject;
    AuxVerb auxVerb = AuxVerb::Be;
    AuxForm auxForm = AuxForm::Base;
};

const Lexeme* lookupWord(std::u16string_view word);   // lowercase lookup; nullptr if not closed-class

} // namespace stoppard
```
`lexicon.cpp` — one sorted static array of `{const char16_t* word; Lexeme}` entries, `std::lower_bound` on lowercase ASCII-folded query. Data (word lists from spec §6.2, features as designed in the tests). Deliberate v1 decisions (comment in file): contracted words (`can't` etc.) are decomposed by the tokenizer *before* tagging → only particles `'ll 'd 've 're 'm 's n't` are lexed; ambiguous dual-role words (`her` = object pronoun or possessive; `will` = modal or content verb; `have` = aux or content verb) keep **one** lexicon entry (first-listed role); context disambiguation is an M2 tagger concern; `'s` → `AuxVerb::Be` (M2 disambiguates is/has).

Word lists (from SPEC.md §6.2, feature-marked):
- **Determiners** — singular (−1): a, an, this, that, each, every, another, either, neither; plural (+1): these, those, many, several, few, both, various, numerous; both (0): the, some, any, all, no.
- **Pronouns** — I(1,−1,Subj), me(1,−1,Obj), we(1,+1,Subj), us(1,+1,Obj), you(2,0,Subj), he(3,−1,Subj), him(3,−1,Obj), she(3,−1,Subj), her(3,−1,Obj), it(3,−1,Subj), they(3,+1,Subj), them(3,+1,Obj); possessive determiners: my, your, his, her, its, our, their; possessives: mine, yours, his, hers, its, ours, theirs.
- **Modals**: can, could, will, would, shall, should, may, might, must, cannot.
- **Auxiliaries**: am/is/are/was/were (Be: Base/ThirdSingular/Past), have/has/had (Have: Base/ThirdSingular/Past), do/does/did (Do: Base/ThirdSingular/Past), been (Be PastParticiple), being (Be Gerund), having (Have Gerund), doing (Do Gerund), done (Do PastParticiple).
- **Prepositions**: in, on, at, of, with, by, for, to, from, about, after, before, between, under, over, through, without, behind, beside, beyond, despite, during, inside, into, near, onto, out, outside, past, since, throughout, toward, towards, underneath, until, upon, within, among, amongst, around, across, along, against, off, above, below, per, plus, minus, via, except, including, regarding.
- **Conjunctions**: and, or, but, nor, because, although, though, if, unless, while, whereas, since, so, yet, as, than.
- **Negators**: not, never, no, none, nobody, nothing, nowhere, neither.
- **Interrogatives**: what, who, whom, whose, which, when, where, why, how.
- **Particles**: 'll, 'd (Modal); 've, 're, 'm (Auxiliary Have/Be); 's (Auxiliary Be); n't (Negator).

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit** — `git add -A && git commit -m "feat: closed-class lexicon with features"`

---

## Task 4: Morphology

**Files:**
- Create: `src/morphology.h`, `src/morphology.cpp`, replace body of `tests/test_morphology.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
#include <gtest/gtest.h>
#include "stoppard/morphology.h"
using namespace stoppard;

TEST(Morphology, IrregularVerbClassification) {
    auto f = classifyVerbForm(u"went");   EXPECT_EQ(f.form, VerbForm::Past);           EXPECT_EQ(f.lemma, u"go");
    f = classifyVerbForm(u"has");         EXPECT_EQ(f.form, VerbForm::ThirdSingular);  EXPECT_EQ(f.lemma, u"have");
    f = classifyVerbForm(u"been");        EXPECT_EQ(f.form, VerbForm::PastParticiple); EXPECT_EQ(f.lemma, u"be");
    f = classifyVerbForm(u"took");        EXPECT_EQ(f.lemma, u"take");
    f = classifyVerbForm(u"seen");        EXPECT_EQ(f.lemma, u"see");
    f = classifyVerbForm(u"gotten");      EXPECT_EQ(f.lemma, u"get");                 // AmE participle
}

TEST(Morphology, RegularVerbClassification) {
    auto f = classifyVerbForm(u"walking"); EXPECT_EQ(f.form, VerbForm::Gerund);        EXPECT_EQ(f.lemma, u"walk");
    f = classifyVerbForm(u"jumped");       EXPECT_EQ(f.form, VerbForm::Past);          EXPECT_EQ(f.lemma, u"jump");
    f = classifyVerbForm(u"carries");      EXPECT_EQ(f.form, VerbForm::ThirdSingular); EXPECT_EQ(f.lemma, u"carry");
    f = classifyVerbForm(u"hoped");        EXPECT_EQ(f.lemma, u"hope");
    EXPECT_FALSE(classifyVerbForm(u"cat").known);
}

TEST(Morphology, Paradigm) {
    auto v = lookupVerbForms(u"walk");
    EXPECT_TRUE(v.known);
    EXPECT_EQ(v.third, u"walks");  EXPECT_EQ(v.past, u"walked");
    EXPECT_EQ(v.participle, u"walked");  EXPECT_EQ(v.gerund, u"walking");
    v = lookupVerbForms(u"carry");
    EXPECT_EQ(v.third, u"carries");  EXPECT_EQ(v.past, u"carried");  EXPECT_EQ(v.gerund, u"carrying");
    v = lookupVerbForms(u"hop");
    EXPECT_EQ(v.past, u"hopped");  EXPECT_EQ(v.gerund, u"hopping");
    v = lookupVerbForms(u"hope");
    EXPECT_EQ(v.gerund, u"hoping");
    v = lookupVerbForms(u"die");
    EXPECT_EQ(v.gerund, u"dying");
    v = lookupVerbForms(u"agree");
    EXPECT_EQ(v.gerund, u"agreeing");
    v = lookupVerbForms(u"take");
    EXPECT_EQ(v.past, u"took");  EXPECT_EQ(v.participle, u"taken");
}

TEST(Morphology, NounPlurals) {
    EXPECT_EQ(pluralize(u"cat"), u"cats");
    EXPECT_EQ(pluralize(u"box"), u"boxes");
    EXPECT_EQ(pluralize(u"baby"), u"babies");
    EXPECT_EQ(pluralize(u"key"), u"keys");
    EXPECT_EQ(pluralize(u"knife"), u"knives");
    EXPECT_EQ(pluralize(u"potato"), u"potatoes");
    EXPECT_EQ(pluralize(u"piano"), u"pianos");
    EXPECT_EQ(pluralize(u"child"), u"children");
    EXPECT_EQ(pluralize(u"mouse"), u"mice");
    EXPECT_EQ(pluralize(u"sheep"), u"sheep");
}

TEST(Morphology, NounSingulars) {
    EXPECT_EQ(singularize(u"cats"), u"cat");
    EXPECT_EQ(singularize(u"boxes"), u"box");
    EXPECT_EQ(singularize(u"babies"), u"baby");
    EXPECT_EQ(singularize(u"knives"), u"knife");
    EXPECT_EQ(singularize(u"children"), u"child");
    EXPECT_EQ(singularize(u"mice"), u"mouse");
    EXPECT_EQ(singularize(u"sheep"), u"sheep");
}

TEST(Morphology, NounNumber) {
    EXPECT_EQ(nounNumber(u"cat"), -1);
    EXPECT_EQ(nounNumber(u"cats"), +1);
    EXPECT_EQ(nounNumber(u"gas"), -1);     // -as/-ss/-us/-is endings stay singular
    EXPECT_EQ(nounNumber(u"sheep"), 0);
    EXPECT_EQ(nounNumber(u"children"), +1);
}

TEST(Morphology, CasePreserving) {
    EXPECT_EQ(pluralize(u"Cat"), u"Cats");
    EXPECT_EQ(pluralize(u"CAT"), u"CATS");
    EXPECT_EQ(pluralize(u"Child"), u"Children");
    EXPECT_EQ(singularize(u"Cats"), u"Cat");
    EXPECT_EQ(classifyVerbForm(u"Went").lemma, u"go");
}
```

- [ ] **Step 2: Run — expect FAIL**

- [ ] **Step 3: Implement** — `src/morphology.h`:
```cpp
#pragma once
#include <string>
#include <string_view>

namespace stoppard {

enum class VerbForm { Base, ThirdSingular, Past, PastParticiple, Gerund };

struct VerbForms {
    std::u16string base, third, past, participle, gerund;
    bool known = false;
};

struct VerbFormInfo {
    std::u16string lemma;
    VerbForm form = VerbForm::Base;
    bool known = false;
};

VerbForms lookupVerbForms(std::u16string_view lemma);      // full paradigm (regular rules + irregular table)
VerbFormInfo classifyVerbForm(std::u16string_view word);   // word -> (lemma, form); unknown for bare non-table words
std::u16string pluralize(std::u16string_view noun);
std::u16string singularize(std::u16string_view noun);
int nounNumber(std::u16string_view noun);                  // -1, +1, 0 unknown/both

} // namespace stoppard
```
`morphology.cpp` — design decisions:
- All inputs ASCII-lowercased for lookup; outputs restore source case (match-case: all-caps → ALL-CAPS, capitalized → Capitalized, else lower).
- Irregular verb table (sorted, ~150 entries — full list in the file, including `go/went/gone`, `take/took/taken`, `get/got/gotten`, `see/saw/seen`, `be(am,is,are,was,were,been)`, `have/has/had`, `do/does/did/done`, `make`, `come`, `know`, `think`, `find`, `give`, `tell`, `leave`, `feel`, `put`, `bring`, `begin`, `keep`, `hold`, `write`, `stand`, `hear`, `let`, `mean`, `set`, `meet`, `run`, `pay`, `sit`, `speak`, `lie`, `lead`, `read`, `grow`, `lose`, `fall`, `send`, `build`, `draw`, `break`, `spend`, `cut`, `rise`, `drive`, `buy`, `wear`, `choose`, `catch`, `fight`, `win`, `teach`, `beat`, `hurt`, `feed`, `sell`, `sleep`, `sing`, `fly`, `ride`, `eat`, `drink`, `hit`, `throw`, `shake`, `wake`, `swim`, `swear`, `steal`, `tear`, `forget`, `forgive`, `freeze`, `hide`, `bite`, `blow`, `sink`, `ring`, `swing`, `spring`, `shrink`, `stink`, `strike`, `cling`, `fling`, `slit`, `spit`, `stick`, `string`, `weave`, `win`, `wind`, `wring`, `arise`, `awake`, `bear`, `beat`, `become`, `bend`, `bet`, `bind`, `bleed`, `broadcast`, `burn`, `burst`, `buy`, `cast`, `cling`, `cost`, `creep`, `deal`, `dig`, `dream`, `dwell`, `feed`, `flee`, `fling`, `forbid`, `forgo`, `grind`, `hang`, `kneel`, `knit`, `lay`, `lean`, `leap`, `lend`, `light`, `mistake`, `mow`, `overcome`, `quit`, `read`, `ring`, `rise`, `saw`, `say`, `seek`, `sell`, `send`, `set`, `shed`, `shine`, `shoot`, `show`, `shut`, `sing`, `sink`, `sit`, `sleep`, `slide`, `smell`, `speak`, `speed`, `spell`, `spend`, `spill`, `spin`, `split`, `spoil`, `spread`, `stand`, `steal`, `stick`, `sting`, `stink`, `strike`, `sweep`, `swim`, `swing`, `take`, `teach`, `tear`, `tell`, `think`, `throw`, `thrust`, `understand`, `wake`, `wear`, `weave`, `weep`, `win`, `wind`, `withdraw`, `write`, ...) — table stores `{base, third, past, participle}` (third defaults to regular `-s`); gerund computed by regular rules (which handle `be→being`, `die→dying`). Policy: every entry needed by a rule test must be present (SPEC.md 6.3); tests drive additions.
- Regular rules: 3ps `-s`/`-es` (`-s,-x,-z,-ch,-sh`) / `-ies` (`-y` after consonant); past/participle `-ed`/`-d`/`-ied`; gerund `-ing` with e-drop (single `e` after consonant, not after vowel or in `-ie` → `y`+`ing`), consonant-doubling (one-syllable CVC).
- `classifyVerbForm`: irregular table exact match → (lemma, form); else suffix rules `-ing`→Gerund, `-ed/-ied`→Past, `-ies`→Third, `-s/-es`→Third (candidate — noun ambiguity is the tagger's job); else unknown. Past participle is *same string* as past for regulars — `form` reports `Past` (documented; rules needing participles use `lookupVerbForms`).
- Nouns: `pluralize` = irregular table (child→children, mouse→mice, foot→feet, tooth→teeth, man→men, woman→women, person→people, ox→oxen, goose→geese, sheep/fish/deer/series/species→self) + regular rules (`-s`, `-es` after s/x/z/ch/sh and consonant-`o` except piano/photo exceptions, `-ies` after consonant-`y`, `-ves` for `-fe`/`-f`). `singularize` = inverse (irregular table reverse + regular un-rules). `nounNumber`: irregular table → ±1/0; else `+1` iff word ends in `s`/`es`/`ies`/`ves` (excluding `-ss/-us/-as/-is/-os` endings, and requiring the s-drop stem to roundtrip `pluralize(singularize(w)) == w`); `-1` otherwise; `0` for table "both" nouns.

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit** — `git add -A && git commit -m "feat: morphology (verb paradigms, noun number, case preservation)"`

---

## Task 5: M1 exit-criteria smoke test

**Files:**
- Replace body of `tests/test_smoke.cpp`

- [ ] **Step 1: Write the test**

```cpp
#include <gtest/gtest.h>
#include "stoppard/tokenizer.h"
#include "stoppard/lexicon.h"
#include "stoppard/morphology.h"
using namespace stoppard;

// M1 exit criteria (SPEC.md 12): "I can has a cat." tokenizes and `has` resolves via paradigms.
TEST(Smoke, M1ExitCriteria) {
    auto t = tokenize(u"I can has a cat.");
    ASSERT_EQ(t.size(), 10u);
    EXPECT_EQ(t[0].start, 0);          // I
    EXPECT_EQ(t[2].start, 2);          // can
    EXPECT_EQ(t[4].start, 6);          // has
    EXPECT_EQ(t[8].length, 3);         // cat
    EXPECT_EQ(t[9].kind, TokenKind::Punctuation);

    EXPECT_EQ(lookupWord(u"can")->tag, WordTag::Modal);
    auto f = classifyVerbForm(u"has");
    EXPECT_TRUE(f.known);
    EXPECT_EQ(f.lemma, u"have");
    EXPECT_EQ(f.form, VerbForm::ThirdSingular);
}
```

- [ ] **Step 2: Full build + ctest** — `cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j4 && ctest --output-on-failure -j1` — all green
- [ ] **Step 3: Commit** — `git commit -am "test: M1 exit-criteria smoke test"`

**M1 done.** Exit criteria met: tokenizer + lexicon + morphology green, `has` resolves via paradigms.

---

---

# M2: Tagger + Chunker (SPEC §6.4, §6.5)

## Task 2.1: Tagger

**Files:**
- Create: `src/tagger.h`, `src/tagger.cpp`, `tests/test_tagger.cpp`; Modify: `CMakeLists.txt`

**Design decisions (recorded in code comments):**
- Contraction decomposition happens *inside* `tag()` (before tagging) — sub-tokens keep exact UTF-16 sub-spans (`don't` → `do`(0,2) + `n't`(2,3)).
- Closed-class wins: any lexicon hit → deterministic tag + features, even mid-sentence (`a can of beans` tags `can` as Modal — accepted precision trade-off, documented).
- Auxiliaries are always tagged via lexicon (`has` → Auxiliary even after a modal — rules R1/R4 handle "modal + non-base auxiliary" explicitly).
- Ambiguous content words (`-s`, `-ing`) resolve by context; unresolved → `Unknown`; **rules never fire on `Unknown`**.
- "no"/"neither" tag as Determiner (they're in both spec lists; R8 matches by text, not POS).

- [ ] **Step 1: Write failing tests** (`tests/test_tagger.cpp`)

```cpp
#include <gtest/gtest.h>
#include "stoppard/tagger.h"
using namespace stoppard;

TEST(Tagger, ClosedClassDeterministic) {
    auto t = tag(u"I can has a cat.");
    ASSERT_EQ(t.size(), 10u);
    EXPECT_EQ(t[0].pos, PosTag::Pronoun); EXPECT_EQ(t[0].pcase, PronounCase::Subject);
    EXPECT_EQ(t[2].pos, PosTag::Modal);
    EXPECT_EQ(t[4].pos, PosTag::Auxiliary);      // has = auxiliary (Have, 3ps)
    EXPECT_EQ(t[4].auxVerb, AuxVerb::Have); EXPECT_EQ(t[4].auxForm, AuxForm::ThirdSingular);
    EXPECT_EQ(t[6].pos, PosTag::Determiner);     EXPECT_EQ(t[6].number, -1);
    EXPECT_EQ(t[8].pos, PosTag::Noun);           EXPECT_EQ(t[8].number, -1);
    EXPECT_EQ(t[9].pos, PosTag::Punctuation);
}

TEST(Tagger, ContractionDecomposition) {
    auto t = tag(u"don't");
    ASSERT_EQ(t.size(), 2u);
    EXPECT_EQ(t[0].pos, PosTag::Auxiliary); EXPECT_EQ(t[0].token.start, 0); EXPECT_EQ(t[0].token.length, 2);
    EXPECT_EQ(t[1].pos, PosTag::Negator);   EXPECT_EQ(t[1].token.start, 2); EXPECT_EQ(t[1].token.length, 3);
    t = tag(u"she's");
    EXPECT_EQ(t[1].pos, PosTag::Auxiliary);   // 's = is (Be, 3ps)
    EXPECT_EQ(t[1].auxVerb, AuxVerb::Be);
}

TEST(Tagger, ContextResolvesAmbiguity) {
    auto t = tag(u"the cats");
    EXPECT_EQ(t[1].pos, PosTag::Noun);  EXPECT_EQ(t[1].number, +1);      // after determiner
    t = tag(u"cats");
    EXPECT_EQ(t[0].pos, PosTag::Unknown);                                 // alone: 3ps-or-plural
    t = tag(u"they goes");
    EXPECT_EQ(t[1].pos, PosTag::Verb);  EXPECT_EQ(t[1].verbForm, VerbForm::ThirdSingular);
    EXPECT_EQ(t[1].lemma, u"go");                                         // irregular table
    t = tag(u"walking");
    EXPECT_EQ(t[0].pos, PosTag::Verb);  EXPECT_EQ(t[0].verbForm, VerbForm::Gerund);
    t = tag(u"the walking");
    EXPECT_EQ(t[0].pos, PosTag::Determiner); EXPECT_EQ(t[1].pos, PosTag::Noun);
    t = tag(u"decision");
    EXPECT_EQ(t[0].pos, PosTag::Noun);                                     // -tion
    t = tag(u"quickly");
    EXPECT_EQ(t[0].pos, PosTag::Adverb);                                   // -ly
    t = tag(u"went");
    EXPECT_EQ(t[0].pos, PosTag::Verb);  EXPECT_EQ(t[0].verbForm, VerbForm::Past);
    EXPECT_EQ(t[0].lemma, u"go");
}

TEST(Tagger, ContextBoostAfterModalAndTo) {
    auto t = tag(u"can has");
    EXPECT_EQ(t[1].pos, PosTag::Auxiliary);                                // lexicon beats context
    t = tag(u"can walk");
    EXPECT_EQ(t[1].pos, PosTag::Verb);  EXPECT_EQ(t[1].verbForm, VerbForm::Base);
    t = tag(u"to store");
    EXPECT_EQ(t[1].pos, PosTag::Unknown);                                  // bare after "to": ambiguous
    t = tag(u"to went");
    EXPECT_EQ(t[1].pos, PosTag::Verb);  EXPECT_EQ(t[1].verbForm, VerbForm::Past);
}

TEST(Tagger, UnknownConservatism) {
    auto t = tag(u"helo wrking text");
    for (const auto &tk : t) EXPECT_EQ(tk.pos, PosTag::Unknown);
    t = tag(u"kōrero Māori ki Aotearoa");
    for (const auto &tk : t) EXPECT_EQ(tk.pos, PosTag::Unknown);
}

TEST(Tagger, CodeIsSkipped) {
    auto t = tag(u"Use `rm -rf` now");
    EXPECT_EQ(t[2].pos, PosTag::Code);
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement** — `src/tagger.h`:

```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "stoppard/lexicon.h"
#include "stoppard/morphology.h"
#include "stoppard/tokenizer.h"

namespace stoppard {

enum class PosTag {
    Determiner, Pronoun, Modal, Auxiliary, Preposition, Conjunction, Negator,
    Interrogative, Noun, Verb, Adjective, Adverb, Number, Punctuation, Code, Unknown
};

struct TaggedToken {
    Token token;
    PosTag pos = PosTag::Unknown;
    int number = 0;                  // -1 singular, +1 plural, 0 both/unknown
    int person = 0;                  // pronouns
    PronounCase pcase = PronounCase::Subject;
    AuxVerb auxVerb = AuxVerb::Be;
    AuxForm auxForm = AuxForm::Base;
    VerbForm verbForm = VerbForm::Base;
    std::u16string lemma;            // lowercase lemma when known
};

std::vector<TaggedToken> tag(std::u16string_view text);

} // namespace stoppard
```

`tagger.cpp` — left-to-right single pass with two context flags:
- `verbCtx` — set after subject Pronoun / Modal / Auxiliary; survives Negator/Adverb; consumed by the next content word.
- Noun context — set after Determiner/PossessiveDeterminer/Number/Preposition (except `to`); consumed by next content word.
- Word token algorithm: (1) contraction split → tag parts; (2) `lookupWord` hit → tag + copy Lexeme features; (3) `classifyVerbForm` known → Verb with form; `-tion/-ness/-ment/-ity/-er` → Noun; `-ly` → Adverb; trailing `-s` → context-resolve; (4) bare word → context-resolve: verbCtx → Verb(Base, lemma=word); noun ctx → Noun; else Unknown. Gerund (`-ing`): noun ctx → Noun, else Verb(Gerund). `to`-after: only unambiguous 3ps/past verb forms → Verb, else Unknown.

- [ ] **Step 4: Run — expect PASS**
- [ ] **Step 5: Commit** — `git commit -am "feat: precision-first tagger (context resolution, unknown conservatism)"`

## Task 2.2: Chunker

**Files:**
- Create: `src/chunker.h`, `src/chunker.cpp`, `tests/test_chunker.cpp`; Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

```cpp
#include <gtest/gtest.h>
#include "stoppard/chunker.h"
using namespace stoppard;

static const Chunk *findChunk(const std::vector<Chunk> &cs, ChunkKind k) {
    for (const auto &c : cs) if (c.kind == k) return &c;
    return nullptr;
}

TEST(Chunker, NPWithUnknownModifiers) {
    auto t = tag(u"The big cat sat.");
    auto cs = chunk(t);
    auto *np = findChunk(cs, ChunkKind::NP);
    ASSERT_NE(np, nullptr);
    EXPECT_EQ(np->startToken, 0); EXPECT_EQ(np->endToken, 2);      // the big cat, head cat
}

TEST(Chunker, PronounIsNP) {
    auto t = tag(u"I can go.");
    auto cs = chunk(t);
    auto *np = findChunk(cs, ChunkKind::NP);
    ASSERT_NE(np, nullptr);
    EXPECT_EQ(np->startToken, 0); EXPECT_EQ(np->endToken, 0);
}

TEST(Chunker, PPIsPrepPlusNP) {
    auto t = tag(u"on the table");
    auto cs = chunk(t);
    auto *pp = findChunk(cs, ChunkKind::PP);
    ASSERT_NE(pp, nullptr);
    EXPECT_EQ(pp->startToken, 0); EXPECT_EQ(pp->endToken, 2);
}

TEST(Chunker, VPChain) {
    auto t = tag(u"has been go");
    auto cs = chunk(t);
    auto *vp = findChunk(cs, ChunkKind::VP);
    ASSERT_NE(vp, nullptr);
    EXPECT_EQ(vp->startToken, 0); EXPECT_EQ(vp->endToken, 2);      // aux aux verb
}

TEST(Chunker, SubjectDetection) {
    auto t = tag(u"The cat sat on the mat.");
    EXPECT_EQ(findSubjectHead(t, /*verb*/ 3), 2);                   // cat
    t = tag(u"I can go.");
    EXPECT_EQ(findSubjectHead(t, 2), 0);                            // I
    t = tag(u"We ran and they walked.");
    EXPECT_EQ(findSubjectHead(t, 1), 0);                            // We
    EXPECT_EQ(findSubjectHead(t, 5), 3);                            // they (walked at 5)
    t = tag(u"Go away.");
    EXPECT_EQ(findSubjectHead(t, 0), -1);                           // imperative
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement** — `src/chunker.h`:

```cpp
#pragma once
#include <vector>

#include "stoppard/tagger.h"

namespace stoppard {

enum class ChunkKind { NP, PP, VP };
struct Chunk { ChunkKind kind; int startToken; int endToken; };   // inclusive token range

std::vector<Chunk> chunk(const std::vector<TaggedToken>& tokens);

// Subject head (noun or subject pronoun) of the clause containing the verb,
// scanning left within the sentence; -1 when none (imperative, expletive).
int findSubjectHead(const std::vector<TaggedToken>& tokens, int verbTokenIndex);

} // namespace stoppard
```

`chunker.cpp` — NP: Determiner/PossessiveDeterminer/Number + (Adverb|Unknown|Adjective)* + Noun (head = last token); lone subject/object pronoun → 1-token NP. PP: Preposition + immediately-following NP. VP: (Modal|Auxiliary|Negator)* + Verb. `findSubjectHead`: walk left from `verbTokenIndex-1`; first Noun or subject Pronoun wins; stop (return -1) at Comma, Conjunction, or sentence start.

- [ ] **Step 4: Run — expect PASS**
- [ ] **Step 5: Commit** — `git commit -am "feat: chunker (NP/PP/VP, subject detection)"`

## Task 2.3: M2 exit-criteria smoke

- [ ] **Step 1: Extend `tests/test_smoke.cpp`** — assert `tag(u"I can has a cat.")` yields: `can`=Modal, `has`=Auxiliary(Have,ThirdSingular), `cat`=Noun, `a`=Determiner; and `findSubjectHead` on `"I can has"` = 0.
- [ ] **Step 2: Full build + ctest green**
- [ ] **Step 3: Commit** — `git commit -am "test: M2 exit-criteria smoke"`

**M2 exit:** ambiguity tests green; "I can has a cat." → can=modal, has=aux-have-3ps, cat=noun.

---

# M3: Rule framework + R1-R9 (SPEC §6.6, §7, §14)

## Task 3.1: Rule framework + registry

**Files:**
- Create: `src/rules.h`, `src/rules.cpp`, `tests/rule_test_util.h`; Modify: `CMakeLists.txt` (add framework now; rule files as they land)

- [ ] **Step 1: Write failing tests** — minimal placeholder so the framework compiles (registry returns rules; empty text yields no issues).

- [ ] **Step 2: Run — expect FAIL** (no `rules.h` yet)

- [ ] **Step 3: Implement** — `src/rules.h`:

```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "stoppard/chunker.h"
#include "stoppard/tokenizer.h"

namespace stoppard {

struct Analysis {
    std::u16string_view text;
    std::vector<TaggedToken> tokens;
    std::vector<Chunk> chunks;
    std::vector<SentenceSpan> sentences;
    DialectProfile profile;
};

struct Rule {
    std::u16string id;
    std::u16string description;
    virtual void run(const Analysis&, std::vector<Issue>&) const = 0;
    virtual ~Rule() = default;
};

// Priority order (R1..R9). Overlapping issues: earlier rule wins.
const std::vector<const Rule*>& allRules();

// Full pipeline: tokenize → tag → chunk → rules → dedup (overlapping starts).
std::vector<Issue> runAll(std::u16string_view text, Dialect dialect);

} // namespace stoppard
```

`src/rules.cpp` — `runAll`: tokenize, `tag`, `chunk`, `splitSentences`, `profileFor(dialect)`, build `Analysis`, run each rule in `allRules()` order, then dedup: sort issues by (start, priority); drop any issue whose start falls inside the span of a higher-priority kept issue (`start < last.end`). Expose `allRules()` as an explicit static list in this file (each family header declares one class).

`tests/rule_test_util.h`:
```cpp
#pragma once
#include <gtest/gtest.h>
#include "stoppard/rules.h"
using namespace stoppard;

inline std::vector<Issue> checkAll(std::u16string_view text, Dialect d = Dialect::American) {
    return runAll(text, d);
}
inline void expectClean(const std::vector<Issue>& issues) {
    EXPECT_TRUE(issues.empty()) << "expected no issues";
}
inline void expectIssue(const std::vector<Issue>& issues, int start, int len,
                        const char16_t* suggestion = nullptr) {
    ASSERT_FALSE(issues.empty());
    const auto& it = issues.front();
    EXPECT_EQ(it.start, start);
    EXPECT_EQ(it.length, len);
    if (suggestion) {
        ASSERT_FALSE(it.suggestions.empty());
        EXPECT_EQ(it.suggestions.front().text, std::u16string(suggestion));
    }
}
```
(Add `matchCase(source, replacement)` to `src/morphology.h`/`.cpp` in this task — used by all rules' suggestions: uppercase-all → ALL-CAPS; first-upper → Capitalized; else as-given.)

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: rule framework, registry, dedup, matchCase"`

## Task 3.2: R1 (modal) + R2 (auxiliary)

**Files:** Create `src/rules_modal.{h,cpp}`, `src/rules_aux.{h,cpp}`, `tests/test_rules_modal.cpp`, `tests/test_rules_aux.cpp`; register sources in CMake.

- [ ] **Step 1: Failing tests** — `test_rules_modal.cpp`:

```cpp
TEST(R1, Flags) {
    expectIssue(checkAll(u"I can has a cat."), 6, 3, u"have");
    expectIssue(checkAll(u"She can goes to the store."), 8, 4, u"go");
    expectIssue(checkAll(u"We could went there."), 7, 4, u"go");
    expectIssue(checkAll(u"He might gone home."), 8, 4, u"go");
    expectIssue(checkAll(u"I can going now."), 4, 5, u"go");
    expectIssue(checkAll(u"I can't has a dog."), 7, 3, u"have");   // decomposed n't
    expectIssue(checkAll(u"You must not has it."), 12, 3, u"have");
    expectIssue(checkAll(u"can has been"), 4, 3, u"have");          // modal + non-base aux
}
TEST(R1, Clean) {
    expectClean(checkAll(u"I can have a cat."));
    expectClean(checkAll(u"We will go soon."));
    expectClean(checkAll(u"I could go."));
    expectClean(checkAll(u"I may be going."));                      // be is auxiliary, base
    expectClean(checkAll(u"I must not go."));
    expectClean(checkAll(u"He can wait."));
}
TEST(R1, CasePreservingSuggestion) {
    expectIssue(checkAll(u"Can Goes?"), 4, 4, u"Go");
}
```

`test_rules_aux.cpp`:
```cpp
TEST(R2, DoAuxiliaries) {
    expectIssue(checkAll(u"He did went."), 7, 4, u"go");
    expectIssue(checkAll(u"She does goes."), 8, 4, u"go");
    expectClean(checkAll(u"He did go."));
    expectClean(checkAll(u"She does not go."));
    expectClean(checkAll(u"Did you go?"));                          // no verb directly after did
}
TEST(R2, HaveAuxiliaries) {
    expectIssue(checkAll(u"She has go home."), 7, 2, u"gone");
    expectIssue(checkAll(u"I have went there."), 6, 4, u"gone");
    expectClean(checkAll(u"He has gone."));
    expectClean(checkAll(u"She has walked."));                      // regular past==participle
    expectClean(checkAll(u"I have to go."));                        // obligation idiom guard
    expectClean(checkAll(u"I have got a cat."));                    // have-got idiom guard (§14.4)
}
TEST(R2, Been) {
    expectIssue(checkAll(u"He has been go."), 11, 2, u"going");
    expectIssue(checkAll(u"It has been take."), 11, 4, u"taking");
    expectClean(checkAll(u"He has been gone."));                    // passive
    expectClean(checkAll(u"She has been walking."));
}
TEST(R2, BeForms) {
    expectIssue(checkAll(u"I am has a cat."), 5, 3, u"having");
    expectIssue(checkAll(u"They are went."), 8, 4, u"going");
    expectClean(checkAll(u"I am having a cat."));
    expectClean(checkAll(u"It is gone."));
    expectClean(checkAll(u"She was taken there."));
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement**

`rules_modal.cpp` — `R1ModalVerbForm`: for each Modal token, advance j while token is Adverb or Negator; if tokens[j] is Verb (non-Base) or Auxiliary (non-Base form: 3ps/past/participle/gerund) → Issue at token j: message `u"The form of the verb must agree with the modal."`, suggestion Replace `matchCase(token, lemma-or-aux-base)` (auxiliary base: be/have/do).

`rules_aux.cpp` — `R2AuxiliaryVerbForm`:
- `(do|does|did) (neg)* [Verb]` → non-Base → suggest Base. *Auxiliary* after do → skip (inversion like "Did have he..." — rare, precision).
- `(have|has|had) (neg)* [Verb]` → if next content token is `to` → skip (obligation); if lemma==`get` → skip (have-got); else if `lookupVerbForms(lemma).participle != token` → suggest participle, message `u"Auxiliary 'X' takes the past participle of the verb."`.
- `been (neg)* [Verb]` → Base or 3ps → suggest Gerund; regular Past whose participle==token → clean.
- `(am|is|are|was|were) [Verb]` → 3ps or Past and participle!=token → suggest Gerund; Base/participle → clean. Message: `u"Auxiliary 'X' takes the gerund of the verb."` (be-form variant).
- Guard applies throughout: only confident tags; Unknown verbs never fire.

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: rules R1 modal and R2 auxiliary (with idiom guards)"`

## Task 3.3: R3 (agreement) + R4 (to-infinitive)

**Files:** Create `src/rules_agreement.{h,cpp}`, `src/rules_to_infinitive.{h,cpp}`, `tests/test_rules_agreement.cpp`, `tests/test_rules_to_infinitive.cpp`

- [ ] **Step 1: Failing tests** — `test_rules_agreement.cpp`:

```cpp
TEST(R3, Adjacent) {
    expectIssue(checkAll(u"I has a cat."), 2, 3, u"have");
    expectIssue(checkAll(u"They goes home."), 5, 4, u"go");
    expectIssue(checkAll(u"He walk to school."), 3, 4, u"walks");
    expectIssue(checkAll(u"She run fast."), 4, 3, u"runs");
    expectClean(checkAll(u"I have a cat."));
    expectClean(checkAll(u"She runs fast."));
    expectClean(checkAll(u"Watch it break."));                       // "it"+base guard
    expectClean(checkAll(u"It breaks."));
}
TEST(R3, AcrossAuxiliaries) {
    expectIssue(checkAll(u"I has been there."), 2, 3, u"have");
    expectIssue(checkAll(u"They is going."), 5, 2, u"are");
    expectIssue(checkAll(u"He have a cat."), 3, 4, u"has");
    expectClean(checkAll(u"I have been there."));
    expectClean(checkAll(u"They are going."));
    expectClean(checkAll(u"I am happy."));
    expectClean(checkAll(u"She was here."));
    expectClean(checkAll(u"I can has"));                             // R1 wins on overlap (dedup)
}
TEST(R3, AcrossPPModifiers) {
    expectIssue(checkAll(u"The list of items are here."), 14, 3, u"is");
    expectIssue(checkAll(u"The boxes on the table is here."), 20, 2, u"are");
    expectClean(checkAll(u"The list of items is here."));
    expectClean(checkAll(u"There is a cat."));                       // expletive: inert in v1
    expectClean(checkAll(u"There are many cats."));
}
TEST(R3, CollectiveNouns) {                                          // §14.3
    expectIssue(checkAll(u"The team are winning.", Dialect::American), 5, 3, u"is");
    expectClean(checkAll(u"The team are winning.", Dialect::British));
    expectClean(checkAll(u"The team is winning.", Dialect::American));
    expectIssue(checkAll(u"The police is here."), 8, 2, u"are");     // always-plural, any dialect
}
```

`test_rules_to_infinitive.cpp`:
```cpp
TEST(R4, Flags) {
    expectIssue(checkAll(u"I want to has a cat."), 10, 3, u"have");
    expectIssue(checkAll(u"She tried to went home."), 13, 4, u"go");
    expectIssue(checkAll(u"I need to goes now."), 10, 4, u"go");
    expectIssue(checkAll(u"Ought to has."), 8, 3, u"have");           // §14.4
}
TEST(R4, Clean) {
    expectClean(checkAll(u"I want to go."));
    expectClean(checkAll(u"I look forward to going."));               // gerund never flagged
    expectClean(checkAll(u"I went to the store."));
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement**

`rules_agreement.cpp`:
- **Adjacent**: `Pronoun(subject, person≠0) + Verb` adjacent. {I,you,we,they}+3ps → suggest Base; {he,she,it}+Base → suggest Third, **skip when pronoun is `it`** (object-case ambiguity). Message `u"The verb must agree with the subject."`.
- **Across auxiliaries**: subject Pronoun + (Modal|Auxiliary|Negator)* + Auxiliary with form ∈ {Base, ThirdSingular, Past}; static allowed-subject table: am(1,-1), is(3,-1), are(any plural), was(1,-1|3,-1), were(any plural), has(3,-1), have(any non-3-singular), does(3,-1), do(any non-3-singular) → mismatch → flag the auxiliary, suggest correct form via table (matchCase). Message as above.
- **Across PP modifiers**: for each NP chunk (head = endToken) whose head token is Noun with number ≠ 0 and not in sentence starting with `There`: scan forward to first Verb token, allowing only Preposition/Noun/Adverb tokens in between (any Conjunction or Comma → skip); if that verb is `is/are/was/were` (Auxiliary, auxVerb=Be, form ThirdSingular/Past) → number mismatch → flag, suggest flipped form (`is↔are`, `was↔were`). Collective table (~60: team, government, committee, staff, family, audience, crowd, public, board, council, press, army, navy, club, company, firm, group, union, jury, parliament, class, ...) — if head ∈ table and profile.collectivePluralAgreement → never flag plural verb; if head ∈ always-plural table (police, cattle, clergy, people, vermin, poultry, ...) → flag singular verb (→ plural) in all dialects.

`rules_to_infinitive.cpp` — `R4ToInfinitive`: token is Preposition `to` + next token is Verb or Auxiliary with form ThirdSingular or Past → flag, suggest Base (aux base: be/have/do), message `u"The preposition 'to' takes the base form of the verb."`. Gerund/Base/participle → clean.

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: rules R3 subject-verb agreement (collective-aware) and R4 to-infinitive"`

## Task 3.4: R5 (determiner) + R6 (pronoun case)

**Files:** Create `src/rules_determiner.{h,cpp}`, `src/rules_pronoun_case.{h,cpp}`, `tests/test_rules_determiner.cpp`, `tests/test_rules_pronoun_case.cpp`

- [ ] **Step 1: Failing tests**

```cpp
TEST(R5, Flags) {
    expectIssue(checkAll(u"These cat are cute."), 6, 3, u"cats");
    expectIssue(checkAll(u"Many child were there."), 5, 5, u"children");
    expectIssue(checkAll(u"I saw those box."), 10, 3, u"boxes");
    expectIssue(checkAll(u"This dogs bark."), 5, 4, u"dog");
    expectIssue(checkAll(u"Each cats has a toy."), 5, 4, u"cat");
    expectIssue(checkAll(u"These big cat is cute."), 11, 3, u"cats");   // adjective between
}
TEST(R5, Clean) {
    expectClean(checkAll(u"These cats are cute."));
    expectClean(checkAll(u"This is a dog."));
    expectClean(checkAll(u"These water is cold."));                     // mass-noun guard
    expectClean(checkAll(u"Each cat has a toy."));
}
```

```cpp
TEST(R6, SubjectPosition) {
    expectIssue(checkAll(u"Me went home."), 0, 2, u"I");
    expectIssue(checkAll(u"Him did it."), 0, 3, u"He");
    expectIssue(checkAll(u"Her said no."), 0, 3, u"She");
    expectIssue(checkAll(u"Us saw them."), 0, 2, u"We");
    expectClean(checkAll(u"I went home."));
    expectClean(checkAll(u"Me and John went home."));                   // coordination skip (M4.5)
}
TEST(R6, AfterPreposition) {
    expectIssue(checkAll(u"Give it to she."), 11, 3, u"her");
    expectIssue(checkAll(u"For I, it is hard."), 4, 1, u"me");
    expectIssue(checkAll(u"With he, we go."), 5, 2, u"him");
    expectClean(checkAll(u"I saw her."));
    expectClean(checkAll(u"Between you and I."));                       // PP-internal: M4.5
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement**

`rules_determiner.cpp` — `R5DeterminerNounAgreement`: Determiner(number ≠ 0) + (Adjective|Unknown|Adverb)* + Noun(number ≠ 0): plural det + singular noun → suggest `pluralize`; singular det + plural noun → suggest `singularize`. Skip when noun ∈ mass-noun stoplist (~30: water, milk, air, rice, money, music, furniture, information, advice, homework, news, luggage, equipment, software, bread, butter, cheese, sugar, salt, coffee, tea, oil, sand, dust, grass, traffic, weather, knowledge, literature, research, evidence). Message: `u"The determiner 'X' requires a {plural/singular} noun."`; suggestion matchCase'd.

`rules_pronoun_case.cpp` — `R6PronounCase`:
- Subject position: Pronoun(Object case, person 1..3) whose next content token (skipping Adverb/Unknown) is Verb or Auxiliary → flag pronoun, suggest subject form (me→I, him→he, her→she, us→we, them→they), message `u"Use the subject form of the pronoun here."`. **Skip when next token is Conjunction** (coordination).
- After preposition: Preposition + Pronoun(Subject case) adjacent → flag, suggest object form (I→me, he→him, she→her, we→us, they→them). Skip `it`, `you` (same forms). Message `u"Use the object form of the pronoun after a preposition."`.

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: rules R5 determiner-noun and R6 pronoun case"`

## Task 3.5: R7 (double modal) + R8 (double negative)

**Files:** Create `src/rules_double_modal.{h,cpp}`, `src/rules_double_negative.{h,cpp}`, `tests/test_rules_double_modal.cpp`, `tests/test_rules_double_negative.cpp`

- [ ] **Step 1: Failing tests**

```cpp
TEST(R7, Flags) {
    expectIssue(checkAll(u"I can could go."), 4, 5);                    // Remove
    expectIssue(checkAll(u"She will can do it."), 8, 3);
    expectIssue(checkAll(u"You must might go."), 7, 5);
    expectIssue(checkAll(u"I should would ask."), 9, 5);
    expectIssue(checkAll(u"I could not can go."), 11, 3);
}
TEST(R7, Clean) {
    expectClean(checkAll(u"I can go."));
    expectClean(checkAll(u"I should have gone."));                      // have = aux
    expectClean(checkAll(u"He cannot go."));
    expectClean(checkAll(u"I could not go."));
}
TEST(R7, RemoveKind) {
    auto issues = checkAll(u"I can could go.");
    ASSERT_EQ(issues.size(), 1u);
    ASSERT_EQ(issues[0].suggestions.size(), 1u);
    EXPECT_EQ(issues[0].suggestions[0].kind, SuggestionKind::Remove);
    EXPECT_EQ(issues[0].suggestions[0].text, u"");
}
```

```cpp
TEST(R8, Flags) {
    expectIssue(checkAll(u"I don't never go."), 8, 5, u"ever");
    expectIssue(checkAll(u"I don't know nothing."), 11, 7, u"anything");
    expectIssue(checkAll(u"He doesn't see nobody."), 14, 6, u"anybody");
    expectIssue(checkAll(u"I am not nobody."), 6, 6, u"anybody");
    expectIssue(checkAll(u"She didn't go nowhere."), 13, 7, u"anywhere");
}
TEST(R8, Clean) {
    expectClean(checkAll(u"I don't think anyone saw me."));
    expectClean(checkAll(u"I can't not laugh."));                       // can't-not guard
    expectClean(checkAll(u"There is no doubt."));                       // no-doubt guard
    expectClean(checkAll(u"No way."));
    expectClean(checkAll(u"I no longer go."));
    expectClean(checkAll(u"No one came."));
    expectClean(checkAll(u"I never go."));
    expectClean(checkAll(u"I didn't go, and nobody saw me."));          // comma boundary
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement**

`rules_double_modal.cpp` — `R7DoubleModal`: `Modal (Negator)* Modal` → flag second modal, suggestion `Suggestion{SuggestionKind::Remove, u""}`, message `u"Two modal verbs in a row."` (per spec §7).

`rules_double_negative.cpp` — `R8DoubleNegative`: first negator = token text `n't` or `not`; second = token text ∈ {never, no, none, nobody, nothing, nowhere} within ≤3 tokens, same sentence, no Comma/Conjunction between; guards: skip when second is `no` followed by {doubt, way, longer, one} (adjacent); `can't not` is safe by construction (second `not` not in the list). Suggest positive counterpart (never→ever, no→any, none→any, nobody→anybody, nothing→anything, nowhere→anywhere), message `u"Only one negative is allowed in a clause."`.

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: rules R7 double-modal and R8 double-negative with guards"`

## Task 3.6: R9 regionalisms + profileFor (SPEC §14.1-14.2)

**Files:** Create `src/rules_regionalisms.{h,cpp}`, `tests/test_rules_regionalisms.cpp`

- [ ] **Step 1: Failing tests**

```cpp
TEST(R9, Cards) {
    expectIssue(checkAll(u"It's in the cards.", Dialect::British), 5, 11, u"on the cards");
    expectClean(checkAll(u"It's in the cards.", Dialect::American));
    expectIssue(checkAll(u"It's on the cards.", Dialect::American), 5, 11, u"in the cards");
}
TEST(R9, Gotten) {
    expectIssue(checkAll(u"I have gotten it.", Dialect::British), 7, 6, u"got");
    expectClean(checkAll(u"I have gotten it.", Dialect::American));
    expectClean(checkAll(u"I have got it.", Dialect::British));
}
TEST(R9, Weekend) {
    expectIssue(checkAll(u"See you at the weekend.", Dialect::American), 8, 2, u"on");
    expectClean(checkAll(u"See you at the weekend.", Dialect::British));
    expectIssue(checkAll(u"See you on the weekend.", Dialect::British), 8, 2, u"at");
}
TEST(R9, DiscussAbout) {
    expectIssue(checkAll(u"We discuss about this.", Dialect::Indian), 4, 6, u"discuss");
    expectClean(checkAll(u"We discuss about this.", Dialect::American));
}
TEST(R9, ProfileAgreement) {
    EXPECT_TRUE(profileFor(Dialect::British).collectivePluralAgreement);
    EXPECT_FALSE(profileFor(Dialect::American).collectivePluralAgreement);
    EXPECT_FALSE(profileFor(Dialect::Canadian).collectivePluralAgreement);
    EXPECT_TRUE(profileFor(Dialect::NewZealand).collectivePluralAgreement);
}
```

- [ ] **Step 2: Run — expect FAIL**
- [ ] **Step 3: Implement** — `rules_regionalisms.cpp`:

```cpp
// {wrong (lowercase tokens), correction, wrong-in dialects}
struct RegionalismEntry {
    std::u16string_view wrong;      // space-joined tokens
    std::u16string_view correction;
    Dialect wrongIn[6];
    int nDialects;
};
```
Seed table per §14.2: `in the cards`→`on the cards` (Br/Au/In/NZ/Can); `on the cards`→`in the cards` (Am); `gotten`→`got` (Br/Au/In/NZ/Can — fires only when token is Verb with lemma `get` and participle form); `at the weekend`→`on the weekend` (Am); `on the weekend`→`at the weekend` (Br/Au/In/NZ); `discuss about`→`discuss` (In). Match by consecutive token text (case-insensitive); phrase span = token range; suggestion = correction with matchCase on first word; message `u"Regional expression: use \"" + correction + u"\" in this dialect."`. Also implement `profileFor()` here (collective flag: ON for British/Australian/Indian/NewZealand, OFF for American/Canadian; regionalisms: all entries whose wrong-in set contains the dialect).

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: rule R9 regionalisms and dialect profiles"`

## Task 3.7: Data-driven rule-case harness (SPEC §15)

**Files:** Create `tests/test_rule_cases.cpp`, `tests/data/rule_cases/R1_modal.txt` … `R9_regionalisms.txt`, `tests/data/CORPUS_SOURCES.md`; Modify `CMakeLists.txt` (compile definition `TESTS_DATA_DIR` pointing at `${CMAKE_CURRENT_SOURCE_DIR}/tests/data`)

- [ ] **Step 1: Write the harness** — `test_rule_cases.cpp`: for each `R{n}_*.txt` file, for each line `wrong | right | rule-id`:
  - tokenize both sides (split on whitespace, tokens = runs of non-space); token-level LCS alignment; mismatched contiguous runs → expected span (from `wrong` token starts) and suggestion (right tokens joined by space; empty right-run → kind Remove, text "");
  - identical sides → expect **zero** issues;
  - else → `runAll(wrong, American)` → **exactly one** issue matching span + suggestion. (`rule-id` column is documentation + future selective running; not asserted.)
- [ ] **Step 2: Write the data files** — initial content:

`R1_modal.txt`:
```
I can has a cat. | I can have a cat. | R1
She can goes to the store. | She can go to the store. | R1
We could went there. | We could go there. | R1
He might gone home. | He might go home. | R1
I can going now. | I can go now. | R1
I can't has a dog. | I can't have a dog. | R1
You must not has it. | You must not have it. | R1
can has been | can have been | R1
will goes | will go | R1
can have | can have | R1
could go | could go | R1
can't wait | can't wait | R1
```
`R2_aux.txt`:
```
She has go home. | She has gone home. | R2
I have went there. | I have gone there. | R2
He did went. | He did go. | R2
She does goes. | She does go. | R2
He has been go. | He has been going. | R2
It has been take. | It has been taking. | R2
I am has a cat. | I am having a cat. | R2
They are went. | They are going. | R2
I have to go. | I have to go. | R2
I have got a cat. | I have got a cat. | R2
He has gone. | He has gone. | R2
She has walked. | She has walked. | R2
```
`R3_agreement.txt`:
```
I has a cat. | I have a cat. | R3
They goes home. | They go home. | R3
He walk to school. | He walks to school. | R3
She run fast. | She runs fast. | R3
I has been there. | I have been there. | R3
They is going. | They are going. | R3
He have a cat. | He has a cat. | R3
The list of items are here. | The list of items is here. | R3
The boxes on the table is here. | The boxes on the table are here. | R3
The police is here. | The police are here. | R3
The team are winning. | The team is winning. | R3
Watch it break. | Watch it break. | R3
The cat sat on the mat. | The cat sat on the mat. | R3
```
`R4_to_infinitive.txt`:
```
I want to has a cat. | I want to have a cat. | R4
She tried to went home. | She tried to go home. | R4
I need to goes now. | I need to go now. | R4
I look forward to going. | I look forward to going. | R4
I want to go. | I want to go. | R4
```
`R5_determiner.txt`:
```
These cat are cute. | These cats are cute. | R5
Many child were there. | Many children were there. | R5
I saw those box. | I saw those boxes. | R5
This dogs bark. | This dog barks. | R5
Each cats has a toy. | Each cat has a toy. | R5
These water is cold. | These water is cold. | R5
This is a dog. | This is a dog. | R5
```
`R6_pronoun_case.txt`:
```
Me went home. | I went home. | R6
Him did it. | He did it. | R6
Her said no. | She said no. | R6
Us saw them. | We saw them. | R6
Give it to she. | Give it to her. | R6
For I, it is hard. | For me, it is hard. | R6
I saw her. | I saw her. | R6
Me and John went home. | Me and John went home. | R6
Between you and I. | Between you and I. | R6
```
`R7_double_modal.txt`:
```
I can could go. | I can go. | R7
She will can do it. | She will do it. | R7
You must might go. | You must go. | R7
I should would ask. | I should ask. | R7
I could not can go. | I could not go. | R7
I can go. | I can go. | R7
I should have gone. | I should have gone. | R7
```
`R8_double_negative.txt`:
```
I don't never go. | I don't ever go. | R8
I don't know nothing. | I don't know anything. | R8
He doesn't see nobody. | He doesn't see anybody. | R8
I am not nobody. | I am not anybody. | R8
She didn't go nowhere. | She didn't go anywhere. | R8
I can't not laugh. | I can't not laugh. | R8
There is no doubt. | There is no doubt. | R8
No one came. | No one came. | R8
I didn't go, and nobody saw me. | I didn't go, and nobody saw me. | R8
```
`R9_regionalisms.txt` (run under British):
```
It's in the cards. | It's on the cards. | R9
I have gotten it. | I have got it. | R9
See you on the weekend. | See you at the weekend. | R9
It's on the cards. | It's on the cards. | R9
I have got it. | I have got it. | R9
```
(`test_rule_cases.cpp` runs the `R9_*` file under `Dialect::British`, the rest under `American`.)

- [ ] **Step 3: Run — expect PASS** (after fixing any expectations that expose implementation bugs — these data files are the executable spec)
- [ ] **Step 4: Commit** — `git commit -am "test: rule-case data files with diff-based harness"`

**M3 exit:** each family has positive/negative/guard tests; data-driven matrix green; `profileFor` correct.

---

# M4: Engine + corpus + polish (SPEC §5, §8, §9, §10)

## Task 4.1: Engine

**Files:** Create `src/engine.{h,cpp}`, `tests/test_engine.cpp`; Modify `CMakeLists.txt`

- [ ] **Step 1: Failing tests**

```cpp
TEST(Engine, EmptyAndUnicode) {
    EXPECT_TRUE(Engine().check(u"").empty());
    EXPECT_TRUE(Engine().check(u"kōrero Māori ki Aotearoa").empty());   // all Unknown → no rules fire
}
TEST(Engine, UnpairedSurrogateDoesNotCrash) {
    EXPECT_TRUE(Engine().check(u"a \uD800 b").empty());
}
TEST(Engine, DialectState) {
    Engine e(Dialect::British);
    EXPECT_EQ(e.dialect(), Dialect::British);
    e.setDialect(Dialect::American);
    EXPECT_TRUE(e.check(u"I have gotten it.").empty());                 // AmE: clean
    e.setDialect(Dialect::British);
    EXPECT_EQ(e.check(u"I have gotten it.").size(), 1u);
}
TEST(Engine, SortedByStart) {
    auto issues = Engine().check(u"He have this cats and I can could go.");
    // R7 fires at "could" (later), R3 at "have"/"cats" (earlier) — must be sorted
    for (size_t i = 1; i < issues.size(); ++i)
        EXPECT_LE(issues[i - 1].start, issues[i].start);
}
TEST(Engine, DedupKeepsHigherPriority) {
    auto issues = Engine().check(u"I can has a cat.");
    ASSERT_EQ(issues.size(), 1u);                                        // R1 and R3 both fire on "has"
    EXPECT_EQ(issues[0].start, 6);
    EXPECT_EQ(issues[0].message, u"The form of the verb must agree with the modal.");
}
TEST(Engine, SpansWithinBounds) {
    auto issues = Engine().check(u"Me went home and I can has a cat.");
    for (const auto& it : issues) {
        EXPECT_GE(it.start, 0);
        EXPECT_LE(it.start + it.length, 32);
    }
}
```

- [ ] **Step 2: Run — expect FAIL** (no engine.cpp)
- [ ] **Step 3: Implement** — `src/engine.cpp`:

```cpp
Engine::Engine(Dialect dialect) : m_dialect(dialect) {}
Dialect Engine::dialect() const { return m_dialect; }
void Engine::setDialect(Dialect d) { m_dialect = d; }
std::vector<Issue> Engine::check(std::u16string_view text) const {
    return runAll(text, m_dialect);   // stateless pipeline; const → thread-safe by construction
}
```
(`runAll` already sorts + dedups. No mutex needed — document against `GrammarChecker`'s "serialize with a mutex" note; `Engine` holds only the dialect enum.)

- [ ] **Step 4: Run — expect PASS**; **Step 5: Commit** — `git commit -am "feat: Engine (dialect state, stateless check)"`

## Task 4.2: Clean corpus (SPEC §15)

**Files:** Create `tests/data/clean_corpus.txt`, `tests/data/clean_corpus_<dialect>.txt` ×6, `tests/test_corpus.cpp`, `scripts/fetch_corpus.py`, `tests/data/CORPUS_SOURCES.md`

- [ ] **Step 1: Write the corpus test**

```cpp
TEST(Corpus, AmericanClean) {
    auto lines = readLines(TESTS_DATA_DIR "/clean_corpus.txt");
    ASSERT_GT(lines.size(), 100u);
    for (const auto& line : lines) {
        auto issues = Engine().check(std::u16string_view(line));
        EXPECT_TRUE(issues.empty()) << "issue in clean line: " << line;
    }
}
TEST(Corpus, DialectFiles) {
    for (Dialect d : {Dialect::American, Dialect::British, Dialect::Australian,
                      Dialect::Indian, Dialect::Canadian, Dialect::NewZealand}) {
        auto lines = readLines(TESTS_DATA_DIR "/clean_corpus_" + name(d) + ".txt");
        ASSERT_GT(lines.size(), 10u);
        for (const auto& line : lines)
            EXPECT_TRUE(Engine(d).check(std::u16string_view(line)).empty())
                << name(d) << ": " << line;
    }
}
```
(`readLines`/`name` helpers in the test file; NZ file must contain Māori loanwords: "Kōrero ki te marae.", "The whānau gathered for kai." — must stay issue-free via the Unknown path.)

- [ ] **Step 2: Build the corpus** — `clean_corpus.txt`: ~120 sentences hand-drawn from scriba's `README.md`, `AGENTS.md`, `docs/kitchensink.md` prose + classic public-domain sentences (Gutenberg flavor: "The cat sat on the mat."-style plain prose, modal/aux/negation patterns exercised without errors). Per-dialect files: ~12 sentences each, drawn from the §15.4 sources (British: Commons-debates-style plain sentences; Australian: ACE-style mixed register; Canadian: Commons-style; Indian: press-release register, e.g. "The ministry announced the new policy."; New Zealand: Hansard-style + Māori loanwords). All sentences deliberately simple, no coordination/inversion, no collective subjects in the American file.
- [ ] **Step 3: Write `scripts/fetch_corpus.py`** — CLI (`--dialect american --out ...`), sources + URLs per §15.4 table, extracts sentences, applies the same hand-curation rules; documents usage in the header. **Not** run in tests (network); committed corpus files are the source of truth. `CORPUS_SOURCES.md`: provenance + attribution per §15 ("Contains information licensed under the Open Government Licence – Canada.", OPL v3.0 wording, CC BY entries, NZ Hansard PD note).
- [ ] **Step 4: Run tests — expect PASS** (fix any false positive in the corpus: that's a rule bug or an over-tricky sentence — simplify the sentence or fix the rule)
- [ ] **Step 5: Commit** — `git commit -am "test: clean-corpus zero-issue bar (all six dialects)"`

## Task 4.3: Mutations + regression anchors + perf smoke

**Files:** Create `tests/test_mutations.cpp`, `tests/test_regression.cpp`, `tests/test_performance.cpp`; Modify `CMakeLists.txt`

- [ ] **Step 1: Regression anchors** (SPEC §9):

```cpp
TEST(Regression, HarperAnchors) {
    auto issues = Engine().check(u"I has a cat.");
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].start, 2); EXPECT_EQ(issues[0].length, 3);
    ASSERT_EQ(issues[0].suggestions.size(), 1u);
    EXPECT_EQ(issues[0].suggestions[0].text, u"have");
    EXPECT_TRUE(Engine().check(u"This is helo wrking text.").empty());   // never flagged for spelling
    EXPECT_TRUE(Engine().check(u"The cat sat on the mat.").empty());
}
```

- [ ] **Step 2: Mutations** (`test_mutations.cpp`) — for each clean corpus line, apply a guarded operator, expect **exactly one** issue:

| Operator | Anchor (detected in the clean line) | Mutation | Expected rule |
|---|---|---|---|
| Verb-form swap | `[modal] + [Verb Base]` | verb → 3ps | R1 |
| Verb-form swap | `[I/you/we/they] + [Verb Base]` | verb → 3ps | R3 |
| Verb-form swap | `[he/she] + [Verb 3ps]` | verb → Base | R3 |
| Pluralize head | `[sing det] + [sing noun]` (not mass) | noun → plural | R5 |
| Pronoun-case swap | `[prep] + [subject pronoun]` | → object case | R6 |

Guard table: skip sentences containing commas/conjunctions/inversion (only simple declaratives mutate). Operators `negation flip`, `modal insertion`, `tense shift` are deferred to M4.5/M8 (their guard tables arrive with those rules) — noted in `tests/data/rule_cases/mutations.txt` header.

- [ ] **Step 3: Perf smoke** (`test_performance.cpp`) — build ~10k words (`"The cat sat on the mat. "` × 1400), `Engine().check`, assert `< 1000 ms` (generous CI-safe bound; target is <10 ms per SPEC §10 — measured locally in Release and recorded in the file header).
- [ ] **Step 4: Run full suite — expect PASS**; **Step 5: Commit** — `git commit -am "test: regression anchors, mutation matrix, perf smoke"`

**M4 exit:** false-positive corpus zero-issue; full suite green; regression anchors green.

---

# M5: Phase 2 — Scriba integration (SPEC §11)

All steps run in `/home/tpa/code/scriba` (Release build, `timeout 480000` per AGENTS.md).

## Task 5.1: Vendor + CMake

- [ ] **Step 1:** Copy `../stoppard` → `vendor/stoppard/` (exclude `.git/`, `build/`). In `vendor/stoppard/CMakeLists.txt`, wrap the gtest block: `option(STOPPARD_BUILD_TESTS "Build stoppard tests" ON)` and gate `FetchContent/googletest` + `test_stoppard` behind it (scriba builds must not fetch gtest).
- [ ] **Step 2:** In scriba's `CMakeLists.txt` (before the `scriba` target): `set(STOPPARD_BUILD_TESTS OFF CACHE BOOL "" FORCE)`, `add_subdirectory(vendor/stoppard)`; link `stoppard` to `scriba` (current harper link at :89) and to `scriba_spell` (:201).
- [ ] **Step 3:** Build + verify `stoppard` compiles inside scriba: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4` (heavy — allow up to 8 min).
- [ ] **Step 4:** Commit — `git commit -am "build: vendor stoppard, link into scriba and scriba_spell"`

## Task 5.2: StoppardEngine

**Files:** Create `src/StoppardEngine.{h,cpp}`

- [ ] **Step 1:** `StoppardEngine : public GrammarChecker` — ctor `explicit StoppardEngine(const QString &dialect = QStringLiteral("American"))` (maps the 6 dialect names → `stoppard::Dialect`; unknown → American); `setDialect(QString)`; `check(const QString&)`:
  ```cpp
  std::u16string_view view(reinterpret_cast<const char16_t*>(text.utf16()), text.size());
  auto issues = m_engine.check(view);
  QList<Issue> out;
  for (const auto& it : issues) { Issue issue; issue.start = it.start; issue.length = it.length;
      issue.message = QString::fromStdU16String(it.message);
      for (const auto& s : it.suggestions)
          issue.suggestions.append({ static_cast<Issue::SuggestionKind>(s.kind),
                                     QString::fromStdU16String(s.text) });
      out.append(issue); }
  return out;
  ```
  No mutex (Engine is stateless/const — comment explains vs the old harper mutex). **Delete** the `byteOffsetPerChar`/`byteToChar` bridge (`HarperEngine.cpp:39-78,139-154` will be deleted in 5.4).
- [ ] **Step 2:** Minimal compile + unit smoke (temporary): `Engine().check(QStringLiteral("I has a cat."))` via a scratch test, then remove.
- [ ] **Step 3:** Commit — `git commit -am "feat: StoppardEngine implementing GrammarChecker"`

## Task 5.3: Editor + Preferences

**Files:** Modify `src/Editor.cpp`, `src/Preferences.h`, `src/PreferencesDialog.cpp`

- [ ] **Step 1:** `Editor.cpp` — `sharedGrammarChecker()` returns `new StoppardEngine(dialectName)` (replace :52-56); dialect setter: `if (auto *eng = dynamic_cast<StoppardEngine*>(m_grammarChecker)) eng->setDialect(name);` (replace the HarperEngine cast at :1076-1078).
- [ ] **Step 2:** `Preferences.h` — `HarperDialect = "harperDialect"` → `GrammarDialect = "grammarDialect"` (:75); `CurrentConfigVersion` 1→2 (:49); in `migrateSettings` (:150-175): if old `harperDialect` key exists → copy value to `grammarDialect`, then remove old key; add `harperDialect` to the removed-keys list.
- [ ] **Step 3:** `PreferencesDialog.cpp` — rename `m_harperDialectCombo` → `m_grammarDialectCombo`; read/write `GrammarDialect` (:742-752, :977, :1043); add "New Zealand" to the combo (SPEC §14.4; PreferencesDialog.cpp:742-747).
- [ ] **Step 4:** Commit — `git commit -am "feat: grammarDialect pref (with migration) and New Zealand option"`

## Task 5.4: Delete Harper

- [ ] **Step 1:** `git rm src/HarperEngine.{h,cpp} vendor/harper-ffi rust-toolchain.toml` (check vendor/harper-ffi isn't referenced elsewhere first).
- [ ] **Step 2:** `CMakeLists.txt` — remove `HARPER_FFI_*` block (:14-24), harper link (:89), PRE_LINK (:92-96), `scriba_spell` harper link (:201), test PRE_LINK (:203-207).
- [ ] **Step 3:** Build clean-ish: `cmake --build build --target clean 2>/dev/null; cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4` (8-min timeout).
- [ ] **Step 4:** Commit — `git commit -am "chore: remove harper-ffi and its cmake plumbing"`

## Task 5.5: Tests + docs + verify (SPEC §11.6-11.8)

- [ ] **Step 1:** `tests/test_grammar_checker.cpp` → `StoppardEngine`; port `EngineInitialises`, `DetectsGrammarError` ("I has a cat."), `DoesNotFlagSpelling`; replace `DialectAffectsRegionalIdioms` with `RegionalismsFollowDialect` ("It's in the cards." → issue in British, none in American).
- [ ] **Step 2:** Docs — `README.md`: rewrite grammar feature line (:71), drop the Rust-toolchain paragraph (:104); `CONTRIBUTING.md`: replace HarperEngine line (:24) and harper-ffi tree entry (:62) with Stoppard/vendor/stoppard.
- [ ] **Step 3:** Verify — `ctest --output-on-failure -j1` from `build/` (auto xvfb), then `timeout 3 build/scriba || true` (no segfault), then `timeout 3 build/scriba docs/kitchensink.md` for a rendering sanity pass.
- [ ] **Step 4:** Commit — `git commit -am "test, docs: port grammar tests to Stoppard, update docs"`

**M5 exit:** scriba build + ctest green; app runs; harper fully removed; docs updated.

---

## Post-M9: held-out Birkbeck eval + confusable spec

Landing record (2026-08-03), matching SPEC §19.10 and §17.7:

- [x] `scripts/fetch_birkbeck.py` — downloads/filters Mitton's `missp.dat`
      (CC BY-NC-SA 3.0) to the wiki shape (typo ∉ dict, intended ∈ en-US,
      ASCII, lev ≤ 4, dedupe); writes gitignored `bench/birkbeck/` (26,474
      pairs; 3,866 real-word errors excluded, 524 off-dict, 1,212 non-ASCII,
      3,932 too-far, 50 dup); raw corpus never committed.
- [x] `scripts/analyze_dump.py` — per-Levenshtein-bucket (d1…d5+) top-1/top-5
      table from any DumpMissed/BirkbeckReport DUMP output.
- [x] `SpellingParity.BirkbeckReport` — env-gated (`STOPPARD_BIRKBECK_FILE`),
      report-only, can never fail CI. Full run (2026-08-03, n=26,474, ~12 min):
      totals 47.9/60.8 top-1/top-5; d1 78.6/96.1, d2 48.4/61.5, d3 23.5/34.2,
      d4 11.5/16.5. Pool-gap diagnosis: intended word never generated for
      d2 38.5% / d3 65.8% / d4 83.5% of pairs (only 102 of 10,277 misses had
      it in the pool but ranked ≥5) — candidate generation, not ranking, is
      the d2+ lever (see SPEC §19.10).
- [x] Hunspell baseline on Birkbeck (hunspell 1.7.2 + bundled en_US, capture
      `bench/birkbeck/hunspell_suggestions.txt`): on the identical 26,191
      flagged pairs stoppard leads every bucket — total 48.2/61.0 vs
      41.9/56.2 (+6.3pp top-1); d1 +11.6, d2 +5.6, d3 +2.6, d4 +1.2.
      Hunspell also deems 232 typos correct (archaic SCOWL words) — recall
      gap, excluded per the wiki method.
- [x] Confusable-feature spec (SPEC §17.7) — real-word homophones are a
      separate feature, still gated on content-word noun/verb detection (M9's
      word list is plain, no POS); Birkbeck's 3.9k real-word bucket measures
      demand. Implementation is future work, kept OUT of the spelling pass.
- [ ] Confusable-feature implementation (R12 extension): POS pass over the
      dictionary (or curated per-row noun/verb tables), then the three §17.2
      deferred rows + candidate rows of §17.7. Blocked on content-word
      noun/verb detection; revisit after M10.

-- self-check on data flow: the two held-outs are disjoint in intent; wiki
   keeps the committed floors, Birkbeck the report-only eval.

## Self-review notes

- Spec coverage: §4 header → T1; §6.1/6.2/6.3 → T2/T3/T4; §6.4/6.5 → M2; §6.6+§7+§9(rule suites)+§14 → M3; §5+§8+§9(corpus)+§10+§15 → M4; §11+§12(M5) → M5.
- Flagged gap: spec §6.2 lists full contracted words in the lexicon table — plan resolves them via tokenizer decomposition (Task 3 decision), consistent with §6.1.
- Key decisions (flagged vs. spec, all conservative):
  1. Auxiliaries tag as Auxiliary even after modals → R1/R4 explicitly handle "modal/`to` + non-base auxiliary" (spec's "can has→have" still works; dedup keeps R1's message).
  2. "no"/"neither" tag as Determiner (R8 matches by text).
  3. "there is/are" sentences are naturally inert in v1 (post-verbal NPs aren't subjects) — R11 stays M4.5 per spec.
  4. Harness expectations derive from **token-level LCS** of wrong/right (handles insert/remove/replace cleanly, unlike char diff).
  5. Mutations v1 = 5 operators (negation/tense/modal-insertion deferred to M4.5/M8).
  6. Vendored stoppard gates its gtest on `STOPPARD_BUILD_TESTS` so scriba's build never fetches gtest.

# Stoppard — Rule-Based English Grammar Engine

## 1. Overview

Stoppard is a standalone, rule-based English grammar-checking library that replaces Harper (harper-core via `vendor/harper-ffi`) in Scriba. Where Harper is a curated collection of word/phrase-specific patches over shallow POS patterns, Stoppard is built on grammar structure: closed-class lexicons, verb/noun paradigms (morphology), a conservative POS tagger, light chunking, and generalizable rules. Same output contract (spans + messages + suggestions), same UI behavior — no user-facing changes.

## 2. Naming & locations

| Thing | Value |
|---|---|
| Library name | Stoppard (after Tom Stoppard; pun on "stop word") |
| Dev repo | `/home/tpa/code/stoppard` (new git repo, sibling of scriba) |
| Vendored into scriba at | `vendor/stoppard/` (working copy — local changes go straight in, mathml2omml model, no patch workflow) |
| C++ class in scriba | `src/StoppardEngine.{h,cpp}` (implements `GrammarChecker`) |
| Namespace | `stoppard` |
| License | GPL-3.0, header `// Copyright (C) 2026 LazyArse` + standard notice on every `.h/.cpp` |

## 3. Repo layout (`../stoppard`)

```
stoppard/
├── CMakeLists.txt          # C++23, no Qt; targets: stoppard (static), test_stoppard (gtest)
├── SPEC.md
├── README.md
├── LICENSE
├── include/stoppard/stoppard.h   # public API (only Qt-free header)
└── src/
    ├── tokenizer.{h,cpp}
    ├── lexicon.{h,cpp}      # closed-class tables (~400 function words)
    ├── morphology.{h,cpp}   # verb paradigms + noun plurals
    ├── tagger.{h,cpp}
    ├── chunker.{h,cpp}
    ├── rules.h              # Rule interface + registry
    ├── rules_modal.{h,cpp}  # one file per rule family
    ├── rules_aux.cpp
    ├── rules_agreement.cpp
    ├── rules_to_infinitive.cpp
    ├── rules_determiner.cpp
    ├── rules_pronoun_case.cpp
    ├── rules_double_modal.cpp
    ├── rules_double_negative.cpp
    └── engine.{h,cpp}
└── tests/                  # gtest, no Qt
```

## 4. Public API (`stoppard.h`)

```cpp
namespace stoppard {

enum class SuggestionKind { Replace, Remove, InsertAfter };

struct Suggestion { SuggestionKind kind = SuggestionKind::Replace; std::u16string text; };

struct Issue {
    int start = 0;        // offset in UTF-16 code units (== QString index)
    int length = 0;
    std::u16string message;
    std::vector<Suggestion> suggestions;
};

enum class Dialect { American, British, Australian, Indian, Canadian };
enum class Language { None, American, British };   // spelling (§19); None = grammar-only

class Engine {
public:
    explicit Engine(Dialect dialect = Dialect::American);
    std::vector<Issue> check(std::u16string_view text) const;  // thread-safe; snapshots config at call start
    Dialect dialect() const; void setDialect(Dialect d);
    void setLanguage(Language l);                   // spelling dictionary (§19); default None
    void setUserWords(std::vector<std::u16string> words);  // atomic swap (§19.2)
};

}
```

Engine's mutable config (dialect, spelling language, user words) is swapped
**atomically**: `check()` snapshots it at call start, so the no-mutex
thread-safety contract (§5) is unchanged by §19's additions.

UTF-16 spans match `QString` indexing exactly — Scriba's byte↔char conversion bridge (HarperEngine.cpp:39-78, 139-154) is deleted, not reimplemented.

## 5. Pipeline

`check()`: **tokenize → tag → chunk → run rules (priority order) → spelling pass (§19, only when Language ≠ None) → dedup → issues sorted by start**. All state is local to the call; `Engine` holds only the atomically-swapped config snapshot (§4). Lexicon/morphology tables are immutable globals → safe to share across threads.

## 6. Components

### 6.1 Tokenizer
- Word, Number, Punctuation, Whitespace tokens with `[start, len)` in UTF-16 code units.
- Contraction decomposition: `don't`→`do`+`n't`, `she's`→`she`+`'s`, `I'm`→`I`+`'m`, `we'll`→`we`+`'ll`, `can't`→`can`+`n't`, `won't`→`will`+`n't` (won't is irregular — table maps it), `let's`→`let`+`'s`.
- Sentence splitting on `. ! ?` followed by whitespace+capital; used to bound clause context (double-negative, agreement).
- Markdown awareness: fenced code blocks (`` ``` ``...`` ``` ``) and inline `` `code` `` spans become opaque single tokens; `#`-style heading markers are punctuation. Grammar never lints code (mirrors Harper's markdown document behavior).

### 6.2 Lexicon (closed classes — grammar vocabulary, not content phrases)
Table keyed by lowercase word → tag + features:
- **Determiners**: a, an, the, this, these, that, those, each, every, some, any, many, several, few, both, various, numerous, neither, either, all, no (+ number: singular/plural).
- **Pronouns** (person/number/case): I, me, we, us, you, he, him, she, her, it, they, them + possessives (my, mine, our, ours, your, yours, his, her, hers, its, their, theirs).
- **Modals + contractions**: can, could, will, would, shall, should, may, might, must, can't, couldn't, won't, wouldn't, shan't, shouldn't, mayn't, mightn't, mustn't, cannot, 'll, 'd.
- **Auxiliaries**: be (am, is, are, was, were, been, being), have (have, has, had, having), do (do, does, did, done, doing).
- **Prepositions**: in, on, at, of, with, by, for, to, from, about, after, before, between, under, over, through, without, etc.
- **Conjunctions**: and, or, but, nor, because, although, though, if, unless, while, whereas, since, so, yet.
- **Negators**: not, n't, never, no, none, nobody, nothing, nowhere, neither.
- **Interrogatives**: what, who, whom, whose, which, when, where, why, how.

### 6.3 Morphology (the paradigm layer)
- `VerbForms { base, third, past, participle, gerund; bool known; }` keyed by lemma.
- Regular conjugation rules: `-s/-es/-ies` (3ps), `-ed/-d/-ied` (past + participle), `-ing` with e-drop and consonant-doubling.
- Irregular verb table (~200 entries: go/going/went/gone, take/took/taken, get/got/gotten, see/saw/seen, be/am/is/are/was/were/been/being, have/has/had, do/does/did/done, ...). Standard reference list; every entry needed by a rule test must be present.
- Noun number: regular `-s/-es/-ies/-ves` + irregulars (~60: child/children, mouse/mice, foot/feet, tooth/teeth, man/men, woman/women, person/people, ox/oxen, goose/geese, sheep, fish, ...).
- API: `lookupVerbForms(word)`, `classifyVerbForm(word)` (lemma + which form), `pluralize(noun)`, `singularize(noun)`, `nounNumber(word)`. All case-insensitive; suggestions preserve the source's case (match-case).

### 6.4 Tagger (precision-first)
1. Closed-class lookup → deterministic tag.
2. Morphology lookup → verb/noun tag with exact form.
3. Suffix heuristics: `-ing`→gerund, `-ed`→past, `-tion/-ness/-ment/-ity/-er`→noun, `-ly`→adverb, trailing `-s`→ambiguous (3ps-or-plural).
4. Context boost: a word directly after a subject pronoun or modal is treated as a verb.
5. Unknown/ambiguous → `Unknown` tag; **rules must not fire on Unknown** (this is the false-positive guarantee).

### 6.5 Chunker
- **NP**: (det|poss|num) (adj|adv)* (noun|pronoun); head = last nominal.
- **PP**: preposition + NP.
- **VP**: (modal|aux)* main-verb (particles).
- Clause **subject**: first pronoun or NP before the VP (sentence start, or after conjunction/comma).
- Feeds agreement-across-modifiers and subject detection. Chunks store spans for rule targeting.

### 6.6 Rule framework
```cpp
struct Rule { std::u16string id, description; virtual void run(const Analysis&, std::vector<Issue>&) const = 0; };
```
- Rules registered in a fixed **priority order**; after all run, issues overlapping on `start` are deduped — the higher-priority rule wins.
- Every rule must be conservative: all tokens fully resolved, morphology known, no inversion/questions, no coordination (v1).

## 7. Rules (v1)

### R1 ModalVerbForm — "I can **has** a cat"
Pattern: `[modal] (adverb|negator)* [verb that is not base]` → flag the verb; suggest base form.
- Flag: can has→have, will goes→go, could went→go, can going→go, might gone→go.
- Clean: can have, will go, could go, may be going.

### R2 AuxiliaryVerbForm
- `do/does/did (+neg)* [verb]` → must be base. Flag: did went→did go, does goes→does go. Clean: did go.
- `have/has/had (+neg)* [verb]` → must be past participle. Flag: has go→has gone, have went→have gone. Clean: has gone, had been.
  - **Guard**: `have to` (obligation idiom: "I have to go") must never fire.
- `been [verb]` → gerund or participle. Flag: has been go→has been going, been gone (passive ok, "has been gone" ok).
- `am/is/are/was/were [verb]` → flag only when the verb form is unambiguously inflected (3ps or past): is has→is having, are went→are going. **Stretch/lower-confidence**: be + clearly-verbal base (is have→is having). Never fire when the following word could be a noun.

### R3 SubjectVerbAgreement
- **Adjacent**: `[subject pronoun] [verb]` — I/you/we/they + 3ps → base (I has→I have, they goes→they go); he/she/it + unambiguous base → 3ps (he walk→he walks). Match-case suggestion. Not "it" + base (object-case ambiguity: "watch it break").
- **Across auxiliaries/modals**: the first inflecting auxiliary agrees with the subject: I has been→I have been, they is going→they are going, he have→he has.
- **Across PP modifiers** (NP head agreement): `[det] (adj)* [head] (of/with/in PP)* [verb]` — flag verb only when head-number and verb-form are both unambiguous: "the list of items **are**"→is, "the boxes on the table **is**"→are. **Never fire** when: head unknown/ambiguous, pronoun head, question/inversion, any conjunction in the modifier span, or "there is/are" subjects.

### R4 ToInfinitive — "to **has**", "to **went**"
- `to [verb in 3ps or simple past]` → base: to has→to have, to went→to go, to goes→to go.
- **Never** flag `to` + gerund ("look forward to going", "to having done") — only 3ps/past forms are unambiguous errors.

### R5 DeterminerNounAgreement
- Plural determiner (these, those, many, several, few, both, various, numerous) + singular-countable noun → plural: these cat→these cats, many child→many children.
- Singular determiner (this, that, each, every, a, an) + plural noun → singular: this dogs→this dog, each cats→each cat.
- Fire only when noun number is resolvable and the noun is not mass/unknown. Guard: skip "a"/"an" before adjectives where the noun is further away? No — v1 fires only on `det [noun]` adjacency (with optional adjectives that don't change number).
- **Stretch** (not v1): a/an phonetic rule ("a apple", "an university") — requires an exception list (an hour, a university, an MBA); out of scope for v1.

### R6 PronounCase
- Subject position: `[object pronoun (me, him, her, us, them)] (adv)* [verb]` → subject case: me went→I went, him did it→he did it, her said→she said.
  - **v1 skip**: coordination ("me and John went") — needs NP-coordination handling; future.
- After preposition: `[preposition] [subject pronoun (I, he, she, we, they)]` → object case: to she→to her, for I→for me, with he→with him.
  - **Stretch**: "between you and I" (PP-internal "and I"); needs PP-boundary awareness; future.

### R7 DoubleModal
- `[modal] [modal]` (optional negator between) → flag the second modal: can could, will can, must might, should would. Suggestion: Remove (no single correct replacement); message: "Two modal verbs in a row."

### R8 DoubleNegative
- `[negated aux / n't word] ... (≤3 tokens) ... [never | no | none | nobody | nothing | nowhere]` within one clause, no comma/clause boundary between → flag the second negator; suggest the positive counterpart (never→ever, no→any, nothing→anything, nobody→anybody, none→any, nowhere→anywhere).
  - Flag: don't never, doesn't know nothing→anything, not nobody.
  - **Guard**: skip `can't not` ("I can't not laugh" is valid), "no doubt", "no way", "no longer", "no one".
- v1 is adjacency-ish only; negation-at-distance is future work.

## 8. Engine behavior
- Issues sorted by start; overlapping spans deduped (priority winner).
- Message strings: plain English, sentence case, e.g. "The form of the verb must agree with the modal." / "Auxiliary 'do' takes the base form of the verb."
- Suggestions: Replace by default; case-preserving conversions.
- No exceptions escape `check()`; invalid UTF-16 (unpaired surrogates) → skip that token, never crash.

## 9. Testing

- `tests/` gtest suites: tokenizer, lexicon, morphology (regular + irregular + case preservation), tagger (ambiguity + Unknown conservatism), chunker (NP/PP/VP, head extraction), one suite per rule family, engine (ordering, dedup, sort, empty input, unicode).
- Per rule: positive cases (flag, exact span, message, suggestion), negative/clean cases, and idiom-guard cases.
- **False-positive corpus test**: a curated set of known-good sentences (drawn from scriba's README, AGENTS.md, docs/kitchensink.md prose, and the repo's own commit prose) that must produce **zero** issues. This is the acceptance bar for v1 precision.
- Regression anchors from the old suite: "I has a cat." flagged with a replace suggestion; "This is helo wrking text." never flagged for spelling; spans within bounds; clean text "The cat sat on the mat." issue-free.

## 10. Performance
Linear-time pipeline; rules scan with early exits; target <10 ms on a 10k-word document (debounced worker thread in scriba — same as today).

## 11. Phase 2 — Single replace in Scriba

1. `vendor/stoppard/` ← copy of `../stoppard`; `add_subdirectory(vendor/stoppard)`; link `stoppard` into `scriba` (CMakeLists.txt:79-90) and `scriba_spell` (:193-207).
2. `src/StoppardEngine.{h,cpp}` — implements `GrammarChecker`: reads `grammarDialect` pref in ctor; `check()` wraps `QString::utf16()` as `u16string_view` (zero-copy), runs `Engine::check`, maps issues directly (spans already in code units); `QMutex` serialization as today. Delete `byteOffsetPerChar`/`byteToChar` bridge.
3. `Editor.cpp` — `sharedGrammarChecker()` returns `StoppardEngine`; dialect setter via `dynamic_cast<StoppardEngine*>` (Editor.cpp:1076-1078).
4. **Preferences**: `Preferences.h` — replace `HarperDialect` with `GrammarDialect = "grammarDialect"`; bump `CurrentConfigVersion` 1→2; in `migrateSettings`: if `harperDialect` exists → copy to `grammarDialect`, then remove old key; add `harperDialect` to removed-keys list. `PreferencesDialog.cpp` — rename `m_harperDialectCombo`→`m_grammarDialectCombo`, read/write `GrammarDialect` (:742-752, :977, :1043).
5. **Delete**: `src/HarperEngine.{h,cpp}`, `vendor/harper-ffi/`, `rust-toolchain.toml`; CMake: remove `HARPER_FFI_*` block (:14-24), link (:89), PRE_LINK (:92-96), `scriba_spell` link (:201), test PRE_LINK (:203-207).
6. **Tests**: `tests/test_grammar_checker.cpp` → `StoppardEngine`; port engine tests; replace `DialectAffectsRegionalIdioms` with `RulesAreDialectNeutral` ("I can has a cat." flagged in American and British).
7. **Docs**: README.md — rewrite grammar feature line (:71), drop Rust-toolchain paragraph (:104); CONTRIBUTING.md — replace HarperEngine line (:24) and harper-ffi tree entry (:62) with Stoppard.
8. **Verify**: clean-ish release build; `timeout 3 build/scriba` (no segfault); `ctest --output-on-failure -j1` from `build/`. No UI/screenshot changes.

## 12. Milestones

| M | Deliverable | Exit criteria |
|---|---|---|
| M1 | Repo init, spec, tokenizer + lexicon + morphology | unit tests green; `I can has a cat.` tokenizes & `has` resolves via paradigms |
| M2 | Tagger + chunker | ambiguity tests green; NP/PP/VP extraction verified |
| M3 | Rule framework + R1-R8 | each family has positive/negative/guard tests |
| M4 | Engine, dedup, corpus, polish | false-positive corpus zero-issue; full suite green |
| M5 | Phase 2 integration in scriba | build + ctest green; app runs; docs updated |

## 13. Out of scope (future)

Regionalism rules (dialect-aware word choice — replaces Harper's "in the cards" behavior); a/an phonetic rule; "between you and I"; coordinated-subject pronoun case; "There is/are" agreement; negation at distance; punctuation/long-sentence style rules; tense consistency. Spelling-suggestion research (word-frequency ranking, confusion-model tuning) — see the §19.4 research backlog.

See §16 for the roadmap bringing these into scope.

## 14. Dialect support (incl. New Zealand)

### 14.1 Model
`Dialect` gains New Zealand — 6 dialects, fully spelled out:

```cpp
enum class Dialect { American, British, Australian, Indian, Canadian, NewZealand };
```

A dialect is not a flag but a **profile** the engine carries into every `check()`:

```cpp
struct DialectProfile {
    bool collectivePluralAgreement = false;  // "the team are winning"
    std::vector<Regionalism> regionalisms;   // phrase corrections active in this dialect
    Language defaultDictionary;              // spelling dictionary (§19.3), resolves "follow dialect"
};
DialectProfile profileFor(Dialect);          // immutable, built once
```

Profile table (v1):

| Dialect | Collective plural | Default dictionary | Notes |
|---|---|---|---|
| American | no | `American` (en-US) | singular-only |
| Canadian | no | `American` (en-US) | follows American on agreement and spelling |
| British | yes | `British` (en-GB) | |
| Australian | yes | `British` (en-GB) | |
| Indian | yes | `British` (en-GB) | |
| New Zealand | yes | `British` (en-GB) | + Māori loanword exemptions (§19.7); tracks BrE/AuE; Maori loanwords (whanau, kai, marae, iwi...) hit the Unknown tag (never flagged by grammar rules) and are exempt from the spelling pass |

### 14.2 Regionalisms (R9) — the dialect rule
Table-driven mini-rules; each entry `{ dialects-where-wrong, phrase → correction, message }`. Rules R1-R8 consult the profile via `analysis.dialect`; the table is the only dialect-gated phrase data in the engine, and it is *closed and curated* (each entry is a deliberate variant decision, not a word dump).

Seed entries:

| Wrong in | Phrase | Correction |
|---|---|---|
| British/Au/In/NZ/Can | in the cards | on the cards |
| American | on the cards | in the cards |
| British/Au/In/NZ/Can | gotten (perfect participle) | got |
| British/Au/In/NZ | at the weekend → (AmE uses *on*) | on the weekend |
| American | on the weekend | at the weekend |
| Indian | discuss about | discuss |

This seed restores the old `DialectAffectsRegionalIdioms` behavior — the scriba test survives, renamed `RegionalismsFollowDialect`.

**Explicitly out of R9**: spelling variants (color/colour, -ise/-ize) are not
regionalism corrections — they are handled by the R14 spelling pass (§19), whose
dialect-selected dictionary accepts the dialect's orthography ("colour" is
correct under the en-GB list, flagged under en-US).

### 14.3 Dialect affects agreement (R3 extension)
R3 consults `collectivePluralAgreement`: when a noun head is in the **collective-noun table** (~60 words: team, government, committee, staff, family, audience, crowd, public, board, council, press, army, navy...) — dialects with plural agreement never flag "the team are"; American/Canadian accept only singular. Extra table column: always-plural nouns (police, cattle, clergy...) which never take singular agreement in any dialect.

### 14.4 Guards forced by dialect
- R2 must never flag `have got` (idiomatic in both; "I've got a cat" is fine everywhere).
- R1 note: "ought to" pattern variant — `[ought] to [verb]` requires base form ("ought to has" → "ought to have"); same for "have to", "used to", "get to" — the following verb must be base (R4 already catches inflected forms after "to").
- Scriba: Preferences combo gains "New Zealand" (PreferencesDialog.cpp:742-747); `grammarDialect` migration unaffected (names, not values).

## 15. Test example-set sources (concise, by design)

**Format**: data files, not C++ literals — `tests/data/`:

```
tests/data/
├── CORPUS_SOURCES.md      # provenance: source + license per entry block
├── clean_corpus.txt       # known-good, one sentence per line → must produce 0 issues
├── clean_corpus_<dialect>.txt  # one per dialect (6 files), run under its DialectProfile
├── rule_cases/
│   ├── R1_modal.txt       # lines:  wrong | right | rule-id
│   ├── ... one per rule
│   └── mutations.txt      # transformation operators
├── spelling_reference.txt # R14 suggestion-parity set: misspelled | correct (one per line)
└── scripts/fetch_corpus.py  # regenerates clean_corpus.txt from the sources below
```

A case line `I can has a cat. | I can have a cat. | R1` needs **no span bookkeeping**: the harness diffs wrong-vs-right, takes the first differing run as the expected span and the right-side text as the expected suggestion.

**Clean-corpus sources (researched online — licensing verified):**

| Source | Why it works | License |
|---|---|---|
| **Project Gutenberg** (pre-1929 classics: Austen, Dickens, Doyle, Twain...) | the classic PD prose corpus; gives British and American flavor | Public domain (US) |
| **OANC / MASC** (anc.org) — Open American National Corpus, ~15M words, 500K-word balanced subset | "public domain or otherwise free of usage and redistribution restrictions"; already sentence-split with chunks and clause boundaries — handy for the later C1 clause work | PD / free redistribution |
| **VOA Special English scripts** | simplified American English, good for plain-sentence coverage | US-government public domain |

**Explicitly ruled out**: COCA/COHA/BNC/iWeb (licensed, not redistributable — english-corpora.org requires purchase/academic license), Wikipedia/Wiktionary (CC BY-SA — incompatible with GPL embedding), Tatoeba (CC BY, per-sentence attribution impractical), Enron emails (murky license).

**Per-dialect corpus sources (searched online — licensing verified):**

| Dialect | Source | License | Register |
|---|---|---|---|
| American | OANC / MASC (anc.org), VOA Special English | PD / free redistribution | balanced written |
| British | UK Hansard — hansard-api.parliament.uk (REST API, daily updates) | Open Parliament Licence v3.0 | formal spoken |
| Australian | **ACE** — Australian Corpus of English (Macquarie, figshare, doi 10.25949/24629712.v1) — 1M words, 500 samples, 15 categories | CC BY 4.0 | balanced written |
| Canadian | Canadian Hansard — House of Commons (ourcommons.ca; "Hansard Speeches 1979-2018" CSV on Zenodo; lipad.ca 1901-present); Nunavut Hansard Inuktitut-English parallel corpus 3.0 (NRC repository, 1.3M aligned EN sentences) | OGL-Canada v2.0 terms / Crown reproduction practice (verify per-record licence) | formal spoken |
| New Zealand | NZ Parliament Hansard (hansard.parliament.nz) | **Public domain** — parliament.nz explicitly lists debates (Hansard) as not covered by copyright; site content CC BY 4.0 | formal spoken |
| Indian | PMIndia corpus, English side (statmt.org/pmindia — English translations of Indian govt press releases); fallback Kipling/Tagore | verify (released free; confirm CC terms before use) | press-release register |

Also found: **Open Australian Legal Corpus** (HuggingFace, CC BY 4.0, 1.4B tokens of AU legislation/court decisions) — enrichment pool for Australian.

**Attribution**: CC BY / OGL / OPL sources require attribution → `CORPUS_SOURCES.md` keeps a per-file attribution block; OGL uses the prescribed wording "Contains information licensed under the Open Government Licence – Canada."; OPL likewise "Contains Parliamentary information licensed under the Open Parliament Licence v3.0."; NZ Hansard (PD) needs none. A `NOTICE` file in the repo root covers all embedded excerpts.

**Register mix**: Hansards are formal spoken; ACE is the only balanced *written* non-American source — clean corpora per dialect blend both (Hansard + ACE for Australian; Hansard + pre-1929 Gutenberg fiction for the others).

**Verify-before-use** (execution-time task, not blocking): Nunavut corpus licence, PMIndia terms, Zenodo Hansard record licence. If a check fails, fall back to pre-1929 Gutenberg for that dialect.

**Layout**: `tests/data/clean_corpus_<dialect>.txt` — one per dialect (6 files), each run under its own `DialectProfile`; the NZ file's Māori loanwords (kōrero, whānau, marae, Aotearoa) must stay issue-free (Unknown-tag path). Full downloads cached under `scripts/data/` (not committed); the repo stores only the small sampled files + provenance.

**The four-layer source stack:**
1. **Hand-curated minimal pairs** — canonical; every rule path + guard gets a `wrong | right` pair (5-20 per rule).
2. **Known-good corpus** — ~150 sentences drawn from the table above (via `fetch_corpus.py`) + scriba's README/AGENTS/`docs/kitchensink.md` prose; zero-issue acceptance bar.
3. **Mutation operators** — each clean sentence × operator (verb-form swap, pluralize head, pronoun-case swap, article flip, modal insertion, negation flip, tense shift) → assert *exactly one* issue; ~150 sentences scale to thousands of cases. Guard table for operators that are legal on certain constructions.
4. **Real-world mined patterns** — distilled from public error corpora (harper's MIT-licensed tests, LanguageTool examples) as *inspiration only*, rewritten as original minimal pairs in (1).

**Adding a rule** = one `rule_cases/` file + corpus additions. That's the maintenance contract.

## 16. Out-of-scope → in-scope roadmap

The OOS list is seven rules that share **five underlying capabilities**. Build the capabilities once; the rules then drop in cheaply. This is the roadmap — capability-driven, not rule-by-rule.

### Capabilities
| Cap | What | Enables |
|---|---|---|
| C1 Clause segmentation | subordinate-clause boundaries (complementizers: that/if/because/although/since/when/who + commas) | negation-at-distance, comma splice, tense-consistency, reported-speech guards |
| C2 Coordination-aware chunking | `NP and NP`, `VP and VP` | coordinated-subject case, "between you and I", VP tense consistency |
| C3 PP-object grammar | object-NP awareness inside PPs | "between you and I" |
| C4 Phonetics | initial-sound classifier + orthography exceptions | a/an |
| C5 Style profile | per-rule enable flags + thresholds (plumbed to a future Preferences page) | long-sentence, ellipsis, repeated-word, comma splice |
| C6 Tense analysis | VP tense composition + time-adverb lexicon (yesterday/ago/last week/next year...) | tense-consistency |

### Milestones

**M4.5 — Syntax v2 (C1+C2+C3 + expletive recognition)** → three rules land together:
- **Negation at distance** (R8 rewrite): two negators in the *same clause* flag; "I didn't see nobody"→"anybody"; clean: "I don't think anyone saw me", "I can't not laugh", "No sooner had he left than it rained". Guard list grows (can't help but, not only...but also).
- **Coordinated-subject pronoun case** (R6 ext): flag the object-case pronoun(s) in a coordinated subject NP; the suggestion **replaces the whole coordinated phrase, reordered with 1st person last**:
  - "Me and John went" → **"John and I went"**
  - "John and me went" → "John and I went"
  - "Me and you went" → "You and I went"
  - "Him and her went" → "He and she went" (no 1st person → case-fix in place)
  - clean: "John and I went", "My friends and I went"
  - Fires only when the coordinated NP is confirmed clause subject (followed by a VP, no preceding preposition). The span is the full subject, so the diff-based harness still derives it automatically.
- **"Between you and I"** (R6 ext): within a PP, object-case context — "between you and I"→"between you and me", "for John and I"→"for John and me"; clean: "my friend and I went" (subject position, untouched). Discriminator: PP-internal vs clause-subject.
- **"There is/are" agreement** (R11): "There is many reasons"→"There are many reasons", "There are a cat"→"There is a cat"; clean: "There is a cat", "There may be a problem", "There is no doubt". Guards: "a lot of" objects, mass/unknown/coordinated post-NPs, inverted questions (v1 declarative only).

**M6 — Phonetics (C4)** → **a/an rule** (R10): article + next word (the word the article phonetically agrees with — adjectives included). "a elegant dress"→"an elegant dress", "an university"→"a university", "a hour"→"an hour", "a MBA"→"an MBA", "a FBI agent"→"an FBI agent", "an one-off"→"a one-off"; clean: "an hour", "a university", "an honest answer", "a European", "an apple". Fires only when the classifier is confident (known word or acronym); exceptions table (hour, honest, heir, honour; university, user, European, one; letter-pronounced acronyms A/E/F/H/I/L/M/N/O/R/S/X).

**M7 — Style pack (C5)**: comma-splice (already M4.5), **long-sentence** (>N words, threshold in profile), **ellipsis** ("..."→"…"), **repeated-word** ("the the cat"; guards: "had had", "that that"). Style rules are optional-by-profile; exposing toggles in Preferences is a later UI change (screenshot update per AGENTS.md — noted, not in v1).

**M8 — Tense consistency (C6, research-heavy)**: "Yesterday I go to the store"→"went", "I went to the store and buy milk"→"bought"; clean: "She said she goes tomorrow" (backshift exception), "I usually go", "If I were you" (irrealis), "It's time we went". Highest false-positive risk of all — guards first (reported speech, habituals, conditionals, polite would/could), fire report-only (no auto-fix) in the first cut.

**M9 — Spelling engine (R14, stoppard-only; §19)** → the whole spelling feature lands inside stoppard with its own tests, no scriba changes:
- `Language` enum + `Engine::setLanguage`/`setUserWords`, plain-wordlist dictionaries (en-US/en-GB, SCOWL-derived BSD — the LGPL LibreOffice `en_GB.dic` is deliberately not used), dialect→dictionary default (override-capable, §14.1), NZ Māori exemptions, case policy.
- Suggestion engine (candidate generation + n-gram scoring) with the **hunspell-parity bar**: a reference misspelling set (`tests/data/spelling_reference.txt`) captured from hunspell's output during development; acceptance thresholds recorded from the reference run and filed in §19.4 (measured top-1 85.7%, top-5 98.8% over the 419 flagged entries — both ≥ hunspell's own rates).
- Scriba keeps hunspell until M10 — the two engines coexist during M9's test phase, which is exactly what makes the parity bar measurable.

**M10 — Scriba swap-out (§19.9)**: delete `vendor/hunspell` + the hunspell link in `scriba_spell`; rewrite `SpellChecker`'s core onto stoppard's spelling pass (keep the squiggle overlay, context-menu suggestions, user-dictionary add/remove, Preferences → Spelling page); settings unification — dialect selects the default dictionary, the Spelling page language dropdown remains as an explicit override ("Follow dialect" is the default entry); dictionary import redefined from `.aff/.dic` pairs to plain word lists (`.txt`, one word per line); bundled dictionaries no longer ship `.aff` files; `resources/dictionaries/` becomes plain `en-US.txt`/`en-GB.txt` (+ any user-installed lists in `~/.config/scriba/dictionaries/`).

Each milestone adds its `rule_cases/` file + corpus mutations per §15.

## 17. Out-of-scope vs. Stoppard's orthography (word-confusion, R12)

**Naming note:** R10 is the future a/an phonetic rule (M6, §16) and R11 is
"There is/are" agreement (M4.5) — so this rule is **R12**. Both the working
title in earlier task notes and the committed id are R12. Rule-case files use
R12.

### 17.1 The problem boundary — what belongs here

"Orthography" splits into three classes. Only the middle one is R12's job:

| Class | Examples | Owner |
|---|---|---|
| **Spelling** (misspelt words) | recieve, alot, seperate | **R14 (M9, §19)** — plain-wordlist pass, dialect-selected dictionary |
| **Regional orthography** | color/colour, -ise/-ize | **R14's dictionary** (dialect-consistent; §14.2, §19.3) — never R9 |
| **Word-confusion** (wrong-word choice; both spellings legal) | to/too, their/there/they're, its/it's, your/you're, whose/who's, then/than | **R12 (this rule)** |
| **Content-word homophones** | lose/loose, affect/effect, advice/advise, cite/site, brake/break | **OUT (§17.5)** |

The partitioning rule is **provability**: R12 fires only when a closed-lexicon
member of a pair occurs in a text slot that provably requires its sibling. Every
entry needs **exactly one** provable signal (tag context, sentence position,
POS) — otherwise it lands in the OUT class.

### 17.2 Tag provability (the driver)

`its`, `their`, `your`, `whose`, `than`, `to` are closed-lexicon tokens
(`lexicon.cpp`). Their homophone siblings (`there`, `too`, `two`, and the
contractions `it's`/`you're`/`they're`/`who's`, which the tokenizer decomposes
into stem + `'s`/`'re`) are **not** in the lexicon → they tag `Unknown`. R12
therefore fires only on the closed-form member in a slot that provably requires
its sibling:

| wrong (closed token) | provable slot | correction | status |
|---|---|---|---|
| `to` | sentence-final (next is `.`/`,`/end) | `too` | v1 |
| `too` | before base verb (infinitive slot) | `to` | **deferred** — bare verbs tag `Unknown` (no verb dictionary yet, M9); the required signal is not positionally provable |
| `its` | before a Verb | `it's` | v1 |
| `it's` | before a Noun (possessive slot) | `its` | **deferred** — after `'s` the next word tags `VB`, not `N`; "it's time"/"it's fun" would be false positives without noun detection (M9) |
| `your` | before a Verb | `you're` | v1 |
| `their` | before `be`/aux/modal ("there is a") | `there` | v1 |
| `then` | after a comparative (-er / `more`) | `than` | v1 |
| `who's` | before a Noun (possessive) | `whose` | **deferred** — same noun-detection blocker as `it's` (M9) |

### 17.3 Table-driven, closed and curated (R9 model)

Same machinery as R9 (`rules_regionalisms.cpp`): a closed `constexpr` table of
entries, each `{ wrong token → correction }` plus a small per-entry guard
predicate (like R9's `gottenParticiple` at `rules_regionalisms.cpp:127-131`)
that encodes the provable slot, reusing the LCS diff for span/suggestion. Every
entry is a deliberate precision-reviewed decision, not a word dump. Seed table
(v1, all guard-verified): the 5 "v1" rows in 17.2. The three "deferred" rows
are blocked on the M9 dictionary pass and are NOT registered.

- **Message**: short plain-English instruction, e.g. "Did you mean \"too\"
  (also)?" — with the suggestion.
- **Match-case**: suggestions preserve the source's case (as in R9).
- **Registry order**: R12 sorts after R9; both are single-span single-token
  issues, so no overlap with earlier rules exists in the seed set.

### 17.4 Guards forced by R12
- Never fire on UNKNOWN tokens (precision guarantee) — the closed member is
the trigger, its Unknown sibling is the expected replacement.
- `to/too`: never fire when `to` governs an infinitive ("I like to swim") or an
object; only sentence-final "to". The sentence-final signal requires the
preceding word to be a **gerund** ("I like swimming to."); a stranded
infinitive ("I want to.", "I'm going to.") stays clean — the gerund's
preceding token being an auxiliary/modal, or the gerund being "going"
(going-to future), suppresses the fire.
- `its/it's`: fires on "its" before a Verb or be/do/have auxiliary ("its been
  a while" — "been" is an Auxiliary). A gerund (including "being"/"having",
  which tag Auxiliary) only fires when it heads the going-to future ("its
  going to rain"): "its going to the store" (PP) and the possessive gerund
  ("its being here surprised me") stay clean via the Determiner check on the
  word after "to". Never fires on possessive-"its" followed by a noun ("its
  tail").
- `your/you're`: fires on "your" + gerund + "to" + **non-Determiner** ("your
  going to be late"). The determiner check keeps the possessive gerund clean
  ("I appreciate your going to the store").
- `their/there`: fires on possessive-"their" before be/aux/modal ("their is a
  problem"), never on gerund auxiliaries — "their being here matters" is a
  legitimate possessive gerund. Never fires on bare "there"/expletive followed
  by a verb.
- `then/than`: fires only when the preceding word is a comparative — surface
  `more`/`less`, or an "-er" ending — and, for "-er" words, the following word
  is a Pronoun or Determiner ("bigger then him", "taller then a house"). The
  Pronoun/Determiner requirement separates a true comparative from a
  content-noun "-er" word ("water then sleep" stays clean, as do "the then
  president", "then we left").
- **Zero-false-positive bar**: every entry must keep `clean_corpus*.txt`
  zero-issue and the mutation "exactly one issue" invariant.

### 17.5 Explicitly OUT (document, don't lint)
- Content-word homophones (lose/loose, affect/effect, advice/advise, brake/break,
  cite/site, weather/whether) — both sides tag `Unknown` in their asserting
  contexts in v1, so they are unprovable and out of scope. Left to a future
  dictionary/NLU pass.
- **a/an** — the Phonetics rule (C4/M6, R10 by roadmap) — deliberately separate
  from R12; it needs a sound classifier, not a word-confusion table.

### 17.6 Regression/recall layer
In addition to the precision (clean) + positive/negative (per-rule) tests, R12
adds a **recall check**: `tests/data/recall_corpus.txt` — real-world error
sentences in the same `wrong | right | rule` format, not rule-split, run by a
new `TEST(RRecall)`. Unlike the rule-case harness (§15, exact span via LCS),
Recall asserts **presence only**: the `wrong` side must produce at least one
issue whose start falls within the LCS-derived mismatch span (reordering
errors like R13's "us one" produce insert-only runs; the presence check is
what makes them testable there). v1 target a few
hundred lines, sourced as original minimal pairs (Harper's MIT/MT test patterns
and LanguageTool examples are *inspiration* only, never copied — §15 layer 4).
`clean_corpus*` remains the zero-false-positive gate; the recall corpus is the
zero-false-negative counterpart.

### 17.7 Confusable-word feature (spec'd 2026-08, implementation future)

**Status: spec only — no code.** Decision (2026-08): real-word confusables are
a **separate feature** from R14 spelling, driven by a future dictionary/NLU
pass; the spelling pass never fires on them. This subsection pins the spec so
implementation doesn't re-derive it.

**The data signal.** The held-out Birkbeck eval (bench/, §19.10) excludes
3,866 real-word errors — typos that ARE dictionary words (the homophone
class). That count is the size of the demand: real-word confusables are not
rare in real text, they're invisible to a non-word checker.

**Scope stays provability-partitioned (17.1).** Nothing here reopens
§17.5: content-word homophones (lose/loose, affect/effect, advice/advise,
cite/site, brake/break) remain OUT until a POS signal exists. The feature
extends R12's closed-lexicon table only.

**Capability dependency (the actual blocker).** The three deferred rows of
17.2 (too→to, it's→its, who's→whose) are gated on **content-word noun/verb
detection** — the M9 word-list dictionary is a plain set (no POS), so M9 does
NOT deliver that signal. What M9 *did* deliver: a large curated word list +
the suggestion pipeline, i.e. the substrate. The unlock is a **POS pass over
dictionary words** (or a curated closed table of "known nouns/verbs per row")
— either turns the deferred rows provable:
- `it's`→`its`: fires when `it's` is followed by a known Noun ("it's tail"),
  suppressed when followed by Adjective+Verb patterns (copula "it's fun").
- `who's`→`whose`: same noun-following slot ("who's book").
- `too`→`to`: fires when `too` is followed by a known Base Verb (infinitive
  slot "too go"), i.e. the closed-clue check of §17.4.

**Candidate provable rows for the first cut (all closed-lexicon members, all
need exactly one provable signal):**
- `a`/`an`-adjacent rows (article confusion "a apple" — note: distinct from
  R10 phonetics, which is sound-based; R12 rows here are shape-based) —
  deferred, needs the same noun signal.
- `than`/`then` is already v1 (§17.2); `their`/`there` is v1.
- New: `two`/`to`/`too` disambiguation between the v1 `to`/`too` pair is
  already covered by the sentence-final guard; `two` rows need Number
  detection (a `two` + singular-noun slot) — same noun signal.

**Non-goals (locked):** no neural/frequency ranking for confusables; the
table stays closed, curated, and precision-barred (§17.3, §17.4). Birkbeck
is eval-only — its real-word bucket measures demand, it never tunes rows.

## 18. Pronoun + numeral ("us one" → "one of us", R13)

### 18.1 The error and its provability

"for us one" → "for one of us". The error is **word order + missing "of"**: a
plural pronoun is immediately followed by the numeral `one` where the numeral
must precede the pronoun with "of" between.

Why the rule is provable:
- `we`, `us`, `they`, `them` are closed-lexicon pronouns (`lexicon.cpp:215,237,242`).
- `one` is **not** in the lexicon; it tags `Unknown` (a bare word with no
  context after an object pronoun clears both contexts — tagger.cpp:99-113).
- Rules never fire on Unknown — so R13 fires on the **closed pronoun** with a
  **surface-text check** of the following token (`a.text.substr(...)` — the same
  mechanism R4 uses for the "to" check, rules_to_infinitive.cpp:36).

### 18.2 Seed table (v1)

| wrong | correction | guard |
|---|---|---|
| `us one` | `one of us` | object pronoun `us`/`them` or subject `we`/`they` followed by surface `one` |
| `them one` | `one of them` | same |
| `we one` | `one of us` | same |
| `they one` | `one of them` | same |

- Suggestion preserves the source's case (`matchCase`): "Us one" → "One of us".
- The suggested text is "one of <pronoun>" (the correction column).
- `you` is excluded (ambiguous number, `lexicon.cpp:264`); `her`/`him`/`me` are
  excluded (singular — "for her one" is not a real-world error pattern).
- Numerals other than `one` are excluded: "us two" is idiomatic ("for us two"),
  so the rule must not generalize to any numeral.
- **Registry order**: R13 sorts **before R6** (pronoun case). "Us one asked"
  is also a subject-slot error (R6 suggests "We" — the wrong fix); with R13
  earlier, its "One of us" wins the dedup.

### 18.3 Guards (zero-false-positive bar)
- Fires only on the closed pronoun; the following token must be the **surface
  text** "one" (folded case-insensitive), adjacent (no punctuation between).
- Object pronouns (`us`/`them`) additionally require a **preceding
  Preposition** ("for us one" is the error; the ditransitive "Give us one."
  is correct English). Sentence-initial "Us one asked" has no preceding token
  and fires — matching the `matchCase` example above.
- Never fires on "one of us" / "one of them" themselves — there the pronoun
  follows the preposition, not "one".
- Never fires when "one" is followed by an Unknown token ("us one cat" is
  nonsense, not an error worth flagging; conservative skip — the Unknown
  check stands in for the noun check until M9).
- Sentence-initial "one" ("One of us went") is untouched — the pronoun is not
  immediately before it.
- Keeps `clean_corpus*.txt` zero-issue and the mutation "exactly one issue"
  invariant.

### 18.4 Test harness note (important)
The rule-case harness (`tests/test_rule_cases.cpp` `diffRuns`) derives expected
spans via **token-level LCS**. Reordering cases like "us one" → "one of us"
produce insert-only edit runs (zero-length spans) that no issue can match — the
LCS model cannot express a swap. Therefore:
- **Positive cases** for R13 live in a dedicated gtest suite
  (`test_rules_pronoun_numeral.cpp`) asserting exact start/length/suggestion,
  **not** in `rule_cases/R13_*.txt`.
- The `rule_cases` file (if any) holds only identical-side lines (clean checks)
  and substitution-style cases where LCS is sound.

### 18.5 Recall corpus
"for us one" and "for them one" lines belong in `recall_corpus.txt` (§17.6) —
run by `TEST(Recall)`, which asserts at least one issue (start may differ from
the LCS-derived span; only presence is checked there — see 17.6).

## 19. Spelling (R14) — the dictionary pass

**Naming note:** R10 (a/an, M6), R11 (there-is/are, M4.5), R12 (word-confusion,
§17), R13 (pronoun+numeral, §18) are taken — this rule is **R14**. Rule-case
files use R14.

### 19.1 The problem boundary
Spelling moves **in scope** (§17.1's original "Hunspell — Stoppard never fires"
is superseded): stoppard replaces the vendored hunspell engine in scriba
(M10). Stoppard's spelling is a **dictionary pass**, not a tag-context rule —
misspelt words carry no POS signal, so the existing rule machinery does not
apply. What stays out:
- **Word-confusion** (R12) and **content-word homophones** (§17.5) are
  spelling-correct — the pass never fires on them ("lose"/"loose" are both in
  the dictionary).
- **Regional orthography** (color/colour, -ise/-ize) is handled by the
  dictionary, not by R9 (§14.2): the en-US list accepts "color", the en-GB
  list "colour" (and both -ise and -ize, which are legal in British).
- The pass never fires on **closed-lexicon tokens** (pronouns, determiners,
  auxiliaries, contractions like "it's"/"don't" — the tokenizer decomposes
  these; they are exempt by construction).

### 19.2 Model and Engine API
```cpp
enum class Language { None, American, British };
```
- `Language::None` = grammar-only (today's behavior for any consumer that does
  not opt in). `check()` runs the grammar rules always, the spelling pass only
  when a language is set.
- `Engine::setLanguage(Language)` selects the active dictionary; the default
  is `None`.
- `Engine::setUserWords(std::vector<std::u16string>)` — atomic swap of the
  per-user word set (scriba's `SpellChecker` user dictionary, M10). The engine
  stays stateless between `check()` calls and the swap is not visible mid-
  check, preserving the no-mutex thread-safety contract (`StoppardEngine.cpp`
  depends on it).
- The pass consumes the tokenizer's output. **Trigger set**: every `Word`
  token that is not a closed-lexicon entry (§6.2) and not a contraction
  fragment (§6.1's decomposition, e.g. `n't`, `'s`, `'ll`) — *regardless of
  the tagger's output*: the tagger's suffix heuristics (§6.4.3) can tag a
  misspelling ("recieving" → `-ing` gerund), so the pass must **not** key off
  the `Unknown` tag. Hits are skipped; misses produce `Issue`s (one per
  token, span = the token). This is the one intentional exception to §6.4's
  "rules must not fire on Unknown" — spelling is a dictionary pass, and its
  precision guarantee is the dictionary, not the tagger.

### 19.3 Dictionaries and the dialect default
- **Format**: plain word lists, one word per line, lowercase, **base +
  inflected forms** (plurals, -ed/-ing/-s, comparatives) — no affix engine.
  Parsed once into an in-memory set.
- **Sources**: SCOWL-derived word lists (BSD-style) for both en-US and en-GB.
  The LGPL LibreOffice `en_GB.dic` is **deliberately not used** (GPL-3.0
  embedding is compatible, but SCOWL's BSD keeps the data license uniform).
  Pinned: the `scowl-2020.12.07` tarball (same vintage as scriba's bundled
  en_US). Size estimate: ~60–65k base words per variant, expanding to ~150k
  lines / ~1–1.2 MB once inflections are spelled out (exact numbers recorded
  in `data/` after the first build).
- **Build (reproducible; offline at test time)**: `scripts/fetch_dictionaries.py`
  downloads the pinned tarball, selects the en-US/en-GB variant sets, expands
  inflections (plurals, 3ps, -ed/-ing, comparatives — affix-expansion logic
  if the tarball ships flagged `.dic` forms, or a pre-expanded source if one
  exists; mechanism decided at implementation after inspecting the tarball),
  lowercases, dedupes, sorts. The generated lists are **committed** — the
  script reruns only when SCOWL is bumped; `data/CHECKSUMS.txt` records the
  sha256 of the source tarball and of each generated list.
- **Vendored in stoppard** (`data/en-US.txt`, `data/en-GB.txt`), loaded from a
  path scriba supplies — scriba keeps its qrc-extract-to-config pattern
  (`~/.config/scriba/dictionaries/`, M10), so the engine needs no file-bundle
  logic beyond "load this path".
- **Dialect default (settings unification, §14.1)**: each `DialectProfile`
  carries `defaultDictionary`; an explicit `Engine::setLanguage` call
  overrides it. Scriba's Preferences → Spelling dropdown becomes an override
  whose default entry is **"Follow dialect"**; user-installed dictionaries
  (plain word lists, M10) appear in the same dropdown.

| Dialect | Default dictionary | Rationale |
|---|---|---|
| American | American | |
| Canadian | American | follows American (agreement §14.3, spelling) + a small Canadian spelling allowance (`data/canadian-en.txt`): en-US dictionary plus the handful of standard Canadian forms en-US lacks ("centre", "colour", "theatre"…) |
| British / Australian / Indian / New Zealand | British | en-GB orthography; NZ adds Māori exemptions (§19.7) |

### 19.4 Suggestions (the hunspell-parity bar)
Hunspell's pipeline is reproduced faithfully — generator order *is* the
ranking (as in hunspell/aspell: edits are tried in highest-probability-first
order):
1. **Simple pass (generator order)** — REP replacements first (REP pairs
   from the keyboard tables; a REP hit marks the suggestion "good" and
   closes the ngram gate), then swapchar (adjacent transpositions plus the
   4/5-letter double-swap patterns), longswapchar (non-adjacent swaps within
   distance 4), badcharkey (skipped — the sources carry no KEY), extrachar,
   forgotchar, movechar, badchar (the letter generators iterate the TRY
   letters in order), doubletwochars (the "vacacation" back-reference
   pattern), twowords (dictionary word-pair splits, spaced and dashed).
   Candidates are deduped and dictionary-checked; first hits win the top
   slots (capacity 5).
2. **ngramsuggest (append)** — when the REP pass produced nothing, up to 4
   further candidates are appended: dictionary scan bounded to words within
   4 letters of the typo (length buckets), roots scored by hunspell's
   substring-anywhere multi-size `ngram()` + common-prefix, filtered by the
   mangled-word threshold, re-ranked by the LCS/position/weighted-2-gram
   score, then substring-deduped against the existing list. The scorer
   weights are tunable (one place) so the §19.4 rates can be tuned against
   the reference set without changing the pipeline shape.
- **Keyboard tables**: harvested at M9 from SCOWL's bundled `speller/en.aff`
  (TRY string + 90 REP pairs; the shipped `en_US.aff`/`en_GB.aff` carry no
  KEY, so there is no adjacent-key table). Committed as
  `data/keyboard-en-US.txt` / `data/keyboard-en-GB.txt` (documented format),
  so M10's deletion of the `.aff` files loses nothing.
- **Research backlog (post-M9, does not block the parity bar)**:
  - *Word-frequency ranking*: Norvig-style ranking needs a frequency list,
    which the SCOWL data does not provide. Candidate: a bundled top-N
    frequency list (license, size, offline constraints to verify) to break
    ties the current scorer cannot (e.g. "recieve" → relieve vs receive).
  - *Confusion-model tuning*: the committed reference set (422 pairs; 419
    flag the target, the other three are valid dictionary words) can
    train a small typo→correction error model (Brill-style) to push top-1
    above hunspell's own rate.
  - *SymSpell indexing*: deletion-index candidate lookup if the §10 perf bar
    ever gets tight (speed only — no quality effect).

**The bar (M9 acceptance)**: two committed artifacts —
`tests/data/spelling_reference.txt` (422 common errors: recieve, seperate,
occured, definately, untill, accomodate, ...; format `misspelled |
intended-correct`, one per line) and the **capture run** made at M9 start:
hunspell (scriba's bundled dictionaries) suggests on every entry and its
top-5 is recorded. `tests/test_spelling_reference.cpp` asserts, per dialect
dictionary:
- intended-correct in stoppard's **top-1** ≥ hunspell's own top-1 hit rate on
  the same set, and ≥ 85% absolute (90% remains the stretch guide — see the
  research backlog);
- intended-correct in stoppard's **top-5** ≥ 98%.

**Measured (M9, the committed constants)**: the reference set's 422 entries
include three words that are themselves valid dictionary words (loosing,
directer, summery), which the engine — like hunspell's own `spell()` — must
not flag; the bar is measured over the 419 flagged entries. Per dialect
(en-US / en-GB — both identical):
- stoppard top-1 **359/419 = 85.7%** ≥ hunspell 357/419 = 85.2% (US), 343/419
  = 81.9% (GB);
- stoppard top-5 **414/419 = 98.8%** ≥ hunspell 412/419 = 98.3% (US), 409/419
  = 97.6% (GB).
`tests/test_spelling_reference.cpp` asserts top-1 ≥ 359 and top-5 ≥ 414 per
dialect, keeping the pipeline at or above the measured run.

### 19.5 Case policy and token rules (parity with hunspell)
- Lookup folds the token to lowercase; a capitalized word (sentence-initial or
  mid-sentence) passes if its lowercase form is in the dictionary.
- **ALL-CAPS** words are not flagged.
- Unknown capitalized words **are** flagged (hunspell behavior — proper nouns
  are not in the dictionary either).
- Suggestions are case-preserving via `matchCase` (§17.3, R9 pattern).
- **Possessives**: the tokenizer's decomposition (§6.1) splits `dog's` →
  `dog` + `'s`, so the pass sees the base and "John's" is never flagged
  wholesale. A word that still contains an internal apostrophe after
  decomposition (e.g. "O'Brien" — not in the contraction table) is **skipped**
  (proper-noun pattern).
- **Digits**: any word token containing a digit (dates, "5th", versions) is
  skipped.
- **Hyphenated compounds** ("well-known"): skipped in v1 (per-part checking is
  a later refinement — splitting invites "un-" prefix false positives).
- **Brands / mixed case** ("iPhone", "markdownParser"): folded lookup flags
  them — same behavior as hunspell; the user dictionary (§19.6) is the escape
  hatch.

### 19.6 User dictionary
`Engine::setUserWords` (19.2) carries scriba's user words. Scriba's existing
file format is preserved in M10 (one word per line, optional count header —
`SpellChecker::parseWordList` already skips it), so no user data migration is
needed.

### 19.7 New Zealand: Māori loanwords
The NZ clean corpus's Māori words (kōrero, whānau, marae, Aotearoa, iwi) are
not in en-GB; without policy they would become the first spelling false
positives. **The NZ profile carries an exemption list** (curated, ~30–60
entries at M9) that is unioned into the active dictionary when the profile is
NZ. The clean NZ corpus stays zero-issue under spelling (§19.8).
- **Macrons**: lookup is accent-insensitive — both the token and the
  dictionary are normalized (NFD, strip combining marks) at compare time, and
  the lists store de-accented forms — so "whānau" and "whanau" both pass no
  matter how the writer typed them.
- Seed list (de-accented; grows to ~30–60 entries at M9): whanau, hapu, iwi,
  marae, kai, korero, mihi, karakia, haka, powhiri, whakapapa, whenua,
  manuhiri, tangihanga, whanaunga, mahi, pakeha, kaumatua, wananga, tupuna,
  taniwha, kohanga, reo, Aotearoa. Proper nouns are not otherwise exempt
  (§19.5 flags capitalized unknowns), so the list must include them.

### 19.8 Tests and the zero-FP gate
- `tests/test_spellcheck.cpp` — per-rule suite: positives
  (`rule_cases/R14_spelling.txt`, which fits the LCS harness: "recieve |
  receive | R14" is a single-token swap; "alot | a lot | R14" exercises the
  insert path; the harness asserts the top suggestion — ranked-quality
  coverage lives in the reference suite below), negatives (every
  clean-corpus word is dictionary-clean under its dialect's dictionary), case
  policy, token rules (digits, possessives, hyphenated, apostrophed names),
  closed-lexicon immunity, user-words override.
- `tests/test_spelling_reference.cpp` — the §19.4 parity set.
- **The clean corpora are now dictionary-coverage-dependent**: a word that is
  legitimate but absent from the list becomes an FP. The zero-FP gate stays,
  but its precondition is "dictionary covers the corpus" — corpus additions
  may require dictionary additions (reviewed at merge time, not auto-accepted).
- Mutation "exactly one issue" invariant extends to spelling mutations.
- Performance: the §10 ten-thousand-words bar is re-measured with spelling
  enabled (wordlist lookup is a hash-set hit; only misses pay for suggestion
  generation).

**M9 done =** all of: parity assertions green against the capture run (§19.4);
clean corpora (en-US, en-GB, NZ) zero-issue under their dialect defaults with
spelling on; R14 positives/negatives/token rules/mutations green; performance
bar re-measured with spelling on; `data/en-US.txt`/`en-GB.txt` + keyboard
tables committed with `CHECKSUMS.txt`; the scriba tree untouched (its `git
status` shows nothing beyond the vendored SPEC.md sync); full stoppard suite
green.

### 19.9 Milestone split and scriba integration
- **M9 (stoppard-only)**: §19.1–19.8 land inside stoppard with tests. Scriba
  is untouched and keeps hunspell — this is what makes the parity bar
  measurable (§19.4).
- **M10 (scriba swap-out)**: delete `vendor/hunspell` and the hunspell link in
  `scriba_spell`; rewrite `SpellChecker`'s core (`checkWord`/`suggestions`) to
  delegate to the stoppard engine; keep the squiggle overlay, context-menu
  suggestions, user-dictionary add/remove, and the Preferences → Spelling page
  (dropdown becomes the "Follow dialect" override, §19.3); dictionary import
  accepts plain word lists instead of `.aff/.dic` pairs; bundled dictionaries
  become `en-US.txt`/`en-GB.txt` qrc resources; the old `.aff`/`.dic` files
  are removed. M10 must re-run the full scriba test suite (spell-checker,
  spell-highlighter, settings-migration, preferences) before the hunspell
  subtree is deleted.

### 19.10 Held-out Birkbeck benchmark (eval-only, 2026-08)

Roger Mitton's Birkbeck spelling-error corpus (`missp.dat`,
https://titan.dcs.bbk.ac.uk/~roger/corpora.html — 36,133 misspellings of 6,133
targets, **CC BY-NC-SA 3.0**) is the **second held-out set**. License and the
AGENTS data rule mean the raw corpus is NOT committed: `scripts/fetch_birkbeck.py`
filters it to the same shape as `spelling_wikipedia.txt` (typo ∉ dictionary,
intended ∈ en-US dictionary, ASCII, Levenshtein ≤ 4, deduped) and writes the
pair file under gitignored `bench/`.

- **Eval-only discipline.** Birkbeck never tunes floors, weights, or tables —
  it only reports. R14's committed gates remain the reference set (§19.4) and
  the Wikipedia holdout (test floors); Birkbeck cannot fail CI
  (`SpellingParity.BirkbeckReport` skips unless `STOPPARD_BIRKBECK_FILE` is set).
- **Real-word bucket.** 3,866 raw errors (~11% of the whole corpus) are
  real-word typos — the homophone class, §17 — excluded from scoring,
  counted as R12's demand signal (§17.7). The filter as a whole keeps
  26,474 of 36,133 (73%).
- 2026-08-03 measurement (full run, n=26,474; ~12 min; top-1/top-5):
  totals **47.9/60.8**; d1 n=9041 78.6/96.1, d2 n=7810 48.4/61.5,
  d3 n=5728 23.5/34.2, d4 n=3895 11.5/16.5 (no d5+ pairs survive the
  lev ≤ 4 filter). The wiki set runs ~14pp higher per bucket (d1 93.0,
  d2 82.0, d3 40.0) — Birkbeck is learner/school data, deliberately not
  tuned against.
- **Pool-generation gap is the dominant miss mode** (rank=-1 = intended word
  never generated, 10,277 of 26,474): d1 2.8%, d2 38.5%, d3 65.8%, d4 83.5%.
  Only 102 misses had the intended word present but ranked ≥ 5. Ranking
  work cannot fix pool misses — the lever is candidate generation
  (scan/heap coverage) at d2+. Recorded for the research backlog.
- **Hunspell comparison on the same pairs** (hunspell 1.7.2, bundled
  `en_US.aff`/`en_US.dic`, capture in `bench/birkbeck/hunspell_suggestions.txt`;
  identical 26,191 pairs after the wiki-method exclusion — hunspell
  `spell()` deems 232 typos correct, so both engines are measured only on
  pairs hunspell flags):

  ```
  dist     n   stoppard top-1  top-5    hunspell top-1  top-5    delta top-1
  ----  ----   --------------  -----    --------------  -----    ----------
  d1    8909   79.2%   96.5%            67.6%   93.5%             +11.6
  d2    7733   48.8%   61.9%            43.3%   55.2%              +5.6
  d3    5681   23.7%   34.4%            21.1%   28.3%              +2.6
  d4    3868   11.6%   16.6%            10.4%   13.5%              +1.2
  total26191   48.2%   61.0%            41.9%   56.2%              +6.3
  ```
  Stoppard beats hunspell at **every** distance bucket on Birkbeck (the
  reverse of the reference-set relationship in §19.4 — there the parity bar
  made top-1/415 ≥ hunspell's 357, and the wiki set sits at +5.6pp; Birkbeck
  widens the edge to +6.3pp, largest at d1). The 232 unmeasured typos are
  mostly archaic SCOWL words hunspell accepts as correct (actable, agon,
  ane, aroid, arsis) — a recall gap, not a ranking one.
- **Throughput (2026-08-04)**: both engines on the same 2,000-typo input,
  repeated runs, no competing load: hunspell ≈111 s vs stoppard ≈58 s —
  stoppard ~1.9–2.0× faster (~34 vs ~18 suggestions/s). Suggestion
  generation dominates; the grammar rules are negligible. Captures and
  tool binaries live in gitignored `bench/` (see `bench/README.md`).
- **Per-distance table** policy matches the wiki capture: counts by
  Levenshtein bucket (d1…d5+) so hunspell-parity deltas are attributable.

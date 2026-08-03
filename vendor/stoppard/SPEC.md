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

class Engine {
public:
    explicit Engine(Dialect dialect = Dialect::American);
    std::vector<Issue> check(std::u16string_view text) const;  // thread-safe; no mutable state
    Dialect dialect() const; void setDialect(Dialect d);
};

}
```

UTF-16 spans match `QString` indexing exactly — Scriba's byte↔char conversion bridge (HarperEngine.cpp:39-78, 139-154) is deleted, not reimplemented.

## 5. Pipeline

`check()`: **tokenize → tag → chunk → run rules (priority order) → dedup → issues sorted by start**. All state is local to the call; `Engine` holds only the dialect. Lexicon/morphology tables are immutable globals → safe to share across threads.

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

Regionalism rules (dialect-aware word choice — replaces Harper's "in the cards" behavior); a/an phonetic rule; "between you and I"; coordinated-subject pronoun case; "There is/are" agreement; negation at distance; punctuation/long-sentence style rules; tense consistency.

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
};
DialectProfile profileFor(Dialect);          // immutable, built once
```

Profile table (v1):

| Dialect | Collective plural | Notes |
|---|---|---|
| American | no | singular-only |
| Canadian | no | follows American on agreement |
| British | yes | both accepted |
| Australian | yes | |
| Indian | yes | |
| New Zealand | yes | tracks BrE/AuE; Maori loanwords (whanau, kai, marae, iwi...) hit the Unknown tag and are never flagged by construction |

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

**Explicitly out**: spelling variants (color/colour, -ise/-ize) — Hunspell owns spelling; Stoppard never touches orthography.

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
| **Spelling** (misspelt words) | recieve, alot, seperate | Hunspell — Stoppard never fires |
| **Regional orthography** | color/colour, -ise/-ize | **Explicitly OUT** (§14.2) — Hunspell |
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

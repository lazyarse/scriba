# AGENTS.md

## What

Stoppard — standalone rule-based English grammar engine (C++23, no Qt). Replaces Harper in
Scriba (vendored as `vendor/stoppard/`, working-copy model). GPL-3.0, `// Copyright (C) 2026 LazyArse`.

## Build

First configure fetches GoogleTest via FetchContent (needs network). No Qt.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4
cd build && ctest --output-on-failure -j1
```

When vendored into scriba, `STOPPARD_BUILD_TESTS` must be OFF (scriba's build never fetches gtest).

## Structure

- `include/stoppard/stoppard.h` — the only public header (Issue/Suggestion/Dialect/Engine)
- `src/` — tokenizer, lexicon (closed-class tables), morphology (verb/noun paradigms),
  tagger (precision-first), chunker (NP/PP/VP + subject detection), `rules.h`/`rules.cpp`
  (registry + dedup), one `rules_*.{h,cpp}` per rule family, engine
- `tests/` — gtest suite per component; `tests/data/` = clean corpora + `rule_cases/`
- `scripts/fetch_corpus.py` — regenerates corpus files (dev tool, not run in tests)
- `SPEC.md` — the contract; `PLAN.md` — implementation plan (checkbox steps)

## Key files

- `SPEC.md` — every behavior decision lives here; PLAN.md implements it
- `src/tagger.cpp` — context resolution + Unknown conservatism (rules never fire on Unknown)
- `src/rules.cpp` — `allRules()` priority order; overlapping issues → higher priority wins
- `tests/test_rule_cases.cpp` — diff harness: `wrong | right | rule-id` lines, token-level LCS

## Dialect profiles (SPEC §14)

A dialect is a `DialectProfile` carried into every `check()`: `collectivePluralAgreement` +
`regionalisms` (R9 table). `profileFor()` lives in `src/rules_regionalisms.cpp`.

| Dialect | Collective plural | Notes |
|---|---|---|
| American | no | singular-only |
| Canadian | no | follows American on agreement |
| British | yes | both accepted |
| Australian | yes | |
| Indian | yes | |
| New Zealand | yes | tracks BrE/AuE; Māori loanwords (whānau, kai, marae...) hit the Unknown tag and are never flagged by construction |

Spelling variants (color/colour, -ise/-ize) are explicitly out — Hunspell owns orthography.

## Tests & `tests/data/`

- gtest suite per component + per rule family (positive / negative / idiom-guard cases)
- `tests/data/clean_corpus.txt` — ~120 known-good sentences, **zero-issue bar** (American)
- `tests/data/clean_corpus_<dialect>.txt` — one per dialect, each run under its own
  `DialectProfile`; the NZ file's Māori loanwords must stay issue-free (Unknown-tag path)
- `tests/data/rule_cases/R{n}_*.txt` — `wrong | right | rule-id`; harness derives the
  expected span + suggestion via token-level LCS; identical sides expect zero issues;
  `R9_regionalisms.txt` runs under British, the rest under American
- `tests/data/CORPUS_SOURCES.md` — attribution per file (CC BY / OGL / OPL wording); corpus
  text only from open-licensed sources (SPEC §15)
- Mutation tests: each clean sentence × guarded operator → exactly one issue

## Conventions

- C++23, Qt-free, `std::u16string_view` end-to-end; spans in UTF-16 code units (== QString index)
- Every `.h/.cpp` starts with the GPL-3.0 header (copy from `include/stoppard/stoppard.h`)
- Rules are conservative: all tokens resolved, morphology known, no inversion/coordination (v1)
- Suggestions are case-preserving (`matchCase` in morphology)
- Tests: TDD per PLAN.md task; new rule = one `tests/data/rule_cases/R{n}_*.txt` + gtest suite
- Commit: conventional style (`feat:`, `fix:`, `test:`, `chore:`), one per task
- Always build with `-DCMAKE_BUILD_TYPE=Release`

## Vendored drift (Scriba)

- [ ] **Remove once upstream fixes it:** `src/engine.cpp`, `src/spellcheck.{h,cpp}` carry a
  local `foldWord` fix — `Engine::setUserWords` folds entries to match the case-/accent-
  insensitive spelling lookup — that the upstream source repo (`../stoppard`) does not yet
  have. A `../stoppard` → `vendor/stoppard/` sync overwrites it; re-apply the three hunks
  after every copy until the fix lands upstream.
  Tracked at `../stoppard/docs/bug-case-insensitive-user-dictionary.md` (bug report + the
  `git apply` patch).

- [ ] **Remove once upstream fixes it:** `include/stoppard/stoppard.h` replaces
  `std::atomic<std::shared_ptr<const Config/SpellData>>` with mutex-guarded
  `std::shared_ptr` snapshots (`config()`/`setConfig()`/`spellData()`/`setSpellData()`
  private accessors). P0718R2 (`atomic<shared_ptr>`) is not implemented in libc++, so the
  atomic form fails to compile on macOS (AppleClang/libc++); GCC's libstdc++ accepts it
  unconditionally, so Linux builds hide the bug. Re-apply the header + `src/engine.cpp`
  changes after every sync.

- [ ] **Remove once upstream fixes it:** `CMakeLists.txt` guards
  `target_compile_options(stoppard PRIVATE -Wall -Wextra -Wpedantic)` with
  `if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")` — MSVC rejects `/Wextra` (D8021).

- [ ] **Remove once upstream fixes it:** `src/rules_double_negative.cpp` declares the
  `std::find_if` result as `auto neg` (iterator) instead of `const auto *neg` — MSVC's
  `std::array` iterators are `_Array_const_iterator` class types, not raw pointers, so
  `auto *` deduction fails with C3535 on Windows (GCC/Clang return raw pointers and accept
  it). Re-apply the one-line change after every sync.

## Gotchas

- `tests/` compile with `TESTS_DATA_DIR` pointing at the source `tests/data/` — never run tests
  from a different working directory layout without reconfiguring
- Committed corpus files are the source of truth; `fetch_corpus.py` is for regeneration only
- R1 and R3 both fire on `I can has` — dedup keeps the higher-priority (R1) message; don't
  "fix" that in one rule without touching the other
- `has`/`have`/`do` etc. tag as Auxiliary even after modals — R1/R4 handle non-base auxiliaries
- Mass nouns (R5), idiom guards (R2 `have to`/`have got`, R8 `can't not`/`no doubt`) are
  hard requirements — the clean-corpus zero-issue bar protects them
- New source files go in `src/` + the `add_library(stoppard ...)` list in CMakeLists.txt

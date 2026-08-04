# Stoppard

A standalone rule-based English grammar **and spelling** engine written in
C++23. No external runtime dependencies. Built to replace the Harper grammar
checker and the Hunspell spell-checker in
[Scriba](https://github.com/LazyArse/scriba) via a single source swap.

Stoppard is conservative by design: grammar rules only fire on words it can
resolve — modal + verb form, subject–verb agreement, determiner/noun
mismatches, pronoun case, and a handful of dialect regionalisms. Spelling is a
dictionary pass, so it is just as decisive: a word is in the dictionary or it
isn't, and when it isn't, Stoppard ranks corrections with state-of-the-art
techniques. Everything runs fully offline.

- `SPEC.md` — the contract: exactly what the engine checks, in which dialects, and how
  errors are reported.
- `PLAN.md` — the implementation plan (M1–M5) with task-by-task checkboxes.

## Spelling

Spelling (R14, SPEC §19) is a dictionary pass over the same tokenizer output
as the grammar rules: every content word is looked up in a hash set of ~90k
base + inflected forms per dialect (SCOWL-derived, BSD-licensed;
`data/en-US.txt`, `data/en-GB.txt`). The active dictionary follows the
dialect — American or British — with a small Canadian spelling allowance and
a curated Māori exemption list (macron-insensitive, so "whānau" and "whanau"
both pass) under their respective profiles.

When a word misses, suggestions come from a Hunspell-parity pipeline — the
same generator order Hunspell uses, so candidates are ranked probability-first:

- **Simple generators first**: common-typo REP pairs ("recieve" → receive),
  adjacent and long-distance transpositions, then the TRY letter generators —
  extra char, forgotten char, moved char, bad char, double char — iterated in
  keyboard-frequency order.
- **ngramsuggest second**: a length-bucketed dictionary scan scores every
  word within 4 letters of the typo with multi-size substring ngram similarity
  plus common-prefix weighting, filters by the mangled-word threshold, and
  re-ranks by LCS/position score and word-frequency rank (`data/freq-en.txt`)
  to break ties raw similarity cannot.
- Suggestions are case-preserving, and policy skips (digits, hyphenated
  compounds, ALL-CAPS, contraction fragments, internal apostrophes) mirror
  Hunspell's token behavior.

The parity bar is measured, not hoped for: a committed reference set of 422
common errors (recieve, seperate, definately, untill, accomodate, …) asserts
intended-correct in top-1 ≥ 85% and in top-5 ≥ 98%. In the measured run
(359/419 = 85.7% top-1, 414/419 = 98.8% top-5) Stoppard beats Hunspell's own
rates on the identical set (85.2% / 98.3% US, 81.9% / 97.6% GB).

Spelling is opt-in: `setLanguage()` selects the dictionary and
`setDictionaryPaths()` loads the word lists; without them the engine is
grammar-only, exactly as before. A per-user dictionary (`setUserWords`) rides
on top for proper nouns and brands, and single-word queries are exposed as
`isMisspelled()` / `spellSuggestions()`. Clean text costs one hash-set lookup
per word — the 10k-word spelling check runs in ~240 ms in a release build, and
only misses pay for suggestion generation.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4
```

The first configure fetches GoogleTest via FetchContent (requires network).

## Test

```bash
cd build && ctest --output-on-failure -j1
```

## Usage

Stoppard is a static library with a single public header and no runtime
dependencies beyond the C++ standard library. Text in and text out is UTF-16;
spans (`Issue::start`, `Issue::length`) are offsets in UTF-16 code units, which
match Qt `QString` indexing exactly.

```cpp
#include <stoppard/stoppard.h>

#include <iostream>
#include <string>
#include <string_view>

// Minimal UTF-16 -> UTF-8 printer so u16string messages render on std::cout.
void print(const std::u16string_view s)
{
    for (char16_t c : s) {
        if (c < 0x80) {                       // ASCII
            std::cout << char(c);
        } else if (c < 0x800) {               // 2-byte
            std::cout << char(0xC0 | (c >> 6)) << char(0x80 | (c & 0x3F));
        } else {                              // 3-byte
            std::cout << char(0xE0 | (c >> 12)) << char(0x80 | ((c >> 6) & 0x3F))
                      << char(0x80 | (c & 0x3F));
        }
    }
}

int main()
{
    stoppard::Engine engine;                  // American by default
    engine.setDialect(stoppard::Dialect::British);
    engine.setLanguage(stoppard::Language::British);
    engine.setDictionaryPaths("data/en-US.txt", "data/en-GB.txt",
                              "data/maori-nz.txt", "data/canadian-en.txt");

    auto issues = engine.check(u"She go to school.");
    for (const auto &it : issues) {
        std::cout << "[" << it.start << "," << it.start + it.length << "] ";
        print(it.message);
        std::cout << " -> ";
        if (!it.suggestions.empty()) {
            print(it.suggestions[0].text);
        } else {
            std::cout << "(no suggestion)";
        }
        std::cout << '\n';
    }

    // Spans are UTF-16 code units, so they index into std::u16string directly.
    const std::u16string text = u"I has a cat.";
    for (const auto &it : engine.check(text)) {
        print(text.substr(it.start, it.length));
        std::cout << " corrected to ";
        print(it.suggestions[0].text);
        std::cout << '\n';
    }

    // Spelling is per-word too, with case-matched suggestions.
    if (engine.isMisspelled(u"recieve")) {
        std::cout << "recieve -> ";
        print(engine.spellSuggestions(u"recieve")[0]);
        std::cout << '\n';
    }

    // Checks are stateless and thread-safe: one Engine may be shared, or
    // constructed per call.
    stoppard::Engine view(stoppard::Dialect::American);   // no Qt, offline-safe
    if (view.check(u"The cat sat on the mat.").empty())
        std::cout << "clean sentence: no issues\n";
}
```

Every sentence a profile accepts has at least one error-injected mutation
covered by the test suite — see `docs/mutations.md` for the operator table.

## License

GPL-3.0 — see [`LICENSE`](LICENSE).

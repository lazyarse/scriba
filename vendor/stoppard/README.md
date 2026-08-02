# Stoppard

A standalone rule-based English grammar engine written in C++23. No external
runtime dependencies. Built to replace the Harper grammar checker in
[Scriba](https://github.com/LazyArse/scriba) via a single source swap.

Stoppard is conservative by design: it only flags errors it can prove — modal + verb form,
subject–verb agreement, determiner/noun mismatches, pronoun case, and a handful of
dialect regionalisms. It never guesses on words it doesn't know, and it works fully
offline.

- `SPEC.md` — the contract: exactly what the engine checks, in which dialects, and how
  errors are reported.
- `PLAN.md` — the implementation plan (M1–M5) with task-by-task checkboxes.

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

GPL-3.0 — see `LICENSE`.

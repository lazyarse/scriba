# harper-ffi

Plain-C ABI wrapper around [`harper-core`](https://crates.io/crates/harper-core)
for the Scriba C++ Markdown editor. It replaces the previously planned
shell-out to `harper-cli`: grammar checking now runs in-process through a
small, panic-safe FFI surface.

## Behavior

- Lints English **Markdown** text with the curated Harper lint group
  (grammar, style, punctctuation, ...).
- The `SpellCheck` rule is **disabled** (`harper_init` calls
  `set_rule_enabled("SpellCheck", false)`): Scriba owns spelling correction
  via Hunspell, so spelling suggestions never leak into the lint output.
- All offsets reported through the ABI are **byte offsets** into the UTF-8
  input buffer passed to `harper_lint` (harper-core's native `Span`s are
  character indices; the wrapper converts them).
- Every function is null-safe and panic-safe: `catch_unwind` wraps the
  engine and lint paths, invalid UTF-8 yields a null list, and out-of-range
  issue indices return zero/null instead of trapping.

## Build

Requires **rustc 1.96.1 or newer** (pinned via `rust-toolchain.toml`).

> Why: harper-core 2.7.0 fails to compile on rustc 1.92–1.96.0 with
> `E0308` in `for_free_of_charge.rs`, `in_demand_in_depth.rs` and
> `naked_eye.rs` (a rustc regression in fn-item array coercion, fixed in
> the 1.96.1 point release). See [Automattic/harper#3730](https://github.com/Automattic/harper/issues/3730).
> No fixed harper-core version has been published to crates.io yet.

```bash
cd vendor/harper-ffi
cargo build --release        # fetches pinned deps from crates.io via Cargo.lock
```

Artifacts: `target/release/libharper_ffi.a` (static) and
`target/release/libharper_ffi.so` (shared).

## FFI surface

| Function               | Signature                                            | Notes                                  |
| ---------------------- | ---------------------------------------------------- | -------------------------------------- |
| `harper_init`          | `(u8) -> HarperEngine*`                              | English Markdown engine, SpellCheck off. `u8` = dialect code: 0 American (default), 1 British, 2 Australian, 3 Indian, 4 Canadian; unknown codes fall back to American. Null on failure. |
| `harper_lint`          | `(engine, u8*, len) -> HarperIssueList*`             | Byte offsets. Null on failure.         |
| `harper_issues_len`    | `(list) -> usize`                                    | 0 for null list.                       |
| `harper_issue_start`   | `(list, i) -> usize`                                 | Byte offset of issue `i`. 0 if invalid.|
| `harper_issue_len`     | `(list, i) -> usize`                                 | Byte length of issue `i`. 0 if invalid.|
| `harper_issue_message` | `(list, i, out_len*) -> u8*`                         | UTF-8 message, `*out_len` receives length. |
| `harper_free_issues`   | `(list)`                                             | Frees a list from `harper_lint`. Null-safe. |
| `harper_free`          | `(engine)`                                           | Frees an engine from `harper_init`. Null-safe. |

## Dependencies

Dependencies are **not vendored** in this repository. `Cargo.lock` pins the
exact crate versions; builds fetch them from crates.io (same approach as the
existing `mathml2omml` FetchContent). To vendor for fully-offline builds:

```bash
cd vendor/harper-ffi
cargo vendor --locked vendor
# then add a .cargo/config.toml pointing crates-io at ./vendor
```

## Tests

```bash
cd vendor/harper-ffi
cargo test
```

Tests exercise the real FFI surface end-to-end: grammar detection
(`I has a cat.`), spell-check absence (`This is helo wrking text.` must not
produce "Did you mean ..." messages), UTF-8 byte-offset correctness
(`Héllo wörld.`), null-pointer handling, and span bounds.

## License

The wrapper itself is MIT. Note that `harper-core` 2.7.0 is published under
**Apache-2.0** (its manifest license), not MIT.

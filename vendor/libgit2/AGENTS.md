# AGENTS.md

## What

libgit2 v1.9.6 vendored into Scriba for the Mermaid Git Graph assistant
(`GitGraphBuilder`, `src/GitGraphBuilder.cpp`). Straight upstream copy under
`vendor/libgit2/` — no Scriba-local source changes. **Not git-tracked**: the
repo's `.gitignore` keeps `vendor/*` ignored (only md4c, hunspell,
mathml2omml, and stoppard are tracked), so a fresh clone needs libgit2 placed
here to build. To fetch upstream:

```bash
# from https://github.com/libgit2/libgit2, tag v1.9.6
# copy the source tree (NOT tests/examples/docs) into vendor/libgit2/
```

libgit2 is GPLv2-with-linking-exception (see `COPYING`).

## Build

libgit2 is built via the top-level scriba CMakeLists (not standalone). The
vendored `CMakeLists.txt` is used verbatim, with these values forced before
`add_subdirectory(vendor/libgit2)` and scriba's own restored afterwards:

```cmake
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(USE_SSH OFF CACHE BOOL "" FORCE)
set(USE_HTTPS OFF CACHE BOOL "" FORCE)
set(USE_NSEC OFF CACHE BOOL "" FORCE)
set(USE_BUNDLED_ZLIB ON CACHE BOOL "" FORCE)
set(REGEX_BACKEND "builtin" CACHE STRING "" FORCE)
set(LIBGIT2_EMBEDDED ON)
add_subdirectory(vendor/libgit2)
```

Why these values:

- `BUILD_TESTS`/`BUILD_CLI`/`BUILD_EXAMPLES`/`BUILD_FUZZERS` OFF — the tests
  need libgit2's own clar suite and would download deps; scriba never builds
  them. `BUILD_TESTS` is scriba's own option name too, which is why the value
  round-trips through `_scriba_BUILD_TESTS`.
- `USE_SSH`/`USE_HTTPS` OFF — the Git Graph panel only reads local refs/objects;
  network/auth support would drag in OpenSSL/libssh2 system deps.
- `USE_BUNDLED_ZLIB ON` + `REGEX_BACKEND builtin` — use the pcre2/zlib copies
  already in `vendor/libgit2/deps/`, so the build is fully offline.
- `LIBGIT2_EMBEDDED ON` — upstream option (read in `cmake/PkgBuildConfig.cmake`)
  that suppresses libgit2's pkgconfig install rules; keeps the scriba package
  clean. There is no Scriba-local patch behind it.

The resulting static `libgit2` target is linked by the `scriba_gitgraph`
library (`src/GitGraphBuilder.cpp`) and the test targets that compile it, with
`Threads::Threads` and (on Linux) `dl`.

## Structure

- `src/` — the library proper (libgit2 sources; headers under `src/libgit2/`)
- `deps/` — bundled third-party deps used when the matching `USE_BUNDLED_*` /
  `REGEX_BACKEND builtin` options are on: `pcre2`, `zlib`, `xdiff`, `llhttp`
- `include/` — public headers (`include/git2.h` umbrella), installed headers
- `cmake/` — upstream CMake modules (option handling, dep selection)
- `CMakeLists.txt`, `COPYING`, `LICENSE`, `README.md`, `AUTHORS`, ... — upstream

## Key files

- `src/libgit2/` — library implementation
- `src/libgit2/include/git2.h` — public API umbrella used by `GitGraphBuilder.cpp`
- `src/libgit2/*.h` — per-topic public headers (repository, branch, revwalk, ...)

## Gotchas

- Scriba builds libgit2 **statically**; never flip `BUILD_SHARED_LIBS` ON in
  the top-level integration or the shipped binary depends on a system libgit2
  that isn't guaranteed to exist.
- The public headers are generated into the libgit2 build tree; consumers must
  include the build-tree include dir (see the `scriba_gitgraph` target's
  `target_include_directories`), not just the source `include/`.
- Re-adding `BUILD_TESTS ON` (e.g. for debugging libgit2 upstream) will
  clobber scriba's own test flag — that's exactly why the top-level block
  snapshots and restores it.
- Do not add the GPL-3.0 banner to libgit2 sources; they keep their upstream
  license headers.

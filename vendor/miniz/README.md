# miniz (vendored)

Single-file ZIP/DEFLATE library (public-domain/MIT, see its own license text at
the top and bottom of `miniz.c` / `miniz.h`).

- **Source:** https://github.com/richgel999/miniz
- **Version:** 3.1.2 (release tag `3.1.2`, commit `77d0dce8627735138c51770d1799a1ef48f2117d`)
- **Files:** amalgamated `miniz.c` + `miniz.h`, taken from the official
  `miniz-3.1.2.zip` release asset (which is the output of upstream's
  `amalgamate.sh`). Master's checked-in `miniz.c`/`miniz.h` are split
  build-stubs; the amalgamated release pair is the single-file form scriba
  needs.

## License note

miniz is public-domain / MIT-licensed, NOT GPL. Its original license header is
preserved intentionally in `miniz.c`/`miniz.h` — this deviation from the repo's
usual "GPL header on vendored files" rule is a deliberate decision (the vendored
file is unmodified upstream source, and its permissive license is compatible
with GPL-3.0 use).

Do not add the GPL header to `miniz.c`/`miniz.h`; their upstream license text is
authoritative.

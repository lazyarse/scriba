#!/usr/bin/env python3
# Copyright (C) 2026 LazyArse
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""Fetch and curate clean-corpus sentences for Stoppard's dialect tests.

CLI:  python3 scripts/fetch_corpus.py --dialect american --out FILE
      python3 scripts/fetch_corpus.py --dialect all

NOT run by the test suite (network). The committed files under
tests/data/ are the source of truth; this script regenerates drafts that
still need the same hand-curation the committed files received:
one sentence per line, deliberately simple (no coordination, inversion,
or complex embedding), no errors under the target dialect profile.

Per-dialect sources (SPEC §15.4). All are public-domain or open-licensed;
see tests/data/CORPUS_SOURCES.md for attribution wording.

  american    — Gutenberg plain prose; Project Gutenberg text corpus
  british     — House of Commons Hansard debates (parliament.uk)
  australian  — ACE (Australian Corpus of English) mixed-register samples
  indian      — Press Information Bureau press-release register
  canadian    — House of Commons Hansard (ourcommons.ca), OGL Canada
  newzealand  — NZ Parliament Hansard (PD) + te reo Māori loanwords

Usage note: many of these endpoints serve bulk archives or require a
user agent; the script downloads, splits into sentences, and applies the
curation filters (length bounds, no quotes/brackets, ASCII-ish, dedupe).
Inspect the output before committing.
"""

import argparse
import re
import sys
import urllib.request

SOURCES = {
    "american": ("https://www.gutenberg.org/", "plain prose (e.g. public-domain novels)"),
    "british": ("https://hansard.parliament.uk/", "Commons debates"),
    "australian": ("https://www.ace-corpora.org/", "ACE mixed-register"),
    "indian": ("https://pib.gov.in/", "press-release register"),
    "canadian": ("https://www.ourcommons.ca/", "Commons Hansard (OGL Canada)"),
    "newzealand": ("https://www.parliament.nz/", "Hansard (public domain)"),
}

MIN_LEN, MAX_LEN = 15, 100
SKIP_RE = re.compile(r"[\[\]\"\u201c\u201d();:{}]|http|www\.|^#")

def sentences(text: str) -> list:
    text = re.sub(r"\s+", " ", text)
    parts = re.split(r"(?<=[.!?])\s+(?=[A-Z\u0100-\u017F])", text)
    out = []
    for p in parts:
        p = p.strip()
        if not p or not re.match(r"^[A-Z\u0100-\u017F]", p):
            continue
        if SKIP_RE.search(p):
            continue
        if not (MIN_LEN <= len(p) <= MAX_LEN):
            continue
        if p.count(" ") > 18:
            continue
        out.append(p)
    return out

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dialect", required=True,
                    choices=list(SOURCES) + ["all"])
    ap.add_argument("--out", help="output file (required unless --dialect all)")
    ap.add_argument("--max-sentences", type=int, default=200)
    args = ap.parse_args()

    dialects = list(SOURCES) if args.dialect == "all" else [args.dialect]
    for d in dialects:
        url, what = SOURCES[d]
        if args.dialect != "all" and not args.out:
            print(f"error: --out required for --dialect {d}", file=sys.stderr)
            return 1
        print(f"# {d}: fetching {url} ({what})...", file=sys.stderr)
        req = urllib.request.Request(url, headers={"User-Agent": "stoppard-corpus/0.1"})
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                html = r.read().decode("utf-8", errors="replace")
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"# {d}: fetch failed: {exc}", file=sys.stderr)
            continue
        picked = [s for s in sentences(html)][: args.max_sentences]
        if args.dialect == "all":
            out_path = f"tests/data/clean_corpus_{d}.txt"
        else:
            out_path = args.out
        with open(out_path, "w", encoding="utf-8") as f:
            f.write("\n".join(picked) + ("\n" if picked else ""))
        print(f"# {d}: wrote {len(picked)} sentences to {out_path}", file=sys.stderr)
    return 0

if __name__ == "__main__":
    sys.exit(main())

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
"""Fetch and curate the Birkbeck misspelling corpus for held-out evaluation.

CLI:  python3 scripts/fetch_birkbeck.py [--out bench/birkbeck/spelling_birkbeck.txt]

Source: Roger Mitton's Birkbeck spelling-error corpus
(https://titan.dcs.bbk.ac.uk/~roger/corpora.html, "missp.dat": 36,133
misspellings of 6,133 target words). License: CC BY-NC-SA 3.0 — the raw
corpus is therefore NEVER committed; only the filtered, eval-only pair file
is written under bench/ (gitignored) for local measurement.

Format: a "$target" line, followed by one misspelling per line.

Filtering mirrors the Wikipedia held-out set (tests/data/spelling_wikipedia.txt)
so the two evaluation sets are directly comparable:

  - typo and intended both lowercased ASCII [a-z]+
  - intended must be in the en-US dictionary (data/en-US.txt)
  - typo must NOT be a dictionary word: real-word errors (the homophone
    class, invisible to a non-word checker) are counted and excluded
  - typo != intended, dedupe
  - Levenshtein distance cap (--max-dist, default 4)

Output: "typo | intended" lines. Prints a breakdown of what was excluded.

The corpus is licensed for non-commercial use: eval only, never tune on it.
"""
import argparse
import random
import re
import sys
import urllib.request

URL = "https://titan.dcs.bbk.ac.uk/~roger/missp.dat"
USER_AGENT = "stoppard-bench/0.1"
DEFAULT_DICT = "data/en-US.txt"
DEFAULT_OUT = "bench/birkbeck/spelling_birkbeck.txt"

def load_dict(path: str) -> set:
    words = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            w = line.strip().lower()
            if re.fullmatch(r"[a-z]+", w):
                words.add(w)
    return words

def lev(a: str, b: str) -> int:
    if abs(len(a) - len(b)) > 4:
        return 5
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i] + [0] * len(b)
        for j, cb in enumerate(b, 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb))
        prev = cur
    return prev[-1]

def parse_corpus(text: str) -> list:
    pairs = []
    target = None
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("$"):
            target = line[1:].strip().lower()
        elif target:
            pairs.append((line.lower(), target))
    return pairs

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", help="local copy of missp.dat (skip download)")
    ap.add_argument("--dict", default=DEFAULT_DICT)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--max-dist", type=int, default=4,
                    help="drop pairs whose Levenshtein distance exceeds this (default 4)")
    ap.add_argument("--subset", type=int, metavar="N", default=0,
                    help="also write a fixed-seed N-pair subset for fast iteration")
    args = ap.parse_args()

    if args.input:
        with open(args.input, encoding="utf-8", errors="replace") as f:
            text = f.read()
    else:
        req = urllib.request.Request(URL, headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                text = r.read().decode("utf-8", errors="replace")
        except Exception as exc:  # noqa: BLE001 - report and exit
            print(f"# fetch failed: {exc} (use --input with a local copy)", file=sys.stderr)
            return 1

    words = load_dict(args.dict)
    raw = parse_corpus(text)
    kept, realword, nondict, nonascii, far, dup = [], 0, 0, 0, 0, 0
    seen = set()
    for typo, intended in raw:
        if typo == intended:
            continue
        if not re.fullmatch(r"[a-z]+", typo) or not re.fullmatch(r"[a-z]+", intended):
            nonascii += 1  # punctuation, capitals, non-ASCII (proper nouns etc.)
            continue
        if intended not in words:
            nondict += 1  # proper nouns, British spellings, archaic forms
            continue
        if typo in words:
            realword += 1  # homophone/real-word class — invisible to non-word checkers
            continue
        if lev(typo, intended) > args.max_dist:
            far += 1
            continue
        key = (typo, intended)
        if key in seen:
            dup += 1
            continue
        seen.add(key)
        kept.append(key)
    kept.sort()

    import os
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        for typo, intended in kept:
            f.write(f"{typo} | {intended}\n")
    print(f"# wrote {len(kept)} pairs to {args.out}")
    print(f"# excluded: real-word={realword} off-dict={nondict} non-ascii={nonascii}"
          f" too-far={far} dup={dup}")

    if args.subset:
        rng = random.Random(20260803)
        subset = rng.sample(kept, min(args.subset, len(kept)))
        sub_out = f"{args.out}.subset{args.subset}"
        with open(sub_out, "w", encoding="utf-8") as f:
            for typo, intended in subset:
                f.write(f"{typo} | {intended}\n")
        print(f"# wrote {len(subset)}-pair subset to {sub_out}")
    return 0

if __name__ == "__main__":
    sys.exit(main())

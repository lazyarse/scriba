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
"""Levenshtein-distance table from a DumpMissed/BirkbeckReport DUMP output.

CLI:  python3 scripts/analyze_dump.py < dump.txt
      python3 scripts/analyze_dump.py --missed < dump.txt

Reads "DUMP typo | intended | rank=N | top5=..." lines on stdin (produced
by SpellingParity.DumpMissed or BirkbeckReport with STOPPARD_DUMP_MISSED=1)
and prints the per-distance accuracy table in the same bucket layout as the
capture headers (tests/data/spelling_hunspell_wikipedia.txt):

  d1   n=2492  93.0/99.9

Buckets: d1, d2, d3, d4, d5+ (Levenshtein distance between typo and
intended). Distances beyond 4 are clamped into d5+ to keep the DP bounded.

With --missed, additionally prints the error-class breakdown of every word
that failed top-5 (rank < 0 or >= 5): subs (single letter replaced), extra
(one letter too many), forgot (one missing), swap (adjacent transposition),
or multi (everything else).
"""
import argparse
import re
import sys

LINE_RE = re.compile(r"^DUMP ([^|]+) \| ([^|]+) \| rank=(-?\d+)(?: \| top5=.*)?$")

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

def bucket(d: int) -> str:
    return f"d{min(d, 5)}" if d < 5 else "d5+"

def error_class(typo: str, intended: str) -> str:
    if len(typo) == len(intended):
        diffs = [i for i in range(len(typo)) if typo[i] != intended[i]]
        if len(diffs) == 1:
            return "sub"
        if len(diffs) == 2 and diffs[1] == diffs[0] + 1 \
                and typo[diffs[0]] == intended[diffs[1]] \
                and typo[diffs[1]] == intended[diffs[0]]:
            return "swap"
        return "multi"
    if abs(len(typo) - len(intended)) == 1:
        if len(typo) > len(intended):
            for i in range(len(typo)):
                if typo[:i] + typo[i + 1:] == intended:
                    return "extra"
            return "multi"
        for i in range(len(intended)):
            if intended[:i] + intended[i + 1:] == typo:
                return "forgot"
        return "multi"
    return "multi"

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--missed", action="store_true",
                    help="also print the error-class breakdown of top-5 misses")
    args = ap.parse_args()

    buckets = {}
    missed = {}
    for line in sys.stdin:
        m = LINE_RE.match(line.strip())
        if not m:
            continue
        typo, intended, rank = m.group(1).strip(), m.group(2).strip(), int(m.group(3))
        b = bucket(lev(typo, intended))
        stats = buckets.setdefault(b, [0, 0, 0])
        stats[0] += 1
        if rank == 0:
            stats[1] += 1
        if 0 <= rank < 5:
            stats[2] += 1
        elif args.missed:
            cls = error_class(typo, intended)
            entry = missed.setdefault(b, {}).setdefault(cls, 0)
            missed[b][cls] = entry + 1
    if not buckets:
        print("no DUMP lines found on stdin", file=sys.stderr)
        return 1
    total = sum(s[0] for s in buckets.values())
    t1 = sum(s[1] for s in buckets.values())
    t5 = sum(s[2] for s in buckets.values())
    print(f"total  n={total:5d} {t1 * 100.0 / total:5.1f}%/{t5 * 100.0 / total:5.1f}%")
    for b in sorted(buckets, key=lambda k: (k == "d5+", int(k[1:].rstrip("+")))):
        n, top1, top5 = buckets[b]
        p1 = top1 * 100.0 / n
        p5 = top5 * 100.0 / n
        print(f"{b:5s} n={n:5d} {p1:5.1f}/{p5:5.1f}")
        if args.missed:
            classes = missed.get(b, {})
            if classes:
                order = [c for c in ("sub", "extra", "forgot", "swap", "multi") if c in classes]
                print("       missed:", " ".join(f"{c}={classes[c]}" for c in order))
    return 0

if __name__ == "__main__":
    sys.exit(main())


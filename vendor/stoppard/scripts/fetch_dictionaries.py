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
"""Build Stoppard's plain-wordlist spelling dictionaries (SPEC §19.3).

CLI:
  python3 scripts/fetch_dictionaries.py            # build data/en-US.txt, data/en-GB.txt
  python3 scripts/fetch_dictionaries.py --check    # verify the committed files reproduce

Downloads the pinned SCOWL tarball (scowl-2020.12.07, BSD), runs its own
mk-list script for the same compositions the official SCOWL hunspell
dictionaries use (en_US at size 60; en_GB-ize + en_GB-ise at size 60 — the
British list therefore accepts both -ise and -ize spellings), filters the
offensive/profane words exactly like SCOWL's make-hunspell-dict does, then
lowercases, dedupes and sorts. The result is a pure surface-form word list
(base + inflected forms, no affix engine).

NOT run by the test suite (network). The committed files under data/ are the
source of truth; this script reruns only when SCOWL is bumped (update
SCOWL_URL/SCOWL_SHA256, regenerate, update CHECKSUMS.txt).
"""

import hashlib
import os
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

SCOWL_URL = "https://downloads.sourceforge.net/wordlist/scowl-2020.12.07.tar.gz"
SCOWL_SHA256 = "5587667caa20c4891390c2d42dbb4d5c4c3f41bee77af1457ece3ba23fb859cc"
SIZE = 60

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(REPO, "data")

VARIANT_WORDS = {
    "en-US.txt": ["en_US"],
    "en-GB.txt": ["en_GB-ize", "en_GB-ise"],
}

# Keyboard/suggestion tables from SCOWL's own speller/en.aff (SPEC §19.4):
# TRY = letter-frequency fallback string, REP = common wrong-letter pairs.
# One shared table serves both dictionaries (that is the official hunspell
# composition — a single en.aff accompanies every SCOWL .dic).
KEYBOARD_FILES = ["keyboard-en-US.txt", "keyboard-en-GB.txt"]

OFFENSIVE = ["offensive.1", "offensive.2", "profane.1"]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch_tarball():
    tarball = os.path.join(tempfile.gettempdir(), "scowl-2020.12.07.tar.gz")
    if os.path.exists(tarball) and sha256_file(tarball) == SCOWL_SHA256:
        return tarball
    print("downloading", SCOWL_URL)
    urllib.request.urlretrieve(SCOWL_URL, tarball)
    actual = sha256_file(tarball)
    if actual != SCOWL_SHA256:
        sys.exit(f"sha256 mismatch for SCOWL tarball: {actual}")
    return tarball


def extract_scowl(tarball):
    work = tempfile.mkdtemp(prefix="scowl-")
    with tarfile.open(tarball) as tf:
        tf.extractall(work, filter="data")
    return os.path.join(work, "scowl-2020.12.07")


def mk_list(scowl_dir, dialects):
    env = dict(os.environ, LANG="C", LC_ALL="C", LC_CTYPE="C", LC_COLLATE="C")
    out = subprocess.check_output(
        ["perl", "mk-list", "--accents=strip", *dialects, str(SIZE)],
        cwd=scowl_dir, env=env, text=True)
    return out.splitlines()


def offensive_set(scowl_dir):
    excluded = set()
    for name in OFFENSIVE:
        with open(os.path.join(scowl_dir, "misc", name)) as f:
            excluded.update(line.strip() for line in f if line.strip())
    return excluded


def build_list(scowl_dir, dialects):
    words = mk_list(scowl_dir, dialects)
    excluded = offensive_set(scowl_dir)
    folded = {w.lower() for w in words} - excluded
    # Apostrophe forms ("ability's", "a's") come from SCOWL's materialized
    # possessives but can never match a token: the tokenizer splits "'s"
    # and mid-word apostrophes ("O'Brien") are skipped (§19.5). Drop them.
    folded = {w for w in folded if "'" not in w}
    return sorted(folded)


def write_list(out_path, words):
    with open(out_path, "w") as f:
        f.write("\n".join(words))
        f.write("\n")


def build_keyboard(scowl_dir, out_path):
    lines = ["# TRY/REP harvested from SCOWL 2020.12.07 speller/en.aff (BSD); SPEC §19.4"]
    with open(os.path.join(scowl_dir, "speller", "en.aff")) as f:
        for raw in f:
            line = raw.rstrip()
            if line.startswith(("TRY ", "REP ")):
                lines.append(line)
    write_list(out_path, lines)


def build():
    tarball = fetch_tarball()
    scowl_dir = extract_scowl(tarball)
    os.makedirs(DATA, exist_ok=True)
    print(f"building {len(VARIANT_WORDS)} lists from {scowl_dir}")
    for name, dialects in VARIANT_WORDS.items():
        words = build_list(scowl_dir, dialects)
        path = os.path.join(DATA, name)
        write_list(path, words)
        print(f"  {name}: {len(words)} words, {os.path.getsize(path)} bytes")
    for name in KEYBOARD_FILES:
        path = os.path.join(DATA, name)
        build_keyboard(scowl_dir, path)
        print(f"  {name}: {len(open(path).read().splitlines())} lines, {os.path.getsize(path)} bytes")


def update_checksums():
    tarball = os.path.join(tempfile.gettempdir(), "scowl-2020.12.07.tar.gz")
    lines = [
        f"scowl-2020.12.07.tar.gz sha256 {SCOWL_SHA256}",
    ]
    for name in VARIANT_WORDS:
        path = os.path.join(DATA, name)
        lines.append(f"{name} sha256 {sha256_file(path)}")
    for name in KEYBOARD_FILES:
        path = os.path.join(DATA, name)
        lines.append(f"{name} sha256 {sha256_file(path)}")
    with open(os.path.join(DATA, "CHECKSUMS.txt"), "w") as f:
        f.write("\n".join(lines))
        f.write("\n")
    print("  CHECKSUMS.txt updated")


def check():
    tarball = fetch_tarball()
    scowl_dir = extract_scowl(tarball)
    ok = True
    for name, dialects in VARIANT_WORDS.items():
        words = build_list(scowl_dir, dialects)
        with open(os.path.join(DATA, name)) as f:
            committed = f.read().splitlines()
        if words == committed:
            print(f"  {name}: reproducible (len {len(words)})")
        else:
            ok = False
            print(f"  {name}: MISMATCH ({len(words)} built vs {len(committed)} committed)")
    sys.exit(0 if ok else 1)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        check()
    elif len(sys.argv) == 1:
        build()
        update_checksums()
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()

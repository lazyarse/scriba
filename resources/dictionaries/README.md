# Bundled Spellcheck Dictionaries

Plain word lists (`.txt`, one word per line) bundled with Scriba and
registered in `resources/scriba.qrc`. Extracted from the vendored stoppard
repository (`data/`), which derives them from [SCOWL](http://wordlist.sourceforge.net)
plus dialect allowance lists and Māori exemptions.

| File             | Source | License |
|------------------|--------|---------|
| `en-US.txt`      | stoppard `data/` (SCOWL US) | SCOWL BSD-style |
| `en-GB.txt`      | stoppard `data/` (SCOWL GB) | SCOWL BSD-style |
| `maori-nz.txt`   | stoppard `data/` (SCOWL NZ + Māori exemptions) | SCOWL BSD-style |
| `canadian-en.txt`| stoppard `data/` (Canadian allowances) | SCOWL BSD-style |
| `keyboard-en-GB.txt` | stoppard `data/` (keyboard-first GB variants) | SCOWL BSD-style |

## Adding / upgrading a bundled dictionary

1. Add or update the word list in stoppard's `data/` (keep it in sync with
   `vendor/stoppard/data/`).
2. Copy it to `resources/dictionaries/<name>.txt`.
3. Register it in `resources/scriba.qrc`.
4. Rebuild. The bundled copy is extracted to
   `~/.config/scriba/dictionaries/bundled/` on first use (a stale copy whose
   `# scriba-dict-version:` marker doesn't match the bundle is superseded to
   `.bak`).

User-installed word lists are not bundled: Preferences → Spelling →
"Import Word Lists…" copies a `.txt` list into the per-user config directory
(`~/.config/scriba/dictionaries/`), where it applies to every language and
dialect. "Remove" deletes an installed list again (bundled lists cannot be
removed).

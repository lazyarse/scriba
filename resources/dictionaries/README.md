# Bundled Spellcheck Dictionaries

Hunspell (`*.aff` / `*.dic`) dictionaries bundled with Scriba and registered in
`resources/scriba.qrc`.

| File   | Source | Version | License |
|--------|--------|---------|---------|
| `en_US.aff` / `en_US.dic` | [LibreOffice dictionaries](https://github.com/LibreOffice/dictionaries/tree/master/en), derived from [SCOWL](http://wordlist.sourceforge.net) | 2020.12.07 | SCOWL BSD-style (see [README_en_US](https://github.com/LibreOffice/dictionaries/blob/master/en/README_en_US.txt)) |
| `en_GB.aff` / `en_GB.dic` | [LibreOffice dictionaries](https://github.com/LibreOffice/dictionaries/tree/master/en) | current master | LGPL (Kevin Atkinson's original wordlist, extensively extended) |

## Adding / upgrading a bundled dictionary

1. Download the `.aff` and `.dic` pair from the source above.
2. Save as `resources/dictionaries/<lang>.{aff,dic}`.
3. Register both files in `resources/scriba.qrc`.
4. Rebuild.

User-installed dictionaries are not bundled: Preferences → Spellcheck →
"Add Dictionary…" copies an `.aff`/`.dic` pair into the per-user config
directory (`~/.config/scriba/dictionaries/`).

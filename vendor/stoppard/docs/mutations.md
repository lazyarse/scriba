# Mutation corpus operators

Seed source: `tests/data/clean_corpus.txt` (universal-American clean lines).
`tests/test_mutations.cpp` applies one guarded error-operator per seed and
requires the engine to produce exactly one issue whose suggestion restores
the seed.

| Operator                         | Anchor (in clean seed)          | Mutation            | Rule |
|----------------------------------|---------------------------------|---------------------|------|
| Verb-form swap (modal governs)   | `[Modal] + Verb-Base`           | verb -> 3ps         | R1   |
| Verb-form swap (subject-pronoun) | `[I/you/we/they] + Verb-Base`   | verb -> 3ps         | R3   |
| Verb-form swap (subject 3sg)     | `[he/she] + Verb-3ps`           | verb -> Base        | R3   |
| Pluralize NP head                | `[sing Det] + [sing Noun]`      | noun -> plural      | R5   |
| Pronoun-case          | `[Prep] + [object Pronoun]`     | pronoun -> subject  | R6   |

## Guard

Seeds that are not simple declaratives are skipped: commas, `?`/`!`, and
conjunctions (`and/or/but/because/although/while/if/unless/whereas`). Only
one operator anchors per seed (first hit wins).

Two further guards keep the "exactly one issue, suggestion restores seed"
invariant honest given that the lemmatizer is suffix-based without a
dictionary:

- The mutated word's reverse-map must be deterministic. A verb-3ps anchor is
  skipped when its plural does not round-trip back to its lemma
  (`classifyVerbForm(third).lemma != lemma`); a plural-noun anchor is skipped
  when `singularize(plural) != noun` (e.g. "houses" cannot decide between
  hous/house).
- Pluralizing a clause-*subject* noun is skipped: it would flip
  subject-verb agreement and seat a second issue ("This books is
  interesting." is two errors, not one).
- For the `[he/she] + Verb-3ps -> Base` anchor, the base form must itself be
  re-tagged as a real base verb ("needs -> need" works; a lemma that the
  suffix-classifier re-tags as past/participle is skipped).

## Deferred operators

Landing with their guard tables in later milestones:

- Negation flip (R2/R8 rules) — M4.5
- Modal insertion (R1) — M4.5
- Tense shift (R2/R4 tense) — M8

These need cross-sentence state or additional rule guards that don't exist
yet; their anchors are documented here so the matrix extends cleanly.
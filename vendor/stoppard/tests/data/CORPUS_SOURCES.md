# Rule-case corpus provenance

Each file in `rule_cases/` is a data-driven rule-case matrix consumed by
`tests/test_rule_cases.cpp` (PLAN Task 3.7). One rule-case per line in the
format:

    wrong | right | rule-id

- `wrong` — a sentence containing an error that rule `R{n}` must flag.
- `right` — the corrected form; the harness derives the expected issue
  (span + suggestion) from the token-level LCS diff between the two sides.
- `rule-id` — documentation and family prefix (not asserted beyond matching
  the file it lives in).

Semantics (asserted by the harness):

- If the two sides are identical, `wrong` must produce **zero** issues.
- Otherwise `wrong` must produce exactly the issues implied by the diff:
  each contiguous mismatched run yields one issue spanning the deleted
  words; the suggestion is the inserted words joined by spaces (a
  delete-only run expects a `Remove` suggestion).

The `R9_regionalisms.txt` matrix is run under `Dialect::British` (the
regionalism profile that drives rule R9); all other matrices run under
`Dialect::American`.

## Sources

- Example sentences from SPEC §15 (per-rule sections): R1 modal verbs,
  R2 auxiliaries, R3 subject-verb agreement, R4 to-infinitives,
  R5 determiner-noun agreement, R6 pronoun case, R7 double modals,
  R8 double negatives, R9 regionalisms.
- Guard sentences (identical sides) cover the idiom/edge-case guards the
  rules were built with: `Watch it break.` (causative bare infinitive),
  `I look forward to going.` (gerund after "to"), `Between you and I.`
  (coordinated PP), `I can't not laugh.` (negator stacking), `There is no
  doubt.` / `No one came.` (R8 guards), mass nouns (`These water`), and
  dialect-neutral R9 forms.

## Notes

- The corpus is the executable spec: if a line's expectation does not
  match the engine's output, the line (or the engine) is wrong and must be
  reconciled before the matrix goes green.
- Spans are UTF-16 character offsets into `wrong`, matching the engine's
  issue spans; attached punctuation ("she." → "she") is excluded from
  spans and suggestions, mirroring the tokenizer's word/punctuation split.
- Each line encodes exactly one edit: the diff-based expectations cannot
  express a second error that only becomes visible after the first edit is
  applied (e.g. PLAN's "These cat are cute." also has "are"→"is" once the
  determiner-noun pair is fixed). Such sentences use a modal verb that is
  invariant under the noun's number ("These cat can run.").


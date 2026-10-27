# Bounded rules evaluation

Last updated: 2026-10-26

State: bounded evaluator, preview adapter, schema-6 Save/Apply, and rules sheet implemented. The rules-enabled `3f94fe6` baseline passed all 72 cases in Release and ASan/UBSan on 2026-10-26, closing the earlier Debug-only gap. Current rules-enabled documents use schema 12 with [account-scoped workspaces](accounts.md) and [bulk ledger receipts](search-ledger.md). See [project status](status.md) for current verification and remaining limitations. The accepted [rules policy](../features/004-rules-and-explanations.md) and [durable-rules contract](durable-rules.md) remain unchanged.

## Matching and derived fields

The optional `SpentIt::rules` target uses [PCRE2 10.49](../third_party/pcre2/README.md) through a narrow adapter. Matching searches immutable original fields, using UTF-8, Unicode properties and case-insensitive defaults. Explicit pattern flags can request case-sensitive matching; anchors select whole-field matching. Invalid UTF-8/NUL input is rejected without normalization. Backslash-C is forbidden to preserve valid capture boundaries. JIT is not used.

Templates contain literal text, `$$` for a dollar sign, and `${0}` through `${32}` for numeric captures. Zero means the complete match. References to nonexistent captures or malformed syntax fail. A valid optional capture that did not participate expands to empty text. Output is bounded before publication.

Ordered transaction rules run first, then source-split rules. First assignment to each field wins; compatible and competing later assignments are explained as shadowed. Parent category classification is a temporary suggestion, not another financial allocation. Original unsplit records receive the transaction category on their associated original allocation. Actual source splits require explicit category assignment or parent inheritance. Missing parent classification is explained and leaves the field unchanged.

The engine receives explicit retained source associations. Reordering current splits does not change which source memo a rule sees. New manual splits have no source association and are excluded from source-split matching. The interface contains no monetary amounts or financial dates, so it cannot change money, dates or split structure.

Reapplication recomputes source/rule-owned fields from original facts. Manually owned fields, including blanks, remain protected unless replacement was explicitly requested for the preview. Replacement only changes fields receiving a matching rule action; unmatched manual fields remain intact. Each applied field records rule identity/version. The pure application adapter reconstructs proposed DraftRows with exact amounts, dates, structure, allocation identities and binding snapshots preserved. It requires retained field provenance for existing rule-owned fields and emits new provenance alongside the proposal. Schema 6 stores those marks and replays saved rule versions during verify. The sheet shows the current preview's overlap and removal text for the records just evaluated. It does not display the full saved-version history that verify replays.

## Resource and error contract

The current engineering limits are explicit rejection boundaries, not truncation:

- 4 KiB pattern, 64 KiB source field/output, 32 captures, 64 nested pattern parentheses.
- Per-pattern 1 MiB compiled code and 8 MiB allocator budget; per-match 8 MiB allocator budget, 4 MiB match heap, depth 128 and 100,000 matching/callout steps.
- At most 128 rules and 16 MiB total compiled code in a ruleset.
- At most 4,096 records, 4,096 allocations per record and 16,384 aggregate current/source allocations per chosen preview scope.
- 16 MiB total input text and generated field bytes, 100,000 rule comparisons/explanations, and 1,000,000 aggregate callouts per preview scope.

All selected inputs are validated before producing a result. Cancellation checks run during validation and matching. Invalid patterns, invalid input, exhausted resources, cancellation, invalid templates and dependency/internal failures are distinguished from a valid non-match. No partial result is returned when the chosen scope fails.

The rules page prints these published limits. Printing them is not a measurement of representative-workload responsiveness. The installed PCRE2 library is not instrumented by SpentIt's sanitizer flag. Packaging and other-platform verification remain open.

## Verification

Standalone Debug, Release and ASan/UBSan each passed all 16 registered engine/parser/regex/evaluation/application-preview cases. Coverage includes exact financial preservation, aggregate-allocation bounds, validation cancellation, manual blanks, obsolete rule results, source-association reorder/forgery and explicit replacement. Final builds and sanitizer logs were clean. Schema 6 Save and Apply have a separate Debug `rulestore.*` run, recorded in [project status](status.md). The rules sheet has a separate Debug `import.rules_ui` run, also recorded there. Neither run is Release or sanitizer evidence.

The historical Debug-only persistence/UI results above are superseded by the 2026-10-26 72/72 Release and ASan/UBSan baseline runs in [project status](status.md). The installed PCRE2 library itself remains uninstrumented.

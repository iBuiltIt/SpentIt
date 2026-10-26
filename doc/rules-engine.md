# Bounded rules evaluation

Last updated: 2026-10-25

State: bounded standalone engine and pure application preview adapter implemented. Schema 6 Save, Preview, and Apply persistence is implemented in the rules-enabled import store and verified in Debug only, as recorded in [project status](status.md). The sheet editor is not implemented. The owner accepted the [bounded rules policy](../features/004-rules-and-explanations.md). This document records the evaluator contracts. The storage contract is [durable rules](durable-rules.md).

## Matching and derived fields

The optional `SpentIt::rules` target uses [PCRE2 10.49](../third_party/pcre2/README.md) through a narrow adapter. Matching searches immutable original fields, using UTF-8, Unicode properties and case-insensitive defaults. Explicit pattern flags can request case-sensitive matching; anchors select whole-field matching. Invalid UTF-8/NUL input is rejected without normalization. Backslash-C is forbidden to preserve valid capture boundaries. JIT is not used.

Templates contain literal text, `$$` for a dollar sign, and `${0}` through `${32}` for numeric captures. Zero means the complete match. References to nonexistent captures or malformed syntax fail. A valid optional capture that did not participate expands to empty text. Output is bounded before publication.

Ordered transaction rules run first, then source-split rules. First assignment to each field wins; compatible and competing later assignments are explained as shadowed. Parent category classification is a temporary suggestion, not another financial allocation. Original unsplit records receive the transaction category on their associated original allocation. Actual source splits require explicit category assignment or parent inheritance. Missing parent classification is explained and leaves the field unchanged.

The engine receives explicit retained source associations. Reordering current splits does not change which source memo a rule sees. New manual splits have no source association and are excluded from source-split matching. The interface contains no monetary amounts or financial dates, so it cannot change money, dates or split structure.

Reapplication recomputes source/rule-owned fields from original facts. Manually owned fields, including blanks, remain protected unless replacement was explicitly requested for the preview. Replacement only changes fields receiving a matching rule action; unmatched manual fields remain intact. Each applied field records rule identity/version. The pure application adapter reconstructs proposed DraftRows with exact amounts, dates, structure, allocation identities and binding snapshots preserved. It requires retained field provenance for existing rule-owned fields and emits new provenance alongside the proposal. Schema 6 stores those marks and replays saved rule versions during verify. The sheet does not display that history yet.

## Resource and error contract

The current engineering limits are explicit rejection boundaries, not truncation:

- 4 KiB pattern, 64 KiB source field/output, 32 captures, 64 nested pattern parentheses.
- Per-pattern 1 MiB compiled code and 8 MiB allocator budget; per-match 8 MiB allocator budget, 4 MiB match heap, depth 128 and 100,000 matching/callout steps.
- At most 128 rules and 16 MiB total compiled code in a ruleset.
- At most 4,096 records, 4,096 allocations per record and 16,384 aggregate current/source allocations per chosen preview scope.
- 16 MiB total input text and generated field bytes, 100,000 rule comparisons/explanations, and 1,000,000 aggregate callouts per preview scope.

All selected inputs are validated before producing a result. Cancellation checks run during validation and matching. Invalid patterns, invalid input, exhausted resources, cancellation, invalid templates and dependency/internal failures are distinguished from a valid non-match. No partial result is returned when the chosen scope fails.

Limits must be measured and surfaced by the eventual editor/application workflow; passing small engine tests is not a claim of representative-workload responsiveness. The installed PCRE2 library is not instrumented by SpentIt's sanitizer flag. Packaging and other-platform verification remain open.

## Verification

Standalone Debug, Release and ASan/UBSan each passed all 16 registered engine/parser/regex/evaluation/application-preview cases. Coverage includes exact financial preservation, aggregate-allocation bounds, validation cancellation, manual blanks, obsolete rule results, source-association reorder/forgery and explicit replacement. Final builds and sanitizer logs were clean. Schema 6 Save and Apply have a separate Debug `rulestore.*` run, recorded in [project status](status.md). That run is not Release or sanitizer evidence, and no UI rule behavior has been verified.

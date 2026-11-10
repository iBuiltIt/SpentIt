# Bounded rules evaluation

Last updated: 2026-11-09

Category-gated evaluation is the implementation. Formats are **70 with rules /
69 without**. Current-value matching is not implemented: matching still searches
immutable original fields. See [feature 004](../features/004-rules-and-explanations.md).

## Matching and derived fields

The optional `SpentIt::rules` target uses [PCRE2 10.49](../third_party/pcre2/README.md) through a narrow adapter. Matching searches immutable original fields, using UTF-8, Unicode properties and case-insensitive defaults. Explicit pattern flags can request case-sensitive matching; anchors select whole-field matching. Invalid UTF-8/NUL input is rejected without normalization. Backslash-C is forbidden to preserve valid capture boundaries. JIT is not used.

Templates contain literal text, `$$` for a dollar sign, and `${0}` through `${32}` for numeric captures. Zero means the complete match. References to nonexistent captures or malformed syntax fail. A valid optional capture that did not participate expands to empty text. Output is bounded before publication.

Source-split rules run first, then transaction rules, retaining list order within
each phase. Every rule assigns an explicit category. Only Uncategorized
allocations qualify; the first categorizing match wins. Transaction rules fill
remaining Uncategorized allocations in split transactions without changing their
shared payee/memo. There is no parent-category inheritance or forced replacement.
Selected-rule evaluation reserves earlier matches using saved surrounding rules,
but exports only the selected rule's changes. Preview counts partition matching
transactions into categorized, earlier-rule exclusions and eligible rows.

The engine receives explicit retained source associations. Reordering current splits does not change which source memo a rule sees. New manual splits have no source association and are excluded from source-split matching. The interface contains no monetary amounts or financial dates, so it cannot change money, dates or split structure.

Reapplication preserves existing categories and manual text. Deliberately clearing
a category permits another rule assignment. Each applied field records its rule
identity/version; exact amounts, dates, split identities and unchanged category
bindings remain intact. Disabled or nonmatching rules never undo earlier work.
The storage adapter validates transitions and retains atomic receipts and Undo.

## Resource and error contract

The current engineering limits are explicit rejection boundaries, not truncation:

- 4 KiB pattern, 64 KiB source field/output, 32 captures, 64 nested pattern parentheses.
- Per-pattern 1 MiB compiled code and 8 MiB allocator budget; per-match 8 MiB allocator budget, 4 MiB match heap, depth 128 and 100,000 matching/callout steps.
- At most 128 rules and 16 MiB total compiled code in a ruleset.
- A single evaluator call permits 4,096 records, 4,096 allocations per record and 16,384 aggregate current/source allocations. Account/pending previews stream per transaction; the legacy committed reapply API retains its 4,096-row scope limit.
- 16 MiB total input text and generated field bytes, 100,000 rule comparisons/explanations, and 1,000,000 aggregate callouts per preview scope.

All selected inputs are validated before producing a result. Cancellation checks run during validation and matching. Invalid patterns, invalid input, exhausted resources, cancellation, invalid templates and dependency/internal failures are distinguished from a valid non-match. No partial result is returned when the chosen scope fails.

The rules page prints these published limits. Printing them is not a measurement of representative-workload responsiveness. The installed PCRE2 library is not instrumented by SpentIt's sanitizer flag. Packaging and other-platform verification remain open.

## Verification

Standalone Debug, Release and ASan/UBSan each passed all 16 registered engine/parser/regex/evaluation/application-preview cases. Coverage includes exact financial preservation, aggregate-allocation bounds, validation cancellation, manual blanks, obsolete rule results, source-association reorder/forgery and explicit replacement. Final builds and sanitizer logs were clean. Schema 6 Save and Apply have a separate Debug `rulestore.*` run, recorded in [project status](status.md). The rules sheet has a separate Debug `import.rules_ui` run, also recorded there. Neither run is Release or sanitizer evidence.

The historical Debug-only persistence/UI results above are superseded by the 2026-10-26 72/72 Release and ASan/UBSan baseline runs in [project status](status.md). The installed PCRE2 library itself remains uninstrumented.

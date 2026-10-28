# 004 — Rules and explanations

Status: **active**  
Last updated: 2026-10-27  
Implementation: bounded evaluator, preview adapter, durable Save/Preview/Apply and rules sheet implemented. The schema-6 baseline passed all 72 tests in Release and ASan/UBSan on 2026-10-26. Current rules-enabled documents use experimental schema 20; rules-off documents use schema 21. Schema 8 was an earlier account-scoped checkpoint and is rejected without migration. See [status and verification](../doc/status.md), [engineering contract](../doc/rules-engine.md), and [durable rules](../doc/durable-rules.md). Bulk committed rule reapplication remains deferred.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded catalog and account transfer destinations are implemented. Category reports remain unimplemented.

- Regex matching is a core requirement.
- Rules are displayed and edited in a sheet-style interface.
- Rules can normalize displayed payees, assign categories, and populate memos from source data.
- Rules address transaction and split fields, including split memos. Split classification can use the parent payee's classification.
- Show rule collisions/overlaps and their consequences; ordering may resolve intentional overlaps.
- A transaction with a matching rule offers rule editing; an unmatched transaction offers rule creation. Rule editing has explicit commit/discard controls.
- Preserve original import fields and explain the rules responsible for derived values.

## Proposed design

Use PCRE2 directly where needed to control execution limits. A rule includes identity, enabled state, ordered position, scope, input field, regex/options and actions. The accepted bounded policy below replaces configurable continuation with first assignment per field. Record the applied rule version and field contributions so explanations remain meaningful after later rule edits.

Evaluate matches against defined source fields by default. Make parent-category inheritance explicit; do not replace source split memos or recursively rerun arbitrary rules. Specify phases/dependencies before implementation. Capture replacement, case sensitivity, Unicode semantics, and empty-field behavior require explicit contracts.

Preview changes before applying edited rules. Do not silently recategorize committed history. When several rules contributed, show each rule and its changes rather than pretending one rule explains the entire row. Preserve manual overrides unless the user explicitly chooses to replace them.

Classify feedback separately: multiple rules matching can be compatible; competing assignments to the same field can be a conflict; later assignments can be shadowed by an earlier field winner. Evaluate concrete overlaps on existing transactions and test cases. Do not claim exhaustive overlap detection for arbitrary PCRE2 expressions or unseen inputs.

Example: a payee matching `Tfl Travel.*` yields Transport; a split memo matching `Wise Charges for:.*` yields Service Fees; a split matching `CARD-.*` explicitly inherits Transport from its parent's classification.

Bound pattern/input size and matching resources, provide cancellation, and keep evaluation off the UI thread. A compilation error or resource-limit failure is a visible error, not a non-match or permission to commit partially classified results silently.

## Acceptance criteria

- Transaction-field and split-field rules produce deterministic, explainable results on shared fixtures.
- The synthetic Wise-shaped import allocates its fee and transport amount correctly without source mutation.
- Invalid patterns and resource exhaustion are surfaced without hanging the UI.
- Reordering rules previews affected transactions and competing field assignments.
- Compatible overlaps, conflicting writes, and shadowed rules have distinct feedback.
- Rule edit/discard leaves existing rules/results unchanged when cancelled; reapplication is explicit and reviewable.
- Historical explanations remain valid after rules change or are deleted.

## Open questions

- Per-field first assignment and fixed phases are accepted below; no configurable stop/continue is planned initially.
- Implement explicit parent classification in the fixed transaction-then-split phases without recursive dependencies.
- Regex flags, replacement syntax, rule versioning, manual-override semantics, and transaction/split selection in the editor.
- Rule Save is accepted as document-global and independent of draft commit/discard; implement and verify that separation.

## Reference

- [PCRE2 API and resource controls](https://www.pcre.org/current/doc/html/pcre2api.html)

## Accepted bounded evaluation policy — 2026-10-25

Run fixed transaction-then-split phases against immutable imported fields; do not recursively match normalized outputs. Within a phase, the first matching rule assigning a field wins, while later rules may still assign other fields. Show compatible overlaps, competing assignments and shadowed assignments distinctly. Manual field edits, including deliberate blanks, remain protected unless the user explicitly replaces them in a preview.

Saving rules durably versions document-wide definitions independently of import state. Discarding an import does not discard saved rules. Preview and Apply are separate: apply the complete chosen pending-record scope atomically, preserving field provenance and previous values. Reapplication recomputes rule-owned fields from source facts while retaining manual fields, and previews removal of obsolete derived results as well as new assignments. Existing committed records never change automatically; bulk committed reapplication is deferred.

Initial actions change displayed payees, memos and assignable category identities only. They do not change financial dates, amounts, split structure or transfer relationships. Matching defaults to case-insensitive UTF-8 regex search with explicit anchors for whole-field matching. Invalid expressions, resource failures and oversized results are visible operation errors. Capture-template syntax and measured resource limits are engineering contracts to specify before implementation.

For the Wise-shaped example, source payee `^Tfl Travel.*` supplies display payee TfL and parent classification Transport. Source split memo `^Wise Charges for:` assigns Service Fees; `^CARD-` explicitly inherits the parent classification. The -29.31 total remains exactly -0.14 plus -29.17; parent classification never creates another allocation. A single-allocation transaction's category action targets that allocation. Missing parent classification leaves inheritance unapplied with an explanation.

Source-split rules require stable retained source-allocation associations, not current ordinal guesses. Manually created splits are excluded from source-split matching initially and retain manual classification. Catalog lifecycle and starter names remain governed separately by feature 010. The owner accepted this bounded policy on 2026-10-25. Concrete capture-template syntax, resource limits and persistence contracts remain engineering work.

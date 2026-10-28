# 004 — Rules and explanations

Status: **active**  
Last updated: 2026-10-28  
Implementation: bounded evaluator, preview adapter, durable Save/Preview/Apply, rules sheet, and selected-account committed reapplication are implemented. The schema-6 baseline passed all 72 tests in Release and ASan/UBSan on 2026-10-26. Current rules-enabled documents use experimental schema 22; rules-off documents use schema 21. Schema 20 and schema 8 are rejected without migration. See [status and verification](../doc/status.md#point-7-committed-rule-reapplication--2026-10-27), [engineering contract](../doc/rules-engine.md), and [durable rules](../doc/durable-rules.md). Feature 004 stays active. Starter-catalog spelling and the Auto/Medical hierarchy stay open. Reports are [feature 006](006-reporting-drill-down.md).

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

Source-split rules require stable retained source-allocation associations, not current ordinal guesses. Manually created splits are excluded from source-split matching initially and retain manual classification. Catalog lifecycle and starter names remain governed separately by feature 010. The owner accepted this bounded policy on 2026-10-25. Concrete capture-template syntax, resource limits and persistence contracts remain engineering work. The deferred bulk reapplication in the paragraph above is the explicit selected-account command in the next section. That 2026-10-25 sentence is the decision as it stood then.

## Accepted committed reapplication — 2026-10-27

The owner said Go for point 7 and declined a further scope question. This command is the committed reapplication that the 2026-10-25 policy left deferred. It does not mark feature 004 landed.

- The user reapply runs on the selected account's active committed entry heads. Drafts, removed rows, rows in other accounts, and generated `~transfer-` rows stay out of the evaluation.
- One preview, one Apply, and one group Undo. Cancel writes nothing. A zero-change preview writes nothing and does not consume the operation id. An id that already belongs to another command fails even when the fresh preview changes nothing.
- Manual fields, including blanks, stay unless Replace manual was part of that preview. Amounts, dates, split structure, evidence bytes, and transfer links stay. The account balance does not change.
- When a rule would change the category text or binding of a live transfer-linked allocation, storage leaves the category, binding, owner, and rule mark as they were and the preview names the skip. The link stays. A payee change on that row can still be written. Owner or mark changes alone do not end a link.
- Pending Apply still refuses a committed batch. Save publishes rules and does not change rows. Rules do not create transfers, allocations, money, or dates.
- Rules documents are schema 22. Rules-off documents stay schema 21, and preview and Apply fail with `Rules require a new experimental dataset`. Schema 20 is rejected with `Unsupported experimental format; no migration was attempted`. There is no migration. Application id remains `1397770577`.
- The category picker, Uncategorized filter, and per-row explanations already existed and were not rebuilt. Starter-catalog spelling and the Auto/Medical hierarchy stay open.
- A preview of more than 4096 active committed rows is refused. That 4097-row case was not executed. The refusal of a reapply that would change committed money is implemented and was not executed.

Verification is the selected macOS run in [status](../doc/status.md#point-7-committed-rule-reapplication--2026-10-27). This checkpoint is committed as `b6b9346`.

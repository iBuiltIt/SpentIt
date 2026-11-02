# 004 — Rules and explanations

Status: **active**  
Last updated: 2026-11-01  
Implementation: bounded evaluator, preview adapter, durable Save/Preview/Apply, rules sheet, and selected-account committed reapplication are implemented. The schema-6 baseline passed all 72 tests in Release and ASan/UBSan on 2026-10-26. Current rules-enabled documents use experimental schema 40; rules-off documents use schema 39. Schemas 22/21, schema 20 and schema 8 are rejected without migration. See [status and verification](../doc/status.md#point-7-committed-rule-reapplication--2026-10-27), [engineering contract](../doc/rules-engine.md), and [durable rules](../doc/durable-rules.md). Feature 004 stays active. Starter names and the independent category model were subsequently accepted on 2026-10-28 in feature 010. Reports are [feature 006](006-reporting-drill-down.md).

## Simple generated patterns and regex key — owner Go 2026-11-01

Accepted and implemented; verification is in [status](../doc/status.md). Generated
literal rules use `^text$` without an unnecessary non-capturing group. Plain source
text stays plain; source text containing regex metacharacters uses `\Q…\E`,
including safe handling of a literal `\E` within the source. Existing stored
expressions are not rewritten on load or merely by saving other changes.

Whole field handles simple anchored patterns and the older grouped form. Toggling
it preserves quoted text, escaped symbols, character classes, captures and the
editor's case-sensitive prefix. New grouping is needed only for outer alternatives
when adding anchors. The helper does not replace regex validation or override the
expression's matching behavior. Incomplete quoted/grouped/class syntax is left
for the user to complete before toggling.

A compact key immediately beneath Pattern explains `^`, `$`, `.`, `.*`, `*`, `+`
and `\Q…\E`. It uses one line when wide enough and two on narrower layouts,
with the quantifiers described as repetitions of the preceding item.

## Rule scope and account Apply — owner Go 2026-11-01

Accepted and implemented, superseding the separate read-only single-rule preview,
current-import Apply and accepted-account Reapply UI below. Verification and
remaining limits are recorded in [status](../doc/status.md).

- The Rules screen and focused editor offer **All transactions in this account**
  (default) and **New transactions only**. New means not yet committed; it covers
  eligible transactions across all imports in that account, not just one batch.
  Committed transactions remain eligible for categorisation. Import-time rules
  retain their incoming-transaction scope.
- Preview this rule and Preview all enabled rules both use the chosen scope.
  Apply uses the same saved rule identity or enabled ruleset, account, scope and
  manual replacement choice. Changing the editor, scope or replacement choice
  requires another preview. Other dataset changes invalidate the prepared request.
- After preview, **Apply to this account now** is available on either screen.
  Unsaved definitions offer **Save and apply**: Save completes durably first,
  followed by a fresh preview of those saved definitions and one Apply. Save and
  Apply are separate durable operations; an Apply failure does not unsave the
  definitions. No transaction Commit is performed by either action.
- Matches remain a count and one Date / Imported payee / Amount row per matching
  transaction. Manual values are protected unless Replace is explicitly checked;
  live transfer categories, money, dates and allocations remain unchanged.
- One atomic change journal covers matching changed new and committed rows,
  retaining rule attribution and committed history. Cancellation before the final
  commit gate and any failure before durable completion leave all transaction
  changes unapplied. Retry checks the durable request rather than repeating a
  saved operation. No evaluation chunk commits separately.
- **Undo rule application** restores the whole operation, including mixed new and
  committed rows. It is available while that application is the current journal
  operation for this account; later dataset mutations end that Undo sequence.
  Reopening does not discard the durable journal. Individual committed members
  cannot be undone separately. The separate Reapply entry button is removed;
  historical reapply-group Undo remains supported.
- Preview streams the existing eligible ledger membership, with per-transaction
  evaluator bounds and 128 MiB retained preview payload accounting. Apply retains
  only changed snapshots, with 512 MiB payload accounting and cancellation. The
  old 4,096-row accepted-reapply limit does not constrain this account Apply path.
  Source-less generated counterparts are excluded. Invalid/exhausted evaluations
  fail explicitly without truncated success.

The existing snapshot journal tables are reused; no format change is introduced.
The legacy pending Apply and committed Reapply storage APIs remain for historical
receipts, importer/internal callers and verification. They are no longer competing
choices on the normal Rules screen.

## Match-list preview and non-reverting Apply — owner Go 2026-11-01

Accepted follow-up; implementation and verification are recorded in
[status](../doc/status.md). This supersedes earlier full-recomputation/removal
behavior for the Rules workspace and accepted-account reapply.

- Preview lists only transactions matched by enabled rules, once per transaction,
  with Date, Imported payee and Amount. Multiple matching fields, splits or rules
  do not multiply rows. Show a match count or “No matches.” Do not display
  field-by-field explanations, unchanged nonmatches or before/after prose.
- A match is not a claim that a transaction will change. Manual protection and
  transfer protection remain in force, including matches with no writable fields.
- Preview this rule remains read-only, evaluates only the open definition, and
  includes accepted and new imported transactions in the selected account.
  Preview all enabled rules retains its current pending-import scope; applying
  accepted transactions remains an explicit separate reapply operation.
- Save remains separate from Preview and Apply. Enabled boxes modify saved rule
  settings; an unsaved full preview explains that saving and previewing again is
  required before Apply. An unsuccessful preview must not leave an old Apply
  prepared. Matching lists do not include protected source-less counterparts.
- Apply changes only fields assigned by an enabled matching rule. Disabling or
  deleting a rule, or editing its pattern so that it no longer matches, must not
  by itself reverse that rule's earlier values or attribution. Fields untouched
  by matching actions retain their exact values, bindings and ownership. An
  enabled matching action may still replace an earlier rule-owned value; manual
  values require explicit Replace. Ordering and transaction/split phases remain.
- Rule compilation happens once per preview/Apply. Account and pending previews
  stream transaction snapshots through bounded per-transaction evaluation; the
  old 4,096-account-row ceiling is removed for those previews and pending Apply.
  Per-record text/allocation/regex limits and cancellation remain. Failure or
  cancellation returns no partial preview. Compact matched rows are retained for
  display; a fixed-height virtualized list creates only visible delegates.
- Pending Apply prepares changed transactions before one durable transaction;
  it rechecks batch, ruleset and category revisions at the commit gate, retains
  atomicity and retry receipts, and never commits evaluation chunks separately.
  Memory for atomic Apply grows with changed transactions. Compact preview result
  memory grows with matches; source/draft snapshots are evaluated one at a time.
  Retained payload accounting is capped at 128 MiB for preview results and
  512 MiB for pending Apply preparation. Exceeding a budget fails visibly before
  writes; results are never silently truncated. These are payload budgets, not
  process RSS bounds.
- The separate accepted-account reapply keeps its existing 4,096-row limit and
  group Undo contract. Its evaluation also preserves fields untouched by enabled
  matching actions. This Go does not authorize automatic accepted-row changes.

No schema migration or historical receipt reinterpretation is introduced. Earlier
financial history remains readable and independently verified.

## Transaction rule editor — owner Go 2026-11-01

Accepted and implemented; verification is recorded in [status](../doc/status.md).
This supersedes the earlier editor layout where they differ.

- One transaction action offers Create rule, Edit rule, or Edit rules. Retained
  field assignments take precedence over today's matching rules. Multiple rules
  open a chooser with category assignments first and the affected fields shown.
  Deleted responsible rules are identified; creation remains available when none
  of the responsible rules can be edited. Editing opens the current definition,
  not an old applied version. A manual replacement ends that field's current
  rule ownership; persistent history is unchanged.
- Creation uses the original imported payee as the exact matching pattern and
  initial name, and the current ordinary category as the action. Multiple splits
  require an explicit split choice; the dialog identifies that the resulting rule
  matches the imported split memo. Split-memo creation also remains available. Transfer account names
  are never copied into rule category actions.
- Transaction entry opens a focused editor with Rule name, Match, Pattern and
  Changes labels, an editable category completion control, and Enabled control.
  All rules returns to ordered rules management. Save retains the open editor;
  Save changes definitions only.
- Preview this rule evaluates only that editor definition, before or after Save,
  against the current account's imported ledger transactions, including accepted
  and new rows. Disabled rules do not execute. Only matching rows are displayed.
  Manual protection and transfer protection still apply. Values not assigned by
  the tested rule remain unchanged, including other rules' assignments. This
  preview is read-only and never prepares Apply.
- Preview all enabled rules remains a separate management action using the
  existing pending-import scope and full ordered ruleset semantics. The Enabled
  checkbox controls rule execution, not a temporary preview selection. Saved
  full previews and the separate accepted-account reapply retain their existing
  atomic Apply contracts.
- Transfer suggestion exclusions are managed from Review, not above the rule
  editor. No matching policy, durable schema or financial write contract changes.

The later match-list follow-up above removes the 4,096-account-row preview limit
while retaining per-transaction evaluator limits.
Generated transfer counterparts have no imported source and are not evaluated.
Single-rule testing does not model interactions with other rules; use the full
preview for ordering and overlap checks. Native accessibility and other-platform
verification are not implied by synthetic Qt checks.

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
- The category picker, Uncategorized filter, and per-row explanations already existed and were not rebuilt. Starter names and the independent category model were subsequently accepted on 2026-10-28 in feature 010.
- A preview of more than 4096 active committed rows is refused. That 4097-row case was not executed. The refusal of a reapply that would change committed money is implemented and was not executed.

Verification is the selected macOS run in [status](../doc/status.md#point-7-committed-rule-reapplication--2026-10-27). This checkpoint is committed as `b6b9346`.

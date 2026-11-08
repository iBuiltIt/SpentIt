# 004 — Rules and explanations

## Rules workspace usability — owner Go 2026-11-08

Implemented presentation cleanup; verification is in [status](../doc/status.md).
This supersedes older placement and button-label descriptions, not rule policy.

- Empty Rules shows No rules yet and New rule. Search appears with rules;
  Removed rules appears only with saved deleted definitions. One new draft remains
  the limit; New rule and Copy into new rule hide while it exists.
- The ordered list retains row Enabled and applicable Up/Down controls. Selected
  rules remain visible outside search results, with explicit feedback when the
  search matches nothing or the selected rule is outside the results.
- The selected definition has Save rule / Discard changes (Discard draft for new
  rules) beneath its fields, visible only with changes. Remove rule stages removal;
  the summary then offers Save removal / Keep rule. Removing a definition does not
  reverse transaction changes. Removed definitions show readable scope, pattern
  and actions and can be copied into a new identity.
- Enabled selected rules expose a separate Apply section naming the rule. An
  account picker stays inside Rules, preserving selection, search and independent
  drafts. Account changes invalidate preview output and update navigation context.
  Transactions selects All transactions (default) or New transactions only, with
  explicit Uncategorized eligibility. Preview is optional; the sole application
  button is Apply rule or Save and apply rule. No all-rules action is introduced.
- Preview identifies rule, account and scope, retains reconciled exclusion/eligible/
  change counts and the eligible row list. Undo application appears only when the
  existing account operation is undoable, in a separate strip naming the account;
  it is independent of current rule selection. Temporarily busy controls stay
  present but unavailable, rather than becoming empty-state instructions.
- The movable leave dialog lists changed rule names and New / Edited / Remove
  states in a bounded scroll area. Save all changes and Discard all changes retain
  their existing all-drafts semantics. Cancel, Escape and window close stay in
  Rules. Saving validates all changes atomically and never applies transactions.

No matching, category eligibility, ordering, manual/transfer protection, financial,
retention or format change. Current-value matching remains separately gated.
Manual prose and pictures are deferred under MAN-015 in the
[manual backlog](../doc/manual-update-backlog.md#man-015--rules-workspace-usability).

## Current-value matching — accepted direction 2026-11-08

The owner accepted current transaction/split values as ordinary matching inputs
for both manual and imported entries, preserving category gating, ordering,
manual text protection and the prohibition on rule-created transfers. Explicit
imported-original conditions are deferred. Implement before first release with
clear previews and an experimental-format boundary if necessary to prevent
silent reinterpretation. This is not implemented or implementation-authorized
by the planning decision. See [release scope and sequence](../doc/first-release-plan.md).
The original/current distinction described in older implementation notes remains
actual behavior until the separately authorized change is verified.

Status: **active**  
Last updated: 2026-11-08  
Implementation: bounded evaluator, preview adapter, durable Save/Preview/Apply, rules sheet, and selected-account committed reapplication are implemented. The schema-6 baseline passed all 72 tests in Release and ASan/UBSan on 2026-10-26. Current rules-enabled documents use experimental schema 68; rules-off documents use schema 67. Schemas 22/21, schema 20 and schema 8 are rejected without migration. See [status and verification](../doc/status.md#point-7-committed-rule-reapplication--2026-10-27), [engineering contract](../doc/rules-engine.md), and [durable rules](../doc/durable-rules.md). Feature 004 stays active. Starter names and the independent category model were subsequently accepted on 2026-10-28 in feature 010. Reports are [feature 006](006-reporting-drill-down.md).

## Shared category completion — owner Go 2026-11-02

The rule editor and ledger use one shared category-completion field. On first
focus, select the text so typing replaces it. Subsequent clicks and dragging
support normal text selection. Typing keeps focus in the field and immediately
filters the open list by case-insensitive substring, including aliases. Up/Down
moves the highlight; Enter/Tab or a mouse click accepts it. Escape cancels the
incomplete selection. Category names, including ampersands, display literally.

Callers retain their own eligible choices and assignment commands. Rules choose
an existing ordinary category and update only the local rule draft; no account
transfer or Uncategorized choice is added. Save still validates the category.
The ledger retains Uncategorized, eligible account destinations, immediate
assignment, split navigation and session Undo. The shared presentation component
does not own financial commands. See [status](../doc/status.md) for verification.


## Next implementation: category-gated rules and imported categories — 2026-11-02

**State: owner Go implemented; verification recorded in [status](../doc/status.md).**
The split-first policy and import-wide decision below supersede the earlier
per-field evaluator and isolated selected-rule Preview/Apply. Formats 42/41
reject earlier experimental files without migration.

### Category-resolution performance follow-up — owner Go 2026-11-04

Apply selected and automatic completion now use the existing bounded
operation-scoped statement and account-alias caches. This avoids reconstructing
account aliases (including reparsing grouped source files) for each allocation.
Existing alias-signature invalidation, category gates, transfer protection,
transaction boundaries and rollback behavior remain authoritative. Diagnostics
identify this command and separate category assignment from automatic rules.
See [status](../doc/status.md) for synthetic measurements and failure verification.

### Why this changes

With enabled rules in this order:

1. `INDIGO PARK.*` → category Auto: Parking.
2. `INDIGO .*` → category Books and payee INDIGO BOOKS.

The previous per-field winner policy could turn an imported INDIGO PARK transaction
into payee INDIGO BOOKS with category Auto: Parking. The owner rejected that
mixed result. Earlier discussion proposed unconditional first-match stopping;
the final direction instead makes categorization the eligibility/stopping gate.
Do not implement the intermediate forced-run proposal.

### Accepted rules policy

- A rule must assign an explicit category. Optional payee/memo actions may
  accompany it, but payee-only or memo-only rules cannot be previewed, saved or
  applied. Validate below the UI as well as in the editor.
- Remove the split **Use transaction category** action. An inherited category is
  not a substitute for the required explicit category.
- Rules run after import on the new transactions and on explicit
  application from the Rules panel, including Save and apply. Saving a definition
  alone must retain its established save-only meaning.
- Rules operate only on Uncategorized items. Once a category is assigned, later
  rules must not rename the payee, change the memo or supply another category.
  For ordinary unsplit transactions, the first matching enabled categorizing rule
  finishes rule processing for that transaction.
- There is no force/replace-already-categorized mode. To recategorize, the user
  searches in the ledger (including regex searches), deliberately clears the
  relevant categories with the existing bulk assignment command, then runs rules.
  Preserve the atomic assignment and Undo contracts; clearing a category is not
  permission to clear money, payee, memo or transfer relationships.
- Retain New transactions only / All transactions in this account as independent
  scope choices. Both scopes remain restricted to Uncategorized items. Accepted
  transactions are not exempt from the rule gate and do not require automatic
  acceptance or a special force option.
- Preserve the one-screen rule editor, independent selected-definition saves,
  pending local drafts, optional visible Preview, group application Undo, and
  movable Save / Discard / Stay navigation confirmation. Do not restore the All rules
  button or the editor's redundant Enabled checkbox.
- Pre-release datasets are disposable and none need legacy support. Do not add
  migration, repair, grandfathering, disabled-old-rule UI or special handling for
  saved rules without a category. Reject incompatible experimental formats if a
  format change is necessary; do not silently convert data.

### Selected-rule preview and Apply

The owner wants to distinguish a correct pattern from a rule that gets no work
because earlier rules already handle its matches. The selected rule must be
evaluated in ordered context, not as an isolated override.

The requested explanation is, for example:

> 20 transactions match this pattern. 16 are handled by earlier rules.
> 4 will be processed by this rule.

List the four remaining eligible transactions. This supersedes the earlier idea
of listing every blocked match with an annotation. Preserve a clear distinction
between raw pattern matches, already-categorized exclusions, matches handled by
earlier enabled rules, and rows actually changed; counts must reconcile. Manual
or transfer protections can mean an eligible match does not produce a change.

Either inspect earlier matches after finding this rule's matches or evaluate an
ordered read-only context; the algorithm must preserve those explanatory counts.
Do not apply earlier rules' other actions as a side effect of applying just the
selected rule. Preview and Apply must agree, including when Preview is skipped.
Use saved surrounding definitions plus the selected edit, consistent with the
independent-save contract; unrelated unfinished drafts must not block or silently
alter an application. The selected edit replaces its saved definition in saved order (a new rule appends);
unrelated draft order/enabled changes do not alter its context. Selected Save
retains the established independent-save behavior.

### Imported category resolution

Imported categories matter. Existing recognized ordinary categories are used;
references to ordinary category names absent from the dataset must be collected
and presented for an explicit import decision:

- **Create categories:** create the missing categories and use them for their
  imported assignments.
- **Use Uncategorized:** clear those effective assignments to Uncategorized,
  making them eligible for rules. Retain the original imported text in the
  existing source representation; do not rewrite imported bytes.

The accepted consolidated checklist lists category names once; affected counts
remain a possible later addition.
Resolve the choice before automatic categorization can misclassify these rows.
Do not treat an unresolved imported label as an intentional assigned category or
silently discard it before the user chooses. Account aliases and explicit
transfer destinations follow their transfer-resolution workflow, not this prompt.

Integrate with the existing complete-QIF-catalog staging/publication, multi-account
mapping, retained new transactions, and incomplete-import cleanup contracts.
Do not introduce a second permanent source archive. The settled import-wide checklist and cancellation/restart behavior are below.

### Settled split and import decisions — owner Go 2026-11-02

Implementation is authorized. Split rules run first in their displayed order,
independently on each Uncategorized source split. Transaction rules then run in
their displayed order and fill only remaining Uncategorized allocations. The first
categorizing match wins per allocation. On split transactions the transaction
phase changes categories only, never the shared payee/memo. On unsplit transactions
optional payee/memo actions accompany the category; manual text stays protected.
Split memo actions affect only their own allocation. Existing transfer protection
and explicit split-selection requirements for ledger clearing remain.

The missing-category decision is import-wide after all accounts/categories are
known. Show each missing ordinary category once in a scrollable checklist,
initially checked, with All / None. Apply Selected creates and assigns checked
categories; unchecked assignments become Uncategorized. Use Uncategorized creates
none. Recognized categories retain their assignments; transfers use their own
resolution workflow. Closing leaves a durable pending decision across restart,
and rules wait until resolution. Explicit Abort uses import-owned cleanup.
Original imported text remains unchanged. Pending choices must be distinguished
from interrupted preparation, which still receives automatic cleanup.

### Next-session execution and verification

1. Read this handoff, current status, category/import contracts and implementation.
   Preserve the already-built attribution UI follow-up included in this checkpoint.
2. Use the settled split-first policy and import-wide checklist below.
3. Implement consistent validation and eligibility in engine/application/storage,
   import, preview and Apply. Keep heavy evaluation on the worker, exact money,
   protections, cancellation, atomicity, durable history and retry behavior.
4. Update UI labels/counts and remove obsolete force/inherit-category options.
   Preserve the independently saved selected-rule workflow.
5. Verify with synthetic fixtures:
   - overlapping INDIGO-style patterns cannot combine a later payee with an
     earlier category; the first categorizing rule wins;
   - all no-category rule forms are rejected before Preview/Save/Apply;
   - categorized imported/manual/rule-owned items stay unchanged;
   - explicit ledger clearing permits rerun in both new-only and all-account
     scopes, without changing acceptance, balances or unrelated fields;
   - raw/prior-rule/already-categorized/eligible/changed preview counts reconcile,
     including zero remaining matches and direct Apply without Preview;
   - selected-rule Apply respects earlier rules without saving unrelated drafts
     or applying unrelated actions; scope/order changes invalidate preparations;
   - agreed split cases, transfer protection and manual protections;
   - missing-category Create/Use Uncategorized, existing categories, duplicates,
     shared/multi-account sources, cancellation, restart, retry and import cleanup;
   - atomic failure/rollback and Undo across new and accepted transactions,
     reopen verification, and appropriate rules-off behavior.
6. Record actual checks and platform limits. Record current runtime results in status; reports and other unrelated work
   remain deferred.

See [the next-work roadmap](../doc/roadmap.md#next-session-category-gated-rules--2026-11-02),
[category model](010-categories-and-selection.md),
[import mapping/catalog lifecycle](../doc/qif-account-imports.md),
and [import cleanup](../doc/ui-import-cleanup.md).

## Actionable ledger rule attribution — owner Go 2026-11-01

The ledger's Create/Edit rule button sits at the left edge of its row. Beside
Edit rule, name the actual assigned field and the editable rule, for example
“Category assigned by ‘Books’”. Distinguish split numbers for multi-split rows.
Do not display internal revisions or generic “Assigned by” text. Unnamed rules
use “Unnamed rule”, not an identity. Only available rules contribute this
field-specific text; the existing deleted-rule explanation and Create fallback
remain. This changes presentation only. See [status](../doc/status.md).

## Single rules workspace and independent edits — owner Go 2026-11-01

Accepted and implemented under the owner's follow-up Go. This supersedes the
focused/all-rules presentation, mandatory visible preview before Apply, and
whole-editor Save behavior described below. Storage still uses atomic full-image
ruleset requests; the UI constructs those requests from saved definitions plus
only the selected rule's change.

- One screen contains search (with inline clear), the ordered rule list and the
  expanded selected editor. There is no Categorisation/New rule/Edit rule
  subheading or All rules switch. Search matches names, patterns and action
  summaries; the selected row stays visible even if it does not match.
- One new unsaved rule may exist at a time. It is appended at the bottom, selected
  and brought into view with its editor. New rule and Copy into new rule are
  unavailable until it is saved or discarded. Transaction-created drafts use the
  same workspace and limit.
- Changed and new rows have a pale-yellow background; the new draft is labelled
  New. Selection has its own outline/highlight. Selecting another rule retains
  outstanding edits. Pending deletion is marked rather than silently saved.
- Save and Discard beside the editor concern only the selected rule. A selected
  save preserves unrelated saved definitions and outstanding local drafts. Saving
  the new rule removes its New state and permits another new draft.
- Preview this rule remains optional. Save and apply saves only the selected
  definition, then internally prepares a fresh validated single-rule account
  application. Apply to this account now similarly prepares and applies the saved
  selected rule directly. The chosen account scope and manual replacement policy
  remain explicit; money, transfer protection and group Undo retain the existing
  contracts. Saving and applying remain separate durable operations.
- Preview all enabled rules and the redundant editor Enabled checkbox are removed.
  Row Enabled controls retain document-wide rule semantics. Legacy all-rule
  preview/storage commands remain available internally. Preview results disappear
  when the selected rule or preview inputs change.
- Owner follow-up Go, 2026-11-02: leaving with outstanding edits opens a separate,
  movable, application-modal dialog titled “Save changes before leaving Rules?” with
  Save / Discard / Stay. Its text gives the number of changed rules. Keep the
  current rule, search and scroll position underneath; do not dim the workspace.
  The title bar allows moving the dialog aside; editing waits for an answer.
  Application modality avoids the attached macOS sheet presentation. Height follows
  the contents, with 20-pixel outer padding and a 16-pixel message/button gap.
  Escape and the window close control mean Stay while the prompt is active.
  Once dismissed, the hidden dialog accepts close requests so it cannot veto
  application shutdown. Final closure after the document audit is also accepted.
  Selecting another rule within
  Rules continues to retain local edits without prompting.
- Discard drops every remaining unsaved edit and new draft, retaining already
  saved rules and applied transaction changes. Save validates all pending
  definitions before one atomic ruleset save, without applying transactions.
  Validation failure dismisses the dialog, cancels navigation and reveals the
  invalid rule for correction, leaving saved definitions unchanged. Dialog
  actions and dismissal are unavailable while the save/discard is running.
  This supersedes the nonmodal leave bar and automatic selection on leaving.

Verification must include an incomplete new draft alongside an edited existing
rule, optional-preview Save and apply, persistence after reopening, discard-all
retaining prior saves/applications, invalid save-all atomicity, selection retention
on leaving, movable dialog placement, and Escape/close cancellation. See [status](../doc/status.md) for
actual results and platform limits.

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

- Owner follow-up Go, 2026-11-03: separate **Create rule…** and **Edit rule…**
  buttons treat Transaction (payee) and each imported Split (memo) as targets.
  Create offers targets without an editable applied rule; Edit offers targets with
  one. A sole choice opens directly; multiple choices show target labels, including
  split amount/memo. No choices disables the respective button. Individual split
  creation buttons are removed. If several rules contributed to one target, Edit
  lists each contributor with that target. Deleted rules allow creation again.
- Applied-version scope distinguishes a transaction rule that categorized a split
  from a split rule. Current matching alone does not occupy a target. Editing opens
  the current definition; retained attribution remains visible. Manual replacement
  ends that field's current rule ownership; history is unchanged.
- Transaction creation uses the original imported payee as the exact pattern/name,
  with transaction scope even on split transactions. On an unsplit transaction its
  ordinary category seeds the action; on split transactions the user chooses the
  category rather than copying one arbitrary split. Split creation uses the
  original split memo and that split's current ordinary category. Newly added
  splits without imported source fields are not creation targets. Transfer account
  names are never copied into rule category actions. Existing category eligibility
  and shared-text restrictions remain unchanged.
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


## Ruleset-save retention — owner Go 2026-11-08

Accepted scope: address measured ruleset-save duplication only, provided paired
measurements justify the bounded implementation. Other retention domains are
unchanged. Current rules and old definitions referenced by retained transactions,
including import-Undo restoration inputs, must survive successful close.

Implementation: save requests and ordered snapshots remain complete during a
session for lost-acknowledgement recovery and verification. Each changed save also
records a compact canonical request digest including expected revision, order,
identity, enabled state and every definition field. After successful close, exact
retries report already completed and require refreshing current data; changed
request-ID reuse is refused. No historical result is fabricated from current rules.

Close atomically removes full save requests and superseded snapshots, retaining
one current ruleset baseline. That baseline includes deleted identities and their
last definition versions so future sessions cannot revive them. Rule definitions
remain while referenced by heads, the baseline or any retained draft/entry field
or allocation; unreferenced intermediate definitions expire. Compact operation
identity/revision/order records remain for existing provenance relationships.
No per-save full-ruleset archive survives close.

Full verification validates baseline structure, retained definitions and provenance,
then replays only subsequent session saves, checking requests against their digests,
snapshots and current heads. Failed/cancelled closes and crashes preserve either
the old recoverable session or the complete published baseline. Encryption,
normal clean-marker behavior, backup audits and explicit checks are unchanged.

Experimental formats are **68 with rules / 67 without**; older formats are rejected
unchanged, without migration. See [status](../doc/status.md) for measurements,
executed fault/recovery tests and platform limits. Manual updates remain deferred.

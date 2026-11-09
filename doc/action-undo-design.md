# Unified transaction Undo and whole-import rollback

Last updated: 2026-11-09
State: **replacement implemented; see [verification record](action-undo-implementation.md)**.

Owner Go, 2026-11-08: after reading this replacement and confirming that it
replaces the rejected implementation approach, the owner authorized implementation
in the new session. This includes the visible scope choices below. The earlier
design-session stop remains historical; it does not block this new authorization.
No storage-budget increase, generic row capture, reviewer loop, final History
layout or manual publication is authorized by this Go.

Historical design-session instruction: the owner authorized design only and
requested that work stop on completion.
Implementation belongs to another agent in a new session under that session's
instructions. This document does not grant implementation Go or restart the old
agent/reviewer workflow. Runtime and storage results for the later authorized implementation are recorded
in the [implementation report](action-undo-implementation.md).

## 1. The model

One saved user operation is one Undo step, across the file. Use the existing
transaction snapshots and operation-specific restoration logic to reverse user
changes. Treat an import as one whole-import rollback using its source ownership.
**Importing a row must create no ordinary edit-Undo record for that row.**

Example:

| User operation | One Undo step does this |
| --- | --- |
| Import a file into three accounts | Removes that import's contributions across all three accounts and restores earlier relationships it changed. |
| Apply a rule to 12,000 transactions | Restores the actual changed transactions together, using their retained prior states. |
| Rename one payee | Restores that payee edit and any effects belonging to it. |
| Accept new transactions | Returns exactly that accepted set to New and restores the acceptance-related state. |

Undo traverses those steps newest first. Import occupies one position in this
order, but does not pass its inserted rows through the ordinary edit journal.
There is no Redo, selective skipping, or automatic undo-to-here.

Accounts and categories created by an import **remain after its rollback**, as
the owner explicitly accepted during this rethink. The proposed retained setup
also includes confirmed aliases and the account's explicitly chosen initial
opening, matching today's import rollback. Existing accounts' openings never
change merely because of import or import rollback. A retained new account with
a nonzero opening still has that opening afterward: this is financial-posting
rollback, not byte-for-byte restoration of the entire document.

A 50,000-row import requires storage and deletion work for 50,000 rows. It does
not require 350,008 extra inverse entries describing their insertion. Extra
restoration state is needed only where import changes pre-existing state that
cannot be recovered from ownership and already-retained versions.

## 2. Authority and deliberate scope choices

Read the [owner's rethink](action-undo-rethink-handoff.md),
[current status](status.md), [session contract](session-history-baseline.md),
[storage acceptance](session-storage-comparison.md) and
[current import rollback contract](import-undo.md).
The [rejected design](action-undo-rejected-design.md) and
[rejected inventory](action-undo-rejected-inventory.md) preserve the investigation;
their row-capture policies and implementation instructions do not apply.

Owner-established requirements are the rejection of generic import row capture,
whole-import atomic reversal, ordinary Undo for user transaction/entry changes,
permission to retain import-created accounts/categories, design-only work here,
and the existing financial, security, recovery and successful-close guarantees.
The earlier file-wide ordering and reversible acceptance requirements are retained.

The owner said to proceed with design after being asked about definition scope,
without separately specifying each definition family. The following are explicit
**replacement design choices**, not claims that every detail received separate
owner approval. They must be visible when the owner reviews this handoff:

- Standalone rule, report, category and account definitions and display settings
  are outside ordinary Undo. Saving them does not silently clear the chain.
- Opening corrections and currency reassignment remain financial actions in the
  chain because they change ledger money. Existing eligibility and exact conversion
  policy remain; no new currency capability is introduced.
- Reversing an eligible older import remains the existing destructive source-owned
  removal. Making that removal itself undoable is dropped from this design.
- Account-wide Discard retains its existing destructive scope and is not made
  undoable. Older-import removal and Discard require an empty active Undo chain.
- Undo of Save and apply restores the transaction application; the saved rule
  definition remains. The combined forward operation is atomic.
- Initial import decisions belong to the import workflow. It finishes or rolls
  back before unrelated saved commands or a successful close can proceed.

These choices remove the old requirement to retain a deleted import for undoing
its deletion, and remove the all-document-mutations conversion. If the owner
later wants either capability, design it explicitly; do not silently restore the
94-table journal under the word "unified".

## 3. What participates

| Operation | Treatment |
| --- | --- |
| Manual transaction creation; edit or repair of New/accepted entries; split edits; selected rename, restore or deletion | One action for the complete chosen operation. Retain affected transaction and relationship states, including originals/ownership when actually changed. |
| Ordinary category assignment; transfer assignment/release; explicit transfer changes | One action including all dependent postings and relationships, across accounts. |
| Review Merge, batch Merge, Link, separate/not-linked and amount-difference choices; suggestion exclusions/removal | One action restoring the decision, source associations and financial effects together. |
| Explicit rule application, including mixed New/accepted selections | One action for the changed set. Evaluated but unchanged rows add no Undo members. Automatic work during import belongs to Import. |
| Correct opening; currency reassignment | One financial account action using typed prior amounts, currency definitions and affected relationship state. |
| Accept | One action for its exact newly accepted membership and associated state changes. |
| Import | One source/group action plus necessary restoration references for pre-existing state. No per-new-row action membership or inverse capture. |
| Initial import mapping, category choices and automatic transfer/rule processing | Children of that import, with no independent Undo entries. |
| User repair, category/transfer resolution or edits after the import workflow has completed | New actions for the user changes actually made. Never reopen a completed Import root to absorb later edits. |
| Standalone definition/settings saves, Save all definitions, report Save and show | Not Undo steps. Preserve active Undo dependencies as described below. Showing/calculating a report is a read. |
| Older-import removal; account-wide Discard | Separate confirmed operations, allowed only with no active action and no preparing import. Not Undo steps. |
| Selection, navigation, filters, previews, Help, Check file, backup creation | No step; preserve the chain. |
| Document creation, close/switch, restore from backup, passwords and filesystem locks | Existing lifecycle boundaries; never reversed by transaction Undo. |

A semantic no-op adds no step, but retains retry protection where appropriate.
Children cannot commit independently or expose independent Undo entry points.
Keystrokes remain unsaved editor state; native text Undo continues to handle them.

### Definitions can coexist with transaction Undo

Undo restores transaction assignments by identity and saved provenance; it does
not restore the current names, Income flags, ordering or definitions of catalogs.
Category/account names displayed after Undo use the current definitions. Rule
Undo restores the recorded result's predecessor without rerunning current rules.
Retain referenced historical category/rule definitions while an active action
needs them, through the existing dependency-pruning mechanism.

Rule/report saves, category creation/rename/Income changes and display preferences
leave the head unchanged. They advance the freshness epoch, so already-prepared
commands must refresh. An Undo preview created afterward can still undo the head.
Archived categories may retain/restore their historical assignments; this narrow
restoration is not permission for new forward assignments to archived categories.

Definition deletion must check active Undo dependencies as well as current use.
Refuse deleting a category/account required by an active inverse. Account archive
must also refuse while an active action needs writable access to that account;
retain today's other archive restrictions. Explain the affected action and that
successful close ends session Undo. Do not silently discard history or allow the
save to strand a later Undo. Rule deletion can retain an older definition solely
for saved assignment provenance, as the current ruleset retention already permits.

The nonparticipating command tests must establish that these operations do not
rewrite transaction/relationship state. Any discovered such write needs an explicit
typed action treatment or a refusal before shipping; it cannot be called a harmless
settings change merely because that is its UI location.

## 4. One coordinator, existing domain restoration

Keep SQL in storage and use cases behind the application repository interface.
The serial worker submits one complete operation. Introduce a small durable action
header and one common history/Undo API; replace competing eligibility stacks.
Reuse `review_answers`, `review_members`, `review_sources`, `review_link_history`,
existing draft/entry snapshots and account/rule receipts where they already hold
the necessary state. Do not duplicate those bodies into a second generic journal.
The [operation inventory](action-undo-inventory.md) identifies adaptation points.

An action header needs: unique ID, session, parent, chronological sequence, kind,
state and a typed payload reference, plus a bounded label/count/account summary.
The document keeps an active head and monotonic state epoch. Import headers also
identify their source/group and preparation stage. Exact names are implementation
choices, not a generic schema-generation framework.

The repository exposes current head, paginated action summaries,
`preview_undo(action_id, token)`, `undo_action(operation_id, preview)` and durable
outcome lookup. The preview names the complete affected scope and any refusal;
execution revalidates that exact head and token. No inverse SQL or arbitrary
before-state comes from the UI. Ordinary roots become applied on commit; import
roots move from preparing to applied or cancelled. Undo marks applied roots undone.
Cancelled preparation retains retry status without becoming an Undo step.

Payloads follow the operation:

- Transaction/Review/rule action: references to the existing before/after domain
  snapshots and explicit members/association changes. Add missing fields to the
  owning receipt rather than registering all SQL writes.
- Manual creation: the created identity and ownership scope; no copied nonexistent
  before-state. Remove only its created posting/material when it is the head.
- Opening/currency action: existing account restoration inputs, restricted to the
  monetary fields and affected postings; do not restore a later account rename.
- Acceptance: exact acceptance group, prior decisions and the state transitions
  specified in section 6.
- Import: source/group identity and references/deltas for pre-existing objects
  changed by the import. Its ordinary new rows are found through source ownership.

Each public mutator is classified at its application/repository boundary as an
ordinary action, import child, compatible definition change, empty-chain removal,
or lifecycle operation. Nested storage methods join that context. Standalone
legacy/internal financial calls must enter the same coordinator. Unknown mutators
fail closed. This is a command-coverage obligation, not per-table row capture.

Forward writes, payload references, completion receipt, head and epoch commit in
one SQLCipher transaction. The preparing-import stages are the explicit durable
multi-stage exception below. Failed validation/cancellation before the commit gate
changes neither the saved data nor the head. Publish UI success only after the
durable outcome is known. A saved operation followed by refresh failure stays saved.

### Successive Undo without rewinding physical revisions

Keep append-only financial revisions where the production implementation uses
them. Restoring A's values after undoing B may create a newer physical revision;
that must not make Undo A impossible. Conversely, coincidentally equal values
must not authorize bypassing a newer action.

Use explicit restoration lineage for domain objects changed by Undo. A restoration
result records the retained snapshot/state reference it restores, together with its
own new physical revision and the owning Undo receipt. Reuse existing Review Undo
member mappings where possible; otherwise add a small typed restoration record.
This is per restored transaction/relationship/decision, never per imported storage
row. Reference chains may resolve to a retained original logical state, but cannot
outlive the referenced payload without a baseline publication.

Example: A creates payee state v1; B creates v2; Undo B appends v3 restoring v1.
Undo A checks that the current head is A and v3 is an authenticated restoration
of A's v1, then appends the prior value. Every restoration must validate the complete
saved domain state, including splits, ownership, decisions, associations and links.
Neither payee equality nor a claimed `restored_from` ID alone is sufficient.

Only the coordinator can create these records. The verifier checks their target,
contents and producing Undo against the chain; forward edits cannot forge them.
Do not relax existing legacy equality/revision guards globally. Replace them at
the common, head-authorized restoration boundary. Current revision/dependency
stamps are refreshed to the newly written revisions only for relationships whose
recorded logical validity was restored; stale Review decisions stay stale.

Every prepared command uses `{session, epoch, head}` plus its domain scope.
Epoch advances on every saved semantic mutation and Undo, including compatible
settings changes. It never rewinds. Old previews remain stale after change/Undo.
Entity/action IDs and any ordering allocator whose rows can disappear must not be
reused. Audit current MAX+1 allocators, especially manual record/source order.
Physical revision counters continue upward; unrelated definition revisions do not
need the rejected design's universal backwards-head/ancestry conversion.

Undo is a durable command, not another undoable head. After A, B, Undo B, C, the
active chain is A → C. B remains an undone session summary, not C's parent. No Redo
payload is kept solely for B. Retain its restoration lineage only while required
by current state, another active action or verification of this session.

Retry lookup precedes freshness refusal. Identical completed requests return their
original status, including subsequently undone; conflicting reuse refuses. A retry
of Undo B never pops A. After successful close, compact already-completed receipts
may replace detailed results under the existing session contract.

## 5. Whole-import rollback

### Ownership is the boundary

Use one source/group identity for the complete import, across all mappings.
Existing `source_associations`, batch ownership and transfer dependencies provide
the removal set. A numeric high-water mark can delimit an allocation/order range,
but cannot establish ownership of existing/shared transactions by itself.

No trigger or wrapper captures inserts into draft, allocation, source, occurrence,
version or field tables for ordinary Undo during import. Automatic rules and newly
generated counterparts belong to its stored result, not an edit history for each
new posting. Never hide those inverse records under another table or codec name.
Required imported originals and current interpretations still remain. Intermediate
import versions must not be retained solely as ordinary edit-Undo payload; existing
validation, recovery and surviving-dependency needs remain separate obligations.

For pre-existing state, distinguish:

1. An added source association: its source owner identifies it for removal; do not
   additionally save a "delete this inserted row" inverse for each association.
2. A changed existing transfer/proposal or displaced generated counterpart: retain
   the precise earlier relationship and ownership references needed to restore it.
3. A replaced/removed earlier association or modified existing transaction: retain
   the actual typed prior state once, or reference its existing immutable version.
4. New setup: keep the accounts, categories, aliases and explicit initial openings
   under section 1; they require no import reversal images.

This permits restoration storage proportional to actual changes to existing
objects. An import changing many existing objects can legitimately need many
references. It does not justify journaling every newly imported row.

### Preparation and publication

Before the first durable capture write, save a source-bound preparing intent with
its parent head and operation identity in that same transaction. It owns capture,
materialization, mapping effects, initial category choices and automatic work.
Block unrelated saved mutations until the workflow finishes or is cancelled.
Help and supported read-only inspection remain available.

Capture and materialization retain their existing durable stages. Each stage's
pre-existing-state restoration references commit with the changes they describe.
There are no child Undo steps. The final stage marks the one root applied and
advances the head atomically. Retained parse failures or unresolved Review questions
can be a completed import outcome; acceptance and later repairs remain separate.
An initial category-choice dialog is still part of preparation until answered.
Do not keep a successful import preparing merely because its entries are New.

Cancellation rolls back the import's contributions, retaining the permitted setup
of a materialized import. Capture-only cancellation uses the current narrowly
checked incomplete-import cleanup, which may remove untouched provisional accounts;
the permission to retain completed setup does not require changing that policy.
Failed cleanup leaves a recoverable preparing intent and blocks unrelated writes.
A successful close requires this workflow to finish or cancel first.

After a crash, recover SQLCipher and validate the recorded stage. Capture-only
incomplete work follows existing cleanup. Materialized work awaiting an initial
decision is retained for resume or explicit rollback, never mistaken for capture-only
cleanup. Applied roots recover as completed imports with their chain. The new
stage record must distinguish these cases without relying on an in-memory flag.

### Executing Undo Import

Only the current head's import can be undone through the session chain. Later
edits, Review decisions, imports and acceptance must be undone individually first.
This makes removal eligibility simpler and prevents overwriting newer work.

In one storage transaction: revalidate head/token and source ownership; restore the
recorded pre-existing relationships; remove the source's contributions and only
exclusively owned new material; retain shared originals and required earlier state;
validate; record completion and pop the head. Preserve the current import rollback's
independent verification gate for destructive collection.

Adapt `account_rollback.inc`; do not call it unchanged. It currently clears
`review_stack`, uses a one-slot rollback receipt and may collect history required
by earlier actions. The replacement must retain the parent action's payloads,
restore their logical dependencies, and use the common durable retry mechanism.
Its source/transfer conflict checks and acceptance protections remain authoritative.
No external input file needs to be opened again; no external file is deleted.

### Older imports and Discard

Successful close expires ordinary action ordering while keeping today's eligible
source rollback capability. An older import containing accepted transactions is
still ineligible. Shared-source and transfer conflicts still refuse atomically.

Older-import removal is a new confirmed destructive command, not an undoable
removal followed by retained copies of everything it deleted. Account-wide Discard
follows the same rule. Both require an empty active chain and no preparing import,
so they cannot destroy restoration inputs for earlier user actions. This restriction
is deliberately conservative and includes otherwise unrelated current actions.

With a nonempty chain, explain that the user must undo the current actions or
successfully close/reopen before proceeding; never perform either automatically.
Current-session imports use newest-first Undo instead of this bypass. A successful
removal preserves compact retry status and existing cross-session restoration
requirements for surviving data. Subsequent user edits start a new chain from the
resulting saved state. No early expiry of an active chain is introduced.

## 6. Reversible acceptance

Keep acceptance in the chain. Existing `dataset_acceptance.inc` shows why changing
a single accepted flag is insufficient. A typed acceptance receipt must identify:

- Exactly the newly accepted entries and generated/local counterpart sides, using
  the existing acceptance group/member records rather than a second member list.
- Pre-acceptance decisions and validity stamps changed by Ignore warnings/Include.
- Prior batch/source completion states and acceptance references.
- Account frozen/confirmation state actually changed by acceptance, and transfer
  protection/accepted-unmatched states it changed.
- Any existing semantic state changed by category publication/retained-name cleanup;
  retain created category definitions under the same setup exception as import.

Reference existing draft/entry and transfer snapshots. Acceptance does not copy
whole monetary records just to remember that they became accepted. Capture only
missing metadata needed for exact transaction/relationship restoration.

Undo acceptance is one typed transition to the recorded pre-acceptance membership
and decisions. Previously accepted entries remain accepted. Ordinary balances and
default report allocations remain equal across Accept and Undo Accept, including
ignored warnings, local counterparts and duplicate-only sources. No balancing
payment is created. Later accepted-entry edits must already have been undone.

Accepted overlays/history cannot be erased while active actions or surviving
relationships reference them. Record the acceptance reversal explicitly; current
visibility follows active acceptance membership. Extend verification to understand
revoked acceptance without replaying it as currently accepted. Reacceptance uses
new operation identity and revision allocation above retained versions, not a reused
revision-zero overlay. Update affected publication/verification assumptions together.
This is a focused acceptance transition, not a rewrite of every table's identity model.

Required example: Import → edit New row → Accept → edit accepted row → Undo edit →
Undo Accept → Undo New-row edit → Undo Import. Every step must preserve the expected
identity, originals, decisions and transfer state as well as exact amounts.

Quit-time acceptance is saved before close. If close fails/cancels, it remains a
saved undoable action. Successful close makes its resulting state the new baseline.

## 7. Presentation and lifecycle

All financial Undo buttons/shortcuts ask the common repository head. Contextual
buttons cannot secretly undo a different action or bypass a newer account's work.
Show the actual action and its complete scope, including affected accounts/counts.
Import remains clearly named as whole-import rollback. Do not list inserted rows
as individual changes. Full Change History layout/search remains separate work.

Use durable action data after reopen/crash; remove `sessionEdits` as an independent
source of eligibility. Unsaved buffers keep existing Save/Discard/Cancel protections.
Before Undo, identify buffers whose base or referenced entities will change. Discard
only those explicitly chosen; Cancel does nothing. Saving first creates a new action
and requires refreshing the Undo target, never silently undoing that new save.
Independent unaffected rule/report drafts remain. Block if safe retention is unclear.

Save and apply needs one composed worker/repository operation: validate the proposed
rule and scope, then save the definition and apply inside one transaction. Failure
saves neither half. Its Undo reverses only the changed transactions; label it Undo
rule application and explain that the saved rule remains. A definition-only result
adds no transaction Undo step. Save and show report keeps a saved definition even
if later report calculation/rendering fails and reports that outcome accurately.

Successful document close atomically expires ordinary actions/restoration payloads,
publishes the current baseline and clean marker, and prunes only unneeded data.
Include active action references in reachability before that boundary. Preserve
current values, originals, recognition, rule provenance and eligible import rollback
inputs afterward. Failed/cancelled close and crashes retain the session chain.

Normal open/close adds no full audit/hash or routine full-file copy. Full abnormal
recovery, backup/restore and explicit checks validate the baseline, current session
chain, restoration lineage and retained import dependencies. Backup of an open
session includes a consistent recoverable chain. No automatic repair, plaintext
sidecar, lock bypass or weaker durable commit is permitted.

Undone activity may keep bounded session labels/counts, with unavailable old details
reported honestly after payload collection. It is not a permanent history archive.

## 8. Storage acceptance and verification

No new numeric growth budget is approved. Keep the existing `cf8f1a5` identical-
workload acceptance and also compare against unchanged current production at
`b4b65df`. Report both physical size and retained payload/index bytes, plus peak
simultaneous file/journal use and latency. Do not mask growth through VACUUM,
padding, weaker durability, smaller inputs or missing dependencies. If new behavior
cannot meet an existing gate, present the specific cause and measurements for an
owner decision; do not recommend a budget increase before checking the model.

The first structural gate is independent of bytes: import-only must produce one
root, **zero ordinary transaction-Undo members for new imported rows**, and no
per-inserted-row inverse journal. Report extra existing-object restoration counts
separately. Initial setup and automatic rules cannot smuggle new-row capture back in.
No schema-wide immutable-body/membership split is required by this design.

Use synthetic workloads: small reproducible chains, approximately 19,000 accumulated
transactions with bulk categorization and later small imports/edits, and the separate
50,000-row import-only stress case. Include transfers, duplicate-only imports, bad
records, changed existing relationships and repeated successful closes. Compare
identical forward workload checkpoints as well as complete forward/Undo results.

Required verification before delivery:

1. Every inventory command has a classification and cannot bypass the head. Both
   rules configurations, legacy API calls and cleanup paths are covered.
2. Before/forward/Undo equality for affected financial state, originals, ownership,
   decisions, relationships and exact report allocations. Exclude only documented
   retained setup, compatible definitions, monotonic metadata and caches.
3. Mixed file-wide chain, repeated same-row edits, same-value forward changes,
   branching, restoration-lineage forgery/corruption, stale tokens, exact/conflicting
   retries, retry after Undo and successful close. Earlier actions stay reachable.
4. Import into several accounts; shared sources; duplicate-only and failed-row
   outcomes; old generated/accepted transfer restoration; existing openings unchanged;
   retained setup and new-account opening explained; no imported-row edit journal.
5. Initial category choices and automatic rules remain import children; later user
   repair is an action. Crash/cancel at capture, materialization, decision and final
   publication; failed cleanup; no unrelated write or false successful close.
6. Full acceptance sequence above; mixed prior-accepted/New membership; ignored
   warnings, frozen accounts, local transfers, duplicate-only and no-op cases;
   reacceptance and backup restoration with revoked acceptance history.
7. Definitions between user actions, retained archived category assignments, deleted
   rules/provenance, account rename, guarded account archive/deletion and category
   deletion. Configuration saves never silently invalidate the chain.
8. Older-import removal and account Discard: nonempty-chain refusal changes nothing;
   empty-chain behavior, cross-session dependencies, lost acknowledgement and backup
   restoration retain today's financial/conflict guarantees. No undo-of-removal archive.
9. Disk-full/write/sync faults, cancellation and exits on both sides of each new
   durable boundary; chain and financial state commit together. Successful-close
   expiry, failed-close preservation, dirty recovery and open-session backups.
10. Native shortcut/context scope, dirty buffers, unrelated drafts, busy/progress,
    import decisions and account switching. Tests must assert real outcomes rather
    than merely checking that an action count changed.

Run appropriate existing financial/storage/UI regressions and ASan/UBSan. Record
rules-disabled and platform limits; do not infer Linux/Windows coverage from macOS.
No runtime verification was performed in this design session.

## 9. Workspace disposition and next-session handoff

Production `src/` is unchanged by the rejected investigation. No reset of production
code or Git history is necessary. Preserve earlier session-baseline/retention work.
The rejected prototype and its measurements remain untouched in this design session.

When implementation is separately authorized, retire the rejected experiment from
the normal test source/build surface before developing its replacement. The exact
files and CMake block are listed in the [operation inventory](action-undo-inventory.md#rejected-prototype-disposition).
Retain the historical reports/source snapshots in `review/action-undo-prototype/`
and the rejected documents for traceability. Do not run a blanket `git reset --hard`
or `git clean`, and do not reset the updated owner stop/replacement documentation.

Recommended implementation order after new-session authorization:

1. Confirm this replacement's visible scope choices with the owner/new-session
   instructions. Inspect the then-current working tree; retire only the rejected
   opt-in prototype. Choose a fresh experimental format pair; current production is
   68/67, with unchanged rejection of older formats and no migration requirement.
2. Establish the common header/head/epoch and typed adapters for existing user
   transaction/Review/rule Undo. Prove repeated mixed-action Undo and recovery before
   converting more commands. No production UI switch with partial coverage.
3. Integrate one import root, source rollback and preparation lifecycle. Pass the
   zero-imported-row-journal gate and measurements before broader implementation.
4. Add typed acceptance reversal, monetary account actions, dependency protections
   and the composed Save and apply command. Prove the full chain through import.
5. Route UI Undo to the common head, complete regression/fault/storage verification,
   update actual status, and expose only the fully covered format/implementation.

This is a handoff sequence, not execution authorization in this session. A storage
experiment may test these mechanisms but cannot substitute another universal row
journal. Further review/delegation follows the owner's new-session request; none
was started here. Final History presentation and the deferred manual pass remain
separate. Track future manual effects in [MAN-017](manual-update-backlog.md#man-017--unified-transaction-undo-and-whole-import-rollback-design-only).

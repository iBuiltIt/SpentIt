# Unified session action Undo — design and implementation handoff

Last updated: 2026-11-08

State: **accepted product direction; independently approved engineering design;
Designer FINAL; owner implementation Go granted 2026-11-08**. Independent review approved
revision 1 with zero unresolved blocking design issues. This document is the
reviewed implementation handoff, not a claim that the behaviour below exists.

## Authority and boundaries

The owner accepted:

1. One file-wide action chain, undone strictly newest first across all accounts;
   no selective skipping and no automatic multi-action Undo.
2. Acceptance participates in that chain. Undo acceptance returns exactly its
   newly accepted set to New without changing monetary values or default reports.
3. Current-session Import is fully reversed, including setup created within it.
   An eligible import from a prior completed session can instead be removed by a
   new, itself undoable **Remove import** action.

The owner authorized design, independent review and a durable handoff, then
approved the reviewed design and granted implementation Go on 2026-11-08.
Implementation belongs to a fresh session using the design-review-implement
workflow: an Implementer agent followed by independent implementation review.
The final
Change History layout is deliberately not settled here.

Read [current status](status.md), [first release scope](first-release-plan.md),
[session baseline](session-history-baseline.md), [storage acceptance](session-storage-comparison.md),
[import rollback](import-undo.md), [acceptance](import-commit-discussion.md),
[ledger editing](ledger-editing.md) and [feature 011](../features/011-committed-editing-and-reversal.md).
The accepted direction supersedes the old action-specific Undo restrictions only
when implemented under Go. Existing matching, category eligibility, financial
arithmetic, source ownership, forward-command validation and transfer policy do
not change. Deferred category/exclusion/account retention cleanup is not general
permission for unrelated retention refactoring.

## Current implementation facts

- `review_sheet_store.inc` has a file-wide `review_stack` and mutation generation.
  Its top reversal can reactivate the preceding stack item, provided there was no
  intervening non-stack mutation. Ledger edits, selected edits/deletion, category/
  transfer assignment, Review decisions and current Rules Apply share it.
- `import_model.cpp` additionally keeps `sessionEdits`; it clears that list when
  another action is top. Storage may retain a reachable earlier edit while the UI
  loses its entry point. Manual creation has another latest-generation-only Undo.
- `committed_store.inc`, `bulk_store.inc`, `rule_reapply_store.inc` append inverse
  revisions with exact-revision/dependency guards. They do not implement a common
  backwards cursor. `account_lifecycle.inc` has separate opening/currency Undo.
- `dataset_acceptance.inc` mutates acceptance references, draft decisions (including
  ignored warnings), account freezing, local transfer protection/acceptance,
  category publication and source/batch state. Reversing only
  `accepted_transactions` would be incorrect.
- `account_rollback.inc`, `incomplete_import_cleanup.inc` and pruning physically
  collect source material. They cannot support restoring a removed import unless
  its needed material remains encrypted and reachable until the Undo boundary.
- Several mutation APIs lack caller operation IDs, including report catalog save
  and amount-format save. Identity allocators based on current maximum values
  cannot safely reuse identities after Undo and a new branch.

These are source-inspection findings, not results of new runtime tests.

## Product contract

### What counts as one action

A completed user command is one root action, including all automatic dependent
work. Children have explanatory effects but no independent Undo. One action can
span accounts, currencies, definitions, sources and relationships. A saved change
on one transaction is an action with one affected transaction; selecting 24 rows
and renaming them is one action with 24 affected transactions. Grouping is stored
explicitly, never inferred from adjacent times, equal values or operation-ID text.

A command that saves no semantic change adds no action. A receipt-only retry is
not a new action. The implementation must check all affected state, not use zero
amount, zero transaction count or zero monetary delta as proof of a no-op.

A combined **Save and apply** is one action containing definition Save and
transaction Apply. **Save all changes** is one action for its exact saved set.
Selected Save remains selected-only. This requires a new composed application/
repository command for Save and apply: validate the selected saved definition and
application scope against one token, perform any optional preview before mutation,
then save and apply inside one root transaction with no intervening UI prompt.
If validation, evaluation, cancellation or saving fails before commit, neither the
definition nor transactions change and the independent drafts remain available.
The current QML Save → preview → Apply callbacks cannot remain the implementation
of a combined action. A read-only preview may use a proposed definition without
persisting it; Apply revalidates it and the scope/token before durable work.

**Save and show report** records only its Save. Showing the computed report is a
read afterward, not another action: a calculation/rendering failure after a
successful Save leaves that definition saved and undoable, with an accurate
“saved, report could not be shown” result. It must not roll the Save back or imply
that retrying the Save is necessary. Automatic rule changes,
matching and generated transfers performed as part of Import belong to Import;
a later explicit rule application, repair or transfer setup is a new action.

### Ordered chain and activity record

The document has one durable active action head. A forward action records that
head as its parent, then becomes the head atomically with its changes. Undo may
name only the current head. Undo restores its complete prior semantic document
state and moves the head to its parent. No per-account or per-transaction exception.

Undo is a durable command with an outcome receipt, but it is **not** a new
undoable head. Otherwise repeated Undo would alternate between doing and undoing
one action. No Redo is introduced. Keep session activity metadata sufficient to
show that an action was undone; do not offer to undo it twice.

After A, B, Undo B, C, the active chain is A → C. B remains marked undone for this
session and is not a parent of C. Its reversal payload can be reclaimed once no
retained purpose needs it. Undo C then Undo A reaches the session baseline.
Historical inspection must distinguish the abandoned B from active actions.
After an undone action's inverse payload is released, retain only its session
summary, affected identities/counts and undone status; detailed historical
Before/After inspection is unavailable rather than reconstructed from current
values. Inspecting a surviving current transaction remains possible but is named
as current inspection. Active action details must remain available through their
retained before-state and successor/current references. No snapshot is retained
solely to implement Redo or a permanent record of abandoned branches. This detail
availability limit must be explicit in the eventual UI design.

Earlier-action inspection can name the actual current head and intervening
active actions. A transaction-only timeline is not a complete dependency chain:
a later action in another account still has to be undone first. Returning from
inspection must not change which action is undone.

### All document-changing commands participate

Before shipping, maintain a checked command/table inventory. The initial coverage
is below; absence from this list is not permission for an uncovered write.

| Family | Root commands and exact reversal boundary |
| --- | --- |
| Transactions | Manual creation; new/accepted edits and repairs; split changes; selected payee rename/restore/delete; category assignments; all affected identities, ownership, originals and relationships restore together. |
| Review | Merge, batch Merge, Link, separate/not-linked choices, amount-difference decisions, hidden-suggestion exclusions and their removal; all decisions, source associations, counterpart retirement and matching state restore. |
| Transfers | Creation, explicit resolution/setup, assignment, category release, match/unmatch, retarget, unlink and other exposed saved changes; retain the existing forward eligibility, reverse the complete operation. |
| Rules | Definition create/edit/delete/ordering/enabling, saved all-draft changes, current account Apply and any still callable legacy pending/reapply paths. Saved definitions do not themselves apply to transactions. |
| Categories | Create/rename/archive/unarchive/delete-unused, Income flag and supported catalog settings, plus definition publication performed inside another root action. |
| Accounts | Create/rename/archive/reactivate/delete-empty, aliases, opening correction and currency reassignment. Initial document creation is the baseline, not an undoable deletion of the document file. |
| Reports | Definition create/edit/duplicate-save/delete and Save all; computed results, search, charts and drill-down navigation are reads. |
| File settings | Saved amount/date formatting and other file-owned preferences. Application window/theme/device preferences are outside this document chain. |
| Imports | Complete source import including setup, duplicate-only results, retained failures and automatic transformations; later repair/resume/setup commands are separate actions once initial import has completed. |
| Acceptance | Exact newly accepted membership plus all associated decisions, freezes, categories, batch/source/transfer changes caused by that root. |
| Removal | Eligible older-source Remove import, and account-owned Discard new transactions; restoration includes collected source material and relationships, not just visible transaction rows. |

Legacy/internal mutator entry points must either join the caller's root action or
create the same root type when called standalone. They must not remain alternate
user-visible Undo systems. Forward validations remain authoritative at the
repository boundary.

Selection, inspection, searches, previews, Help, explicit Check file and backup
creation do not add actions or invalidate the chain. Recomputed caches do not add
actions. Hidden user-semantic writes discovered in a purported read must be moved
into its owning action or classified explicitly before release; silently clearing
Undo is prohibited.

## Import and acceptance semantics

### Current-session import

Undo Import restores the state before that complete import: its source ownership,
recognition contributions, matching/link arrangements, generated counterparts,
new transactions, failures and automatic rule effects. It restores prior existing
entities and removes only setup created by that import (accounts, categories,
aliases and their opening choices). Pre-existing definitions are not deleted.
All later uses of those entities must already have been undone by strict order.
The imported file on disk is never modified or deleted.

Import must have one durable root intent across mapping/category prompts and
multi-stage preparation. Persist an encrypted `preparing` action with a pinned
parent before the first import-caused document write. Child durable stages belong
to this same root. Block unrelated document mutations while it is preparing;
read-only Help/inspection may continue where already supported. An unresolved
import decision must not expose a partly committed root as a normal Undo item.

Use the existing durable capture/staging approach, but record all setup changes
against the root. Final publication atomically marks that root `applied` and
advances the head. A terminal result containing retained parse/import problems
counts as a completed action with those problems in its effects. Later repair is
its own action. Automatic incomplete-import cleanup may cancel only preparing
roots, restoring their pre-root setup as well as unpublished source material.
It cannot collect completed imports.

Cancellation before publication rolls back the entire preparing root. If cleanup
fails, keep it recoverable and prevent unrelated mutations; do not falsely report
cancelled. Crash recovery audits and resumes cleanup of the incomplete root under
the existing incomplete-import policy, without advancing the active head. A crash
after publication recovers the completed action. The root intent, parent pin and
stage markers must make the distinction durable and idempotent. Successful close
is not permitted with an unresolved preparing root: finish or cancel it first.

### Acceptance

Undo acceptance restores **exactly** the pre-action semantic state, not a generic
“mark every accepted row New” transformation. Capture its newly accepted set and
all writes done by acceptance, including ignored-warning Include decisions,
retained-category publication/cleanup, acceptance references, account freezing,
batch/source lifecycle, local counterpart status and protection. Transactions
already accepted beforehand remain accepted. No new payment, balancing entry or
changed monetary value is introduced. The ordinary balance and default report
results remain equal before and after acceptance/Undo acceptance; internal
accepted-only totals and protection metadata change consistently.

A current-session sequence Import → Edit → Accept → Edit can be undone fully in
reverse. Once acceptance is undone, the earlier current-session import is
reachable. Acceptance in a prior successfully closed session is baseline state;
it cannot be undone in this session. The existing rule forbidding removal of an
import containing accepted transactions remains for **Remove import**.

Quit-time acceptance is a normal root action before close. If close fails or is
cancelled, that acceptance remains saved and undoable. If close succeeds, it is
part of the new baseline. Do not silently undo a successful acceptance because a
subsequent close failed.

### Remove an older import; discard new transactions

**Remove import** is a new forward action for eligible sources whose original
Import is outside the active session chain. It retains the current source-owned
rollback eligibility, shared-source preservation and transfer/source conflict
refusals. It does not allow removing accepted transactions or unaccepting baseline
transactions. Undo removal restores the exact state immediately before removal,
including source bytes, errors, source ownership, review decisions and links,
without reopening the original external file.

A source imported in the active chain is reached through ordinary Undo Import;
Remove import must not be a selective shortcut around newer actions. Older imports
may be removed after current-session work where the existing ownership/conflict
checks allow it; this is a new forward operation, not skipping history.

Account-wide **Discard new transactions** remains a distinct forward action with
its existing account-owned scope and eligibility; it is now itself undoable.
Both forms temporarily pin data their existing collectors would otherwise delete.
After successful close, removal becomes baseline and exclusively owned material
is eligible for final collection. It is not archived forever. Imported setup from
a prior session remains baseline setup: older-source removal retains it as today.

## Proposed storage architecture

### One action coordinator, one reversal source of truth

Keep the existing dependency boundaries. Add application types in
`src/app/include/spentit/action_history.hpp`, repository history/Undo APIs and a
storage implementation such as `src/storage/action_history.inc`. The serial
worker owns the repository; QML neither chooses inverse SQL nor synthesizes money.

Use a durable, encrypted **transaction-local inverse write journal** for document
semantic state, paired with typed action metadata and domain validation. This
replaces special-case inverse commands as the source of truth for Undo. Reuse
existing immutable snapshot/source payloads by reference wherever they already
encode the required before-state; do not retain both a full typed receipt and a
second full copy of the same snapshot for Undo.

The implementation should contain a schema-owned manifest classifying every table
and each mutating repository API as:

- semantic state/history required to interpret current state: reversible;
- durable operation receipts, identity/sequence high-water values and security/
  session markers: monotonic, not reversed;
- disposable derived caches: invalidated/rebuilt, not restored;
- provisional import material: owned by its preparing root;
- retained source/import-removal payload: pinned by reachability until no owner.

For reversible tables, schema-generated capture triggers record the first
pre-action image of each touched key (or absence for an inserted key), using
explicit versioned codecs and stable keys. A schema manifest, rather than generic
SQL text supplied by UI, determines capture and restore. Capture remains in the
same SQLCipher transaction as the mutation. Large existing immutable source or
snapshot payloads are referenced/pinned instead of copied; destructive collection
of such payloads is deferred while pinned. No plaintext sidecar or full-file copy.

### Concrete payload liveness and removal representation

Use **immutable payloads with explicit active membership**, not “leave the old
live row in place and hope readers ignore it.” Split large immutable source bytes,
parsed-original payloads and reusable snapshot bodies from their active semantic
root/membership rows where the present schema combines them. A payload ID identifies
one immutable stored body. Live source/batch/occurrence/snapshot memberships and
current transaction/relationship/original references point to those bodies. An
inverse entry stores the prior membership/metadata plus payload IDs, not another
copy of the bodies. `action_payload_pins(action,payload)` supplies the temporary
reference for restoration. Metadata without an existing immutable body is stored
once in its typed inverse entry.

For a Remove import/discard action, calculate the exact ownership closure under
the existing rules; pin needed payloads before removing active memberships in the
same transaction. Normal ledger, Review, reports, source enumeration, duplicate
recognition and eligible-import lists follow **active semantic memberships**, never
payload-table existence or pin rows. Retained originals used by a surviving
transaction remain visible through that transaction's legitimate current original
reference, not through the removed import. The removed source must not generate
recognition candidates or Review questions merely because Undo still pins bytes.

Undo restores the recorded memberships/rows and prior relationships referencing
the same payload IDs. No parse, external-file read or new identity is required.
Reverse Import removes its active memberships and import-created setup; source
payloads with no remaining owner can be reclaimed after successful reversal since
Redo is absent. Shared payloads remain if another live owner requires them.

Reachability roots are: current semantic memberships/heads and surviving originals;
retained cross-session import-removal/recognition/relationship dependencies;
active-session inverse entries and their pins; and preparing-import ownership.
Disposable caches and compact command receipts are **not** payload roots. Forward
rollback/removal invokes collection only after pins are recorded. Undo releases
only its target's pins after memberships are restored and validates remaining
reachability. Close expires session pins/inverses, then collects only payloads
unreachable from the other retained roots. Recovery/backup verification checks
missing pins/payloads, orphan memberships and visibility boundaries explicitly.
This necessary liveness split is not authorization to clean up unrelated deferred
category/Review-link/exclusion/account history.

The storage prototype must measure the representation itself: no hidden duplicate
source body in an inverse blob plus payload table, and no scan treating pinned
but inactive sources as live. Any transient or retained size excess remains a
storage-gate failure requiring an owner decision.

### Capture and restoration enforcement

A storage write context distinguishes forward root, child, Undo restoration,
preparing import, and close-time cleanup. Writes to reversible tables without a
valid context fail. Manifest/trigger completeness is checked against the schema
at build/test time and authenticated format validation. Record coverage must
include triggers and cascading effects, not only explicit top-level SQL writes.
The old immutability triggers need a narrow storage-only restoration allowance;
there must be no general “disable integrity” switch exposed to commands.

Existing `register_financial`, `register_category`, `register_rule_save`,
`register_rule_apply`, `register_acceptance`, `register_account` and exclusion
triggers insert IDs into `operation_registry`. Those registry rows survive Undo.
Restoring an old semantic operation row must therefore use a narrowly authorized
restore path rather than attempting another forward registration. The new trigger
policy is: in normal forward context, preserve the existing unique-ID collision
behaviour; in Undo restoration context, skip forward registration/capture **only**
for an exact table/key authorized by the current head's recorded before-image,
and require the existing registry ID/domain to agree with that original row.
Missing registration, a different domain, a row outside the target inverse set,
or arbitrary caller-supplied contents refuses the entire Undo. Root/child ownership
and request digest are checked through the action/receipt mapping as well. The
restore capability is scoped to the storage transaction and cleared on every exit;
it is not a connection-wide bypass left active after errors. Tests must inject
same-ID/different-domain and unauthorized-restore attempts. FK, uniqueness,
financial validation and malformed-payload checks remain enabled throughout.

An inverse restores all touched semantic rows to the root's before-image, with
foreign-key-safe ordering/deferred constraints within one transaction. Rows
inserted by the action are removed; previously removed rows are restored; prior
head pointers and reference memberships return exactly. Validate complete affected
financial/domain invariants and FK/integrity constraints before commit. Never
invoke present-day rule matching, parsing or report definitions to reconstruct
an old result. All needed values come from retained durable material.

Restoring historical semantic row versions is safe only with the separate global
monotonic token described below. Do not keep the current mix of “append a new
correction then hope old exact revisions match.” Legacy operation/revision rows
needed for active-state interpretation are included in the reversible manifest;
retry identity registries are not. Verification must understand active journal
ancestry rather than requiring every undone legacy event to remain active forever.

The row-journal mechanism is an implementation proposal requiring an early
representative prototype and storage measurements. It is not permission to bypass
financial validations or weaken the existing storage acceptance to make it fit.

### Minimal durable records and APIs

Proposed logical records (exact SQL names may follow repository conventions):

- Document session ID, monotonic state epoch, active head, monotonic action order.
- Action ID, session, parent, kind, preparing/applied/undone state, user-facing
  scope metadata and child membership. Chronological activity order is separate
  from active parent ancestry.
- Inverse entries/references keyed by action and semantic table/key, plus pins for
  immutable material retained solely for reversal. Coalesce repeated writes to a
  key within one root to its first before-image; a net no-op key needs no payload.
- Command receipt: operation ID, request digest/domain, durable completion and
  target action/Undo outcome. After expiry keep only the existing compact retry
  information; no action descriptions or historical monetary archive survives.

`ActionToken = {session, state_epoch, head}` is required for every prepared write
and Undo. Epoch increases for each published forward action and each Undo; it is
never rewound. Restored per-object revisions must not resurrect stale previews,
selection tokens, category choices or requests. Identity and sequence allocators
also remain monotonic; create→Undo→create uses a new identity even when a MAX query
would otherwise reuse the removed one. Parent ancestry does not depend on dense
sequence numbers. High-water allocation applies to **all** reusable identifiers:
account/entity IDs, batch/occurrence allocations where allocated, entry and draft
revision IDs, category/ruleset/report revisions, financial/history sequence IDs,
action IDs and operation child IDs. A restored current head may point to an older
version; the next new version is allocated above its durable high-water mark,
never as restored-head + 1 or MAX(active rows) + 1. Revision payloads referenced
by surviving originals or import-removal dependencies cannot be overwritten.

Verification and all lookup paths must replace dense revision/predecessor
assumptions with explicit predecessor/ancestry metadata or recorded before/after
references. For example, branch version 8 may follow restored version 3 while
versions 4–7 are undone/retained only by other roots. Neither revision - 1 nor
“all numbers up to max must exist” defines its predecessor. The format conversion
must inventory these assumptions in batch events, ledger versions, catalogs,
rule application verification and account currency history; a new counter alone
is insufficient. Existing forward formulas using +1 must be converted with the
allocator and predecessor metadata, and verification must reject wrong ancestry
rather than merely tolerating missing rows.

APIs expose paginated action summaries, action effects/membership, the active
chain/top, and `preview_undo(action, token)` / `undo_action(operation, preview)`.
An earlier action returns the exact newer head/intervening chain, not a misleading
individual-row eligibility result. Applying Undo checks the root/head and epoch
again. A preview can be stale even when values look unchanged. New API entry
points without existing operation IDs must receive them before mutation.

Command retry is checked before freshness rejection: an identical completed
command returns its original completion status; conflicting ID reuse refuses.
Retrying an action already undone must return **completed, subsequently undone**
(or compact already-completed after expiry), never reapply it. A fresh intentional
repeat uses a new operation ID. A repeated Undo receipt never pops another action.
A completed no-op request also receives retry protection.

### Forward and Undo transaction flow

1. Resolve unsaved UI buffers and capture a current token. Validate the explicit
   command scope and existing financial/security rules.
2. Open the root transaction/context; children join it rather than adding heads.
   Capture inverse state through the manifest while executing the existing forward
   operations. Provisional multi-stage Import is the explicit exception above.
3. Validate affected closures (including cross-account/source links), register
   receipt, publish action and new head/epoch together, then commit durably.
4. Publish fresh presentation state only after the durable outcome is known.

Undo uses the same boundary with the current top's complete inverse. It restores
all affected domains, invalidates caches, records the Undo receipt, marks the target
undone, moves the head and increments epoch atomically. It does not create new
counterpart IDs, re-evaluate rules, or recalculate old money from current settings.
Do not leave active entries pointing to removed categories/rules/sources during
any committed state. Cancel before the durable gate leaves both data and chain
unchanged. A saved-but-unrefreshed result keeps the saved outcome and uses existing
accurate recovery messaging; never invite blind replay.

## UI integration contract, not final layout

Every document Undo entry point delegates to `undo_action`; remove the special
ledger/history/Review/rules/account selectors. If a contextual entry point names
an older action, navigate to its blocker rather than silently undo a different
scope. The global shortcut names/uses the actual top. Text editing keeps native
text Undo while its buffer is unsaved. Persisted action state reconstructs all
availability after crash recovery; no `sessionEdits` shadow stack remains.

Before global Undo, resolve dirty editors whose saved base or referenced identities
would be changed/removed. **Cancel** keeps everything. **Discard affected edits**
discards only those named drafts and proceeds. **Save affected edits** saves them
as a new action and then refreshes the proposed Undo target: it must not secretly
undo that new save or continue to the old target. Unrelated independent rule/report
drafts remain intact. If safe non-overlapping retention cannot be established,
refuse Undo and identify the conflicting drafts; never silently rebase/overwrite
unsaved definitions. Existing navigation guards remain until replaced by an
explicitly agreed equivalent.

History is action-first. Member inspection and transaction timelines are reads;
showing a transaction in the ledger identifies that exact current row. Filters
must never hide the broader scope of the top action/Undo preview. UI labels and
modal details follow [the language guide](user-facing-language.md). Final layout,
search, narrow-window behaviour and Help/manual text are a subsequent design task.

## Session, recovery and external boundaries

Successful close atomically publishes current state as baseline, expires ordinary
action/effect/inverse payloads and Undo receipts as appropriate, releases pins,
prunes only unreachable material, and marks the document clean. Preserve current
state/originals/recognition and eligible prior-import removal dependencies. Failed
or cancelled close preserves all chain and pins. No routine full-file audit/hash
is added. Crash recovery first recovers SQLCipher, then validates baseline,
retained dependencies and journal completeness before permitting writes.

Backup snapshots must include a consistent current state and recoverable chain;
restore verification validates that snapshot before activation. Creating/deleting
an external backup, replacing a document from backup, creating a document, choosing
a path, and switching/closing documents are gateway/lifecycle operations, not
financial actions undone by this chain. Undo never deletes an external file or
changes passwords/locks. Restore replaces the active document through the existing
explicit recoverable gateway process; it is not a selective transaction Undo.

## Storage, compatibility and rollout gates

Current formats are 68/67. Introduce a new rules-enabled/disabled experimental
format pair after checking the then-current repository; no migration or backwards
compatibility work. Earlier formats are rejected unchanged. The old schemas and
verifier assumptions cannot remain writable side-by-side with the new chain.

Preserve the existing `cf8f1a5` storage acceptance at every identical measured
checkpoint: physical file bytes and retained payload/index bytes, without VACUUM,
padding, weaker durability or omitted dependencies. Also measure peak temporary
session journal/pin bytes for large import/removal/Undo. New functionality may
need more transient retention, but there is **no automatic waiver**: if the design
cannot meet the existing criterion, stop with measurements and a specific owner
decision rather than silently relaxing it. No permanent per-action facts layer.

Implementation sequence after Go:

1. Freeze API and schema/command inventory, state manifest, compound-action policy
   and typed journal codecs. Build a small real prototype covering a rename,
   acceptance and older-import removal; measure storage before broad conversion.
2. Implement coordinator, durable tokens/receipts/high-water IDs and restore/verify
   foundation in an isolated experimental format. No production UI switches yet.
3. Convert every command family and cleanup path; replace alternative Undo paths.
   Existing forward-command semantics and exact financial invariants remain tests.
4. Integrate common UI availability/guards with a minimal accurate presentation;
   do not claim the later History usability redesign is complete.
5. Run full relevant functional/fault/recovery/storage acceptance. Only expose the
   new format/build as complete when all mutators are covered. If work spans
   commits, incomplete builds remain explicitly development-only; do not present
   partial chain coverage as delivered.
6. Independent implementation review against this design, reconcile contracts and
   status, and add manual backlog work. Manual text/screenshots remain deferred.

Expected implementation areas: `import_repository.hpp`, new action types;
`import_store.cpp` schema/format/initialization/format validation and allocator;
all storage `*_store.inc`, acceptance, rollback, cleanup, snapshot/pruning and
account files in the inventory; `document_gateway.cpp` lifecycle; `import_model`
API/state and `ImportMain.qml` routing/guards; affected UI/storage/application
fixtures. Engine financial calculations should not need policy changes.

## Required verification

- Inventory coverage test: every semantic table has a reversible/pinned policy;
  all repository writes require a root or approved lifecycle context, including
  cascades, category Income fast path, report Save and format settings.
- Canonical semantic-state assertions for each action: forward then Undo equals
  the complete before-state, excluding monotonic receipts/IDs/epochs and caches.
  Check both rules configurations. Compare identities/ownership/recognition and
  relationships, not just balances or visible rows.
- Mixed chain covering every family and multiple accounts, then undo to baseline;
  unrelated actions still enforce strict order. No older-head bypass through any
  legacy API, shortcut, contextual button or restored UI state.
- Composed Save/apply: invalid definition, stale scope, evaluation failure,
  cancellation and I/O failure save neither half; lost acknowledgement resolves
  one complete root; Undo restores definition and transactions together; unrelated
  drafts survive. Save-and-show report rendering failure preserves the saved
  definition and its Undo action, without duplicate Save on retry.
- Registry-aware restoration of financial/category/rule/account/acceptance rows:
  original same-domain ID succeeds without duplicate registration; different-domain,
  missing-owner and out-of-inverse-set attempts fail atomically. No restoration
  capability survives cancellation, exception or rollback.
- Monotonic entry/draft/catalog/ruleset/report versions after Undo/branch, including
  pinned old revisions; reject stale tokens and incorrect predecessor ancestry.
- Removed pinned sources are absent from ledger/Review/reports/recognition/import
  lists while surviving transactions retain their legitimate originals; Undo with
  external file absent restores exact membership. Check pin/copy byte accounting
  and corrupted/missing payload references.
- Compound Save/apply and Save all, selected-only changes, deletion/restoration,
  signed splits, overflow/precision, categories and rules referenced by reports,
  manual/imported/accepted/new transfers, linked/generated counterpart closures.
- Import→automatic rules→edit→accept→edit→Undo to baseline; multi-account import
  creates/removes setup; pre-existing setup survives; duplicate-only imports and
  shared sources restore recognition; malformed retained failures and later repair.
- Acceptance with ignored warnings, category creation, generated/local transfers,
  mixed pre-accepted/new rows, multiple sources and no-op/metadata-only cases.
  Assert ordinary money/reports unchanged and prior protection/review state exact.
- Older-source removal/discard then Undo with the original external file absent;
  source ownership and transfer match restoration, accepted-source refusal,
  prior-session setup preservation, removal chains and reimport recognition.
- Preparing Import crash/cancel at each durable stage; no interleaved action;
  complete setup rollback or recoverable blocked cleanup; published import survives.
- A→B→Undo B→C branching; repeated/contradictory IDs; retry after Undo and after
  successful close; stale preview before forward/Undo/branch; all identity
  allocators never reuse IDs. No Redo accidentally created by Undo receipts.
- Recover each supported action/Undo after process exit immediately before/after
  durability, lost acknowledgement, disk-full/write/sync failures and refresh
  failure. Assert state and head agree, never a half-restored domain.
- Successful close expiry, cancelled/failed close, acceptance during failed quit,
  crash-recovered chain, baseline verification, pin reachability and bounded
  cleanup; backup restore with live chain and after close. Keep normal open/close
  free of routine full audit/hash.
- Dirty editor guard across all workspaces, unchanged unrelated drafts, affected
  deleted definitions, Save creating a new top, busy/cancel/Help modality, account
  switch/filter/report drill-down not changing Undo scope, native keyboard routing.
- Run the existing storage comparison workloads at every recorded checkpoint and
  add large import/removal plus repeated Undo/branch workloads. Record time,
  physical bytes, live/freelist/retained bytes, peak journal and source pins.
- Release plus ASan/UBSan where applicable, rules-disabled, macOS native interaction.
  Linux/Windows results must be explicit; do not claim unexecuted portability.

## Assumptions approved with the reviewed design

- No Redo, selective Undo, or automatic undo-to-here is included.
- Combined Save/apply is one action; successful no-op requests have receipts but
  no visible chain entry.
- Preparing imports exclude other document mutations until completion/cancellation.
- Remove import targets prior-session imports; current-session imports are reversed
  through the strict chain. Account Discard remains a distinct reversible action.
- All document definition/settings mutations participate; external/lifecycle and
  application-only preferences do not.
- Storage acceptance is a hard gate; an incompatibility requires a new owner
  decision, not an implementation-only exception.

## Design review revision 1

Independent review requested three blocking clarifications. Revision 1 addressed
them and received **APPROVED**, with zero unresolved blocking design issues:

1. Restored operation rows and persistent ID registries: added narrow
   owner/domain-checked restore trigger rules, monotonic allocation for every
   version family, and explicit replacement of dense/predecessor assumptions.
2. Removed-but-pinned material: selected immutable payloads plus active semantic
   memberships and temporary action pins; specified live-query visibility,
   restoration and collector/verifier roots without an external-file dependency.
3. Combined actions: required one composed Save/apply repository transaction with
   prevalidation and no partial Save; separated saved report definition from a
   later calculation/rendering failure. Added corresponding test obligations.

Also clarified that undone activity summaries survive within the session, but
historical details cease to be available when their reversal payload is reclaimed.
Engineering is independently approved as a design and remains storage-prototype-
gated. The reviewer noted the broad storage/verifier refactor, required complete
command/table/ancestry inventory, explicit undone-detail expiry and unproven
storage feasibility as non-blocking implementation risks. **Designer FINAL:** the
approved revision is accepted without substantive changes. The owner subsequently
approved this reviewed design and granted implementation Go for a fresh session
on 2026-11-08. No implementation or new runtime verification is claimed.

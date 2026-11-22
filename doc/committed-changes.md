# Ordinary committed changes

Last updated: 2026-11-09

Accepted transactions can be corrected or removed with strict Undo.
Removal changes the balance directly and leaves the original source and
amount available. A bank refund is its own imported transaction. Undo
appends the restored state, has no force override, and does not undo
acceptance as part of an ordinary correction. See [file-wide Undo](action-undo-design.md)
and [Change history](action-undo-design.md#change-history). Domain and storage
validation refuse moving an accepted transaction to another account or
rewriting its source-occurrence links. See [ledger editing](ledger-editing.md),
[selection commands](search-ledger.md), and [transfer edits](transfers.md#committed-edits).

## Current interpretation and retained history

Version checks run before storage-setting changes. See
[release schema](release-schema.md).

Original source evidence, original import versions and import receipts remain immutable. Storage keeps complete committed entry revisions and a current head for each included occurrence. Revision zero is its original included publication. A correction appends a current interpretation; removal appends an inactive interpretation; eligible Undo appends the earlier state as a new revision. Removed entries remain available through history and source inspection. Actual bank refunds remain separate imported transactions.

The exact current balance is the opening balance plus active current interpretations. Draft projection adds eligible pending entries exactly once. Corrected dates affect current chronological placement; permanent occurrence and sequence identities remain stable. Historical import receipts retain the before/change/after values recorded when those imports committed.

A shared financial timeline interleaves original import commits and later committed changes. A common immutable order also includes draft review events. Opening verification replays financial transitions, verifies original publication snapshots, checks dependency history and compares reconstructed entry heads and balances with current queries. It rejects missing/forged histories, snapshots, heads or review stamps instead of repairing them. Capacity checks reject exhausted identities/revisions rather than relying on SQLite's fallback row-ID allocation.

## Preview, apply and recovery

The C++ worker retains the typed proposed command, exact expected entry/dependency token, financial preview sequence and operation identity. QML displays formatted before/after snapshots and account balance impact; it does not calculate authoritative money or reconstruct the token.

Preview is read-only. Apply checks freshness and all financial invariants, then writes interpretation, allocations, head, dependencies and history atomically. Identical operation retries return the recorded receipt; changed payloads or cross-kind operation-ID reuse fail. Unrelated financial changes require a fresh account-balance preview but do not make an otherwise eligible Undo invalid.

A known durable receipt resolves a lost acknowledgement. An unreadable outcome closes the session and requires reopening/history inspection. If a change is known saved but its subsequent view refresh fails, the UI says it was saved durably and closes the session; it does not imply rollback. A failed attempt before saving preserves the editor buffer. A receipt from a previous action cannot stand in for the current attempt.

## Dependencies and duplicate review

Strict Undo requires the target revision and all recorded dependencies to remain unchanged. Dependencies include each batch that ever explicitly referenced the target through retained duplicate-decision history, with its revision and state. Deciding away, editing/reviewing that referencing batch, committing it or discarding it cannot erase the newer dependency. Unrelated batches do not block Undo. Original whole-batch publication is inspectable but is not an undoable correction in this scope. A later ordinary edit of either side of a transfer, a newer transfer relationship, or a newer source association also blocks Undo. The entry's own publication association does not. A date-only correction of a linked import keeps the link and says so in the preview. An amount correction or removal ends the link, leaves a committed counterpart's amount, and removes a purely generated uncommitted counterpart in the same operation. Linked rows are refused by bulk category and bulk removal.

Duplicate matching still uses immutable original source facts. A byte-identical reimport matching a removed occurrence becomes unresolved rather than automatically excluded or restored. Explicit choices are Keep removed or Include as a separate transaction. Restoration uses a separate eligible history Undo; import review never restores a committed entry implicitly.

An exclusion records its target revision and whether that transaction was still active. Saving another draft field carries that stamp unchanged; only an explicit review decision refreshes it. A later edit of the matched transaction does not require a new decision when the original date, amount, payee, and bank id still match and the transaction has not been removed. Removing the matched transaction, or a duplicate decision whose original facts no longer match, leaves that pending row out of the projected balance until the user chooses Already imported or Include as a separate transaction. Accepting anyway cannot add that row again. Historical committed exclusions remain valid against the state recorded at their original review event.

## UI scope

Double-click opens the ledger editor for an accepted transaction. Follow the
[ledger editing contract](ledger-editing.md) for save, selection, navigation
and unsaved-input guards. User-facing Undo follows the file-wide
[Change history contract](action-undo-design.md#change-history). Storage
revision and source inspection preserve immutable source fields. Copying a
source value changes only the explicitly active editor buffer.

## Boundaries and verification

The plain contracts are in `src/app/include/spentit/committed_changes.hpp` and `import_repository.hpp`. Storage implementation is in `src/storage/committed_store.inc`, compiled with the existing import store. Financial validation still uses the independent engine. The Qt model and QML contain no SQL or authoritative money arithmetic.

Independent storage regressions cover interleaved receipts, correction/removal/Undo, exact arithmetic, original evidence, global operation identities, preview freshness, later dependency conflicts, removed duplicate multiplicity, stale/reconfirmed exclusions, genuine legacy schemas, backup restoration and authenticated corruption. Failure tests exercise write/sync faults, process exits and both durable boundaries for each new operation. UI regressions cover guarded editing, retained buffers, explicit removal/Undo, removed reimports, and before/after/unknown acknowledgements and refresh failures.

Run the affected `committed.*` storage tests and `import.committed_ui` in
the [application build](building.md). Cover strict restoration and both sides
of durable commit before retrying.

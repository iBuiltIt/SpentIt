# Ordinary committed changes

Last updated: 2026-10-31

State: bounded experimental implementation verified on available macOS configurations. The owner accepted [feature 011](../features/011-committed-editing-and-reversal.md). This implements ordinary per-account corrections, removal and strict Undo. Opening-balance correction and currency reassignment are the point-4 account commands. The [2026-10-27 transfer edits](../features/011-committed-editing-and-reversal.md#accepted-committed-transfer-editing--2026-10-27) add transfer-aware correction, removal, and Undo for the locked cases. Moving a committed row, rewriting source-occurrence links, and migrations are not part of this workflow.

The current follow-up supports committed bulk category changes/removal and strict atomic group Undo, including retained group history after reopening. See [search and ledger](search-ledger.md) for the accepted scope, retry/recovery semantics and verification. The original single-row implementation details and historical verification below remain useful; experimental-format compatibility is not an obligation before the owner announces a release.

## Current interpretation and retained history

Schema 4 introduced this workflow. Current [account-scoped formats](accounts.md) retain it in schema 40 with rules/bulk receipts and schema 39 without rules. Earlier experimental layouts are rejected without migration. Version/shape checks run before storage-setting changes. Historical compatibility fixtures now verify rejection and preservation of their contents.

Original source evidence, original import versions and import receipts remain immutable. Schema 4 stores complete committed entry revisions and a current head for each included occurrence. Revision zero is its original included publication. A correction appends a current interpretation; removal appends an inactive interpretation; eligible Undo appends the earlier state as a new revision. Removed entries remain available through history and source inspection. Actual bank refunds remain separate imported transactions.

The exact current balance is the opening balance plus active current interpretations. Draft projection adds eligible pending entries exactly once. Corrected dates affect current chronological placement; permanent occurrence and sequence identities remain stable. Historical import receipts retain the before/change/after values recorded when those imports committed.

A shared financial timeline interleaves original import commits and later committed changes. A common immutable order also includes draft review events. Opening verification replays financial transitions, verifies original publication snapshots, checks dependency history and compares reconstructed entry heads and balances with current queries. It rejects missing/forged histories, snapshots, heads or review stamps instead of repairing them. Capacity checks reject exhausted identities/revisions rather than relying on SQLite's fallback row-ID allocation.

## Preview, apply and recovery

The C++ worker retains the typed proposed command, exact expected entry/dependency token, financial preview sequence and operation identity. QML displays formatted before/after snapshots and account balance impact; it does not calculate authoritative money or reconstruct the token.

Preview is read-only. Apply checks freshness and all financial invariants, then writes interpretation, allocations, head, dependencies and history atomically. Identical operation retries return the recorded receipt; changed payloads or cross-kind operation-ID reuse fail. Unrelated financial changes require a fresh account-balance preview but do not make an otherwise eligible Undo invalid.

A known durable receipt resolves a lost acknowledgement. An unreadable outcome closes the session and requires reopening/history inspection. If a change is known saved but its subsequent view refresh fails, the UI says it was saved durably and closes the session; it does not imply rollback. A failed attempt before saving preserves the editor buffer. A receipt from a previous action cannot stand in for the current attempt.

## Dependencies and duplicate review

Strict Undo requires the target revision and all recorded dependencies to remain unchanged. Dependencies include each batch that ever explicitly referenced the target through retained duplicate-decision history, with its revision and state. Deciding away, editing/reviewing that referencing batch, committing it or discarding it cannot erase the newer dependency. Unrelated batches do not block Undo. Original whole-batch publication is inspectable but is not an undoable correction in this scope. A later ordinary edit of either side of a transfer, a newer transfer relationship, or a newer source association also blocks Undo. The entry's own publication association does not. A date-only correction of a linked import keeps the link and says so in the preview. An amount correction or removal ends the link, leaves a committed counterpart's amount, and removes a purely generated uncommitted counterpart in the same operation. Linked rows are refused by bulk category and bulk removal.

Duplicate matching still uses immutable original source facts. A byte-identical reimport matching a removed occurrence becomes unresolved rather than automatically excluded or restored. Explicit choices are Keep removed or Include as a separate transaction. Restoration uses a separate eligible history Undo; import review never restores a committed entry implicitly.

An exclusion records its target revision/activity when reviewed. Saving another draft field carries that stamp unchanged; only explicit review reconfirmation refreshes it. A later correction/removal of the target makes the pending exclusion stale, withdraws the authoritative projection and blocks whole-batch commit. Historical committed exclusions remain valid against the state recorded at their original review event.

## UI scope

Committed inspection is read-only until Edit committed record is chosen. Corrections and removals show an exact preview before Apply. Cancelling preview retains the unsaved buffer. Save / Discard / Stay routes a committed Save through preview; pending navigation proceeds only after durable success.

The Committed changes page provides bounded newest-first discovery, including removed entries, and per-entry immutable history. History rows, entry details and preview allocations are virtualized. Undo controls explain ineligibility instead of offering force-undo. Source fields remain immutable, and copying a source field only changes an explicitly active editor buffer. Existing ledger filters are not broadened automatically when a correction/removal changes visibility.

## Boundaries and verification

The plain contracts are in `src/app/include/spentit/committed_changes.hpp` and `import_repository.hpp`. Storage implementation is in `src/storage/committed_store.inc`, compiled with the existing import store. Financial validation still uses the independent engine. The Qt model and QML contain no SQL or authoritative money arithmetic.

Independent storage regressions cover interleaved receipts, correction/removal/Undo, exact arithmetic, original evidence, global operation identities, preview freshness, later dependency conflicts, removed duplicate multiplicity, stale/reconfirmed exclusions, genuine legacy schemas, backup restoration and authenticated corruption. Failure tests exercise write/sync faults, process exits and both durable boundaries for each new operation. UI regressions cover guarded editing, retained buffers, explicit removal/Undo, removed reimports, and before/after/unknown acknowledgements and refresh failures.

Run the existing optional import-UI build as described in [the UI report](import-ui.md). Relevant CTest names are `committed.*` and `import.committed_ui`; the full import/engine/storage suite remains required for integration. Final executed configuration results and limitations are recorded in [project status](status.md). No private financial inputs were used. Native macOS synthetic UI checks do not establish accessibility, other-platform support or production readiness.

Verification on 2026-10-25: Release and ASan/UBSan each passed all 51 registered cases plus nine affected final-source reruns. Debug passed all 51 across separate invocations plus 17 affected final-source reruns. Native and offscreen committed-UI runs passed 7/7 each; the native correction preview was visually inspected. The independent committed fault sweeps exercised 49 positions for each of three operation kinds, for both injected I/O failures and abrupt exits, plus six explicit durable-boundary exits. Authenticated corruption coverage includes 14 cases. See project status for scope limitations.

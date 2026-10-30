# Next work: dataset-wide acceptance of new transactions

Last updated: 2026-10-29

## Owner direction and authority

The owner settled the four transaction decisions below and gave **Go for implementation on 2026-10-29**. That authorization remains in effect for this model; it does not authorize reports or unrelated changes. The owner subsequently confirmed: preserve existing account/category creation and opening-review behavior while making account rollback remove transaction work only. Do not invent a new creation or opening-cleanup policy.

This direction supersedes earlier requirements for per-account import commit, separate draft-to-committed publication, and compulsory permanent original-import evidence retention. Older feature and implementation documents describe the existing system where they conflict with this handoff; they must not silently reintroduce those requirements. Exact arithmetic, encryption, atomic durable writes, occurrence multiplicity and correction of accepted transactions remain required.

## Accepted transaction and commit model

- Imported transactions live in the normal ledger with a **new/uncommitted** status. They are saved as work proceeds and survive quitting/reopening. Uncommitted does not mean unsaved.
- Keep the word **Commit**. It means: “At this point in time, I am happy that the new transactions in these accounts reflect reality.”
- Commit accepts **all current uncommitted transactions across the dataset**, not one account at a time. Generated transfer counterparts are included. Change their status together atomically; do not require a separate copy of their financial contents merely to accept them.
- **Committed means accepted, not permanently uneditable.** Later corrections remain possible. Forgetting new transactions and correcting accepted transactions are distinct actions.
- No routine confirmation is needed for an explicit Commit with no issues. If there are review concerns, explain them, let the user navigate to affected transactions, and offer **Ignore and commit** for overridable concerns. Do not treat an uncertain application judgment as more authoritative than the user.
- On quit with new/uncommitted transactions, ask whether to commit them now. Yes commits before quitting; No quits with everything still saved and uncommitted. Reopening restores that status. A Cancel option returns to the application. A failed commit must not be presented as successful or silently lose pending work.

## Import errors and retained material

The owner rejects a required separate permanent “import evidence” archive. Loaded transactions, before and after acceptance, are the working records. Creation/matching ownership needed for correct rollback is not a requirement to preserve original files forever.

Detect import errors during import. Show successfully imported transactions as uncommitted and make failed records or incomplete import visible. Raw QIF for a failed record and manual repair are possible treatments; a clear “some data could not be imported” result is another. Accepted completion policy: successfully imported rows remain usable and can be committed while failed records remain visibly flagged until repaired or explicitly discarded. Commit never dismisses those failures. The exact error UI remains an implementation-design detail. An unreadable amount or failed write cannot be made valid by Ignore and commit. Do not invent values or silently drop failed records.

**Forget means remove the import's material, not retain a discarded-import archive.** For an entirely forgotten import, remove its exclusively owned transactions, raw records if retained, review state and matching associations. No requirement remains to retain evidence that this forgotten import occurred. This describes application data removal, not a promised forensic erasure of storage media or previously created backups. Do not add an audit tombstone solely to preserve a forgotten import. Treatment of shared material follows ownership below; account/category creation is deferred.

## Transfer creation, linking and forgetting

A transfer creates a counterpart in the other account if none exists, or links to an existing counterpart. Track that distinction so forgetting can remove newly created objects without deleting pre-existing transactions.

Accepted example:

1. Checking imports a payment to Visa and creates an uncommitted Visa counterpart.
2. Dataset-wide Commit accepts both transactions together.
3. A later Visa import contains the matching payment. Show a match without counting a second payment.
4. Forgetting that later Visa import removes its new material and matching association, leaving the Visa transaction originally generated from checking intact.

If an uncommitted origin is forgotten, its exclusively generated uncommitted counterpart goes too. If the import only linked to an existing transaction, remove that association, not the existing transaction. A committed transaction must not disappear as a side effect of forgetting a later import.

Before dataset-wide commit was selected, the owner preferred keeping an independently committed generated Visa transaction when its still-uncommitted checking origin is forgotten, ending the link instead. Dataset-wide commit eliminates that partial-acceptance case for new transactions: both sides are accepted together.

## Transaction decisions settled — 2026-10-29

- **Account-wide rollback:** the action previously called Forget rolls back all uncommitted work belonging to the selected account, including its effects in other accounts. It preserves committed transactions. Selected-row removal remains Delete; this is not a selected-import or selected-row command. “Discard uncommitted changes” is suggested wording, not a finalized UI label.
- **Created versus linked counterparts:** remove an exclusively generated uncommitted counterpart when its originating work is discarded. When the work only linked to an existing transaction, remove the association in both directions and preserve the existing transaction.
- **Shared ownership:** checking creates an uncommitted Visa entry (one source); Visa independently imports and matches it (two sources); checking rolls back (one source remains). Remove checking's contribution and transfer link, but retain Visa's transaction as uncommitted. Rollback removes only its own sources, never another retained source. The owner describes this as garbage collection/reference counting. Tracking source identities rather than only a count is a proposed implementation detail; the physical representation still needs design. Committed transactions remain protected. Matching must preserve legitimate repeated-payment multiplicity.
- **One balance:** include all active committed and uncommitted transactions. Remove the actual/projected distinction; Commit changes acceptance status, not the balance. Future reports use the same default inclusion policy, with report implementation still deferred.
- **Import errors:** successful rows remain usable and can be committed; failed records remain visibly flagged until repaired or explicitly discarded. Commit must never silently dismiss failures or invent missing values. Unrepresentable amounts and failed writes cannot be overridden into valid transactions.

## Implementation proposal and deferred scope

The four transaction policy questions are settled; do not reopen them as undecided on the next handoff.

Changing account/category creation, opening-balance handling and their cleanup is deferred. Preserve their existing behavior during the transaction-model change, as explicitly confirmed by the owner after Go. Physical schema, history representation, duplicate-recognition inputs after removing the original-evidence requirement, retry handling and efficient status updates need a reviewable implementation proposal. The subsequent owner decision below supersedes the earlier undecided validation-cache suggestion.

## Performance context

The owner reports that a fresh QIF import improved substantially after matching/alias changes, with six opening-balance confirmations remaining, but an account commit took approximately **18 seconds**. This is owner-reported end-to-end time, not a measured database-commit duration. Validation, writes, durable storage and UI refresh costs have not been separated for that workload.

The owner objects to repeating expensive readiness checks after readiness has already been established. Financial correctness is not a blanket justification for repeated scans or copying transactions. The simpler acceptance model is a product decision independent of any unproven performance claim. Measure relevant phases on synthetic workloads when performance implementation is authorized; private data requires separate authorization.

## Existing implementation — not the accepted target


Read-only inspection found that `ImportStore::commit` checks group readiness and opening confirmation, loads rows to validate decisions/transfer resolution and retain source associations, updates transfer relationships, and then loads included rows again to write their initial committed revisions, splits and metadata. It also records history and freezes the opening. These changes share one atomic database transaction. The UI subsequently refreshes the ledger and balances. Drafts and immutable evidence are already persisted before commit; commit publishes a separate authoritative interpretation. This explains the operations, not why they should take 18 seconds.

Relevant implementation: [commit](../src/storage/import_store.cpp), [group readiness and opening publication](../src/storage/import_group_store.inc), [committed revisions](../src/storage/committed_store.inc), and [UI command/refresh](../src/ui/import_model.cpp).


The current application still uses per-account publication, separate committed revisions, retained source evidence and protected drafts. Its Abort retains discarded state rather than implementing the newly requested Forget semantics. The redundant account-commit confirmation is still present. The one-balance/normal-ledger read path is implemented in the current working tree; the dataset-wide acceptance and account rollback storage changes remain incomplete.

## Next work and verification

Resume from this accepted model and the settled transaction decisions above; bring account/category creation back into scope when requested. Continue the authorized implementation using the storage proposal below; another Go is not required for this accepted scope. Do not substitute a cache-only optimization for the requested model change.

The original handoff was documentation-only. The current implementation slice and actual verification are recorded in [project status](status.md). No private data was accessed.

## Storage implementation proposal — not yet implemented

Inspection confirms that batch state currently controls transaction acceptance, duplicate lookup, transfer protection, editing, and history replay. `commit` copies drafts and allocations into initial committed revisions; blocked batches cannot publish any rows. `discard` and grouped Abort retain immutable source/history material. Wrapping those account commits in an outer transaction would not implement the accepted model.

The remaining storage work must proceed together:

1. Give each ledger transaction independent acceptance state and a stable identity, separate from import/error completion. Commit records references to existing financial revisions, not copies of their amounts and splits. Corrections of accepted transactions retain persistent revisions and strict Undo dependencies.
2. Represent ownership with identifiable source memberships. A source contribution has an account owner; a transfer relationship is not itself another ownership root. Account rollback removes that account's uncommitted contributions, then removes only uncommitted transactions with no retained owner. This avoids reference cycles between reciprocal transfer links. Preserve independently supported and committed transactions, and remove both directions of a discarded link.
3. Store import failures separately from acceptance. Successful transactions can be accepted while failed records remain reviewable and repairable; explicit discard removes a failure. Duplicate review and transfer matching must use transaction state rather than assuming every row in a batch has the same state.
4. Implement one dataset acceptance command and one account rollback command, each with a single durable boundary and retry-safe outcome resolution. Acceptance includes generated counterparts. Keep currencies separate in receipts; never add currency totals together.
5. Rework history verification around those memberships and acceptance references before enabling physical cleanup. Delete exclusively owned raw records, review state and matching associations; keep a shared source only while retained work needs it. No forgotten-import audit tombstone. Preserve existing account/category creation and opening-review behavior.
6. Wire Commit without a routine confirmation, review/Ignore for actual overridable concerns, account rollback with a cross-account effect preview, and quit-time Yes/No/Cancel. No keeps uncommitted work saved. A failed or uncertain commit must not close the document as if it succeeded.

Use a new experimental schema and reject earlier formats unchanged; do not add migration work. Verify mixed valid/failed imports, repeated matching occurrences, shared-source ownership, committed corrections/Undo after acceptance, cross-account rollback, exact balance reconciliation, encrypted backup restoration, and failure/interruption/retry on both sides of each durable boundary. The existing per-account publication tests do not establish these new guarantees.


## Commit uses current review — accepted follow-up, 2026-10-29

The owner observed another approximately 13-second commit and gave Go for this rule: imports and edits validate their changed transactions and affected relationships when they happen; Commit uses the resulting warnings. Zero warnings should not trigger another financial/duplicate review pass. Existing overridable warnings need the accepted warning prompt, not another validation pass. Storage constraints, operation identity/revision checks, exact arithmetic and atomic durable writes still apply. Reopening verifies stored data; recovery from a failed operation may reconstruct session review state.

Implemented a connection-owned prepared row review, refreshed before mutation durability and reconstructed after verified opening. Row/version and duplicate-target/source-association stamps retain unaffected reviews; transfer warnings follow current account names and resolved allocation endpoints. All mutation families route through review refresh. Rollback drops speculative results, including source/transfer caches. Commit consumes prepared inclusion, duplicate and transfer-warning results and the reviewed drafts, replacing its two source-row parsing/validation loops. Group readiness consumers reuse the current review snapshot; immutable mapped-source parsing and unchanged transfer graphs are reused. The ordinary bounded validated-row cache now follows row revisions rather than discarding every row after every write; live duplicate/association/retention fields refresh separately.

This is an incremental change to the existing publication implementation, **not completion of dataset-wide status-only acceptance**. It still writes initial committed entry revisions, allocations, associations, category publication, transfer transitions and history. Group aggregate review and dependency enumeration still scan affected/current collections; this is not a claim that every edit is constant-time. Full verification still runs at open and explicit verification. Existing blocking versus overridable-warning UI policy is unchanged. Dataset-wide acceptance, shared-source rollback, partial-error acceptance and quit prompting remain unfinished. No private workload timing improvement is claimed.

Verification and remaining limits are recorded in [project status](status.md).

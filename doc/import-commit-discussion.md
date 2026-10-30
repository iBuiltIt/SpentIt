# Next work: dataset-wide acceptance of new transactions

Last updated: 2026-10-29

## Owner direction and authority

The owner accepted the transaction/commit direction below and requested this handoff. This is documentation authorization, **not Go for implementation**. Discuss the remaining material decisions and obtain explicit Go before changing runtime behavior. Reports remain deferred. Account creation and category-import creation are explicitly outside this discussion for now.

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

Detect import errors during import. Show successfully imported transactions as uncommitted and make failed records or incomplete import visible. Raw QIF for a failed record and manual repair are possible treatments; a clear “some data could not be imported” result is another. The exact error UI and disposition of failed records are not yet selected. An unreadable amount or failed write cannot be made valid by Ignore and commit. Do not invent values or silently drop failed records.

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

**Still to settle:** before any commit, import A creates a counterpart, import B independently supplies a matching transaction, then A is forgotten while B is retained. The proposed treatment is to keep the counterpart as B's uncommitted transaction and remove A's link. That ownership handover was suggested, but not explicitly settled. Do not infer authorization to delete B's independently retained transaction or to commit it automatically. Repeated equal payments still require multiplicity-safe matching; uncertain matches must not silently collapse distinct payments.

## Remaining decisions before implementation

No further decision is needed merely to record the accepted model. Before implementation, discuss:

- **Forget scope:** all current new transactions, a selected import, selected transactions, or some combination. The discussion establishes removal semantics, not the exact available commands.
- **Overlapping uncommitted imports:** settle the A/B ownership-handover case above.
- **Balances and reports:** new transactions are normal saved ledger rows, but the discussion has not explicitly decided whether default balances/reports include them or retain a committed-only view. Do not silently carry forward the old exclusion policy or silently change totals.
- **Import error completion:** choose how failed records remain visible, are repaired or explicitly skipped, and how successful rows can be accepted while errors remain. Classify actual review concerns versus unrepresentable data using concrete examples.

Account/category creation, opening-balance handling and their cleanup are deferred at the owner's request. They must be considered before implementing the complete import lifecycle, not invented during this transaction discussion. Physical schema, history representation, duplicate-recognition inputs after removing the original-evidence requirement, retry handling and efficient status updates need a reviewable implementation proposal. Validation caching is only an earlier suggestion, not an adopted design.

## Performance context

The owner reports that a fresh QIF import improved substantially after matching/alias changes, with six opening-balance confirmations remaining, but an account commit took approximately **18 seconds**. This is owner-reported end-to-end time, not a measured database-commit duration. Validation, writes, durable storage and UI refresh costs have not been separated for that workload.

The owner objects to repeating expensive readiness checks after readiness has already been established. Financial correctness is not a blanket justification for repeated scans or copying transactions. The simpler acceptance model is a product decision independent of any unproven performance claim. Measure relevant phases on synthetic workloads when performance implementation is authorized; private data requires separate authorization.

## Existing implementation — not the accepted target


Read-only inspection found that `ImportStore::commit` checks group readiness and opening confirmation, loads rows to validate decisions/transfer resolution and retain source associations, updates transfer relationships, and then loads included rows again to write their initial committed revisions, splits and metadata. It also records history and freezes the opening. These changes share one atomic database transaction. The UI subsequently refreshes the ledger and balances. Drafts and immutable evidence are already persisted before commit; commit publishes a separate authoritative interpretation. This explains the operations, not why they should take 18 seconds.

Relevant implementation: [commit](../src/storage/import_store.cpp), [group readiness and opening publication](../src/storage/import_group_store.inc), [committed revisions](../src/storage/committed_store.inc), and [UI command/refresh](../src/ui/import_model.cpp).


The current application still uses per-account publication, separate committed revisions, retained source evidence and protected drafts. Its Abort retains discarded state rather than implementing the newly requested Forget semantics. The redundant account-commit confirmation is still present. None of the new behavior above is implemented.

## Next work and verification

Resume from this accepted model, settle the remaining transaction decisions, then bring account/category creation back into scope when requested. Prepare the bounded implementation proposal and wait for Go. Do not substitute a cache-only optimization for the requested model change.

This handoff changes documentation only. No application launch, private-data access, runtime tests or performance measurements were performed. Prior implementation evidence remains in [project status](status.md). Documentation checks for this update are recorded there.

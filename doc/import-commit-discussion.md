# Next session: rethink importing and committing

Last updated: 2026-10-29

## Owner direction

The next session is a discussion of the **whole approach to importing and committing transactions**, not an instruction to implement another isolated optimization. Discuss the model and tradeoffs first; wait for an explicit Go before changing behavior. Reports and other deferred work remain deferred.

The owner reports that a fresh single-QIF import into a new document (no starter categories, no password, otherwise defaults) worked substantially better after the repeated-transfer and account-alias changes. The remaining six blockers were opening-balance confirmations. This is owner-reported experience, not agent verification of private data.

## Objection to repeated commit checks

The owner reports an approximately **18-second account commit**. Their objection is that the blocker stage has already checked readiness: when there are no blockers and nothing has changed, repeating expensive checks during commit appears redundant. Either the readiness result is trustworthy and reusable, or the UI is presenting an incomplete assurance. Do not dismiss this objection by invoking financial correctness as a blanket justification for repeated scans.

The 18 seconds is an end-to-end observation, not a measured database-commit duration. No phase timings were captured for that private workload. The relative costs of validation, committed-record writes, durable storage and UI refresh remain unknown.

## Verified current implementation

Read-only inspection found that `ImportStore::commit` checks group readiness and opening confirmation, loads rows to validate decisions/transfer resolution and retain source associations, updates transfer relationships, and then loads included rows again to write their initial committed revisions, splits and metadata. It also records history and freezes the opening. These changes share one atomic database transaction. The UI subsequently refreshes the ledger and balances. Drafts and immutable evidence are already persisted before commit; commit publishes a separate authoritative interpretation. This explains the operations, not why they should take 18 seconds.

Relevant implementation: [commit](../src/storage/import_store.cpp), [group readiness and opening publication](../src/storage/import_group_store.inc), [committed revisions](../src/storage/committed_store.inc), and [UI command/refresh](../src/ui/import_model.cpp).

## Discussion agenda — no design adopted yet

- What should import, review, draft and commit mean to the user? Is the current separation useful, and which steps should be visible?
- Does publishing a separate committed copy justify its storage and processing costs, or should the representation change while retaining evidence and history?
- Which checks belong in readiness, which must remain at the atomic write boundary, and how should stale readiness be explained?
- Could a retained validation result be reused when all relevant revisions are unchanged? This was suggested in conversation, **not accepted as a complete design**. Any design must account for cross-account transfers, category/account changes, duplicate targets, retries, reopening and concurrent/background changes within the application.
- How should per-account commit, linked destination drafts, opening-balance review and whole-import Abort fit the overall model?
- Measure validation, publication writes, durable completion and refresh separately before attributing the delay or selecting an optimization. Use synthetic workloads by default; private data access requires separate authorization.

Exact money, immutable evidence, occurrence multiplicity, atomicity, durable outcome recovery and history remain requirements. The discussion has not authorized weakening them or changing the existing per-account commit contract.

## Commit confirmation dialog

The owner explicitly wants the redundant “Commit this account’s reviewed import?” confirmation removed. The discussed proposal is a direct **Commit account** button using existing validation and atomic publication. **Not implemented; no Go for this change was given.** Carry it into the broader discussion rather than silently treating the dialog as already removed.

## Handoff state

Repeated-transfer matching and account aliases/category destinations are implemented; see [status and actual verification](status.md). This handoff records the next discussion only. Do not launch the app against private data, start profiling, redesign storage, remove the dialog or implement validation caching merely from this document.

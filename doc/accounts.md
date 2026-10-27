# Account-scoped durable QIF workflow

Last updated: 2026-10-26

This implements the multi-account prerequisite for [transfer lifecycle work](../features/008-transfer-matching-and-lifecycle.md). Accounts can be created and selected in the durable UI, with independent ordinary QIF imports, opening balances, drafts, committed balances, corrections and history. The subsequent [bounded transfer integration](transfer-engine.md) adds links, generated counterparts, protected drafts and provisional evidence matching. [QIF transfer destinations](qif-transfers.md) now require explicit resolution through those commands. This remains a synthetic-data experiment.

## Persistence and command boundary

`ImportRepository` exposes account enumeration, creation, creation-outcome lookup and selection. Commands run on the existing serial worker. Each account has a positive stable integer identity and a retained currency definition; setup creates account 1. Batch identities and operation identities remain document-wide. Every batch has immutable account ownership and each account may have its own pending or retained-evidence import.

The selected account is session state, initially account 1 on reopen. SQL predicates bind to that selection through a connection-local function that only returns its integer identity. It performs no database access or monetary arithmetic. Record commands additionally check batch ownership, rejecting cross-account requests rather than silently interpreting amounts in the selected currency. Selecting an unknown account leaves the previous selection intact. Categories and saved rules remain document-wide; rule Apply affects only the selected account's pending batch.

Duplicate recognition stays within an account, including identical-file recognition. The same file imported into another account is independent evidence, not permission to suppress its transactions. Committed and projected totals contain only the selected account, with its exact currency and opening balance. Ordinary bulk operations cannot span accounts.

Creation atomically records the account and an immutable creation receipt. Reusing an account identity with the same name, currency definition and original date is an idempotent retry, even after its opening balance changes; different creation content is rejected. A durable outcome lookup supports lost acknowledgements. An unreadable outcome closes the UI session for reopen and inspection. Failed post-creation selection or refresh reports that creation was durable and closes the session.

The document-wide financial sequence still orders operations and invalidates stale previews. Verification replays that sequence into separate account balances, using each batch owner's currency. It verifies every account, including accounts not currently selected. Backups contain all accounts, source evidence, history and creation receipts and are verified by restoring and reopening them.

## UI behavior and limits

The sidebar has a scrolling account list and **+ New account**, which opens an inline name field; Enter creates the account. New accounts start with zero on today's date, defaulting to the initial setup currency. Opening balance must still be explicitly confirmed before the first import commit; that account's first commit freezes it. Switching accounts uses the existing Save / Discard / Stay guards for unsaved transaction, category and rule edits.

Switching clears previews, receipts and history from the previous account and loads the selected account on the worker. The transfer integration now restores each account’s transaction selection, search, sort and viewport within the session. Back/Forward traverses visited account screens and honors unsaved-edit guards. Counterpart navigation reveals its row and honors the same guard. Account rename/archive/delete and currency reassignment are not added here. There is no consolidated cross-currency balance.

## Experimental formats

Rules-enabled builds create and open schema **12**; rules-off builds create and open schema **13**. These are distinct experimental layouts, not an upgrade sequence between builds. Both include account ownership and creation receipts; only schema 12 contains rule persistence and bulk receipts. Older experimental formats and the other build's layout are rejected before storage-setting changes, without migration or modification of their contents. This follows the owner's pre-release format policy; existing compatibility fixtures now test rejection rather than continued operation.

## Verification

See [project status](status.md#account-scoped-durable-workflow--2026-10-26) for actual configurations and results. The new `accounts.basics` checks independent simultaneous drafts, exact CAD/JPY balances, identical imports in separate accounts, stale cross-account commands, scoped rules/bulk changes, correction/Undo, creation retries, restart and restored backups. `accounts.validation` checks malformed authenticated account ownership and creation records. `accounts.io` and `accounts.crashes` sweep account creation and second-account commit writes/syncs, plus both durable-commit boundaries, and verify all-or-none results and identical retries after reopening.

The UI tests cover account switching, unsaved-edit Stay/Discard, independent commits, reopen, lost creation acknowledgements, unknown outcomes and failed selection/refresh after known creation. The existing shared navigation tests exercise Save as well. No personal financial inputs are used. Other-platform, physical-power-loss and full accessibility results are not inferred from these tests.

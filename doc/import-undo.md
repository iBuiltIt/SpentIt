# Undo an uncommitted import

Accepted owner Go: 2026-11-03. Implementation and verification are recorded in
[status](status.md). Formats remain **46 with rules / 45 without**.

## Accepted behavior

- Account context menu **Undo last import…** targets the newest eligible source
  imported into that account. It is disabled when there is no eligible import;
  generated transactions alone do not enable it. A duplicate-only or failed import
  can still have contributions worth undoing.
- Review **Undo import…** opens an account selector and newest-first import list.
  Choose an import, inspect its affected accounts and removal counts, then confirm
  **Undo import**. Cancel changes nothing.
- Both entry points undo the entire selected file import, including a file mapped
  to several accounts. The existing **Discard new transactions…** remains a
  different, account-wide operation.
- Earlier imports and their edits remain. Undo removes only this source's
  contributions, failures and exclusively owned new transactions. A transaction
  supported by another source remains; removing its final source later collects
  it. An accepted transaction is never deleted by import undo.
- A later import can match a committed transfer. Undoing the later import removes
  its match and restores the prior transfer arrangement, including a pre-existing
  generated counterpart. A Review Link into committed work is reversed through
  persistent history when its recorded transaction and link results are unchanged.
  Later conflicting edits/links refuse the operation atomically.
- Account/category creation, confirmed account aliases and opening settings remain,
  as with account-wide discard. Undo does not move the mistaken import into another
  account; import the file again into the correct account afterward.
- An import containing accepted transactions is not eligible for whole-import undo.
  Account-wide discard remains available for its remaining uncommitted work/errors.

## Import identification and format decision

Existing documents retain source contents, batches, account mappings and source
associations, but not original filenames. The owner explicitly chose to retain
current-format compatibility instead of introducing a new experimental format for
filename metadata. The picker uses file type, transaction date range, record count
and newest-first order. Internal source IDs are not displayed. Identical-looking
imports are distinguished by list position; filenames are not promised after restart.

## Storage and recovery

The repository exposes source enumeration, a mutation-generation-bound preview,
atomic undo and durable outcome lookup. UI code owns neither SQL nor financial
transaction boundaries. Source identity scopes the command; existing ownership
associations decide which rows survive. Scope is not inferred from dates or from a
transaction being the most recently displayed row.

Ordinary cleanup extends the account rollback machinery to one source. Its
connected transfer history must not expand deletion into an unrelated link.
Source material required by retained transactions remains. Explicit rollback
history permits a shared ledger row to remain after its original source relinquishes
ownership; its remaining owner is required by verification.

Review Links touching accepted transactions use recorded prior snapshots and append
reversal history. Imported records required to replay that retained history remain
stored in a discarded batch, excluded from the ledger and import-undo list. This is
retention for the surviving transaction's history, not a general forgotten-import
archive. When such an import also contains rows owned by another source, undoing that
other import first can be required; no force override is provided.

Both paths verify before durability and store a retry receipt in the existing
one-slot rollback receipt. Retry identities include the selected source. A stale
preview refuses; an unknown durable outcome closes the document and asks for
reopening. A lost acknowledgement is resolved from the receipt. A refresh failure
after success reports that the import was undone and requires reopening.

## Verification obligations

Use synthetic imports for wrong-account A/A then B/A, earlier/later shared sources,
duplicate-only imports against accepted records, mapped multi-account files,
invalid rows, stale previews, retries, original draft edits, generated counterparts,
committed transfer matches, explicit Review Link reversal, reopen and restored
backup. Exercise disk-full/write failures and process exits before/after durability.
Check both UI entry points, empty-list/disabled behavior and account filtering.

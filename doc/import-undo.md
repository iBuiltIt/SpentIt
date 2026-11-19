# Undo an uncommitted import

Accepted owner Go: 2026-11-03. Implementation and verification are recorded in
[status](status.md). The owner approved removing account-menu Undo on 2026-11-18.

## Accepted behavior

- File-wide **Undo** reverses the latest saved action. When that action is an
  import, Undo rolls back the whole source regardless of the selected account.
  A duplicate-only or failed import can still have contributions worth undoing.
- Review **Undo import…** opens an account selector and newest-first import list.
  Choose an import, inspect its affected accounts and removal counts, then confirm
  **Undo import**. An older import requires an empty file-wide Undo stack.
  Cancel changes nothing. See [older imports](action-undo-design.md#older-imports-and-discard).
- Both commands affect the entire file import, including a file mapped to several
  accounts. After Undo of acceptance, a link between two batches in the same
  import is cleared by that import's Undo regardless of the batches' order.
  **Discard new transactions…** operates on the account.
- Earlier imports and their edits remain. Undo removes only this source's
  contributions, failures and exclusively owned new transactions. A transaction
  supported by another source remains; removing its final source later collects
  it. An accepted transaction is never deleted by import undo.
- When a later import automatically links two bank postings, Undo restores the
  earlier posting and any generated counterpart it had before the import.
  This applies in either transfer direction, including after Undo of acceptance.
  If the import also linked postings between its own accounts, the source is
  removed as one operation in either account order.
- Preview checks the saved results of the match against the current postings and
  links. A conflicting edit to any pair blocks the whole Undo. The file-wide
  Undo control becomes unavailable and Change history explains the refusal. After
  closing and reopening, eligible automatic matches can still be reversed from
  **Review → Undo import…** using their saved transfer history.
- Account/category creation, confirmed account aliases and opening settings remain,
  as with account-wide discard. Undo does not move the mistaken import into another
  account; import the file again into the correct account afterward.
- An import containing accepted transactions is not eligible for whole-import undo.
  Account-wide discard remains available for its remaining uncommitted work/errors.
- An import that still has financial history, while another import holds its
  transactions, is omitted from **Undo import…**. On Change history the row
  stays and names the later import to undo first. When no later action would
  make Undo possible, the row is left off the page. Undoing the holding import
  makes Undo available. Change history and the save use that same check. There
  is no force override.

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
other import first can be required. **Undo import…** omits it until that other
import is undone. Change history keeps the row and names the import to undo
first. No force override is provided.

Both paths verify before durability and store a retry receipt for the operation. Retry identities include the selected source. A stale
preview refuses; an unknown durable outcome closes the document and asks for
reopening. A lost acknowledgement is resolved from the receipt. A refresh failure
after success reports that the import was undone and requires reopening.

## Verification obligations

Use synthetic imports for wrong-account A/A then B/A, earlier/later shared sources,
duplicate-only imports against accepted records, mapped multi-account files,
invalid rows, stale previews, retries, original draft edits, generated counterparts,
committed transfer matches, explicit Review Link reversal, reopen and restored
backup. Exercise disk-full/write failures and process exits before/after durability.
Check file-wide Undo across accounts and the Review picker, including
empty-list behavior and account filtering.

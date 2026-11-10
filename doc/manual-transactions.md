# Manual transactions

Last updated: 2026-11-09

Manual and imported postings share transfer assignment, explicit Link, editing,
and Undo. A manual entry has no imported original until Review Merge attaches
one. Rules still match imported fields until current-value matching is
implemented. Database formats are **70/69**.

## Accepted interaction

The ledger context menu offers **Add transaction…**. A **+ New transaction** button (owner follow-up Go, 2026-11-05) immediately
right of search **Apply** offers the same action, with an Add transaction tooltip.
An existing unsaved editor uses Save / Discard / Stay before starting another.
Save must succeed before continuing.

A new unsaved row opens in the ledger editor. Its date defaults to the focused
selected transaction, or today with no selected transaction. Payee and memo are
blank; the amount is zero at the account currency's scale; category is
Uncategorized. The row remains visible through the current search. Enter saves
the complete validated transaction. Escape cancels creation. Leaving asks
Save / Discard / Stay. An unsaved creation does not affect durable balances or
survive closing the process.

On saving, the transaction joins the new/unaccepted ledger and balance, survives
restart, and participates in dataset-wide Accept transactions. Date changes sort
on save and the editor follows the saved identity. Split sums must balance.
Account destinations become available after the first save, through the existing
atomic transfer assignment.

Show original imported transaction is disabled for an unmatched manual entry,
with **Manually entered** beside it. A later confirmed import match makes the
attached original available. Restore imported values remains unavailable on a
manual survivor; its manual allocations have no imported split identities.
Manual creation is excluded from Undo last import;
account-wide Discard applies to unaccepted manual work as well as imported work.
The standard Undo shortcut (Cmd+Z on macOS, Ctrl+Z on Windows/Linux) in a clean
editor removes the just-created row until another dataset mutation. Per the
owner’s follow-up, there is no temporary Undo add transaction button. Subsequent edits use the
existing session edit Undo sequence. Creation Undo does not survive reopening.

## Rules and deleting saved transactions — fixes authorized 2026-11-03

Rules use a manual transaction's current payee/memo for transaction-level matching.
They may categorize Uncategorized allocations while preserving manual text. No
imported split identities are invented; imported-split rules do not match manual
allocations. A rule-assigned category remains valid on later evaluation and reopen.
If rule checking fails, the editor explains that rules could not be checked and
that the transaction can still be edited; it does not display internal validation
terminology.

Delete selected supports saved new transactions after reopening, including mixed
new/accepted selections. It asks for confirmation after resolving unsaved editor
changes. Deleting does not accept a new transaction. One atomic operation removes
the selected rows from the ledger and balance; other imported rows remain. Deleted
new rows are skipped by rules and acceptance, and cannot silently consume a later
bank import. The session Undo command restores the whole selection and balance.
New linked transfers must be unlinked first; the whole selection is refused if
one such transaction is present. Accepted-only selections retain the existing
bulk deletion preview and persistent group history/Undo.

New-row deletion derives from the existing encrypted selection journal and its
Undo records. These fixes keep formats 48/47 and work with existing manual entries
in those formats. No recreation of a manual entry is needed.

## Right-click category assignment — fix authorized 2026-11-03

Saved manual transactions support ordinary category and Uncategorized assignment
from the ledger context menu, including accepted entries and mixed selections
with imported rows. Storage validates manual/imported identity against the stored
transaction instead of requiring an imported original. Existing split selection,
transfer restrictions, atomic group saving and Undo remain in force. Category
assignment does not create an imported original or accept a new transaction.
Reopening preserves the category and eligible group Undo. Formats remain 48/47;
existing manual entries need no recreation. See [status](status.md) for checks.

## Explicit matching

A later bank import suggests a manual match only in the same account/currency,
with the same date and exact signed effective amount. It never automatically
recognizes an unmatched manual transaction using those fields or payee text.
Review Merge retains the manual transaction's interpretation, including categories,
splits and notes, and attaches the imported occurrence. When several manual entries
are selected, the user chooses which manual entry to keep. Review Undo restores the
separate rows and original associations atomically. Subsequent imports can use
that confirmed imported occurrence for the existing reimport recognition policy.
An explicit selected-row Merge may reconcile different dates; amounts must agree.

## Storage and verification

Formats 48 with rules / 47 without add an explicit manual creation encoding to
the existing encrypted transaction storage. Manual creation has no original
imported amount and no bank-file bytes. Its internal journal payload retains the
initial values for verification and exact retry; it is never displayed as an
import. Older experimental formats are rejected unchanged, without migration.

Verification results and remaining limits belong in [status](status.md).

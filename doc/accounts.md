# Accounts and money

Money is exact decimal minor units with a currency definition identity. The
pinned catalog is the current selection. Retained historical definitions restore
by identity and are never removed or reinterpreted. Arithmetic requires the same
definition. There is no date-based or code-only historical lookup. Missing
definitions fail. See [provenance](../third_party/iso4217/README.md).

The engine split editor is in [split editing](split-editing.md).
Ledger split behavior, amount expressions, and amount formatting are in
[ledger editing](ledger-editing.md).

## Accounts

Each command is atomic. An identical operation id returns the stored receipt.
A conflicting reuse is rejected. Success is reported only after durable
completion.

- **Rename** changes the display name only. The creation name stays, so a later
  import that repeats it does not retarget the renamed account. Empty names are
  rejected. Balances, currency, openings, evidence, and links stay.
- **Archive** keeps the account readable in a separate sidebar section. It is
  not a target for new imports, new transactions, or new transfer matches.
  Archive is refused while that account has a pending import. Reactivation
  restores ordinary use and changes no amounts.
- **Delete empty account** retires the identity permanently. It requires no
  current ledger transactions, a zero opening, no unfinished blocked import or
  live transfer participation or import rollback that would restore it, no
  saved-report selection, and another account
  remaining. Past imports, edits, opening dates and corrections do not prevent
  deletion. Retain the identity and financial history internally; a creation
  retry cannot resurrect it. If an active Undo or Redo depends on this account,
  explicitly confirm clearing the file-wide stacks and save that expiry with
  deletion atomically. Otherwise preserve both stacks. Cancel and failed saves
  preserve the account and history. This is not an Undo step.
- **Correct opening** changes the current opening date, amount, or both. It
  does not change transactions or source facts. A new opening date later than
  any transaction on that account is refused. The stored opening is used
  immediately, including zero. There is no separate opening confirmation.
  Double-click the ledger Opening Balance row to edit it.
- **Currency reassignment** keeps the numeric decimal and rescales to the new
  currency. A non-zero fractional digit that would disappear refuses the whole
  operation. Source bytes and original imported amounts stay in the old
  currency. No rate is stored. Do not add an exchange rate, a combined
  multi-currency total, a home-currency change, or a currency definition
  outside the retained set. A linked transfer changes only this account; the
  other side stays, and the two amounts are not forced equal. New transfer
  matches still require the same currency. The dialog warns that this is not a
  conversion.

## Account names and aliases

The display name is the creation name or the latest explicit rename. Import
naming does not replace it. The alias list is the creation name, rename
history, and explicitly confirmed named-QIF mappings. Matching is Unicode
case-insensitive and literal, with no fuzzy matching. A name that conflicts
with another account alias or an ordinary category is refused.

Alias reads use bounded caches invalidated by writes. A rollback must not
publish a transactional alias cache. Original import bytes stay unchanged.

See [acceptance and Discard](import-commit-discussion.md).

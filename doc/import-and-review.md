# Import and review

Supported imports are the [QIF banking profile](qif-profile.md) and the
[OFX, QFX, QBO, and ASO statement profile](statement-profile.md). Further dialects
are outside the supported scope. Investment and other non-banking QIF
sections are refused. An OFX transfer destination stays a visible blocker.
Named QIF account sections, explicit mapping, and transfer
destinations are in [qif account imports](qif-account-imports.md)
and [qif transfers](qif-transfers.md).

## Opening a statement file

A QIF, OFX, QFX, QBO, or ASO file can start the same import as **Import…**.

- Double-click, Open With, and a drop on the Dock icon use the sidebar
  **Import…** button. No account is chosen yet.
- A drop on a sidebar account uses that account's **Import…**, including when
  another ledger is open. The ledger changes only if that import continues.
- A drop on the open account ledger uses that account's **Import…**. A report's
  transaction list, Review, Rules, Categories, Settings, and empty space use
  the sidebar **Import…** button.
- An archived account asks you to reactivate it and does not import.
- One statement at a time. Several files, or a second file while an import is
  in progress, stops with a message. A busy window asks you to wait and try
  again. An unsaved edit asks you to save or discard first.
- If no SpentIt file is open, or it is waiting for a password, the statement
  waits. Opening, creating, or unlocking the file then starts the import into
  that file. A file that is already open stays open and receives the import.
- A `.spentit` file still opens that file, or leaves the open file in place.
  Any other file is refused. `.spentit.json` is not a statement.
- The command line accepts one statement path and imports it after the
  remembered file opens. Finder uses the app bundle. macOS lists SpentIt in
  Open With and does not take ownership of these types. Windows and Linux
  installers do not register them.

An unnamed QIF file with no chosen account asks for a destination. A QIF
source that already matches one account uses that account. Several source
accounts open the mapping dialog. An ambiguous QIF date asks for the date
order. OFX-family sources always open explicit account mapping.

## What import does

- Source bytes stay immutable. Rules and manual edits do not change them.
- A structurally intact record can be repaired for a date or an amount. Unknown
  fields, bad encoding, malformed destinations, and incomplete splits stay
  blocked. Unavailable original money stays unavailable. No zero is invented.
- Every new-account mapping chooses zero or the reported amount literally.
  Omitting that choice refuses the import before capture. Existing accounts
  keep their openings. Later changes use Correct opening.
- The ordinary balance includes new and accepted rows. Accept transactions
  accepts new rows across the file and leaves visible failures in place.
  Duplicate-only sources can complete without accepting older work.
- Account Discard removes that account's exclusively owned uncommitted material
  and preserves accepted transactions and other sources. It requires an empty
  Undo chain.
- Incomplete preparation is cleaned up on cancellation or the next open.
  Completed uncommitted imports survive restart. Closing is not acceptance.
- File-wide **Undo** reverses the latest saved action, including a whole import
  when it is the latest action. Review **Undo import…** can remove an eligible
  older import when the Undo stack is empty. Accepted transactions are not
  deleted. Created accounts, categories, aliases, and openings remain.
  See [import Undo](import-undo.md).

## Matching

Automatic recognition uses saved original facts.

- When both sides have bank IDs, the strict bank-ID rule is authoritative.
  A repeated incoming ID stays unresolved. Removed targets and conflicting
  facts do not auto-match.
- Without that rule, a unique same-account match on original account, currency,
  exact amount, calendar date, and exact nonempty payee is recognized.
  Repeated keys, malformed originals, and removed targets stay reviewable.
  There is no case folding or whitespace normalization. Automatic recognition
  does not widen to an adjacent day.
- An uncertain match is not permission to drop a transaction. Unresolved rows
  stay in the balance until the user decides.
- Reimport of a removed transaction stays unresolved for an explicit decision.

Local Auto Merge and Auto Link preferences can resolve eligible one-to-one
Review pairs after import and rules. Automatic decisions are part of the import
operation. The [Review contract](review-sheet.md) defines eligibility, date
windows, provisional links, staged decisions and Undo.

See [dataset-wide acceptance](import-commit-discussion.md).

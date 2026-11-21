# 003 — Import and review

Status: **active**
Last updated: 2026-11-17

Supported imports are the [QIF banking profile](009-qif-profile.md) and the
[OFX, QFX, QBO, and ASO statement profile](003-ofx-profile.md). Further dialects
are not authorized. Investment and other non-banking QIF sections are not
imported. An OFX transfer destination stays a visible blocker. There is no
fuzzy match and no cross-account date tolerance. Same-account review can still
ask about a date one day off. That question is in
[doc/review-sheet.md](../doc/review-sheet.md). It is not automatic recognition.
Named QIF account sections, explicit mapping, and transfer
destinations are in [doc/qif-account-imports.md](../doc/qif-account-imports.md)
and [doc/qif-transfers.md](../doc/qif-transfers.md).

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

The existing import questions still apply. An unnamed file with no chosen
account asks for a destination. A file that already matches one account uses
that account. Several accounts open the mapping dialog. An ambiguous QIF date
still asks for the date order.

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
  See [import Undo](../doc/import-undo.md).

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

Two Settings preferences automatically Merge or Link
mutually one-to-one Review pairs after import and rules. They start on when
this file path has no saved choice. A saved off choice stays off. They leave transaction
acceptance explicit. Same-day pairs take priority, followed by mutually unique adjacent-day pairs.
Ambiguous relationships, survivor choices,
multiple eligible split pairs, and amount differences stay manual. A unique
matching split can link without changing the other allocations. Those pairs are
one save inside the import, not separate Undo steps. Cancel saves none of them.
Manual and automatic keep-one merges both show **Merged**, even when the
kept transaction was already accepted. Accept, Undo of the merge, or Unmerge
clears the mark. The exact eligibility, local preference scope, failure
behavior, and Undo contract are in the automatic-review section of
[doc/review-sheet.md](../doc/review-sheet.md).

With Auto Link enabled, a manual Review Link also resolves remaining unambiguous
pairs in its original connected suggestion group. That manual decision and its
follow-ons share one ordinary Undo step, separate from import Undo. Acceptance
stays explicit. See [follow-on links](../doc/review-sheet.md#automatic-follow-on-links-after-a-manual-link--owner-go-2026-11-16).
**Apply decisions** saves pending transfer links, rejections and checked
**Accept without bank match** decisions together. Select waiting-only
transfers individually or by the account holding the generated entries.
Review card Link now saves only its pair; individual saves preserve unrelated
pending choices. Exact one-to-one suggestions sort first and start checked.
Differing amounts start unchecked and single-amount candidates show
**Amounts differ — review before linking**. Choose the candidate and use Link
to resolve the difference before saving; Apply decisions omits that card.
Untouched multiple-choice cards are omitted. See
[pending decisions](../doc/review-sheet.md#pending-transfer-decisions--owner-go-2026-11-17).

Review behavior is in [doc/review-sheet.md](../doc/review-sheet.md). Dataset-wide
acceptance is in [doc/import-commit-discussion.md](../doc/import-commit-discussion.md).

# 008 — Transfer matching and lifecycle

Status: **active**
Last updated: 2026-11-18

Transfers are explicit links between two allocations. A generated counterpart
stays in one currency. An explicit Link may join two existing allocations in
different currencies and keeps each posted amount. The engine
contract is [doc/transfer-engine.md](../doc/transfer-engine.md). QIF bracket
destinations resolve through [doc/qif-transfers.md](../doc/qif-transfers.md).
Rules cannot create transfers. The ledger category menu can assign an account,
which uses the existing transfer commands.

Manual and imported postings share transfer assignment, explicit Link, editing,
and Undo. A generated counterpart and an independently imported posting stay
distinct.

The new-transaction editor offers same-currency account destinations on its
Category fields before the first save. The original and all generated
counterparts save together in one Undo step. See
[manual transactions](../doc/manual-transactions.md#accepted-interaction).

## Lifecycle

- Generating a counterpart uses the origin date provisionally and an exact
  opposite amount in the same currency definition. Cross-currency generation
  fails. The signed-minimum negation fails as overflow.
- Accepting one side protects a purely generated draft on the other side. It
  does not accept that other side. Clearing the protected draft keeps it.
  Clearing an ordinary imported origin removes its purely generated uncommitted
  counterpart. An independent counterpart is unlinked and left in place.
- Both accounts still accept explicitly. Matching or linking does not accept a
  row by itself.
- Confirmed QIF account mappings may pair unambiguous reciprocal postings:
  opposite amounts, the same currency, and the same date. Equivalent repeated
  groups with equal eligible counts pair one-to-one. Unequal counts and other
  differences stay reviewable. No fuzzy name match and no date tolerance.
- Every unresolved explicit transfer gets a generated counterpart, even when a
  plausible imported posting exists. Review Link retires a generated duplicate.
- The ledger marks both rows when that payment is shown twice in one account.
  Ledger Merge joins the bank lines and removes the created entries, using the
  same command as Review Link. Link to other account is a separate command for
  one selected transaction. A kept merge and a resolved link stay marked on new
  transactions until acceptance. The contract is
  [review sheet](../doc/review-sheet.md).
- Selecting the imported bank posting and the eligible extra entry in one
  account lets ledger Merge override a saved **Not this transfer** answer for
  that pair. It saves as one Merge action. Undo restores the extra entries and
  the prior answer. See [Merge after Not this transfer](../doc/review-sheet.md#merge-after-not-this-transfer).
- Link of unequal amounts in one currency is refused unless the user uses the
  amount-difference action, which splits the larger allocation, categorizes
  the remainder, and links the common amount. No fee and no rate are invented.
- Link of two existing allocations in different currencies keeps both posted
  amounts. One amount must be outgoing and the other incoming. No rate is
  stored or applied. Choosing another account as the category still creates a
  same-currency counterpart and refuses a different currency.
- The automatic Link
  setting starts on when this file path has no saved choice, and a saved off
  choice stays off. It resolves mutually one-to-one, exact-amount Review pairs after import
  and rules, prioritizing same-day pairs before remaining mutually unique pairs
  throughout the existing seven-day window. Exactly one eligible allocation pair
  may link even when other splits exist; those splits stay unchanged. Multiple eligible allocation pairs and amount
  differences stay manual. Other suggestions require
  an explicit choice. The links are one save inside the import. Cancel saves none
  of them. Unmerge still breaks one link. A later import with another
  eligible match [reopens a provisional link](../doc/review-sheet.md#reopen-an-automatic-link-after-a-later-import)
  between two New postings for a decision in Review. Undo of that import
  restores the earlier link.
  With Auto Link enabled, a manual Review Link also links remaining mutually
  unique pairs in its original connected suggestion group. The manual choice
  and follow-ons are one atomic save and one Undo step; unresolved choices stay
  manual. See [follow-on links](../doc/review-sheet.md#automatic-follow-on-links-after-a-manual-link--owner-go-2026-11-16).
  **Apply decisions** saves staged links, Not this transfer answers and
  checked **Accept without bank match** decisions as one atomic operation
  and one Undo. Waiting-only cards can be selected by the account holding
  the generated entries. See [batch acceptance without a bank match](../doc/review-sheet.md#batch-accept-without-a-bank-match--owner-go-2026-11-18).
  Review shows a reciprocal candidate pair once, on its outgoing card when
  possible. A choice offered only from the other side stays available.
  See [one question per transfer](../doc/review-sheet.md#one-question-per-transfer--owner-go-2026-11-12-and-2026-11-18).
  One-to-one suggestions sort first and start
  checked; untouched multiple-choice cards stay out. Review card Link saves
  only its pair and preserves unrelated pending decisions, as does individual
  Not this transfer. Inferred links stay beside related cards; changing a
  partner recalculates automatic choices without overriding explicit decisions.
  This refines the prior follow-on behavior for Review cards.
  See [pending decisions](../doc/review-sheet.md#pending-transfer-decisions--owner-go-2026-11-17).
  See [automatic review](../doc/review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
- **Accounts that don’t need bank matches** omits waiting-only Review cards
  for generated entries in listed accounts. **Accept transactions** records
  their unmatched decisions in the same file-wide save. Possible matches
  stay in Review. See [account preference](../doc/review-sheet.md#accounts-that-dont-need-bank-matches--owner-go-2026-11-18).

## New transfer dates

A [date correction](../doc/ledger-editing.md#correcting-the-date-of-a-new-transfer)
on a New transfer preserves its link and is one Undo step. An eligible New
generated counterpart follows the original's correction. Independent or
accepted counterparts keep their dates. Attached or proposed bank matches
protect the dates.

## New manual transfer edits

A [New manual posting](../doc/ledger-editing.md#editing-a-new-manual-transfer)
remains editable after linking. A generated New counterpart follows eligible
changes to the original in the same operation. Review refreshes and one Undo
reverses the edit across accounts. The editor asks before breaking links and
preserves independent and accepted postings.

## Committed edits

- Merging a purely generated committed duplicate with an independent row of the
  same amount inactivates the generated row and links the independent row. A
  generated row that has its own import evidence is not removed.
- Changing the category of one committed side changes
  only that side and ends the link. A committed counterpart is not deleted. A
  purely generated uncommitted counterpart is removed in the same operation.
- An amount correction on one side of a both-committed link changes that side
  and ends the link. A date-only correction keeps the link and does not change
  the other side.
- A link that still has attached or proposed evidence refuses the correction.
- Category assignment, including Uncategorized, releases selected live links
  atomically through the transfer-aware command, including imported bracketed
  destinations. Original imported allocations remain available. Bulk deletion
  skips live linked rows; break or release the links first.
- Deleting one linked pair from either account lets the user keep the other
  posting or delete both. See [Delete a selection](../doc/ledger-editing.md#delete-a-selection)
  for the category, confirmation and Undo behavior.
- A later edit, a newer transfer relationship, or a newer source association
  blocks Undo. The entry's own publication association does not.

## Empty account cleanup — owner Go 2026-11-17

Released and unresolved relationship snapshots remain historical identities. They
do not by themselves block deletion of an account with an empty current ledger
and zero opening. Current transfer dependencies and eligible import inverses
that would refill the account still block deletion. Dependent ordinary Undo/Redo
requires confirmation before its atomic expiry with account deletion. Review
**Correct** opens the affected transaction even when its ledger editor was closed.

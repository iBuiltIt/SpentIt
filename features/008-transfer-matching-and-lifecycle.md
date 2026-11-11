# 008 — Transfer matching and lifecycle

Status: **active**
Last updated: 2026-11-09

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
- Link of unequal amounts in one currency is refused unless the user uses the
  amount-difference action, which splits the larger allocation, categorizes
  the remainder, and links the common amount. No fee and no rate are invented.
- Link of two existing allocations in different currencies keeps both posted
  amounts. One amount must be outgoing and the other incoming. No rate is
  stored or applied. Choosing another account as the category still creates a
  same-currency counterpart and refuses a different currency.
- Candidate ranking and automatic transfer matching beyond those confirmed
  mapping pairs are not done. Links otherwise require an explicit choice.

## Committed edits

- Merging a purely generated committed duplicate with an independent row of the
  same amount inactivates the generated row and links the independent row. A
  generated row that has its own import evidence is not removed.
- Changing the category of one committed side, or removing that side, changes
  only that side and ends the link. A committed counterpart is not deleted. A
  purely generated uncommitted counterpart is removed in the same operation.
- An amount correction on one side of a both-committed link changes that side
  and ends the link. A date-only correction keeps the link and does not change
  the other side.
- A link that still has attached or proposed evidence refuses the correction.
- Bulk category or removal of a live linked row is refused. Edit those rows
  one at a time.
- A later edit, a newer transfer relationship, or a newer source association
  blocks Undo. The entry's own publication association does not.

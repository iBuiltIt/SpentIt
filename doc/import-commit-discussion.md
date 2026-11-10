# Acceptance and matching

Last updated: 2026-11-09

Accept transactions accepts new transactions across the file in one durable
operation. Failed import rows stay visible. Several imports can be pending.
Duplicate-only sources can complete without accepting older work. The ordinary
balance includes new and accepted rows. Closing does not accept.

Account Discard removes that account's exclusively owned uncommitted material.
It preserves accepted transactions and other retained sources. It requires an
empty Undo chain.

Quit can ask whether to accept before close. No keeps uncommitted work saved.
Cancel stays. A failed or uncertain accept must not close the file as if it
succeeded.

## Matching

Match saved original facts, not later display edits.

- Both sides with bank IDs use the strict bank-ID rule. Conflicting IDs are
  not bypassed. A repeated incoming ID stays unresolved.
- Otherwise a unique same-account match on original account, currency, exact
  amount, calendar date, and exact nonempty payee is recognized. Repeated keys
  stay reviewable. There is no case folding and no adjacent-day widening.
- Removed targets and malformed originals do not auto-match.
- A recognized row posts no second amount. An unresolved candidate stays in
  the balance until the user decides.
- An explicit whole-record bank match to an already accepted generated transfer
  records source ownership only. It does not accept another transaction and
  does not change that transfer's date, payee, or amount.

Review presentation is in [review-sheet.md](review-sheet.md). Import Undo is in
[import-undo.md](import-undo.md).

# 003 — Import and review

Status: **active**
Last updated: 2026-11-09

Supported imports are the [QIF bank-v6 profile](009-qif-profile.md) and the
[OFX, QFX, QBO, and ASO statement profile](003-ofx-profile.md). Further dialects
are not authorized. Named QIF account sections, explicit mapping, and transfer
destinations are in [doc/qif-account-imports.md](../doc/qif-account-imports.md)
and [doc/qif-transfers.md](../doc/qif-transfers.md).

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
- **Undo last import…** and Review **Undo import…** roll back one whole source.
  Accepted transactions are not deleted. Created accounts, categories, aliases,
  and openings remain. See [doc/import-undo.md](../doc/import-undo.md).

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

Review behavior is in [doc/review-sheet.md](../doc/review-sheet.md). Dataset-wide
acceptance is in [doc/import-commit-discussion.md](../doc/import-commit-discussion.md).

## Not done

- Investment and other non-banking QIF sections.
- Automatic OFX transfer destinations. Those records stay visible blockers.
- Fuzzy matching and cross-account date tolerance. Those belong to explicit
  transfer review, and broader automatic matching is deferred. Same-account
  review questions have a separate one-day pass, described in
  [doc/review-sheet.md](../doc/review-sheet.md).

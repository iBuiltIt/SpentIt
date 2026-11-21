# QIF transfer destinations

Last updated: 2026-11-17

See the [QIF banking profile](../features/009-qif-profile.md) for parser behavior. Class suffixes remain separate source metadata. A malformed amount cannot erase a retained transfer destination.

The bounded banking parser now admits `L[Account]` and `S[Account]` destinations and connects their allocations to the [durable transfer workflow](transfer-engine.md). Reports are outside this package. The parser contract for this destination package was `bank-v2`; the current statement parsers are recorded in [status](status.md).

Current extension (2026-10-29): [account names and confirmed aliases](../features/010-categories-and-selection.md) identify transfers even without QIF brackets. Grouped matching uses allocation amounts, so an ordinary fee split does not prevent matching the transfer split. This supersedes the earlier bracket-only admission and same-named ordinary-category policy. Original source bytes are retained unchanged.

## Review and resolution

A bracketed destination remains exact source text, attached to its stable source allocation. Parsing neither chooses an account nor creates a movement. The ledger shows the imported destination. Use the category selector to choose an account or an ordinary category; Review **Correct** opens the affected transaction. Exact account names and confirmed aliases can generate a same-currency counterpart during import; they are not fuzzy matched or normalized. An old export name can be explicitly mapped to a differently named account. Create missing accounts through the existing account workflow first.

For an unresolved allocation, explicitly choose the destination account in its category selector, or use Review to link existing counterparts. The category selector also lists real accounts. Choosing one stores that account identity and uses the same generation or replacement command. It does not search for an existing independent row, and account names/confirmed aliases are reserved transfer destinations. Renaming the account leaves the stored identity in place. An explicit category or Uncategorized choice replaces the current destination without changing the imported source label. Creation, matching, Merge, and replacement use complete atomic commands and durable operation receipts. Matching remains provisional until that importing account commits. Equal amounts alone do not authorize a match. Repeated equal allocations retain distinct identities and must be resolved separately. Cross-currency, self-account and nonexistent destinations are rejected.

Included bracketed allocations cannot commit without a live origin link or a provisional/attached evidence link. Partial resolution of a split does not satisfy this check. Storage enforces it independently of the UI and checks committed relationships again on authenticated open. Explicit ordinary duplicate review remains available; excluded duplicates do not create a second transfer. Drafts and unresolved source names survive restart.

An explicit category correction may replace an imported bracketed destination
with an ordinary category or Uncategorized. The stable split identity and source
ordinal remain, and the original imported fields are unchanged. A live link is
released through the atomic transfer-aware command: an independently imported
counterpart remains, and only a disposable generated draft is removed. Undo and
Redo restore the row and relationship together. Rules cannot perform this
replacement; uncorrected imported transfer destinations stay protected, even
when replacing manual values. Editing cannot invent a bracketed destination on
an unrelated allocation or discard the imported transfer split's association.
Money and rule provenance remain independently validated. Owner Go: 2026-11-17.

Per-account commit, draft protection, discard, later evidence attachment, independent posting dates and counterpart navigation retain the existing [transfer contract](transfer-engine.md). Neither resolution nor matching commits the opposite account. Source-first and destination-first imports use the same explicit workflow.

## Limits and verification

Only the existing banking dialect is extended. Multi-account QIF sections, investment records and category/class suffixes remain unsupported. Empty, nested, unbalanced or class-suffixed brackets fail explicitly. Broader committed transfer reversal, and cross-currency conversion remain outside this package. Merge, unlink, replacement, linking two independently committed equal allocations, account entries in the category selector, and the read-only transfer journal are implemented; see [transfer management](../features/008-transfer-matching-and-lifecycle.md). Grouped evidence links only at the equal allocation, not at the transaction total. This does not claim general QIF compatibility or complete feature 008.

Synthetic tests cover parsing, malformed notation, rules protection, both import orders, unknown/duplicate names, explicit identity choice, unresolved/partially resolved commit rejection, repeated equal allocations, mixed signs, reopen, backup restoration and UI resolution. Existing transfer write/sync and interruption sweeps now use bracketed source and bank fixtures. Do not treat this description as a passing test run.

## Complete named-account export exception — 2026-10-28

The owner superseded repeated manual destination selection for a complete export whose source accounts have already been explicitly mapped. The [confirmed-mapping resolution](qif-account-imports.md#reusing-confirmed-mappings-for-transfer-pairs--accepted-2026-10-28) automatically links mutual unique, same-date, same-currency reciprocal allocations without creating money or duplicate records. Remaining cases stay visible as navigable blockers. This is narrower than general automatic statement matching. Standalone imports, different-date candidates, ambiguous pairs, and cross-currency conversion are not silently resolved. Existing files use the command without a reimport. The release rules database is version 1.

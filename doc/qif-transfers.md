# QIF transfer destinations

Last updated: 2026-10-26

Subsequent [ordinary-QIF/nullable-amount work](status.md#ordinary-qif-and-unknown-amount-repair-checkpoint--2026-10-26) uses bank-v3 and schemas 14/15. The schema-12/13 verification below remains evidence for this earlier destination package. Class suffixes now remain separate source metadata; malformed amounts cannot erase a retained transfer destination.

The bounded banking parser now admits `L[Account]` and `S[Account]` destinations and connects their allocations to the [durable transfer workflow](transfer-engine.md). Reports are outside this package. The parser contract is `bank-v2`; current experimental schemas are 12 with rules and 13 without rules. Older layouts are rejected without migration because their pinned interpretation contract differs.

## Review and resolution

A bracketed destination remains exact source text, attached to its stable source allocation. Parsing neither chooses an account nor creates a movement. The Transfers dialog shows each allocation's amount, source destination and identity. The destination selector starts with an unselected placeholder; account labels include name, currency and stable identity to distinguish duplicate names. Names are never matched automatically or normalized. An old export name can be explicitly mapped to a differently named account. Create missing accounts through the existing account workflow first.

For an unresolved allocation, explicitly choose the destination account and use **Use as transfer**, or select the existing transfer and **Match selected allocation** after reviewing its counterpart. Creation and matching use the existing complete atomic commands and durable operation receipts. Matching remains provisional until that importing account commits. Equal amounts alone do not authorize a match. Repeated equal allocations retain distinct identities and must be resolved separately. Cross-currency, self-account and nonexistent destinations are rejected.

Included bracketed allocations cannot commit without a live origin link or a provisional/attached evidence link. Partial resolution of a split does not satisfy this check. Storage enforces it independently of the UI and checks committed relationships again on authenticated open. Explicit ordinary duplicate review remains available; excluded duplicates do not create a second transfer. Drafts and unresolved source names survive restart.

Ordinary editing cannot delete an imported transfer allocation, replace its bracketed destination with a spending category, or invent a bracketed destination on an unrelated allocation. Use transfer commands for account assignment. Before linking, rules may update descriptive fields and ordinary fee categories, but preserve transfer destinations even when replacing manual values; previews explain that protection. Source bytes, allocation associations, money and rule provenance remain independently validated. Financial corrections before linking retain the existing exact-money validation; linked edits remain guarded by the existing repair/reversal boundary.

Per-account commit, draft protection, discard, later evidence attachment, independent posting dates and counterpart navigation retain the existing [transfer contract](transfer-engine.md). Neither resolution nor matching commits the opposite account. Source-first and destination-first imports use the same explicit workflow.

## Limits and verification

Only the existing banking dialect is extended. Multi-account QIF sections, investment records and category/class suffixes remain unsupported. Empty, nested, unbalanced or class-suffixed brackets fail explicitly. Account-name alias persistence, a unified category/account picker, dedicated transfer journal, linking two independently committed entries, grouped/mismatched evidence, committed repair/unlink/reversal and cross-currency conversion remain outside this package. This does not claim general QIF compatibility or complete feature 008.

Synthetic tests cover parsing, malformed notation, rules protection, both import orders, unknown/duplicate names, explicit identity choice, unresolved/partially resolved commit rejection, repeated equal allocations, mixed signs, reopen, backup restoration and UI resolution. Existing transfer write/sync and interruption sweeps now use bracketed source and bank fixtures. Actual configuration results are recorded in [project status](status.md#qif-transfer-destinations--2026-10-26).

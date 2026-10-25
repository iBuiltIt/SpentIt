# 003 — QIF import and review

Status: **active**  
Last updated: 2026-10-25  
Implementation: none; requirements and proposed design only.

## Accepted requirements

- Import QIF into a persistent uncommitted workspace associated with the target account(s).
- Preserve original QIF evidence immutably. Rules and manual edits operate on an editable interpretation, never overwrite that evidence.
- Compare imported records with previously committed records and flag recognized/possible duplicates. The user can override false positives.
- Recognized duplicates are excluded from commit unless the user changes their disposition. Do not silently exclude uncertain candidates.
- Show the committed balance carried into the draft projection so users can compare the proposed ending balance with their bank.
- Allow adding, editing, deleting, and categorizing draft transactions. Clearing the workspace asks for confirmation.
- Commit explicitly publishes the chosen changes; discard leaves committed accounts and balances unchanged, including other accounts affected by proposed transfers.
- Drafts survive application restart.
- [Transfer matching and lifecycle](008-transfer-matching-and-lifecycle.md) applies to both import orders and searches committed and uncommitted counterparts. Preserve new bank evidence when matching a generated movement; do not treat that evidence as an ordinary duplicate to discard.
- Commits are explicit per account. An origin commit protects its generated destination draft without committing it. Clearing the destination workspace retains such protected drafts; discarding an uncommitted origin removes its purely generated counterpart. Protected drafts affect projected balances only.
- Flag unmatched generated counterparts for manual matching or acceptance as valid pending entries; neither action implicitly commits them.

## Proposed import pipeline

1. Preserve source bytes and record boundaries with import identity and parser diagnostics inside encrypted storage.
2. Parse into structured fields while retaining unknown/uninterpreted fields as evidence. Ask about ambiguous dates or formats rather than guessing silently.
3. Compare original source identity/content with committed records and other drafts. Use occurrence-aware matching, not a unique constraint on date/payee/amount.
4. Apply rules to derived fields and save their explanations separately from source evidence.
5. Review edits, unresolved records, duplicate decisions, and projected balances.
6. Revalidate at commit; atomically write the chosen account operation, linked effects/protection, provenance, history, and its draft-state transitions. Do not commit another account implicitly; its generated counterpart remains a protected draft until explicitly committed there.

Use a durable operation identity so retries after interruption cannot apply an import twice. An error after the durable commit boundary must be resolved by inspecting recorded operation state, not blindly retrying insertion.

Two identical purchases can be legitimate. A fingerprint is matching evidence, not necessarily a bank-issued unique transaction ID. Account identity, occurrence counts, raw fields, and import context matter. Matching must remain understandable and reversible.

## Synthetic Wise-shaped fixture

This intentionally sanitized example captures the structure discussed; it is not a copied personal transaction:

```text
D10/08/2026
NCARD-SYNTHETIC-001
T-29.31
PTfl Travel Ch TFL.GOV.UK/CP
MExample Person
EWise Charges for: CARD-SYNTHETIC-001
$-0.14
ECARD-SYNTHETIC-001
$-29.17
^
```

`P` is payee, `E` is split memo, `$` is split amount, and `S` would supply split category. This sample lacks `S` fields; support this shape deliberately. Its date is ambiguous without a date-format context. The desired classification is Service Fees for -0.14 and Transport for -29.17 through parent-payee classification.

## Acceptance criteria

- Reimporting the same file or an overlapping date range does not silently double-count or drop legitimate repeated transactions.
- Identical repeated purchases, edited committed transactions, changed export formatting, and duplicates across pending imports have explicit tested outcomes.
- Missing split categories, mixed signs, ambiguous dates, malformed records, encoding issues, and split-total discrepancies are visible and reviewable.
- A rules/manual-edit pass never alters original source evidence.
- Projected balances exclude rows marked to be ignored and reconcile to the eventual committed result.
- Clear/discard changes no committed financial state. Discarding an uncommitted origin removes its purely generated draft counterpart; clearing the destination retains counterparts protected by committed origins. Independently supported counterparts are preserved. See feature 008 for lifecycle criteria.
- Restart restores draft content and dispositions; interruption/retry tests prove atomic commit and no duplicate application.
- Manually created records clearly indicate that no QIF source exists.

## Open questions

- Supported QIF dialects, date/encoding profiles, file-size limits, and partial-parse handling.
- Exact recognition confidence rules and review UI; handling uncertain candidates must be settled before implementing automatic exclusion.
- One workspace versus multiple batches per account, commit selection granularity, and multi-account file review.
- Exact transfer matching heuristics and unresolved correction/commit-order cases are tracked in [feature 008](008-transfer-matching-and-lifecycle.md); per-account commits and protected drafts are accepted.
- Whether discarding also removes source evidence or retains limited import history; retained audit evidence must not masquerade as an active draft.

## Reference

- [GnuCash QIF format notes](https://github.com/Gnucash/gnucash/blob/stable/gnucash/import-export/qif-imp/file-format.txt)

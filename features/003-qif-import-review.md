# 003 — QIF import and review

Status: **active**  
Last updated: 2026-10-27  
Implementation: partial. Bank-v3 QIF and the [OFX, QFX, QBO, and ASO statement profile](003-ofx-profile.md) share schema-18/19 source facts and reviewable associations. The inspected ASO export is OFX 1.0.2 SGML credit-card statement syntax, stored as format `aso` and grammar `ofx-sgml`. Remaining QIF product scope stays open. See [point-3 evidence](../doc/status.md#point-3-statement-imports--2026-10-27).

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

## Accepted future import formats

The owner confirmed QIF-first sequencing on 2026-10-25. After the bounded QIF milestone, SpentIt must also support **QFX, ASO, QBO and OFX**. The owner's bank offers these formats but not QIF. This extends the eventual import scope; it does not add these parsers to the current milestone or claim that their dialects are already understood.

The owner has the same real-world account activity exported separately in all four formats. These matching exports may help compare interpretation, record counts, exact amounts, dates, references and preserved evidence across formats. Their equivalence is owner-reported, not independently verified; format-specific omissions, identifiers and semantics must be investigated rather than assumed. In particular, the ASO extension alone does not establish its encoding or grammar. The separate QIF samples are not asserted to contain this same dataset.

Ask the owner for access when needed. Keep private exports out of the repository and routine automated tests; never record their filenames, paths or contents or move/modify the originals. Prefer synthetic equivalents for repeatable cross-format regression tests. Follow the [private-sample handling contract](009-durable-import-milestone.md#private-sample-availability). No samples were opened for this documentation update.

The 2026-10-27 checkpoint specifies and implements OFX, QFX, QBO, and ASO bank and credit-card statement lists in the [statement profile](003-ofx-profile.md). ASO was identified by in-place structural inspection as OFX 1.0.2 SGML credit-card statement syntax. No path or contents were recorded. The paragraphs above record the 2026-10-25 requirement; they are not a claim that the rest of feature 003 is finished.

## Accepted source-evidence direction — 2026-10-25

The owner accepted preserving original imports in their native formats and comparing a shared structured source interpretation. Retain exact QIF/QFX/other source bytes inside the document's encrypted storage; converting to QIF must not replace or erase the original. A parser or conversion defect must remain investigable from retained evidence.

Separate original evidence, parsed source facts and editable ledger interpretations. Matching uses source facts for the target account, with exact money/currency and occurrence identity, rather than the user's edited payee, date or effective amount. Different formats may omit or express fields differently; common fields alone do not establish unique identity.

One transaction can be associated with multiple source occurrences from different imports. Renaming an imported payee changes the editable interpretation without altering either the original facts or the associations. Confirming a later export as evidence of the same transaction adds an association, not another monetary posting. Preserve the match basis and user decision so an incorrect association can be explained and corrected.

Proposed relational realization: an import stores its original bytes; source-occurrence rows identify locations within it and parsed facts; a transaction has a stable identity; an association table links that identity to its source occurrences. These are conceptual responsibilities, not adopted table names or a migration plan. The current QIF experiment preserves bytes and revisions but does not implement the future multi-format association model.

Correction 2026-10-27: schemas 18 and 19 store immutable original bytes, `source_occurrences`, and `source_associations`. QIF and OFX-family imports both use that representation. Schemas 16 and 17 are rejected rather than migrated. The paragraph above describes the experiment before that checkpoint. See the [statement profile](003-ofx-profile.md).

Removing a transaction from the ledger retains imported evidence and persistent history. The owner subsequently accepted ordinary correction/removal/strict Undo and explicit removed-record reimport review in [feature 011](011-committed-editing-and-reversal.md), now implemented per account in the bounded QIF workflow. A removed match is neither automatically restored nor silently suppressed. Cross-format association correction, transfer changes and broader evidence reconciliation remain unresolved.

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

- Supported QIF dialects, date/encoding profiles, file-size limits, and partial-parse handling. The OFX-family bank and credit-card slice, including the inspected ASO label, is specified separately. See [003-ofx-profile.md](003-ofx-profile.md).
- Exact recognition confidence rules and review UI; handling uncertain candidates must be settled before implementing automatic exclusion.
- One workspace versus multiple batches per account, commit selection granularity, and multi-account file review.
- Exact transfer matching heuristics and unresolved correction/commit-order cases are tracked in [feature 008](008-transfer-matching-and-lifecycle.md); per-account commits and protected drafts are accepted.
- Whether discarding also removes source evidence or retains limited import history; retained audit evidence must not masquerade as an active draft.

## Reference

- [GnuCash QIF format notes](https://github.com/Gnucash/gnucash/blob/stable/gnucash/import-export/qif-imp/file-format.txt)

## Accepted durable-import experiment — 2026-10-25

The owner accepted the bounded policies in [feature 009](009-durable-import-milestone.md), including its explicit deferrals. Those decisions govern that experiment only; broader open questions in this feature remain unless explicitly settled there. The headless subset, bounded writable UI and date/split-total repair are implemented; the full workflow remains incomplete.

## Bounded repair implementation — 2026-10-25

The owner selected date and split-total repair first. New experimental schema-2 datasets now support explicit validated repair, retained original evidence/diagnostics, durable history and separate duplicate review. Missing/invalid amounts and unsupported constructs remain blocked; schema-1 documents remain readable without migration. See [the milestone scope](009-durable-import-milestone.md#accepted-bounded-record-repair--2026-10-25) and [implementation/verification](../doc/durable-import.md#bounded-record-repair).

## Account-scoped follow-up — 2026-10-26

The [multi-account prerequisite](../doc/accounts.md) gives each account an independent pending import, duplicate review and ordinary history. Identical source bytes in another account are not automatically excluded. This does not implement transfer evidence attachment or multi-account QIF files. Earlier experimental formats are now rejected without migration; the earlier dated schema-compatibility notes are historical.

## QIF transfer destinations — 2026-10-26

The [bounded destination extension](../doc/qif-transfers.md) admits whole-record and split `[Account]` notation, requires explicit account/link resolution, and guards commit and rule application against losing transfer intent. See that contract and its verification link; multi-account QIF files and the wider transfer repair/reversal scope remain deferred.


## Ordinary profile and unavailable original amounts — 2026-10-26

The [v3 profile](009-qif-profile.md) now admits ordinary Bank/Cash/CCard records and separate class metadata. Structurally intact date/amount repairs preserve unknown original money as unavailable and keep every original split position and transfer destination. Explicit repaired interpretations and duplicate decisions remain separate; no guessed original, zero substitution or automatic repaired-record exclusion is permitted. The [current checkpoint](../doc/status.md#ordinary-qif-and-unknown-amount-repair-checkpoint--2026-10-26) records verification and format scope.

The subsequent [whole-source account workflow](../doc/qif-account-imports.md) implements named sections, explicit mapping or creation, repair counts/progress, staged reported-balance interpretation and whole-import Abort under the accepted policies. Selected current-format verification is recorded in [status](../doc/status.md#whole-source-qif-account-review--2026-10-26). General cross-format associations and wider compatibility remain outside this QIF profile; the feature stays active.

# 008 — Transfer matching and lifecycle

Status: **active**  
Last updated: 2026-10-25  
Implementation: none; accepted product behavior with unresolved design details. The current engine and storage prototype do not implement accounts, transfer matching, or protected drafts.

## Accepted requirements

### Transfer entry and provenance

Accounts appear as distinguishable transfer destinations in the category selector. Selecting an account makes the transaction a transfer and initiates matching/linking to that account. If a counterpart exists, link it rather than create another movement. Otherwise create a counterpart in the destination account's **uncommitted log**, recording which originating transaction caused its creation.

Retain the distinction between a generated counterpart and an independently imported transaction. Preserve original source evidence from each account, the creation relationship, match decisions, and subsequent corrections. A generated counterpart can later acquire independent imported evidence; creation provenance must not be lost, but such a counterpart is no longer disposable merely because it was originally generated.

### Explicit per-account commit

**Committing one account does not commit the counterpart in another account. Only the user's explicit commit for that other account commits it.** This supersedes any earlier assumption that both transfer sides must enter committed financial state together.

For a checking transfer that generates a Visa counterpart:

| Visa counterpart state | Balance/report effect | Clear/discard behavior |
|---|---|---|
| Ordinary generated draft; checking origin uncommitted | Visa projected balance only; excluded from committed balance and ordinary reports | Discarding the originating checking operation also removes its purely generated Visa counterpart. |
| Protected generated draft; checking origin committed, Visa not committed | Still Visa projected balance only; excluded from committed balance and ordinary reports | Clearing Visa's uncommitted workspace leaves this entry in place. Remove/correct it through the originating committed checking transaction. |
| Visa explicitly committed | Included in Visa committed balance; transfer principal remains excluded from spending | Cannot be removed through draft clearing. Later correction/reversal needs the committed-operation policy. |

“Protected draft” names the user's sticky uncommitted state; it is not a synonym for committed. It persists across restarts and is visibly distinguishable from ordinary drafts. Visa review and matching can proceed while the entry is protected. The committed/projected sidebar balance toggle includes it in the projection, making the pending financial effect visible without implicitly committing it.

Committing checking must atomically publish the checking operation, protect its generated Visa draft, maintain the link/provenance, and record history. A failed or interrupted operation must not leave only part of this transition applied. Atomicity applies to the complete chosen operation and its relationship/state changes, not automatic commitment of the other account. Per-account committed balances can therefore temporarily reflect different stages of a transfer; the UI must explain the pending state.

### Import matching and attention

Import matching considers existing committed transactions and uncommitted entries, including ordinary and protected generated counterparts. Importing checking before Visa, or Visa before checking, must not create a second movement when the counterpart is already present. A later bank record supplies evidence for the existing movement and resolves its match; neither matching nor attaching evidence implicitly commits an uncommitted account.

The accounts may record the transfer on different dates. Preserve each bank's own posting date; do not force them to be equal. Equal amounts alone are not sufficient identification: repeated payments may be legitimate. Exact automatic-match confidence, date tolerance, and cross-currency matching remain unresolved.

When generated counterparts do not match naturally during an import, make them stand out as needing attention. The user can manually match them or accept them as valid even though they are absent from the incoming QIF. Acceptance does not commit them and does not prevent a future import from matching them. An export may omit a transfer because of its date range or posting delay; absence is not permission to delete the entry.

### Fix match and deletion

Offer **Fix match** for a missed match that produced a generated duplicate, and for an incorrect existing match. Make repair accessible from either transfer side. The user chooses the correct counterpart; remove the redundant generated entry from the active ledger and link the selected independent entry in one atomic operation. “Disappear” means removal from the active ledger, not destruction of source evidence or committed-change history. A repair is not permission to overwrite later edits or erase independently supported records.

Deletion/reversal depends on provenance and state:

- Removing an uncommitted origin also removes its purely generated, uncommitted counterpart as part of the same operation.
- After the origin commits, the destination's protected generated draft survives clearing/discarding the destination import workspace. To remove that generated movement, edit/reverse the origin and break its transfer connection; destination clearing must not change the committed origin.
- If the counterpart was independently imported/matched, removing the origin removes the link, not that independently supported counterpart. Surface its now-unresolved transfer relationship. The same preservation rule applies when an originally generated entry later acquires independent import evidence.
- If the destination has already committed, or independently supported records or later edits would be affected, the detailed correction/reversal workflow still needs design. Do not infer blanket cascade deletion from the purely generated draft case.

### Match navigation

For both manual and automatic links, a match icon navigates to the counterpart in the other account's ledger, selects it, and brings it on-screen. The counterpart's match icon navigates back. Toolbar Back also returns to the previous ledger location. Navigation respects the [Save / Discard / Stay contract](005-transaction-sheet-navigation.md). Final icon artwork is unspecified.

## Proposed implementation direction

Represent source provenance, transfer relationships, and each account entry's commit state separately. A single committed flag for an entire transfer cannot express the accepted lifecycle. Protection needs a durable relationship to the committed origin; merely disabling a UI delete button is insufficient. Exact schema, identifiers, command APIs, and match algorithm are not selected by these requirements.

Keep transfer recognition distinct from ordinary duplicate suppression: a matching bank record can provide new evidence for an existing counterpart and must not be silently discarded as an irrelevant duplicate. Reuse [import retry and atomicity requirements](003-qif-import-review.md), preserving history and evidence throughout repair.

## Acceptance criteria — not yet implemented or tested

- Synthetic checking-first and Visa-first imports each retain one movement per account, including matching against ordinary drafts, protected drafts, and committed counterparts.
- Different posting dates and legitimate repeated equal payments produce explainable outcomes without silent double counting or dropped evidence.
- Before checking commits, discarding its operation removes the purely generated Visa draft and changes neither account's committed balance.
- Committing checking commits only checking and durably protects the Visa draft. Visa committed balance and ordinary reports exclude it; Visa projected balance includes it exactly once.
- Clearing Visa drafts after checking commits preserves that protected entry, its link, and projection across restart.
- Visa becomes committed only after the user explicitly commits it; matching or accepting an unmatched counterpart alone does not commit it.
- Later Visa import evidence matches the existing movement without adding another amount or losing source records.
- Fix match repairs missed and incorrect links atomically, removes only the redundant generated movement, and preserves independently supported entries and applicable history.
- Origin deletion/reversal distinguishes generated from independently imported counterparts, including a generated entry that later acquired evidence.
- Unmatched attention items allow manual matching or acceptance as valid pending entries and remain matchable by later imports.
- Match navigation works both ways and with Back, including bringing the selected row on-screen and honoring unsaved edits.
- Injected failures, interruption on both sides of durable commit, retry, and backup/restore preserve per-account states, protection, relationships, evidence, and history without duplicate application.

## Open questions

- Match confidence, date windows, references/normalization, duplicate multiplicity, ambiguous candidates, and cross-currency/fee matching.
- Transfer allocations within split transactions, multiple destinations, and how counterpart identity maps to allocations.
- Generated counterpart date and other field defaults before its bank evidence arrives, and reconciliation of differing dates/amounts/memos without losing either source.
- Committing the generated destination before the origin; cancelling only the destination side while both are ordinary drafts; multi-batch selection and ordering.
- Detailed edit/unlink/reversal rules once both accounts are committed, once independent evidence has arrived, or after later edits; preview and conflict handling must preserve independently supported records.
- How pending evidence attachments and repair proposals interact with discarding an import; draft review must not silently alter committed interpretations or links.
- Exact attention/protection labels, acceptance-state persistence, and navigation when an existing filter hides the counterpart.

## Related requirements

- [Accounts, money, and splits](002-accounts-money-splits.md)
- [QIF import and review](003-qif-import-review.md)
- [Documents, storage, and security](001-documents-storage-security.md)
- [Transaction sheet and navigation](005-transaction-sheet-navigation.md)

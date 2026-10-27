# 008 — Transfer matching and lifecycle

Status: **active**  
Last updated: 2026-10-27  
Implementation: the [multi-account workflow](../doc/accounts.md) and [bounded durable transfer integration](../doc/transfer-engine.md) implement manual/imported-allocation creation, generated counterparts, acceptance, provisional bank matching, per-account commit, draft protection and counterpart navigation. [Bounded QIF transfer admission and explicit resolution](../doc/qif-transfers.md) are implemented; unified account-picker integration and repair/reversal remain unfinished. Verified scope and remaining decisions are distinguished below.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded category catalog is implemented; the unified account/category selector remains a follow-up to the current Transfers dialog.

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

## Accepted first implementation scope — 2026-10-26

The owner accepted these five decisions for the first transfer package:

1. **Destination-first commit is allowed.** Explicitly committing the generated destination protects its linked origin draft from workspace clearing. It does not commit the origin. Protection therefore works in either commit order; creation provenance remains unchanged.
2. **Matching initially requires explicit confirmation.** Suggest candidates for review rather than automatically linking them. Preserve both bank records and their own posting dates. Differing amounts require review; confirmation must not silently correct either amount. Automatic matching confidence and mismatch-resolution commands remain later design work.
3. **Generated dates are provisional.** Initially copy the origin date and visibly identify it as provisional. Review the destination bank posting date when independent evidence arrives, preserving the original evidence and prior interpretation.
4. **Transfers belong to allocations.** Support a destination account per allocation, with each counterpart linked to its originating allocation. A split transaction can therefore contain ordinary categories and transfers to multiple destinations. Do not infer identity from allocation position or combine repeated equal allocations.
5. **Cross-currency transfers are deferred.** The first package must reject them with an explicit unsupported-operation error. Do not infer exchange rates or reinterpret amounts. This bounds implementation; it does not remove the broader product requirement. Point 4 currency reassignment, recorded in [feature 002](002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27), may leave an already linked same-currency pair as two preserved opposite decimals in different currencies. That preservation does not create a match, a rate, a fee, or equality, and new transfer generation, proposal, and commit still reject a cross-currency match. It is not transfer repair or committed transfer reversal.

These decisions settle the corresponding first-scope questions below. They do not authorize automatic matching, silent mismatch repair, or cascade removal of committed or independently supported entries. Detailed correction, unlink, Fix match and reversal behavior involving committed entries or later edits remains a separate decision gate.

### Accepted matching and unmatched-commit decisions — 2026-10-26

- A confirmed match from a pending import remains a draft proposal until that importing account explicitly commits. Discarding the import drops the proposal, retains the immutable source evidence, and leaves the existing transfer's financial state and protection unchanged. Confirmation alone must not change committed interpretations or relationships.
- An entry does not need a bank match to be valid or committable. Provide an explicit **Accept as valid without a bank match** action for manual/unmatched entries, including generated counterparts that may never appear in an import. Acceptance records the user's review; it does not fabricate imported evidence, automatically commit the entry, or prevent later matching. The account's explicit commit remains separate.
- Acceptance resolves the lack of a bank match, not inconsistent money. For example, a Checking entry of −100 and a Visa entry of +95 may each be accepted and explicitly committed as valid unmatched entries. They remain unmatched; linking requires the difference to be explicitly accounted for. Do not silently change either amount, assume a fee, or invent a balancing allocation.
- Cross-currency links remain unsupported in the first slice. Acceptance does not bypass exact-money validation, protection, independent-evidence preservation, or unresolved committed repair/unlink/reversal policies. An unresolved link or repair must not trap otherwise valid manual/unmatched entries in an uncommittable state.

These decisions settle provisional matching/discard and unmatched-entry acceptance. They do not specify mismatch-resolution commands, destination-only cancellation or committed repair/reversal. The bounded durable implementation now exists; see [workflow and verification limits](../doc/transfer-engine.md).

### Historical implementation starting point

Inspection on 2026-10-26 found a singleton account table in `src/storage/import_store.cpp`, an account-less command boundary in `src/app/include/spentit/import_repository.hpp`, and QIF tests that explicitly reject transfer destinations in `tests/engine/qif_tests.cpp`. Existing allocation metadata already carries stable identities. The subsequent [account-scoped implementation](../doc/accounts.md) replaces the singleton restriction and adds account ownership to batches. At that prerequisite checkpoint transfer destinations were still rejected; the subsequent [QIF package](../doc/qif-transfers.md) admits them with explicit resolution.

Proposed sequence: introduce account identities and account-scoped import/ledger operations in a new experimental format; then allocation-linked same-currency transfers, symmetric draft protection, and explicit evidence matching. Integrate each complete operation with history, retry identities, backup restoration and interruption tests before exposing it in the durable UI. No migration or backward-compatibility work is required before the owner-announced release gate.

## Adopted bounded implementation and remaining design

Represent source provenance, transfer relationships, and each account entry's commit state separately. A single committed flag for an entire transfer cannot express the accepted lifecycle. Protection needs a durable relationship to the committed origin; merely disabling a UI delete button is insufficient. The bounded storage schema and command contracts are now documented in [the durable transfer guide](../doc/transfer-engine.md); ranking and repair remain unresolved.

Keep transfer recognition distinct from ordinary duplicate suppression: a matching bank record can provide new evidence for an existing counterpart and must not be silently discarded as an irrelevant duplicate. Reuse [import retry and atomicity requirements](003-qif-import-review.md), preserving history and evidence throughout repair.

## Full-feature acceptance criteria — partially implemented

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

## Remaining work and open questions

- Candidate ranking, date windows, references/normalization, duplicate multiplicity and ambiguous candidates. Initial links require explicit confirmation; automatic matching and cross-currency/fee matching remain deferred.
- Detailed matching when independently imported evidence groups multiple transfer allocations differently. Per-allocation destinations and counterpart links are accepted.
- Generated counterpart field defaults other than the accepted provisional origin date, and explicit reconciliation of differing amounts/memos without losing either source.
- Cancelling only the destination side while both are ordinary drafts; multi-batch selection and ordering. Destination-first commit with protection of the origin draft is accepted.
- Detailed edit/unlink/reversal rules once both accounts are committed, once independent evidence has arrived, or after later edits; preview and conflict handling must preserve independently supported records.
- How repair proposals interact with discarding an import. Ordinary pending evidence matches remain provisional until the importing account commits; discard drops those proposals while retaining source evidence and the existing transfer state, as accepted above.
- Unified picker integration and a browsable transfer history. Current acceptance/protection state is durable; the dialog and counterpart reveal are implemented.

## Related requirements

- [Accounts, money, and splits](002-accounts-money-splits.md)
- [QIF import and review](003-qif-import-review.md)
- [Documents, storage, and security](001-documents-storage-security.md)
- [Transaction sheet and navigation](005-transaction-sheet-navigation.md)

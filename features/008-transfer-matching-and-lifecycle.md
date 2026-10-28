# 008 — Transfer matching and lifecycle

Status: **active**  
Last updated: 2026-10-27  
Implementation: the [multi-account workflow](../doc/accounts.md) and [bounded durable transfer integration](../doc/transfer-engine.md) implement manual/imported-allocation creation, generated counterparts, acceptance, provisional bank matching, per-account commit, draft protection, counterpart navigation, allocation-level Merge, unlink and replacement, and a read-only transfer journal. [Bounded QIF transfer admission and explicit resolution](../doc/qif-transfers.md) and account entries in the category selector are implemented. Committed transfer reversal remains point 6. Verified scope and remaining decisions are distinguished below.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded category catalog is implemented. The durable category selector lists real accounts as transfer destinations under the [2026-10-27 decisions](#accepted-transfer-management-decisions--2026-10-27).

### Transfer entry and provenance

Accounts appear as distinguishable transfer destinations in the category selector. Selecting an account makes the transaction a transfer to that account identity. The [2026-10-26 confirmation rule](#accepted-first-implementation-scope--2026-10-26) and the [2026-10-27 selector rule](#accepted-transfer-management-decisions--2026-10-27) require an explicit match. Choosing the account creates a generated destination draft, or replaces the current destination; it does not by itself link an existing independent row. A confirmed Merge or match links that row instead of keeping a second movement. The generated counterpart is created in the destination account's **uncommitted log**, recording which originating transaction caused its creation.

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

The owner named this command **Merge** on 2026-10-27. The accepted behavior is the [transfer-management decisions](#accepted-transfer-management-decisions--2026-10-27). Offer it for a missed match that produced a generated duplicate, and for an incorrect existing match. The user chooses the correct counterpart; remove only a purely generated uncommitted duplicate from the active ledger and link the selected independent entry in one atomic operation. “Disappear” means removal from the active ledger, not destruction of source evidence or committed-change history. A repair is not permission to overwrite later edits or erase independently supported records.

Deletion/reversal depends on provenance and state:

- Removing an uncommitted origin also removes its purely generated, uncommitted counterpart as part of the same operation.
- After the origin commits, the destination's protected generated draft survives clearing/discarding the destination import workspace. To remove that generated movement, edit/reverse the origin and break its transfer connection; destination clearing must not change the committed origin.
- If the counterpart was independently imported/matched, removing or retargeting the origin removes the link, not that independently supported counterpart. Surface its now-unresolved transfer relationship. The same preservation rule applies when an originally generated entry later acquires independent import evidence, and when that counterpart is already committed.
- Removing a committed generated duplicate, rewriting a committed allocation's category, and reversing a committed transfer remain point 6. Do not infer blanket cascade deletion from the purely generated draft case.

### Match navigation

For both manual and automatic links, a match icon navigates to the counterpart in the other account's ledger, selects it, and brings it on-screen. The counterpart's match icon navigates back. Toolbar Back also returns to the previous ledger location. Navigation respects the [Save / Discard / Stay contract](005-transaction-sheet-navigation.md). Final icon artwork is unspecified.

## Accepted first implementation scope — 2026-10-26

The owner accepted these five decisions for the first transfer package:

1. **Destination-first commit is allowed.** Explicitly committing the generated destination protects its linked origin draft from workspace clearing. It does not commit the origin. Protection therefore works in either commit order; creation provenance remains unchanged.
2. **Matching initially requires explicit confirmation.** Suggest candidates for review rather than automatically linking them. Preserve both bank records and their own posting dates. Differing amounts require review; confirmation must not silently correct either amount. Automatic matching confidence and mismatch-resolution commands remain later design work.
3. **Generated dates are provisional.** Initially copy the origin date and visibly identify it as provisional. Review the destination bank posting date when independent evidence arrives, preserving the original evidence and prior interpretation.
4. **Transfers belong to allocations.** Support a destination account per allocation, with each counterpart linked to its originating allocation. A split transaction can therefore contain ordinary categories and transfers to multiple destinations. Do not infer identity from allocation position or combine repeated equal allocations.
5. **Cross-currency transfers are deferred.** The first package must reject them with an explicit unsupported-operation error. Do not infer exchange rates or reinterpret amounts. This bounds implementation; it does not remove the broader product requirement. Point 4 currency reassignment, recorded in [feature 002](002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27), may leave an already linked same-currency pair as two preserved opposite decimals in different currencies. That preservation does not create a match, a rate, a fee, or equality, and new transfer generation, proposal, and commit still reject a cross-currency match. It is not transfer repair or committed transfer reversal.

These decisions settle the corresponding first-scope questions below. They do not authorize automatic matching, silent mismatch repair, or cascade removal of committed or independently supported entries. Merge, unlink, and replacement for the cases in the [2026-10-27 decisions](#accepted-transfer-management-decisions--2026-10-27) are accepted. Committed reversal remains point 6.

### Accepted matching and unmatched-commit decisions — 2026-10-26

- A confirmed match from a pending import remains a draft proposal until that importing account explicitly commits. Discarding the import drops the proposal, retains the immutable source evidence, and leaves the existing transfer's financial state and protection unchanged. Confirmation alone must not change committed interpretations or relationships.
- An entry does not need a bank match to be valid or committable. Provide an explicit **Accept as valid without a bank match** action for manual/unmatched entries, including generated counterparts that may never appear in an import. Acceptance records the user's review; it does not fabricate imported evidence, automatically commit the entry, or prevent later matching. The account's explicit commit remains separate.
- Acceptance resolves the lack of a bank match, not inconsistent money. For example, a Checking entry of −100 and a Visa entry of +95 may each be accepted and explicitly committed as valid unmatched entries. They remain unmatched; linking requires the difference to be explicitly accounted for. Do not silently change either amount, assume a fee, or invent a balancing allocation.
- Cross-currency links remain unsupported in the first slice. Acceptance does not bypass exact-money validation, protection, independent-evidence preservation, or unresolved committed repair/unlink/reversal policies. An unresolved link or repair must not trap otherwise valid manual/unmatched entries in an uncommittable state.

These decisions settle provisional matching/discard and unmatched-entry acceptance. Destination-only cancellation, Merge, and unlink are settled below. Committed reversal remains point 6. The bounded durable implementation now exists; see [workflow and verification limits](../doc/transfer-engine.md).

## Accepted transfer-management decisions — 2026-10-27

The owner locked these decisions for point 5. They settle Merge, unlink, replacement, allocation-level resolution, destination cancellation, and the transfer journal. Committed reversal remains point 6 and is not implemented.

### Link level

A link joins two allocations. It is not the transaction total. A checking total of −100.00 can be a −5.00 fee category plus a −95.00 transfer allocation, and only that −95.00 allocation links to a Visa allocation of +95.00. An unsplit imported row whose amount is 100 does not match a Visa allocation of 95 until the user splits it. If the import is already split so the transfer allocation equals the counterpart, the amounts match. There is no fee command, exchange rate, unequal link, or combined row. Repeated equal allocations stay distinct. Identity is the stable allocation identity, never position.

### Merge

The visible command is **Merge**, from the ledger context menu or Ctrl+M (Command+M on macOS). Qt maps Ctrl to Command on macOS, the same way Ctrl+Shift+B maps for the sidebar. There is one command, not separate Weld and Join commands.

- A purely generated uncommitted duplicate on the same account as an independent row: one atomic operation removes only that generated row from the active ledger and links the independent allocation to the origin. Source evidence and history remain. The surviving row's amount and its own posting date do not change. A protected generated draft is still uncommitted, so explicit Merge may retire it. Workspace clearing still does not retire a protected draft.
- Two independent allocations on opposite accounts, including when both are already committed: record the link only. Both rows stay on the active ledger. Amounts do not change and no row is removed. History records the link.
- Allocation amounts that are not exact opposites, or a cross-currency pair, refuse with an explicit error. The user splits or chooses equal allocations first.
- If the redundant generated row is already committed, the command refuses. It does not remove that row and does not also keep it while linking a second row. The error states that removing a committed generated duplicate is committed reversal and remains point 6.
- Ambiguous selection refuses. If a selected transaction has more than one allocation that could be the endpoint, the user must choose the allocation explicitly. The command does not guess and does not auto-merge. Allocations that belong to different links also refuse until one relationship is chosen.

### Unlink, replacement, and destination cancellation

- Deleting only a generated destination draft is refused. The error explains that the origin controls that generated draft.
- Discarding an uncommitted origin removes that origin and its purely generated uncommitted counterpart from the active ledger.
- Changing the origin allocation from a transfer account to a category, or to a different account, removes a purely generated uncommitted counterpart and applies the new category or new destination in the same atomic operation.
- If the counterpart was independently imported, or is already committed, the command unlinks it, leaves that row and its amount unchanged, and still applies the origin change. The unresolved relationship stays visible. The counterpart is not cascade-deleted.
- An incorrect existing match uses the same rule: retire only a purely generated uncommitted current counterpart; leave an independent or committed current counterpart in place; link the newly chosen equal allocation; one atomic operation.

Workspace clearing of a protected draft still leaves that draft in place. That includes a generated destination whose origin is already committed, and an origin draft whose destination was committed first. Explicit Merge may still retire a protected generated draft because that draft is uncommitted.

### Selection, journal, and commands that still refuse

The 2026-10-26 decisions remain in force: explicit per-account commit, explicit match confirmation, provisional generated dates, pending import proposals, unmatched acceptance, and rejection of new cross-currency links.

Choosing an account in the category selector stores that account's identity. On an unlinked allocation it creates a generated destination draft through the existing generation path. On an existing origin it replaces the destination under the replacement policy above. It does not auto-link an existing independent row. Candidates remain explicit suggestions. A same-named ordinary category does not create a transfer. Renaming the account does not change the stored identity.

The read-only transfer journal lists persisted transfer operations for the document: link identity, accounts, allocation amounts, dates, action, and sequence. It reads history from the Transfers dialog. It is not a second way to edit money.

Point 4 `preserved_amounts` may already show two opposite decimals in different currencies on an old link. That is not a match, a rate, a fee, or equality. These commands do not unlink or repair those rows as a side effect. An explicit retarget or merge of such a link is refused.

Ordinary Undo does not reverse a transfer. Committed transfer reversal, removing a committed generated duplicate, and rewriting a committed allocation's category as part of a transfer remain point 6.

A committed generated side that still has a pending import proposal is not unlinked until that proposal is committed or discarded. Retiring a purely generated uncommitted duplicate may drop its proposal because the user chose the independent row. Discarding an import that is the destination of a still-resolved link is refused; change the origin first. Generated destination drafts are not import batches, so deleting only that draft uses the origin-controls refusal.

New documents stay on experimental schema 20 with rules and 21 without. Merge and retarget fit the existing transfer event and version blobs, so this package did not change the on-disk shape. Application id remains `1397770577`. There is no migration.

### Historical implementation starting point

Inspection on 2026-10-26 found a singleton account table in `src/storage/import_store.cpp`, an account-less command boundary in `src/app/include/spentit/import_repository.hpp`, and QIF tests that explicitly reject transfer destinations in `tests/engine/qif_tests.cpp`. Existing allocation metadata already carries stable identities. The subsequent [account-scoped implementation](../doc/accounts.md) replaces the singleton restriction and adds account ownership to batches. At that prerequisite checkpoint transfer destinations were still rejected; the subsequent [QIF package](../doc/qif-transfers.md) admits them with explicit resolution.

Proposed sequence: introduce account identities and account-scoped import/ledger operations in a new experimental format; then allocation-linked same-currency transfers, symmetric draft protection, and explicit evidence matching. Integrate each complete operation with history, retry identities, backup restoration and interruption tests before exposing it in the durable UI. No migration or backward-compatibility work is required before the owner-announced release gate.

## Adopted bounded implementation and remaining design

Represent source provenance, transfer relationships, and each account entry's commit state separately. A single committed flag for an entire transfer cannot express the accepted lifecycle. Protection needs a durable relationship to the committed origin; merely disabling a UI delete button is insufficient. The bounded storage schema and command contracts are now documented in [the durable transfer guide](../doc/transfer-engine.md). Ranking remains unresolved. Merge and replacement follow the 2026-10-27 decisions; committed reversal remains point 6.

Keep transfer recognition distinct from ordinary duplicate suppression: a matching bank record can provide new evidence for an existing counterpart and must not be silently discarded as an irrelevant duplicate. Reuse [import retry and atomicity requirements](003-qif-import-review.md), preserving history and evidence throughout repair.

## Full-feature acceptance criteria — partially implemented

- Synthetic checking-first and Visa-first imports each retain one movement per account, including matching against ordinary drafts, protected drafts, and committed counterparts.
- Different posting dates and legitimate repeated equal payments produce explainable outcomes without silent double counting or dropped evidence.
- Before checking commits, discarding its operation removes the purely generated Visa draft and changes neither account's committed balance.
- Committing checking commits only checking and durably protects the Visa draft. Visa committed balance and ordinary reports exclude it; Visa projected balance includes it exactly once.
- Clearing Visa drafts after checking commits preserves that protected entry, its link, and projection across restart.
- Visa becomes committed only after the user explicitly commits it; matching or accepting an unmatched counterpart alone does not commit it.
- Later Visa import evidence matches the existing movement without adding another amount or losing source records.
- Merge repairs missed and incorrect links atomically, removes only a purely generated uncommitted duplicate, links two independent opposite allocations without removing either, and preserves source evidence, amounts, and applicable history. A committed generated duplicate remains point 6.
- Origin deletion/reversal distinguishes generated from independently imported counterparts, including a generated entry that later acquired evidence.
- Unmatched attention items allow manual matching or acceptance as valid pending entries and remain matchable by later imports.
- Match navigation works both ways and with Back, including bringing the selected row on-screen and honoring unsaved edits.
- Injected failures, interruption on both sides of durable commit, retry, and backup/restore preserve per-account states, protection, relationships, evidence, and history without duplicate application.

## Remaining work and open questions

- Committed transfer reversal, ordinary Undo of a transfer, removal of a committed generated duplicate, and rewriting a committed allocation's category through a transfer command remain point 6.
- Candidate ranking, date windows, references/normalization, and automatic matching remain deferred. Links require explicit confirmation. Cross-currency and fee matching remain deferred; unequal allocations are split or chosen explicitly before a link.
- Generated counterpart field defaults other than the accepted provisional origin date.
- Reports and the broader transaction-sheet design remain outside the implemented transfer scope.

## Related requirements

- [Accounts, money, and splits](002-accounts-money-splits.md)
- [QIF import and review](003-qif-import-review.md)
- [Documents, storage, and security](001-documents-storage-security.md)
- [Transaction sheet and navigation](005-transaction-sheet-navigation.md)

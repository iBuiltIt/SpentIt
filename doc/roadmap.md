# Coordinated implementation roadmap

Last updated: 2026-10-26

The owner authorized coordinated implementation with delegated agents, continuing through routine implementation and verification without asking for approval at every step. Ask for concrete missing information or material financial/security decisions when dependent work reaches them. Routine editor layout choices can be implemented and revised after owner review. This authorization does not select a license, authorize publication, or settle unresolved financial semantics.

This is sequencing guidance, not a claim that the listed work exists. [Project status](status.md) records implementation evidence; [feature documents](../features/README.md) own requirements and unresolved decisions.

## Current checkpoint — 2026-10-26

The schema-6 verification gap is closed in Release and ASan/UBSan. The bounded search/ledger package now implements the owner-approved grammar, multiselection, and schema-7 atomic committed category/removal/group Undo; see [contract](search-ledger.md) and [verification](status.md). The bounded durable transfer package is now implemented; [QIF transfer-destination admission and explicit account resolution](qif-transfers.md) are now implemented. The owner authorized finishing that package and explicitly excluded proceeding to reports. The owner requested proceeding and accepted its first five scope decisions on 2026-10-26; see [feature 008](../features/008-transfer-matching-and-lifecycle.md#accepted-first-implementation-scope--2026-10-26). The [multi-account prerequisite](accounts.md) is implemented; the bounded transfer links, generated/protected drafts and provisional matching are now implemented. Remaining committed repair/reversal policies still require decisions.

Experimental formats are disposable until the owner explicitly announces a release. Migration, upgrade and backward-compatibility work are not pre-release obligations. Historical roadmap entries below retain the sequence and original checkpoints, not a requirement to build upgrade infrastructure now.

## Roadmap sequence and completion boundaries

1. Complete the bounded durable QIF workflow: operation phases, cancellable opening/file reading, commit-outcome recovery, then durable retained-evidence cancellation. Preserve existing documents; new persistent states require an explicitly versioned format and recovery tests.
2. Bring the accepted navigation shell into the durable UI: resize/hide sidebar, persistent toolbar, guarded Back/Forward and Settings navigation. Extend keyboard/accessibility coverage and keep unavailable financial actions unavailable.
3. Integrate and verify on available macOS configurations. Record actual full-suite results, failure/retry tests and UI inspection; do not inherit earlier prototype performance or platform claims.
4. Implement the accepted ordinary committed correction/removal/strict-undo policy in [feature 011](../features/011-committed-editing-and-reversal.md), preserving import receipts and replayable financial history. Use that foundation for later multi-account documents, per-account drafts and cross-account operations. No experimental-format migration work is required before the release gate.
5. Implement categories and rules against explicit semantics, preserving evidence, manual decisions and historical explanations. Editor appearance is a routine implementation choice; precedence and historical reclassification are financial decisions.
6. Complete search and ledger interactions: combined text/date/source search, selection restoration, multiselection and remaining transaction-sheet behavior under [feature 005](../features/005-transaction-sheet-navigation.md). The bounded combined-search, selection and atomic bulk package is implemented; the full feature-005 layout/accessibility scope remains incomplete. The owner confirmed this follows categories/rules on 2026-10-25.
7. Extend committed changes to the transfer lifecycle after resolving its remaining policy gates. Verify dependent relationships and both sides of each durable boundary.
8. Add exact allocation-based reports and further import formats. QIF remains first; precise future dialects and reporting policies must be specified.
9. Prepare distribution: reproducible dependencies, notices, packaging and recovery documentation, followed by owner-assisted platform testing and explicit licensing/publication decisions.

Independent packages may run concurrently with clear file ownership. Integrate and review changes before treating a package as verified. Prefer finishing an existing vertical slice to creating unconnected scaffolding for later features.

## Concrete input gates

- Committed changes: ordinary correction/removal, removed-record review and strict Undo are accepted and implemented in the bounded scope. Remaining gates concern linked transfers and other explicitly deferred operations.
- Transfers: destination-first commit with symmetric draft protection, explicit matching confirmation, provisional origin dates, per-allocation destinations and initial cross-currency rejection are accepted. Remaining gates include mismatch-resolution commands, destination-only draft cancellation, and committed repair/unlink/reversal with independent evidence or later edits.
- Categories/rules: current taxonomy, lifecycle, rule precedence/phases, manual protection and independent rule Save are accepted. Do not re-ask these decisions. Bulk committed reapplication remains deferred.
- Search/ledger: grammar, inclusive date boundaries, original-record scope, multiselection and bounded committed bulk behavior were accepted on 2026-10-26 and implemented. Broader feature-005 UI scope remains active.
- Reports: refund/income treatment, comparison boundaries, currency policy, and requested report-creation contents.
- Lifecycle: any destructive retention policy, protection changes, or migration behavior after the release gate.
- Distribution: final license and actual checks on unavailable operating systems.

Present proposed behavior and concrete examples when asking. An unresolved item does not block independent work, and previously accepted experimental decisions should not be asked again.


## Earlier verified checkpoints (historical)

The bounded cancellation/recovery and account/Settings navigation packages passed all 40 registered tests in Debug, Release and ASan/UBSan, with affected checks rerun after refinements and bounded native macOS recovery rendering inspected. See [project status](status.md). This does not finish multiselection, general search, accessibility or the full multi-account shell.

The owner then accepted the ordinary committed-change policy in feature 011. The bounded schema-4 backend, UI integration and independent failure tests are complete on available macOS configurations. All 51 registered cases passed in Debug (separate invocations), Release and ASan/UBSan, with affected checks rerun after final integrity refinements. See [committed changes](committed-changes.md). The owner accepted current category organization for all periods and the bounded transaction-then-split, per-field-first, manual-protected rules policy. The bounded category catalog/editor, the standalone rules evaluator/preview adapter, and schema-6 Save/Apply are implemented. The rules sheet is `147a5be`, verified in Debug only. Release and ASan/UBSan coverage of schema 6 and of the sheet are not recorded. Detailed transfer changes and reporting policies remain separate decisions.


## Handoff — 2026-10-26

The owner gave Go for the QIF destination package and explicitly said not to move on to reports. That bounded implementation now admits whole-record and split `[Account]` destinations, requires explicit account/link resolution, preserves transfer intent through rules and ordinary editing, and reuses the existing durable transfer commands. Read [the QIF contract](qif-transfers.md), [transfer contracts](transfer-engine.md), [accepted transfer requirements](../features/008-transfer-matching-and-lifecycle.md), and [current verification](status.md#qif-transfer-destinations--2026-10-26).

### Current state

- Parser `bank-v2`; experimental schema 12 with rules and 13 without rules. New-format datasets are required. Earlier layouts and the other build's format are rejected without migration. No released datasets exist.
- Names are retained as evidence, never auto-resolved. Destination selection uses stable account identities; duplicate names show currency and identity. Missing accounts are created separately. Bracketed allocations remain blocked from commit until explicitly assigned or matched. Unknown names and partial split resolution cannot silently become ordinary category postings.
- The existing transfer lifecycle still governs independent per-account commits, protection, provisional bank matches, discard, retries, history and restoration. Both import orders are exercised with bracketed evidence. Unequal amounts and cross-currency links remain rejected.
- Current test results and initial failures/corrections are recorded in [status](status.md#qif-transfer-destinations--2026-10-26). The earlier 94-case schema-10/11 baseline and sanitizer backup timeout/rerun history remain historical evidence in that file; they are not a claim of a full new-format suite run.
- Native-window interaction, Linux/Windows, full accessibility, physical power loss and representative multi-account/transfer performance remain unverified. The existing intermittent Qt delegate-reset warning is not claimed fixed.

### Resume and verify

Start with a new schema-12/13 synthetic dataset and confirmed opening balances. Import a banking record containing `L[Destination]`, or a split containing `S[Destination]`. Inspect the transfer allocation, choose its destination explicitly in Transfers, or confirm a match to an existing transfer. Commit each account separately. An unresolved destination must block commit; matching must not add a second movement.

Use the existing build trees in [building](building.md#qif-destination-follow-up--2026-10-26). The focused storage regression is `transferstore.qif`; `transferstore.imports` includes protected-import retention. `import.ui` includes `qifTransferResolution`, which checks the second split's stable identity and the destination placeholder. Run `transferstore.io` and `transferstore.crashes` after changing financial command boundaries. Preserve the distinction between selected test results and a complete suite run.

### Remaining work, not authorization to start

A unified category/account picker and browsable transfer journal remain follow-ups. Committed repair/unlink/reversal, destination-only draft cancellation, independently committed-entry linking, differently grouped evidence and amount-mismatch resolution retain their financial decision gates. Automatic matching and currency conversion remain deferred. Reports are outside the completed authorization; stop after this package.

Do not re-ask accepted choices: either account may commit first and protect the other draft; matches require explicit confirmation; generated dates are provisional origin dates; destinations and links belong to allocations; pending bank matches stay provisional until their import commits; discard retains evidence and drops the proposal; manual/unmatched entries may be accepted and committed independently. Acceptance never authorizes unequal-amount linking. The pre-release format policy remains in effect.

No private financial inputs, licensing, publication or release decisions are part of this package.

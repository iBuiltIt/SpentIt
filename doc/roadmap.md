# Coordinated implementation roadmap

Last updated: 2026-10-26

The owner authorized coordinated implementation with delegated agents, continuing through routine implementation and verification without asking for approval at every step. Ask for concrete missing information or material financial/security decisions when dependent work reaches them. Routine editor layout choices can be implemented and revised after owner review. This authorization does not select a license, authorize publication, or settle unresolved financial semantics.

This is sequencing guidance, not a claim that the listed work exists. [Project status](status.md) records implementation evidence; [feature documents](../features/README.md) own requirements and unresolved decisions.

## Current checkpoint — 2026-10-26

The schema-6 verification gap is closed in Release and ASan/UBSan. The bounded search/ledger package now implements the owner-approved grammar, multiselection, and schema-7 atomic committed category/removal/group Undo; see [contract](search-ledger.md) and [verification](status.md). The bounded durable transfer package is now implemented; the next implementation step is QIF transfer-destination admission and explicit account resolution. The owner requested proceeding and accepted its first five scope decisions on 2026-10-26; see [feature 008](../features/008-transfer-matching-and-lifecycle.md#accepted-first-implementation-scope--2026-10-26). The [multi-account prerequisite](accounts.md) is implemented; the bounded transfer links, generated/protected drafts and provisional matching are now implemented. Remaining committed repair/reversal policies still require decisions.

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

The bounded durable transfer package is implemented and verified within the [recorded test scope](status.md#durable-transfer-integration--2026-10-26). Read [transfer contracts and limits](transfer-engine.md), [build commands](building.md#durable-transfer-follow-up--2026-10-26), and [accepted transfer requirements](../features/008-transfer-matching-and-lifecycle.md) before continuing. The owner explicitly authorized this completed package and its commit. Present the next package and wait for an explicit Go before starting new implementation; do not infer authorization from this roadmap.

### What is ready

- Account creation/selection, independent pending imports and ordinary committed histories, account-local duplicate recognition, exact per-account balances, backup restoration, and guarded sidebar/Back/Forward navigation are implemented. Categories and saved rules remain document-wide; Apply and bulk commands are scoped to one account.
- Rules-enabled builds create/open schema 10; rules-off builds create/open schema 11. These are separate experimental layouts. Earlier formats and the other build's layout are rejected without migration. There are no released datasets; do not add compatibility infrastructure before the owner-announced release gate.
- All 94 current cases have passing Release and ASan/UBSan results across the broad runs and targeted additions. Release passed 92/92; the two later-registered split/manual-evidence cases passed separately. ASan/UBSan initially passed 90/92, with two backup sweeps timing out at 1,200 seconds; both passed unchanged with a 2,400-second budget. Debug passed 70 selected functional cases, and rules-off Debug passed all seven transfer storage groups plus UI/QML. Final affected UI/navigation checks passed in all four configurations. Preserve the timeout history rather than describing a clean first sanitizer run.
- Rendering was inspected offscreen. The intermittent Qt delegate-reset warning remains; previous workarounds regressed viewport behavior. Native interaction, Linux/Windows, full accessibility, physical power loss, installed-PCRE2 sanitizer instrumentation and representative multi-account performance remain unverified.

### Resume here

The [durable transfer workflow](transfer-engine.md) now implements manual/import-allocation creation, explicit unmatched acceptance, per-account commit and symmetric draft protection, pending bank proposals/discard/publication, atomic history and counterpart navigation. See [current evidence](status.md#durable-transfer-integration--2026-10-26) before making verification claims.

1. Complete QIF account-destination admission and explicit account resolution; the parser still rejects bracketed destinations. Do not infer an account from an ambiguous name.
2. Add a browsable transfer journal and integrate account destinations with allocation selection. Current durable events are replay-verified, and the UI shows the latest receipt and current links.
3. Resolve committed repair/unlink/reversal and destination-only cancellation policy before implementing those commands. Existing guards deliberately reject those changes.
4. Extend independent matching, grouped evidence and mismatch resolution with exact-money invariants. Automatic matching and cross-currency conversion remain deferred.
5. Verify Linux/Windows and native interactions, and measure representative transfer workloads before claiming responsiveness.

Do not re-ask the accepted transfer choices: either account may commit first and protect the other draft; initial matches require explicit confirmation; generated dates start at the origin date and are provisional; destinations and counterpart links belong to allocations; cross-currency transfers are initially rejected explicitly. Bank matches stay provisional until their import commits; discard drops the proposal while retaining evidence. Manual/unmatched entries may be accepted and committed independently, but unequal amounts cannot link until the difference is explicitly accounted for. Search grammar, inclusive dates, multiselection, committed-only/all-allocation bulk behavior and the pre-release format policy are also accepted.

Ask only when dependent work reaches the remaining financial gates: amount-disagreement resolution, destination-only cancellation while both sides are drafts, differently grouped counterpart allocations, and committed repair/unlink/reversal involving independent evidence or later edits. Do not infer cascade deletion or automatic amount correction. These gates do not block independent work on the accepted lifecycle.

No private financial inputs were used. No license, publication, or release decision was made.

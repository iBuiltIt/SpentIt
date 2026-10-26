# Coordinated implementation roadmap

Last updated: 2026-10-26

The owner authorized coordinated implementation with delegated agents, continuing through routine implementation and verification without asking for approval at every step. Ask for concrete missing information or material financial/security decisions when dependent work reaches them. Routine editor layout choices can be implemented and revised after owner review. This authorization does not select a license, authorize publication, or settle unresolved financial semantics.

This is sequencing guidance, not a claim that the listed work exists. [Project status](status.md) records implementation evidence; [feature documents](../features/README.md) own requirements and unresolved decisions.

## Current checkpoint — 2026-10-26

The schema-6 verification gap is closed in Release and ASan/UBSan. The bounded search/ledger package now implements the owner-approved grammar, multiselection, and schema-7 atomic committed category/removal/group Undo; see [contract](search-ledger.md) and [verification](status.md). The next substantive package is transfer lifecycle work. The owner requested proceeding and accepted its first five scope decisions on 2026-10-26; see [feature 008](../features/008-transfer-matching-and-lifecycle.md#accepted-first-implementation-scope--2026-10-26). The [multi-account prerequisite](accounts.md) is implemented; transfer links, generated/protected drafts and matching remain unfinished. Remaining committed repair/reversal policies still require decisions.

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

The multi-account prerequisite is implemented and verified within the [recorded test scope](status.md#account-scoped-durable-workflow--2026-10-26). Read [account contracts and limits](accounts.md), [build commands](building.md#account-scoped-follow-up--2026-10-26), and [accepted transfer requirements](../features/008-transfer-matching-and-lifecycle.md) before continuing. The owner authorized proceeding with implementation; routine work does not need another Go.

### What is ready

- Account creation/selection, independent pending imports and ordinary committed histories, account-local duplicate recognition, exact per-account balances, backup restoration, and guarded sidebar/Back/Forward navigation are implemented. Categories and saved rules remain document-wide; Apply and bulk commands are scoped to one account.
- Rules-enabled builds create/open schema 8; rules-off builds create/open schema 9. These are separate experimental layouts. Earlier formats and the other build's layout are rejected without migration. There are no released datasets; do not add compatibility infrastructure before the owner-announced release gate.
- All 58 selected functional cases have passing results in Debug, Release and ASan/UBSan across initial and corrected runs. Initial runs passed 56/58; two test-harness issues were corrected. Account I/O/crash sweeps additionally cover 17 creation and 125 second-account commit write/sync positions, both durable boundaries, retry and reopened verification. Final UI refinements passed in all three configurations and rules-off Debug. Do not claim a full 80-case suite pass or a clean first run.
- Rendering was inspected offscreen. The intermittent Qt delegate-reset warning remains; previous workarounds regressed viewport behavior. Native interaction, Linux/Windows, full accessibility, physical power loss, installed-PCRE2 sanitizer instrumentation and representative multi-account performance remain unverified.

### Resume here

1. Continue the first same-currency transfer slice; do not redo account creation or ordinary committed editing. The parser still rejects transfer destinations, and no transfer links, generated counterparts or protected drafts exist yet.
2. Use stable account and allocation identities for relationships. Keep generated provenance separate from immutable bank evidence; attaching imported evidence must not create another financial movement or silently suppress it as an ordinary duplicate. Read the existing account-scoped store, application command boundary and account tests before extending them.
3. Implement complete transfer operations through one storage transaction, including relationship/history changes and protection of the opposite draft. Do not implement cross-account operations as a UI loop of independently committed commands. Add interruption, retry, corruption and restored-backup cases for both account commit orders before exposing the operations.
4. Integrate counterpart navigation with the guarded account navigation. Per-account selection/search/viewport restoration is still missing; account switches currently clear that view state. Account rename/archive/delete, currency reassignment, per-account sidebar balance layout and reports remain separate unfinished work.

Do not re-ask the five accepted transfer choices: either account may commit first and protect the other draft; initial matches require explicit confirmation; generated dates start at the origin date and are provisional; destinations and counterpart links belong to allocations; cross-currency transfers are initially rejected explicitly. Search grammar, inclusive dates, multiselection, committed-only/all-allocation bulk behavior and the pre-release format policy are also accepted.

Ask only when dependent work reaches the remaining financial gates: amount-disagreement resolution, destination-only cancellation while both sides are drafts, differently grouped counterpart allocations, and committed repair/unlink/reversal involving independent evidence or later edits. Do not infer cascade deletion or automatic amount correction. These gates do not block independent work on the accepted lifecycle.

No private financial inputs were used. No license, publication, or release decision was made.

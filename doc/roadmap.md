# Coordinated implementation roadmap

Last updated: 2026-10-25

The owner authorized coordinated implementation with delegated agents, continuing through routine implementation and verification without asking for approval at every step. Ask for concrete missing information or material financial/security decisions when dependent work reaches them. Routine editor layout choices can be implemented and revised after owner review. This authorization does not select a license, authorize publication, or settle unresolved financial semantics.

This is sequencing guidance, not a claim that the listed work exists. [Project status](status.md) records implementation evidence; [feature documents](../features/README.md) own requirements and unresolved decisions.

## Current packages

1. Complete the bounded durable QIF workflow: operation phases, cancellable opening/file reading, commit-outcome recovery, then durable retained-evidence cancellation. Preserve existing documents; new persistent states require an explicitly versioned format and recovery tests.
2. Bring the accepted navigation shell into the durable UI: resize/hide sidebar, persistent toolbar, guarded Back/Forward and Settings navigation. Extend keyboard/accessibility coverage and keep unavailable financial actions unavailable.
3. Integrate and verify on available macOS configurations. Record actual full-suite results, failure/retry tests and UI inspection; do not inherit earlier prototype performance or platform claims.
4. Implement the accepted ordinary committed correction/removal/strict-undo policy in [feature 011](../features/011-committed-editing-and-reversal.md), preserving import receipts and replayable financial history. Use that foundation for later multi-account documents, per-account drafts and cross-account operations. Design schema evolution with explicit preservation of the last known-good document.
5. Implement categories and rules against explicit semantics, preserving evidence, manual decisions and historical explanations. Editor appearance is a routine implementation choice; precedence and historical reclassification are financial decisions.
6. Complete search and ledger interactions: combined text/date/source search, selection restoration, multiselection and remaining transaction-sheet behavior under [feature 005](../features/005-transaction-sheet-navigation.md). Existing literal-filter restoration and navigation are implemented subsets; specify remaining search grammar and boundaries before claiming completion. The owner confirmed this follows categories/rules on 2026-10-25.
7. Extend committed changes to the transfer lifecycle after resolving its remaining policy gates. Verify dependent relationships and both sides of each durable boundary.
8. Add exact allocation-based reports and further import formats. QIF remains first; precise future dialects and reporting policies must be specified.
9. Prepare distribution: reproducible dependencies, notices, packaging and recovery documentation, followed by owner-assisted platform testing and explicit licensing/publication decisions.

Independent packages may run concurrently with clear file ownership. Integrate and review changes before treating a package as verified. Prefer finishing an existing vertical slice to creating unconnected scaffolding for later features.

## Concrete input gates

- Committed changes: ordinary correction/removal, removed-record review and strict Undo are accepted and implemented in the bounded scope. Remaining gates concern linked transfers and other explicitly deferred operations.
- Transfers: destination-first commit, matching disagreements, generated dates, split destinations, and cross-currency policy.
- Categories/rules: current taxonomy, lifecycle, rule precedence/phases, manual protection and independent rule Save are accepted. Do not re-ask these decisions. Bulk committed reapplication remains deferred.
- Search/ledger: specify remaining combined query grammar, date boundaries, source-field scope and selection behavior before implementing the next package.
- Reports: refund/income treatment, comparison boundaries, currency policy, and requested report-creation contents.
- Lifecycle: any destructive retention policy, protection changes, or unsupported migration behavior.
- Distribution: final license and actual checks on unavailable operating systems.

Present proposed behavior and concrete examples when asking. An unresolved item does not block independent work, and previously accepted experimental decisions should not be asked again.


## Verified checkpoint and active work

The bounded cancellation/recovery and account/Settings navigation packages passed all 40 registered tests in Debug, Release and ASan/UBSan, with affected checks rerun after refinements and bounded native macOS recovery rendering inspected. See [project status](status.md). This does not finish multiselection, general search, accessibility or the full multi-account shell.

The owner then accepted the ordinary committed-change policy in feature 011. The bounded schema-4 backend, UI integration and independent failure tests are complete on available macOS configurations. All 51 registered cases passed in Debug (separate invocations), Release and ASan/UBSan, with affected checks rerun after final integrity refinements. See [committed changes](committed-changes.md). The owner accepted current category organization for all periods and the bounded transaction-then-split, per-field-first, manual-protected rules policy. The bounded category catalog/editor, the standalone rules evaluator/preview adapter, and schema-6 Save/Apply are implemented. The rules sheet is `147a5be`, verified in Debug only. Release and ASan/UBSan coverage of schema 6 and of the sheet are not recorded. Detailed transfer changes and reporting policies remain separate decisions.


## Handoff — 2026-10-25

Schema 6 rule persistence is on `main` (`9b7cf40`). The rules sheet is `147a5be`. Evidence and limits are in [project status](status.md). The next package is search and ledger completion: combined text/date/source search, selection restoration, multiselection, and the remaining transaction-sheet behavior in [feature 005](../features/005-transaction-sheet-navigation.md). Specify the remaining search grammar before implementing it. Do not start transfers or reports first. Release and ASan/UBSan coverage of schema 6 and of the sheet are not done. Committed reapplication, other platforms, PCRE2 sanitizer coverage, and physical power loss are not done.

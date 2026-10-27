# Coordinated implementation roadmap

Last updated: 2026-10-27

The owner authorized coordinated implementation with delegated agents, continuing through routine implementation and verification without asking for approval at every step. Ask for concrete missing information or material financial/security decisions when dependent work reaches them. Routine editor layout choices can be implemented and revised after owner review. This authorization does not select a license, authorize publication, or settle unresolved financial semantics.

This is sequencing guidance, not a claim that the listed work exists. [Project status](status.md) records implementation evidence; [feature documents](../features/README.md) own requirements and unresolved decisions.

## Current authorization and delivery checkpoints — 2026-10-26

The owner selected the following eight-point sequence and gave **Go for point 1**. Use one implementation agent per numbered point, in succession; each agent owns discovery, implementation, verification and documentation for its area. This supersedes the earlier permission for concurrent independent packages and the historical sequence below. The coordinator reviews the result and handles the handoff and commit. Do not start the next point without the owner's answer at its checkpoint.

1. **Finish the durable QIF workflow:** specify supported dialects beyond the banking profile; address multi-account sections, category/class notation and required export variations; extend review/repair; complete remaining long-operation cancellation, progress, retry and recovery UX; verify import → review → correction → commit → reopen.
2. **Harden the implementation:** coverage-guided parser fuzzing, combined-workflow/current-format failure and recovery coverage, keyboard/focus/accessibility checks, representative performance, Linux/Windows validation, and the intermittent Qt delegate-reset warning.
3. **Add QFX, QBO, OFX and ASO imports:** establish dialects and ASO contents, preserve native source bytes, implement shared source facts and evidence associations, and reviewable occurrence-aware cross-format duplicates with overlap/failure tests.
4. **Complete account management:** renaming, archiving/reactivation, safe empty-account deletion, committed opening-balance correction, and explicit currency reassignment with exact precision, history, draft and transfer consequences.
5. **Complete transfer management:** Fix match, unlink/replacement, independently imported and committed counterparts, grouped/unequal allocation resolution, cancellation/removal provenance, a transfer journal and unified destination selection, preserving evidence and later changes.
6. **Complete committed editing and reversal:** linked transfers, settled account/source-association corrections, previews/conflicts, atomic retries, history replay and evidence preservation.
7. **Finish categories and rules:** settle committed-record reapplication with preview/manual protection/history, remaining picker/editor and explanation UX, and classification across new formats without changing evidence or money.
8. **Finish search and ledger interactions:** remaining sheet/sidebar layout, keyboard/multiline/narrow-window/layout preferences, remaining bulk behavior, and selection/viewport/unsaved-edit verification across new operations.

After **each point**, update relevant feature documents and project status, write a durable handoff with completed work, actual verification, limitations, unresolved decisions and downstream dependencies, and commit all task changes while preserving unrelated work. Then ask whether to proceed to the next point or stop, and wait. A material financial/security ambiguity is an earlier decision gate; Go does not settle it. Dependencies on later points must be recorded explicitly rather than silently reducing completion scope. Hardening continues in later points as their behavior changes; point 2 is not a final verification claim for future code. Reports and licensing/distribution are outside this eight-point authorization.

Point 1 is complete and committed as `933ede1` and `f77b4ac`. Point 2 is complete and committed as `ec17bb2`. The owner excluded Linux and Windows from point 2; those platforms remain unverified. The owner then authorized point 3. Coordinator review on 2026-10-27 accepted the point-3 implementation and the available macOS verification, with the limits recorded below. ASO is admitted as the inspected OFX 1.0.2 SGML credit-card label. Coordinator review on 2026-10-27 accepted the point-4 implementation at schemas 20 and 21, with the limits recorded below. Points 5–8 have not been started. The owner is asked whether to proceed to point 5 or stop.

## Point 4 completion handoff — 2026-10-27

Read [feature 002](../features/002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27), [accounts](accounts.md#account-commands--2026-10-27), and [point-4 evidence](status.md#point-4-account-management--2026-10-27). One agent implemented this checkpoint from HEAD `f1004ea`. Points 5–8 were not started. No license, publication, conversion, exchange rate, report, or transfer-repair decision was made.

Policies applied:

- New documents use schema 20 with rules and schema 21 without rules. Schemas 18 and 19 are rejected with `Unsupported experimental format; no migration was attempted`. Application id `1397770577` is unchanged. There is one schema shape and no migration.
- Rename, archive, reactivation, empty-account deletion, opening-balance correction, and currency reassignment are explicit historied commands on the existing serial worker. Identical retries return the stored receipt. Conflicting operation-id reuse is rejected. Durable completion precedes a success report.
- Rename keeps the creation name. Archive refuses a pending import and leaves it in place. Deletion retires an untouched zero account that is not the last account and does not reuse the identity. Opening correction replays the prior opening and refuses a date after a committed transaction on that account. Currency reassignment preserves exact decimals, refuses a dropped non-zero fraction or overflow, and leaves source bytes and source-occurrence currency facts on the old currency.
- An already linked transfer may become two preserved opposite decimals in different currencies through `preserved_amounts`. The other side stays. New matches remain same-currency. This recording did not require a new transfer policy, so that case was implemented and no decision gate was left open. Fix match, unlink, transfer repair, and committed transfer reversal were not implemented.
- The currency dialog warns before OK with `This reassigns the account currency without conversion. Numeric amounts stay the same decimal. It does not apply an exchange rate.` Cancel changes nothing. Sidebar rename, archive, archived section, reactivate, and delete are present. No `import.ui` runtime test was run.

Exact commands, the validation failure that was fixed, and the passing counts are in status. The passing selected runs are rules-on Debug 18/18 in 45.14 seconds, rules-on Release 18/18 in 42.92 seconds, rules-on ASan/UBSan 18/18 in 84.47 seconds, and rules-off Debug 13/13 in 38.58 seconds. Representative interruption positions for each new command passed inside `accounts.lifecycle_boundaries`. Linux and Windows are unverified. These selections are not a full suite.

Coordinator review on 2026-10-27 accepted this diff with the limits below. The owner is asked: **proceed to point 5 or stop**.

## Point 3 completion handoff — 2026-10-27

Read the [statement profile](../features/003-ofx-profile.md) and [point-3 evidence](status.md#point-3-statement-imports--2026-10-27). One agent implemented this checkpoint. Points 4–8 were not started. No license or publication decision was made. Coordinator review on 2026-10-27 accepted the diff with the limits below.

Specified from OFX Banking Specification Version 2.3 (October 2020), statement sections 11.4.2–11.4.4, with the header and common-data-type bodies unread and labeled as implementation choices:

- OFX is the statement grammar. QFX, QBO, and ASO are file labels of that grammar, stored separately from `ofx-sgml` and `ofx-xml`.
- Admitted messages are signon plus bank and credit-card statement transaction lists (`STMTTRN`, and `CCSTMTTRN` when a file uses that tag). Investment, securities, bill pay, profile, and pending `STMTTRNP` lists refuse the file before capture.
- Original bytes stay unchanged on disk and in encrypted storage. QIF imports in schema 18/19 use the same occurrence and association tables.
- Duplicate recognition is reviewable. Same `FITID`, currency, and original amount is a disposition-gated candidate. Different `FITID`s on the same day stay distinct. Differing dates across OFX-family evidence, including `aso`, stay uncertain. QIF-to-QIF different dates stay non-candidates.
- ASO was identified by in-place structural inspection of the owner's private export. That export is OFX 1.0.2 SGML credit-card statement syntax. No path, filename, or contents were recorded. Format `aso` uses `ofx-sgml-1` for this shape and `ofx-xml-2` when the body is XML. `SECURITY:TYPE1` is accepted on a readable body. `USERKEY` stays only in the original bytes. A body that is not OFX SGML or OFX XML is still refused, with the original file unchanged and no partial import saved.

Implemented: engine SGML and XML parsers, schema 18/19 evidence tables with `aso` legal in the format checks, group prepare/mapping/commit associations, review UI file choice including `*.aso`, match-basis labels, synthetic tests, one clean sanitizer fuzz campaign of 197,324 executions from before this ASO admission, and a short replay of the synthetic ASO fixture on the rebuilt fuzzer. Currency mismatch refuses without conversion. Statement transfers and several uninterpreted financial aggregates stay visible blockers. Account lifecycle remains point 4. Feature 003 stays active.

Exact commands, pass counts, the two earlier fuzz crashes that were fixed, the ASO replay, and the sweeps that were not rerun are in status. Linux and Windows are unverified. The short ASO fixture replay exited 0. A new multi-hour fuzz campaign was not started.

The owner was asked at this delivery: **proceed to point 4 or stop**. Point 4 is now the [next handoff](#point-4-completion-handoff--2026-10-27). The wait instruction above is the historical stop at point-3 delivery.

## Point 2 completion handoff — 2026-10-26

Read [hardening evidence](hardening.md), [current workload observations](import-ui-performance.md#current-format-point-2-observations--2026-10-26) and [status](status.md#point-2-current-format-hardening--2026-10-26). One agent implemented this checkpoint; no point-3 work started. No financial policy, import acceptance, schema, migration, license or publication decision changed.

Delivered changes:

- Optional pinned libFuzzer runtime construction, synthetic seeds/dictionary/boundary generator and expanded parser invariants; two actual coverage-guided sanitizer campaigns completed 750,204 executions without a parser crash.
- Combined repair/category/rule/balance/commit/correction/Undo/overlap/restore workflows with before/after durable-boundary exits, disk-full/I/O injection, reopening, exact history/evidence checks and retries. Rules-disabled behavior is covered separately.
- Accessible ledger semantics/names/selection and progress/error announcements; scripted keyboard/focus, native Cocoa and hidden-modal-state checks. The observed native AX tree is documented separately from unverified spoken output.
- A bounded Qt delegate-reset fix using cacheBuffer zero, preserving model attachment, reuse, viewport and unsaved editors. Twenty warning-fatal repeat pairs and broad UI checks passed.
- Scoped prepared read-plan reuse, direct exact occurrence-evidence retrieval, reduced search copies and duplicate category-head read removal. No validation, encryption or durability mechanism was disabled.

The latest broad Release/sanitizer functional selections passed 75/75; final focused Debug/Release/sanitizer checks passed 6/6 and rules-off 5/5. Earlier Debug broad 75/75, final combined-I/O sanitizer cases, native Cocoa 3/3 and the exact commands are recorded in the guide. Do not turn these selections into a full 103-case-suite claim. Final benchmark-only frame validation passed a small native paired-cache smoke.

A full 100k diagnostic completed all correctness assertions; its timing was sampled/overlapped and is not a controlled baseline. The final 200k interaction run completed correctness but had no native frame samples. The final 100k attempt produced valid cache-zero scrolling and retained the edit, then failed when default-cache frames were unavailable; it did not reach save/final completion. No extra large rerun is required for this checkpoint. Timings are non-monotonic single-run observations with no established cause or scalability claim.

Remaining verification limits are explicit: actual VoiceOver speech and complete manual assistive traversal, a paired large native cache comparison, 200k native rendering, and the owner-excluded Linux/Windows builds/interactions are unverified. Imports/verified opening remain long operations. Physical power-loss behavior and broader future product workflows are not claimed by these process/I/O tests. Later points must retain and expand these checks as their behavior changes; other import formats will need exact evidence access adapted to their shared source representation.

Implementation edits have stopped after this handoff. The coordinator should inspect the final diff, verify documentation links, commit all checkpoint files (including six explicitly excepted synthetic fuzz seeds), and ask **proceed to point 3 or stop**. Do not start the next point until the owner answers.

Update 2026-10-27: this checkpoint was committed as `ec17bb2`. The owner authorized point 3. The wait instruction above is the historical stop at point-2 delivery. The current authorization is the point-3 handoff.

## Point 1 completion handoff — 2026-10-26

Read the [whole-source QIF contract](qif-account-imports.md), [profile](../features/009-qif-profile.md) and [current verification](status.md#whole-source-qif-account-review--2026-10-26). The intermediate backup/parser/nullable-repair checkpoint was committed as `933ede1`; its earlier schema-12 and schema-14/15 evidence is recorded separately in status. At this handoff, grouped imports used parser bank-v3 and experimental schemas 16/17. New-format datasets were required; no migration or compatibility work was added. Update 2026-10-27: the statement checkpoint used schemas 18/19 and rejected 16/17. Point 4 new documents use schemas 20/21 and reject 18/19. See the [point-3 handoff](#point-3-completion-handoff--2026-10-27) and the [point-4 handoff](#point-4-completion-handoff--2026-10-27).

The five point-1 requirements are implemented:

- The accepted UTF-8 ordinary Bank/Cash/CCard profile has explicit DMY/MDY/four-digit years, separate category/class evidence, account lists and repeated sections. Unsupported financial dialects and malformed account structures fail visibly; transaction-level blockers remain reviewable. No universal QIF claim is made.
- Full unchanged source bytes are stored once. Explicit mapping/creation covers every source account, including metadata-only and empty accounts; no section is silently omitted. Account names remain evidence, not inferred identities or currencies.
- Repair covers dates, split totals and unavailable parent/split money with explicit effective amounts and duplicate dispositions. Upfront total/amount/unsupported counts and saved-repair progress accompany whole-source Abort. Original unknown money, source identities and transfer intent survive repairs.
- Controlled backup and import scanning/parsing/materialization have cancellation/progress and retained-source recovery. Stable operation/mapping identities, durable outcome lookup and explicit unknown-outcome reopening protect retries. Backups remain encrypted and are verified by restoration.
- Import → review → correction → per-account commit → reopen is covered by synthetic engine/storage/model/QML regressions, including grouped transfers, reported balances, duplicate multiplicity, cancellation, Abort after saved repair, and acknowledgement/failure boundaries. Exact results and limitations are recorded in status.

Accepted balance semantics are implemented: only explicitly created accounts offer reported `B` reconciliation; existing openings never change. Exact reviewed change includes generated transfer effects once. Equality supports zero opening; explicit opening/closing/Ignore choices stage exact money and date without changing committed balances. First account financial publication publishes that opening atomically. No grouped or external transfer publication may protect an incomplete source. Whole-source Abort ends at first publication and removes unused group-created accounts from the active list while retaining internal identities/evidence/history. Later per-account protected-discard behavior is explicit.

For resumption, rebuild the existing trees in [building](building.md#whole-source-qif-follow-up--2026-10-26). `accounts.groups` exercises the full source lifecycle and `accounts.group_io` / `accounts.group_crashes` exercise seven storage boundaries. `import.ui` exercises actual QML mapping, balance staging and Abort plus outcome recovery and delegate recreation. Final functional selections passed 74/74 in Debug, Release and ASan/UBSan; current-format Release/sanitizer fault selections each passed 11/11. Rules-off passed nine selected checks; additional authenticated corruption and exact-marker/restore validation passed directly in all four builds. These are selected checks, not a full 101-case suite. Offscreen mapping/review scenes were inspected. No compiler or sanitizer diagnostics occurred; the existing Qt delegate-reset warning remains. The coordinator must run final documentation link/diff checks, commit all point-1 changes, then ask **proceed to point 2 or stop**.

Point 2 owns coverage-guided fuzzing (only deterministic mutation checks have run), broader combined-workflow recovery, accessibility/keyboard audits, representative performance, Linux/Windows and the intermittent Qt delegate-reset warning. Point 3 owns other formats and cross-format evidence/duplicates. General account lifecycle, opening corrections after commit and currency reassignment remain point 4; transfer repair/journal/unlink remains point 5; linked committed reversal remains point 6; broader rules and ledger polish remain points 7–8. These are dependencies and later roadmap scopes, not implemented point-1 claims. Private account names/suffixes/balances from discussion must never enter repository fixtures or documentation.

## Previous checkpoint — 2026-10-26

The schema-6 verification gap is closed in Release and ASan/UBSan. The bounded search/ledger package now implements the owner-approved grammar, multiselection, and schema-7 atomic committed category/removal/group Undo; see [contract](search-ledger.md) and [verification](status.md). The bounded durable transfer package is now implemented; [QIF transfer-destination admission and explicit account resolution](qif-transfers.md) are now implemented. The owner authorized finishing that package and explicitly excluded proceeding to reports. The owner requested proceeding and accepted its first five scope decisions on 2026-10-26; see [feature 008](../features/008-transfer-matching-and-lifecycle.md#accepted-first-implementation-scope--2026-10-26). The [multi-account prerequisite](accounts.md) is implemented; the bounded transfer links, generated/protected drafts and provisional matching are now implemented. Remaining committed repair/reversal policies still require decisions.

Experimental formats are disposable until the owner explicitly announces a release. Migration, upgrade and backward-compatibility work are not pre-release obligations. Historical roadmap entries below retain the sequence and original checkpoints, not a requirement to build upgrade infrastructure now.

## Earlier roadmap sequence and completion boundaries (historical)

1. Complete the bounded durable QIF workflow: operation phases, cancellable opening/file reading, commit-outcome recovery, then durable retained-evidence cancellation. Preserve existing documents; new persistent states require an explicitly versioned format and recovery tests.
2. Bring the accepted navigation shell into the durable UI: resize/hide sidebar, persistent toolbar, guarded Back/Forward and Settings navigation. Extend keyboard/accessibility coverage and keep unavailable financial actions unavailable.
3. Integrate and verify on available macOS configurations. Record actual full-suite results, failure/retry tests and UI inspection; do not inherit earlier prototype performance or platform claims.
4. Implement the accepted ordinary committed correction/removal/strict-undo policy in [feature 011](../features/011-committed-editing-and-reversal.md), preserving import receipts and replayable financial history. Use that foundation for later multi-account documents, per-account drafts and cross-account operations. No experimental-format migration work is required before the release gate.
5. Implement categories and rules against explicit semantics, preserving evidence, manual decisions and historical explanations. Editor appearance is a routine implementation choice; precedence and historical reclassification are financial decisions.
6. Complete search and ledger interactions: combined text/date/source search, selection restoration, multiselection and remaining transaction-sheet behavior under [feature 005](../features/005-transaction-sheet-navigation.md). The bounded combined-search, selection and atomic bulk package is implemented; the full feature-005 layout/accessibility scope remains incomplete. The owner confirmed this follows categories/rules on 2026-10-25.
7. Extend committed changes to the transfer lifecycle after resolving its remaining policy gates. Verify dependent relationships and both sides of each durable boundary.
8. Add exact allocation-based reports and further import formats. QIF remains first; precise future dialects and reporting policies must be specified.
9. Prepare distribution: reproducible dependencies, notices, packaging and recovery documentation, followed by owner-assisted platform testing and explicit licensing/publication decisions.

The earlier authorization allowed independent packages to run concurrently with clear file ownership. It is superseded by the sequential one-agent-per-point authorization above. Integrate and review changes before treating a package as verified. Prefer finishing an existing vertical slice to creating unconnected scaffolding for later features.

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

This is the previous completed QIF destination-package handoff. The current eight-point authorization and point-1 discovery checkpoint above supersede its stop/start instructions.

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

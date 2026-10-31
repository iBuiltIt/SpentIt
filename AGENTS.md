# SpentIt — Where did my money go?

## Next work — owner decisions 2026-10-31

Prioritize [opening/ledger cleanup and interrupted-import rollback](doc/ui-import-cleanup.md)
before the older handoff work. These are accepted requirements, not implemented
behavior or a new implementation Go. Preserve completed uncommitted imports; the
new automatic cleanup targets only incomplete preparation for one import across
accounts. Account-context Discard remains account-owned. Read the contract before
changing source retention, import cancellation, startup cleanup or the ledger UI.

SpentIt is a local, single-user desktop application for understanding spending through reliable imports, reviewable categorization, and explainable reports. Data safety, security, and correctness take priority over speed of development. Losing or silently misrepresenting financial data loses the user's trust.

## Discover the project before changing it

1. Read this file and `doc/status.md`.
2. Read `doc/project-layout.md`, `features/README.md`, and the feature documents relevant to the task.
3. Inspect existing implementation and tests when they exist. Documentation is not evidence that code exists or works.
4. Identify accepted requirements, provisional designs, and open questions before making changes. Ask about material ambiguities; do not invent financial or security behavior.

## Diagnostics hooks and agent profiling

When the owner mentions hooks, diagnostics, introspection, observing the app, or agent-assisted profiling, read [Agent guide: using SpentIt diagnostics hooks for profiling](doc/agent-diagnostics-guide.md) first, then its linked runtime contract. The guide covers build variants, exact-process attachment, endpoint discovery, read-only CLI use, operation/UI correlation, timing interpretation and reproducible workloads. Owner instruction (2026-10-30): a request to use hooks explicitly pre-authorizes native macOS PID-targeted window capture and Accessibility inspection when the usual UI attachment is unavailable or unclear; do not ask for another method-specific Go. Retain and verify the original process identity, never use app-name resolution that can launch a second instance, and establish inspection access before asking the owner to prepare a selection. Follow the guide's standing authorization and OS/tool-permission boundaries. Reading a specifically selected pair's sources is authorized by a request to investigate that pair; generic profiling does not authorize unrelated private-data access, financial actions or implementation changes.

## Where information belongs

- `AGENTS.md`: durable contributor instructions, discovery, and engineering discipline. Keep detailed feature designs elsewhere.
- `doc/`: actual project status, verified implementation facts, architecture decisions once adopted, and project-wide verification/recovery guidance. Keep status current when work changes it.
- `features/`: feature requirements, design proposals, acceptance criteria, unresolved questions, and feature-specific implementation/verification notes. Start at its index.
- `src/`: first-party implementation, separated into `engine/`, `storage/`, `app/`, and `ui/` as described in `doc/project-layout.md`.
- `tests/`: automated tests and synthetic fixtures; `cmake/`: build support; `third_party/`: dependency metadata and deliberately vendored code only.
- Create implementation directories when they receive real content. Keep generated builds and dependency downloads out of the source tree and version control.

Use relative links between repository documents. Do not rely on chat history, private attachment paths, or a particular developer's machine for essential requirements. Preserve meaningful decisions and rationale. Do not copy personal financial records into the repository.

## Status and evidence

- Use the feature lifecycle in `features/README.md`: `active`, `landed`, or `abandoned`.
- Lifecycle is separate from decision certainty. Label accepted requirements, proposed implementation, and unresolved decisions explicitly.
- `active` does not mean implemented. Mark a feature `landed` only when its stated scope is implemented and the required verification is recorded. Split out deferred scope rather than concealing it.
- Record what was tested, how, and the actual result. Never claim a command ran, a platform passed, or a safety property was verified without evidence.
- On incomplete work, state what remains and any limitations. Do not describe scaffolding or happy-path tests as complete financial correctness.
- Update a feature document's last-updated date only when its content changes meaningfully; use ISO dates.

## Non-negotiable data invariants

- Use exact monetary arithmetic with explicit currency and scale. Never use binary floating point for authoritative money values; detect overflow and invalid precision.
- Splits sum exactly to their effective transaction total. Mixed signs are supported. Original imported amounts remain available when a user corrects a total.
- Never combine different currencies without an explicit conversion policy and recorded conversion inputs.
- Follow the [accepted import/commit direction](doc/import-commit-discussion.md): a separate permanent original-import evidence archive is no longer required. Forget removes exclusively owned import material; retain only ownership relationships needed to avoid deleting retained transactions. The current UI uses source-owned account rollback and removes exclusively owned material; shared material remains only while retained transactions require it.
- New/uncommitted transactions persist across restarts. The accepted target includes them in one balance and default future reports, removing the actual/projected distinction; see the [handoff](doc/import-commit-discussion.md). The normal ledger and displayed balance now include uncommitted rows; committed-only calculations remain internal to acceptance receipts and correction/history verification.
- Commit the entire chosen operation atomically, including linked transfers, status transitions, and history. The current UI commits all new transactions dataset-wide; legacy per-account storage entry points remain for existing internal history/test paths. Committed means accepted, not uneditable. Never report success before durable completion.
- Preserve persistent history for committed changes. Reversal must respect later changes and financial relationships; it must not silently overwrite newer work.
- Duplicate recognition must preserve evidence and legitimate repeated occurrences. An uncertain match is not permission to drop a transaction silently.
- Reporting totals must reconcile to the exact allocations and filters shown by drill-down.
- No silent repair, rounding, truncation, skipped records, fallback to plaintext, or destructive migration. Explain and surface errors.

## Security and recovery discipline

Pre-release format policy (owner decision, 2026-10-26): no datasets have been released or exist "in the wild." Current experimental formats are disposable; format changes need no migration, upgrade path, or backward-compatibility work. The owner will explicitly announce the first release, which is the gate for introducing those obligations. Do not add compatibility work solely to preserve earlier experimental schemas. Current-format atomicity, financial correctness, encryption, and failure/recovery verification remain required. Migration guidance below applies when migrations become relevant after that release gate.

- Use established cryptographic libraries and supported interfaces. Do not invent encryption or password derivation schemes.
- Optional no-password mode is an explicit owner-approved waiver of confidentiality, using a documented public format key recoverable without the original installation. Do not describe it as confidential or silently apply it to password-protected data; see [document setup and recovery](features/001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25).
- Keep documents, drafts, source records, and change history encrypted at rest. Review journals, temporary storage, logs, diagnostics, exports, and backups for leakage.
- Never log passwords, keys, full financial records, or sensitive query parameters. Do not commit real statements, account identifiers, databases, credentials, or private fixtures.
- Use synthetic or deliberately sanitized fixtures that preserve the relevant edge cases.
- The owner has private QIF samples and matching QFX/ASO/QBO/OFX exports of one real-world dataset and can be asked for access when needed. Do not record their paths, filenames or contents, move the originals, or commit them. Prefer synthetic structural fixtures; private samples are not routine automated test inputs. See [sample handling](features/009-durable-import-milestone.md#private-sample-availability).
- Treat imported files and regex patterns as untrusted input. Bound resource consumption; distinguish invalid input and execution failure from a valid empty result.
- Do not disable integrity or durability mechanisms to make benchmarks faster or the file layout simpler.
- Test backups by restoring them. Do not assume a live copy of a database's main file is a valid backup.
- Password changes, schema migrations, and document switching must have explicit failure/recovery behavior. Never destroy the last known-good copy on failure.
- The initial product has no password-reset mechanism. Do not imply forgotten-password recovery exists.

## Implementation and verification

- Keep financial logic independently testable from UI presentation. UI validation does not replace domain/database validation.
- Enforce the dependency boundaries in `doc/project-layout.md` through build targets when implementation begins. UI code must not execute SQL or own financial transaction boundaries; the engine must not depend on UI or concrete storage.
- Prefer small, reviewable changes with explicit invariants and error handling. Do not rewrite unrelated working code.
- Test relevant failure paths as part of financial/storage work: disk-full or I/O failure, interrupted operations, retries, malformed input, overflow, and inconsistent relationships.
- Verify atomicity and retry behavior around commit. Test interruptions on both sides of the durable-commit boundary.
- Test duplicate multiplicity, ambiguous dates, signed splits, transfers, currency boundaries, rule ordering/resource limits, and report drill-down reconciliation where relevant.
- Use appropriate memory/undefined-behavior diagnostics and parser fuzzing when C/C++ implementation warrants them.
- Validate supported macOS, Linux, and Windows builds and interactions before claiming portability. Record unavailable platform checks as unverified.
- Keep heavy work off the UI thread. Measure representative workloads before claiming responsiveness.
- Run checks appropriate to the change. Documentation-only work needs consistency/link checks, not invented runtime tests.

## Scope and licensing

Review responsiveness and batch Merge (owner Go, 2026-10-31): the permanent bottom status line serves every right-pane workspace. Duplicate groups can be selected separately from their transaction rows and merged in one atomic batch with one Undo. Pending selection clears on leaving Review or dataset mutation; inspection alone retains it. Stale/invalid groups reject the whole batch, and cancellation before the final commit gate rolls it back. Link remains individual. Review reuses original comparison facts and checks the unchanged engine predicate before saved-choice lookups. Experimental schemas remain 38/37. See [the contract](doc/review-sheet.md#batch-merge-and-stable-status--owner-go-2026-10-31) and [status](doc/status.md) for measurements, verification and limitations.

Import-mapping opening setup (owner Go, 2026-10-30): new-account mappings default to a zero opening or explicitly use the reported amount literally, and suggest the earliest valid date per source account (1900-01-01 fallback). Existing accounts retain their openings. These choices persist atomically with capture, remain valid through repairs and Commit, and replace the later opening prompt for the mapping UI. Current experimental formats are **38/37**; older formats are rejected without migration. See [the contract](doc/qif-account-imports.md#current-mapping-opening-setup--2026-10-30) and [status](doc/status.md#opening-setup-during-import-mapping--2026-10-30).

Review revision implemented (owner Go, 2026-10-30): the working tree now targets schemas **38 with rules / 37 without**, adding the sidebar Review workspace, seven-day/exact-or-explicit-4% link policy, explicit Payee/Memo exclusions, and generated-counterpart waiting tasks. The earlier 34/33 implementation below is the baseline. The owner explicitly settled ambiguous imports: generate a counterpart for every unresolved explicit transfer even when plausible imported postings exist; Review Link retires generated duplicates. Selected Release regression, sanitizer, rules-disabled and financial recovery checks passed; native interaction and other platforms remain unverified. See [status](doc/status.md#next-review-workspace-and-link-learning--2026-10-30) before continuing. Preserve existing changes and this accepted generation policy.

Current Review direction (owner Go, 2026-10-30): the unified account-grouped Review sheet, saved same/separate and link/not-linked decisions, same-account Merge, amount-difference split/category/link, retained source recognition and stacked Review Undo are implemented. See [the implementation contract](doc/review-sheet.md) and status for verification and limits. This Go authorizes manual cross-account suggestions and linking, not fuzzy automatic exclusion. Any intervening dataset mutation ends the safely undoable Review sequence; later Review decisions must not remove that barrier. Current experimental schemas are 38/37; older datasets are rejected without migration.

Common duplicate policy (owner Go, 2026-10-31): all formats use the same original-fact matching policy. Ordinary amount-based duplicate suggestions require the same known calendar date; no OFX-family exception or adjacent-day widening. Storage lookup is a conservative prefilter and the engine makes candidate decisions for import and Review. Bank-ID evidence remains distinct from file-format labels. Transfer matching is outside this correction. See [the contract](doc/review-sheet.md#common-duplicate-policy--owner-go-2026-10-31) and status for verification.

Current matching direction (owner Go, 2026-10-30): implement same-account re-import recognition for unique original account/currency/exact amount/date/payee matches. Exact nonempty parsed payee text and uniqueness on both incoming occurrences and existing identities are the bounded rule; bank-ID conflicts, ambiguous repeats, malformed originals and removed targets must not auto-match. Existing strict bank-ID recognition remains authoritative when both sides have IDs. Cross-account transfer matching, including differing bank/card posting dates, is separate and was not authorized by this Go. See the [accepted correction](doc/import-commit-discussion.md#accepted-same-account-correction--2026-10-30) and status for evidence and remaining review/unlink interaction design.

Current priority (owner direction, 2026-10-29): the accepted dataset-wide commit/new-transaction model in the [handoff](doc/import-commit-discussion.md) is implemented. It includes independent acceptance references, multiple pending imports, strict bank-ID overlap against accepted and uncommitted transactions, duplicate-only completion, one inclusive balance, valid-row acceptance with visible failures, source-owned account rollback and physical cleanup, warning overrides and quit-time Yes/No/Cancel. Commit consumes mutation-maintained review rather than repeating transaction validation. Existing account/category creation and opening-review behavior are preserved. The owner authorized completing the full model, including the storage refactor; no further Go was needed for that scope. Read [status](doc/status.md) for actual verification and platform limits. Matching remains restricted to the accepted decision table and the 2026-10-30 same-account correction above; do not add fuzzy automatic exclusion. Reports and unrelated changes still require their own Go.

Current owner direction (2026-10-28): SpentIt is the application being used and improved piecemeal. Discuss each new change and wait for explicit **Go** before implementation. Launch the local rules-enabled Release application with `./build/SpentIt`; see the README for rebuilding. The welcome/defaults/explicit creation-time overwrite and usable-scrollbar work is complete; see the [current handoff](doc/roadmap.md#application-usability-handoff--2026-10-28). Reports, expanded lifecycle/recovery and distribution remain deferred. The owner's explicit creation-time Overwrite policy supersedes the earlier exclusive-only creation UI; it does not change backup preservation or document locking. Supplied light/dark icon artwork is integrated; local packaging and its platform limits are described in [packaging instructions](packaging/README.md).

The implemented milestones are a standalone C++ financial engine, CMake build/test setup, a pinned currency catalog, engine split editing, retained historical-currency definitions, an optional synthetic-data SQLCipher storage/recovery prototype, an optional synthetic Qt Quick transaction-sheet experiment, and a partial durable QIF workflow with ordinary committed corrections/removal/Undo, category management, a separately testable rules evaluator/preview adapter, schema-6 rule Save/Apply for rules-enabled import documents, and a rules sheet on that import worker. The schema-6/rules-sheet baseline passed all 72 tests in Release and ASan/UBSan on macOS. The durable UI now includes combined search, stable multiselection, and schema-7 atomic committed bulk category/removal/group Undo. The durable workflow now also has account-scoped persistence, independent account workspaces and guarded UI switching and a bounded same-currency transfer workflow with explicit per-account commit, protection, provisional bank matching and navigation. New documents use experimental schema 38 with rules and 37 without, with a dataset-wide display/input date format and optional starter categories. Ordinary categories are independently assignable names with one optional colon, case-insensitive identity matching, and no parent objects. Account display names and confirmed aliases share that namespace and identify transfers, including unbracketed QIF split names. Accounts appear read-only in the category editor; rules/bulk category controls display them with an explicit transfer-workflow restriction. See feature 010. Earlier formats, including schemas 24/23 and 22/21, are rejected without migration. Rename, archive, reactivation, empty-account deletion, opening correction, and currency reassignment are explicit historied commands on that format. OFX, QFX, QBO, and ASO bank and credit-card statement lists share that representation with QIF. ASO is the inspected OFX 1.0.2 SGML credit-card label. QIF whole-record/split transfer destinations require explicit resolution through that workflow. The bank-v6 profile accepts embedded carriage returns in free-text fields and includes complete staged category catalogs (published with the first account commit), ISO dates and validated comma grouping in addition to ordinary Bank/Cash/CCard headers, separate source-class metadata and nullable-original amount repair. Named account sections, explicit mapping/creation, same-date reciprocal transfer linking through confirmed mappings (including equivalent repeated groups with equal eligible counts), navigable import blockers, whole-import repair progress/Abort and staged reported-balance opening review are implemented with selected current-format verification; see `doc/qif-account-imports.md`. Allocation-level Merge, unlink, replacement, the transfer journal, and account destinations in the category selector are implemented. Committed link edits for the locked 2026-10-27 cases are implemented: retiring a purely generated committed duplicate, ending a link when one committed side's amount, category, or removal changes, and keeping a date-only link. Saved rules can be reapplied to the selected account's active committed rows in one preview, one Apply, and one group Undo. Manual fields stay unless that preview replaces them. Amounts, dates, evidence, and a live transfer category stay. A rules-off document has no reapply command. Moving a committed row to another account and rewriting source-occurrence links remain incomplete. The point 8 sheet slice adds session sidebar balances, fixed ledger column ratios, horizontal scroll below 1080 pixels, and viewport retention for bulk, reapply, and history undo. Multiline notes, persisted layout, spoken accessibility, and the rest of the sheet design remain incomplete. On 2026-10-28 the owner said the eight-point sequence was the work before reports. Report implementation waits for an explicit Go and the open questions in `features/006-reporting-drill-down.md`. See `doc/status.md` for verified scope. Further implementation must follow the requested scope; this guidance does not authorize publication or selection of a final license.

The owner wants public GitHub availability and permanent free access: selling SpentIt or its derivatives is prohibited, while businesses may use it internally for free. **The earlier Unlicense preference is superseded; exact licensing terms remain unresolved.** Preserve this distinction rather than imposing a blanket ban on commercial use. See `features/007-licensing-and-distribution.md`. Do not add an Unlicense, GPL, or other license without an explicit decision. Do not represent source visibility as open-source licensing or assume third-party libraries inherit our license.

## Verified fast startup — owner Go 2026-10-31

Normal startup reuses a prior full audit only for an identical full-file SHA-256
and verifier version, with authentication, locking, format and journal gates
retained. Graceful quit hides the window while the worker audits and atomically
publishes owner-private approval in the platform cache. Missing/stale approval
falls back to verification; failures never trigger automatic repair. Initial
Review suggestions load on demand, including before Commit. See
[the implementation contract](doc/fast-startup.md) and [status](doc/status.md) for verification and limits.

## Shorter closing verification — owner Go 2026-10-31

The closing follow-up is implemented: desktop full opening audits retain a
hash-bound session approval, invalidated by writes, rollback or failed verification.
Unchanged close can publish it without repeating the audit. Full audits use bounded
per-audit row and committed-snapshot reuse without omitting history/financial checks.
See [the contract](doc/fast-startup.md) and [status](doc/status.md) for synthetic
close/process-exit measurements and verification limits. Further work needs its own Go.

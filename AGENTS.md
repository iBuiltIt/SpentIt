# SpentIt — Where did my money go?

## Change history workspace — implemented 2026-11-09

The owner accepted the Change history layout and then gave implementation Go.
One saved action is one row. The page reads the live file-wide Undo chain.
It lists New and Accepted transaction changes, the import that brought them
in, and the acceptance that accepted them. Rules and other work that run
during an import stay inside that one Import row. A later rule application
the user starts is its own row. The empty page has no Back to account
button; the window Back button leaves the page. Formats stay **70 with rules / 69 without**.
Read the [handoff](doc/change-history-handoff.md) and [status](doc/status.md).
Do not edit the manual; MAN-020 records that later pass.

## Selected Set memo — owner Go 2026-11-09

The ledger right-click menu offers **Set memo…** beside **Rename payee…**. One
dialog sets the transaction memo for the selection. It fills blank memos unless
**Overwrite N existing memos** is checked. An empty memo with that box on clears
existing memos. Split memos stay unchanged. The command is one atomic Undo and
uses the same selection guards as Rename payee. A memo it writes is manual, so
a later rule leaves it alone. See the
[contract](doc/ledger-editing.md#selected-set-memo--owner-go-2026-11-09). Manual
text stays in MAN-019.

## File exchange — owner Go 2026-11-09

Read the [File exchange contract](doc/file-exchange.md) before export/import work.
File adds QIF Export, SpentIt Export/Import and the existing Backup flow. QIF uses
current unfiltered ledger rows, exact period openings when requested, optional
ordinary categories and retained transfer references. JSON independently selects
Rules, Reports and Categories, with explicit dependency mapping and Income conflict
choices. Import appends rules/reports and merges categories atomically without
applying rules or clearing transaction Undo. Experimental database formats remain
70/69; the independent exchange format is version 1. See status for actual checks;
MAN-018 is deferred manual work, not authorization to edit the manual now.

## Unified Undo replacement — implementation, 2026-11-09

The owner authorized implementation in a new session, including the replacement's
visible scope choices. The earlier design-only stop is historical. Read the
[design](doc/action-undo-design.md), [operation inventory](doc/action-undo-inventory.md)
and [implementation/verification record](doc/action-undo-implementation.md) before
Undo/history changes. The replacement is integrated; see status for verification and limits.
Working experimental formats are **70 with rules / 69 without**.

Ordinary Undo uses one durable newest-first file-wide action chain for transaction
operations, opening/currency correction and acceptance. Import is one atomic
source-owned rollback with no ordinary edit journal for new rows. Retain permitted
import-created setup, including accounts/categories and explicit openings.
Definitions/settings preserve the chain and invalidate previews. Save and apply is
atomic; its inverse retains the definition. Older-import removal and Discard require
an empty chain. Successful close expires ordinary history; preserve recovery on
failure/crash and block successful close during initial import decisions.

The [rethink handoff](doc/action-undo-rethink-handoff.md) and archived rejected
experiment explain why generic storage-row capture was rejected. Do not resume that
architecture, its agent/reviewer loop or proposed storage multipliers. Prototype
sources are archived under `review/action-undo-prototype/retired-source/`.
Preserve financial/recovery/session-expiry and measured storage gates. The Change
History layout in the [handoff](doc/change-history-handoff.md) is implemented;
see status. MAN-017 manual publication remains a later pass.

## First release and next UI phase — accepted 2026-11-08

Read [the accepted release plan](doc/first-release-plan.md) before planning next
work. The owner's usage-driven UI improvements come first, including About;
the remaining UI list is open. About subsequently received implementation Go;
see current status for its native/offline verification and licensing limits.
This is separate from the later busy/readiness
audit. Current-value rule matching and macOS-first release scope are accepted,
not implementation Go. Licensing policy is settled in the plan; exact legal
terms and publication remain gated. Preserve deferred retention scope and the
later batch manual pass. See [About requirements](features/007-licensing-and-distribution.md#about-and-third-party-notices--accepted-2026-11-08).

## Reports usability — owner Go 2026-11-08

Use the [Reports presentation contract](features/006-reporting-drill-down.md#reports-workspace-usability--owner-go-2026-11-08).
Reports has conditional definition actions, one Show report / Save and show action
per row, stable Help, explicit checklist scope and complete chart labels.
Deletion uses a named modal confirmation and saves only that removal, retaining
unrelated drafts and selecting the existing new draft first. Leaving lists changed
reports with Save all changes / Discard all changes / Cancel. Financial policies,
drill-down protections and transaction Undo are unchanged; manual work is MAN-016.
See status for actual verification and limits.

## Rules and ledger usability — owner Go 2026-11-08

Use the [current Rules presentation contract](features/004-rules-and-explanations.md#rules-workspace-usability--owner-go-2026-11-08)
and [ledger entry-point contract](features/004-rules-and-explanations.md#ledger-context-menu-rule-creation--owner-go-2026-11-08).
Rules has an explicit account picker, independent definition actions and a separate
Preview or Apply Rule section. Deletion uses a modal named confirmation, not staged
removal; afterward select the existing new draft first. Leaving uses Save all
changes / Discard all changes / Cancel. Ledger Create rule… requires exactly one
eligible selection and retains existing source-field eligibility and guards.
Current-value matching remains separate. See status for test results and the
unreproduced native crash investigation; do not claim that crash proven fixed.
Manual changes remain in MAN-015 for the later batch pass.

## Bounded ruleset retention — owner Go 2026-11-08

Successful close expires full rule-save requests and superseded ruleset snapshots.
Keep one current baseline including deleted identities, current definitions and
older definitions referenced by retained draft/entry fields or allocations. Keep
compact request fingerprints for exact retry/conflicting-ID handling; failed or
cancelled close preserves the session. Verification seeds the rules baseline and
checks subsequent saves rather than requiring expired request replay. No normal
open/close audit is added. See [the contract](features/004-rules-and-explanations.md#ruleset-save-retention--owner-go-2026-11-08)
and [status](doc/status.md). Experimental formats are **68 with rules / 67 without**;
older formats are rejected unchanged. Category, undone Review-link, exclusion and
account/opening/currency retention cleanup remain deferred and incomplete by
explicit owner direction; revisit only after a separate discussion and Go.

## Explicit import openings only — owner Go 2026-11-08

The legacy staged-balance path is retired. Every new-account import mapping must
explicitly choose zero or the reported amount literally; omission refuses before
capture. Existing-account mappings preserve their openings. Later corrections use
Correct opening. Verification checks the fixed creation-time choice independently
of subsequent transaction edits. Experimental formats are **66 with rules / 65
without**; older formats are rejected unchanged, with no migration. See
[the contract](doc/qif-account-imports.md#legacy-staged-balance-retirement--owner-go-2026-11-08)
and [status](doc/status.md). The subsequent ruleset-retention Go is recorded above;
other retention cleanup remains deferred.

## Session history baseline — correction Go 2026-11-07

Read [the accepted contract](doc/session-history-baseline.md),
[current progress](doc/status.md), and [storage measurements](doc/session-storage-comparison.md)
before history/lifecycle work. The owner rejected `f8a1f4b`'s permanent per-revision
facts and historical monetary replay. Correct them together with the normal-open
verification policy; do not treat that implementation or its passing tests as the
requirement. Every successful close ends ordinary history; failed/cancelled closes
and crashes preserve recovery. Current state, originals, recognition, necessary
relationships and eligible import Undo survive. Compact retry receipts must not
become historical archives. Normal open/close uses an encrypted clean marker with
no routine audit or hashing; reduced offline-change detection is accepted. Retain
full recovery, backup/restore and explicit checks. Storage must not exceed the
pre-change `cf8f1a5` implementation on identical workloads. The corrected experimental
formats are **64 with rules / 63 without**; older formats are rejected unchanged.
Current-baseline verification and dependency pruning replace the permanent facts
layer. Preserve the measured storage acceptance; manual updates remain deferred.

## Deferred manual updates — owner direction 2026-11-06

Record manual work caused by user-visible application changes in the
[manual update backlog](doc/manual-update-backlog.md). Keep affected chapters,
pictures and already-completed text changes explicit. Accumulate these items for
one later owner-authorized manual update pass; do not update manual text or
screenshots as part of feature work unless the owner explicitly requests it.
When that pass includes pictures, recapture them with
[tools/capture_manual_images.py](tools/capture_manual_images.py) as described in
the [backlog](doc/manual-update-backlog.md#recapturing-pictures). Rebuild the
help UI test first, stage the pictures, and copy them into `manual/images` only
with `--install` after the check passes. If a walkthrough fails, update that
walkthrough and rerun. Revise the chapter text and captions in the same pass.
Do not point `SPENTIT_GUIDE_CAPTURE` at `manual/images`.

## Category income grouping — owner Go 2026-11-06

Categories now has a saved Income checkbox. Default workspace/picker order is
Expenses A–Z, Income A–Z, Accounts A–Z; other workspace sorts apply within groups.
Reports has tri-state Expenses/Income group checkboxes, retaining explicit saved
category selections. The flag does not alter monetary signs or report arithmetic.
Read [the contract](features/010-categories-and-selection.md#income-grouping--owner-go-2026-11-06).
Current experimental formats are **58 with rules / 57 without**; earlier formats
are rejected unchanged without migration. See [status](doc/status.md).


## Contextual Help — owner Go 2026-11-06

Read the [Help handoff](doc/help-prototype-handoff.md) before changing Help.
The complete manual and application context links are implemented. Keep Help
navigation independent of financial commands, use fixed bundled topic/section
identifiers, and preserve decision modality and unsaved edits. See the
[contract](features/013-help-system.md#contextual-help--owner-go-2026-11-06)
for target validation and [status](doc/status.md) for verification limits.

## Reports and calendar — owner Go 2026-11-04

Read the [reports/calendar handoff](doc/reports-calendar-handoff.md) before the
next report or date-picker work. The Rules-style report workspace and reusable calendar interaction are settled.
The compact-results/editable-ledger follow-up now also permits grouped-period
bars; its contract supersedes the initial separate-chart-only layout. The owner also accepted
net-spending signs, moving presets/fixed custom dates, direct category rows,
separate currency results and explicit current-entry selection/inclusion policies.
The owner subsequently gave implementation Go. The initial report workspace,
exact allocation totals/drill-down, separate-period charts and shared calendar
are implemented; see [status](doc/status.md) for verification and limits. Current
experimental formats are **56 with rules / 55 without**; earlier formats are
rejected unchanged without migration.


## Private document locks — owner Go 2026-11-04

Private local datasets and backup candidates use persistent empty locks under
the platform application-data directory. Shared/unsupported locations and
existing legacy sidecars retain adjacent protection. Never delete a lock during
a session or silently change namespaces. Read [the locking contract](doc/document-locking.md)
before changing creation, overwrite, lock paths or legacy cleanup; see
[status](doc/status.md) for actual verification and platform limits.

## Amount formatting — owner Go 2026-11-03

Settings offers saved **Use amount formatting** and comma/dot, dot/comma or
space/comma styles. New files default on using their creation-time currency;
the selected style applies throughout the file. Off uses ungrouped decimal-point
numbers. User amounts and expression literals share strict grouping validation
and exact arithmetic; bank-file parsing remains separate. See
[the contract](doc/ledger-editing.md#amount-formatting--owner-go-2026-11-03).
Amount formatting introduced formats 52/51; reporting now uses **56 with rules /
55 without**. Prior formats are rejected unchanged without migration. Earlier
checkpoints below describe their dated formats.

## Uniform transaction origins — owner Go 2026-11-03

The [transaction-origin contract](doc/transaction-origin-handoff.md) covers ordinary
category commands, independent manual/imported transfer postings, accepted account
assignment, counterpart ownership and shared editing/history. Rule matching against
current rather than imported values remains a separate unresolved decision.
Current experimental formats are **50 with rules / 49 without**; older formats are
rejected unchanged, without migration. See [status](doc/status.md) for verification.

## Manual transactions — owner Go 2026-11-03

Ledger **Add transaction…** and **+ New transaction** beside search Apply share an unsaved
inline creation flow, with Save / Discard / Stay, focused-selection date or today,
zero amount and blank text. Enter saves; Escape cancels. Manual entries have no
imported original until an explicit Review Merge attaches one. Same-date/exact
signed amount suggestions never automatically consume unmatched manual entries.
See [the contract](doc/manual-transactions.md) and [status](doc/status.md).
This feature introduced experimental formats **48 with rules / 47 without**;
older formats are rejected unchanged, with no migration. Earlier format numbers
below describe their dated checkpoints.

## Undo a completed import — owner Go 2026-11-03

Account **Undo last import…** and Review **Undo import…** share a source-scoped
atomic rollback, preserving earlier imports and accepted transactions. Undoing a
later transfer match restores the earlier relationship. See
[the contract](doc/import-undo.md) and [status](doc/status.md) for scope and limits.
The import-undo change retained formats 46/45: the owner chose descriptive import
labels rather than a new format to persist filenames. Manual entry introduced 48/47; transaction-origin cleanup now uses 50/49. Account-wide Discard remains separate.

## Split amount editing — owner Go 2026-11-03

The largest eligible split (preferring the total’s sign) initially absorbs the
remainder. Its identity stays fixed; manually editing it disables adjustment.
Both choices persist. Transfer splits are excluded. Only balanced edits save.
Leaving a mismatch offers Change amount / Keep editing; no crash recovery buffer is
included. The [amount-expression follow-up](doc/ledger-editing.md#amount-expressions--owner-go-2026-11-03)
adds exact arithmetic entry and retains only the answer, rejecting fractional minor units. See [the contract](doc/ledger-editing.md#split-amount-editing--owner-go-2026-11-03)
and [status](doc/status.md). Current experimental formats are 50/49.

## Ledger selection editing — owner Go 2026-11-02

Right-click Rename payee and confirmed Restore imported values operate atomically
on one or multiple selected new/committed transactions, with group session Undo.
See [the contract](doc/ledger-editing.md#selected-rename-payee-and-restore--owner-go-2026-11-02)
and [status](doc/status.md) for validation, transfer restrictions and verification.

## Next work: category-gated rules — owner direction 2026-11-02

Read the [next-session handoff](features/004-rules-and-explanations.md#next-implementation-category-gated-rules-and-imported-categories--2026-11-02)
and [roadmap](doc/roadmap.md#next-session-category-gated-rules--2026-11-02) before
further rule/import work. Accepted target: every rule assigns an explicit category,
only Uncategorized items are eligible, no force override or category inheritance,
and missing imported categories require Create / Use Uncategorized. The split-first category gate, ordered selected-rule application, and durable
import-category decision are implemented; see [status](doc/status.md) for verification limits.
The owner gave implementation Go for split-first categorization, transaction
category fallback, and a durable import-wide missing-category checklist. No old-rule migration/compatibility work is wanted for experimental data.

## Single rules workspace — owner Go 2026-11-01

Rules use one searchable list and expanded editor. Save/Discard and optional-
preview Save and apply concern only the selected rule, retaining unrelated drafts.
Only one new draft is allowed. Leaving retains the selected rule and shows a
movable dialog (owner follow-up Go, 2026-11-02), now labelled Save all changes /
Discard all changes / Cancel under the 2026-11-08 usability follow-up.
Save validates all outstanding edits before an atomic definition save and does
not apply transactions.
See [the contract](features/004-rules-and-explanations.md#single-rules-workspace-and-independent-edits--owner-go-2026-11-01)
and [status](doc/status.md). Earlier focused/all-rules UI and mandatory-preview
descriptions are superseded; internal all-rules APIs remain.

## Direct ledger category menu — owner Go 2026-11-01

Right-click Change category uses a scrollable submenu and applies immediately to
new/committed selections through the atomic assignment command, with group Undo.
Ordinary category changes preserve unresolved duplicate-review decisions. Split
transactions require a split choice; ambiguous multi-split selections are refused.
Keep the viewport through assignment and Undo. See [the contract](features/010-categories-and-selection.md#direct-ledger-category-menu--owner-go-2026-11-01)
and [status](doc/status.md); earlier dialog/preview descriptions are superseded.

## Ledger return position — owner Go 2026-11-01

Leaving the ledger records an identity/offset anchor per account. Back/Forward
and account buttons restore it after layout; hidden refreshes must not replace
it, and missing anchors fall back to a clamped row position. Session positions
clear on document changes. See [the contract](features/005-transaction-sheet-navigation.md#ledger-return-position--owner-go-2026-11-01)
and [status](doc/status.md).

## Rule scope and Apply — owner Go 2026-11-01

Rules offer All transactions in this account (default) or New transactions only.
Single-rule and all-enabled previews lead to the same scoped Apply, with Save and
apply for unsaved definitions and one group Undo for mixed new/committed changes.
See [the contract](features/004-rules-and-explanations.md#rule-scope-and-account-apply--owner-go-2026-11-01)
and [status](doc/status.md). The separate Reapply entry point is superseded;
legacy history/receipt APIs remain. Preserve manual and transfer protections.

## Rule match lists — owner Go 2026-11-01

Rules previews show one compact row per matching transaction. Apply and accepted-
account reapply preserve fields untouched by enabled matching actions; disabling
or deleting rules does not itself reverse earlier assignments. Account/pending
previews stream evaluation without the old 4,096-row ceiling; accepted-account
reapply retains its separate limit. See [the contract](features/004-rules-and-explanations.md#match-list-preview-and-non-reverting-apply--owner-go-2026-11-01)
and [status](doc/status.md) for actual verification and limits.

## Opening balance without confirmation — owner Go 2026-11-01

Use the stored opening immediately, including zero. No separate opening
confirmation or confirmation-based Commit blocker remains. Double-click the
ledger Opening Balance row to edit its date/amount using the historied account
correction and Undo. See [the contract](doc/ledger-editing.md#opening-balance-editing--owner-go-2026-11-01)
and [status](doc/status.md) for verification and limits.

## Ledger editing — owner Go 2026-11-01

The [ledger editor and session Undo contract](doc/ledger-editing.md) supersedes
Save draft / Cancel edit. Double-click toggles, selection leaves the editor in
place, Enter accepts a complete validated edit, and session Undo restores it.
The category field now uses inline filtered completion with Enter/Tab selection
and forward/reverse field navigation under the follow-up Go.
Other dataset mutations end this Undo sequence; persistent committed history
remains. See [status](doc/status.md) for verification and limits.

## Opening and import cleanup — owner Go 2026-10-31

The [opening/ledger cleanup and interrupted-import rollback](doc/ui-import-cleanup.md)
received implementation Go and is implemented. See [status](doc/status.md) for
verification and limits before further work. Current experimental formats are
**50 with rules / 49 without**; dated verification in status describes the formats tested at each checkpoint.
Completed uncommitted imports survive restart. Automatic cleanup targets only
incomplete preparation; account-context Discard remains account-owned. Read the
contract before changing retention, cancellation, startup cleanup or the ledger.

SpentIt is a local, single-user desktop application for understanding spending through reliable imports, reviewable categorization, and explainable reports. Data safety, security, and correctness take priority over speed of development. Losing or silently misrepresenting financial data loses the user's trust.

## Discover the project before changing it

1. Read this file and `doc/status.md`.
2. Read `doc/project-layout.md`, `features/README.md`, and the feature documents relevant to the task.
3. Inspect existing implementation and tests when they exist. Documentation is not evidence that code exists or works.
4. Identify accepted requirements, provisional designs, and open questions before making changes. Ask about material ambiguities; do not invent financial or security behavior.

## Diagnostics hooks and agent profiling

When the owner mentions hooks, diagnostics, introspection, observing the app, or agent-assisted profiling, read [Agent guide: using SpentIt diagnostics hooks for profiling](doc/agent-diagnostics-guide.md) first, then its linked runtime contract. The guide covers build variants, exact-process attachment, endpoint discovery, read-only CLI use, operation/UI correlation, timing interpretation and reproducible workloads. Owner instruction (2026-10-30): a request to use hooks explicitly pre-authorizes native macOS PID-targeted window capture and Accessibility inspection when the usual UI attachment is unavailable or unclear; do not ask for another method-specific Go. Retain and verify the original process identity, never use app-name resolution that can launch a second instance, and establish inspection access before asking the owner to prepare a selection. Follow the guide's standing authorization and OS/tool-permission boundaries. Reading a specifically selected pair's sources is authorized by a request to investigate that pair; generic profiling does not authorize unrelated private-data access, financial actions or implementation changes.

## User-facing language — owner direction 2026-11-01

Write from the user's point of view: explain the action they can take and any
consequence needed to make a decision. Do not expose internal tracking terminology
such as “evidence” or add history/preservation assurances that give the user
nothing to act on. For example, the opening editor says “Update the opening date
and/or balance.” Engineering documentation can retain precise internal terms.
Apply the [user-facing language guide](doc/user-facing-language.md) to all UI
copy, including errors and recovery messages. Ask what the user does with each
message, whether it helps a decision, and whether it is the simplest accurate wording.

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
- Preserve durable ordinary history during the document session, including crash recovery. Successful close expires ordinary Undo under the session-baseline contract; retain originals, recognition, financial dependencies and eligible import Undo across sessions. Reversal must respect later changes and financial relationships; it must not silently overwrite newer work.
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

Category transfer assignment (owner Go, 2026-10-31): the ledger's right-click
Change category dialog assigns an account to multiple selected transactions in
one atomic operation with one durable Undo. Changing back to an ordinary category
ends links, removes only disposable generated drafts, and restores categories and
relationships together on Undo. It creates separate counterparts
through the existing transfer commands; no new matching search is added. A split
transaction needs explicit allocation selection. This supersedes the earlier
bulk-selector account prohibition, but rules still cannot create transfers.
See [the contract](features/010-categories-and-selection.md#right-click-transfer-assignment--owner-go-2026-10-31)
and [status](doc/status.md) for verification and limits.

Review responsiveness and batch Merge (owner Go, 2026-10-31): the permanent bottom status line serves every right-pane workspace. Duplicate groups can be selected separately from their transaction rows and merged in one atomic batch with one Undo. Pending selection clears on leaving Review or dataset mutation; inspection alone retains it. Stale/invalid groups reject the whole batch, and cancellation before the final commit gate rolls it back. Link remains individual. Review reuses original comparison facts and checks the unchanged engine predicate before saved-choice lookups. Current experimental schemas are 50/49. See [the contract](doc/review-sheet.md#batch-merge-and-stable-status--owner-go-2026-10-31) and [status](doc/status.md) for measurements, verification and limitations.

Import-mapping opening setup (owner Go, 2026-10-30): new-account mappings default to a zero opening or explicitly use the reported amount literally, and suggest the earliest valid date per source account (1900-01-01 fallback). Existing accounts retain their openings. These choices persist atomically with capture, remain valid through repairs and Commit, and replace the later opening prompt for the mapping UI. Current experimental formats are **42/41**; older formats are rejected without migration. See [the contract](doc/qif-account-imports.md#current-mapping-opening-setup--2026-10-30) and [status](doc/status.md#opening-setup-during-import-mapping--2026-10-30).

Review revision implemented (owner Go, 2026-10-30): the implemented revision introduced schemas 38/37 (now superseded by 42/41), adding the sidebar Review workspace, seven-day/exact-or-explicit-4% link policy, explicit Payee/Memo exclusions, and generated-counterpart waiting tasks. The earlier 34/33 implementation below is the baseline. The owner explicitly settled ambiguous imports: generate a counterpart for every unresolved explicit transfer even when plausible imported postings exist; Review Link retires generated duplicates. Selected Release regression, sanitizer, rules-disabled and financial recovery checks passed; native interaction and other platforms remain unverified. See [status](doc/status.md#next-review-workspace-and-link-learning--2026-10-30) before continuing. Preserve existing changes and this accepted generation policy.

Current Review direction (owner Go, 2026-10-30): the unified account-grouped Review sheet, saved same/separate and link/not-linked decisions, same-account Merge, amount-difference split/category/link, retained source recognition and stacked Review Undo are implemented. See [the implementation contract](doc/review-sheet.md) and status for verification and limits. This Go authorizes manual cross-account suggestions and linking, not fuzzy automatic exclusion. Any intervening dataset mutation ends the safely undoable Review sequence; later Review decisions must not remove that barrier. Current experimental schemas are 50/49; older datasets are rejected without migration.

Common duplicate policy (owner Go, 2026-10-31): all formats use the same original-fact matching policy. Ordinary amount-based duplicate suggestions require the same known calendar date; no OFX-family exception or adjacent-day widening. Storage lookup is a conservative prefilter and the engine makes candidate decisions for import and Review. Bank-ID evidence remains distinct from file-format labels. Transfer matching is outside this correction. See [the contract](doc/review-sheet.md#common-duplicate-policy--owner-go-2026-10-31) and status for verification.

Current matching direction (owner Go, 2026-10-30): implement same-account re-import recognition for unique original account/currency/exact amount/date/payee matches. Exact nonempty parsed payee text and uniqueness on both incoming occurrences and existing identities are the bounded rule; bank-ID conflicts, ambiguous repeats, malformed originals and removed targets must not auto-match. Existing strict bank-ID recognition remains authoritative when both sides have IDs. Cross-account transfer matching, including differing bank/card posting dates, is separate and was not authorized by this Go. See the [accepted correction](doc/import-commit-discussion.md#accepted-same-account-correction--2026-10-30) and status for evidence and remaining review/unlink interaction design.

Current priority (owner direction, 2026-10-29): the accepted dataset-wide commit/new-transaction model in the [handoff](doc/import-commit-discussion.md) is implemented. It includes independent acceptance references, multiple pending imports, strict bank-ID overlap against accepted and uncommitted transactions, duplicate-only completion, one inclusive balance, valid-row acceptance with visible failures, source-owned account rollback and physical cleanup, warning overrides and quit-time Yes/No/Cancel. Commit consumes mutation-maintained review rather than repeating transaction validation. Existing account/category creation and opening-review behavior are preserved. The owner authorized completing the full model, including the storage refactor; no further Go was needed for that scope. Read [status](doc/status.md) for actual verification and platform limits. Matching remains restricted to the accepted decision table and the 2026-10-30 same-account correction above; do not add fuzzy automatic exclusion. Reports and unrelated changes still require their own Go.

Current owner direction (2026-10-28): SpentIt is the application being used and improved piecemeal. Discuss each new change and wait for explicit **Go** before implementation. Launch the local rules-enabled Release application with `./build/SpentIt`; see the README for rebuilding. The welcome/defaults/explicit creation-time overwrite and usable-scrollbar work is complete; see the [current handoff](doc/roadmap.md#application-usability-handoff--2026-10-28). Reports, expanded lifecycle/recovery and distribution remain deferred. The owner's explicit creation-time Overwrite policy supersedes the earlier exclusive-only creation UI; it does not change backup preservation or document locking. Supplied light/dark icon artwork is integrated; local packaging and its platform limits are described in [packaging instructions](packaging/README.md).

The implemented milestones are a standalone C++ financial engine, CMake build/test setup, a pinned currency catalog, engine split editing, retained historical-currency definitions, an optional synthetic-data SQLCipher storage/recovery prototype, an optional synthetic Qt Quick transaction-sheet experiment, and a partial durable QIF workflow with ordinary committed corrections/removal/Undo, category management, a separately testable rules evaluator/preview adapter, schema-6 rule Save/Apply for rules-enabled import documents, and a rules sheet on that import worker. The schema-6/rules-sheet baseline passed all 72 tests in Release and ASan/UBSan on macOS. The durable UI now includes combined search, stable multiselection, and schema-7 atomic committed bulk category/removal/group Undo. The durable workflow now also has account-scoped persistence, independent account workspaces and guarded UI switching and a bounded same-currency transfer workflow with explicit per-account commit, protection, provisional bank matching and navigation. New documents use experimental schema 50 with rules and 49 without, with a dataset-wide display/input date format and optional starter categories. Ordinary categories are independently assignable names with one optional colon, case-insensitive identity matching, and no parent objects. Account display names and confirmed aliases share that namespace and identify transfers, including unbracketed QIF split names. Accounts appear read-only in the category editor; rules/bulk category controls display them with an explicit transfer-workflow restriction. See feature 010. Earlier formats, including schemas 24/23 and 22/21, are rejected without migration. Rename, archive, reactivation, empty-account deletion, opening correction, and currency reassignment are explicit historied commands on that format. OFX, QFX, QBO, and ASO bank and credit-card statement lists share that representation with QIF. ASO is the inspected OFX 1.0.2 SGML credit-card label. QIF whole-record/split transfer destinations require explicit resolution through that workflow. The bank-v6 profile accepts embedded carriage returns in free-text fields and includes complete staged category catalogs (published with the first account commit), ISO dates and validated comma grouping in addition to ordinary Bank/Cash/CCard headers, separate source-class metadata and nullable-original amount repair. Named account sections, explicit mapping/creation, same-date reciprocal transfer linking through confirmed mappings (including equivalent repeated groups with equal eligible counts), navigable import blockers, whole-import repair progress/Abort and staged reported-balance opening review are implemented with selected current-format verification; see `doc/qif-account-imports.md`. Allocation-level Merge, unlink, replacement, the transfer journal, and account destinations in the category selector are implemented. Committed link edits for the locked 2026-10-27 cases are implemented: retiring a purely generated committed duplicate, ending a link when one committed side's amount, category, or removal changes, and keeping a date-only link. Saved rules can be reapplied to the selected account's active committed rows in one preview, one Apply, and one group Undo. Manual fields stay unless that preview replaces them. Amounts, dates, evidence, and a live transfer category stay. A rules-off document has no reapply command. Moving a committed row to another account and rewriting source-occurrence links remain incomplete. The point 8 sheet slice adds session sidebar balances, fixed ledger column ratios, horizontal scroll below 1080 pixels, and viewport retention for bulk, reapply, and history undo. Multiline notes, persisted layout, spoken accessibility, and the rest of the sheet design remain incomplete. On 2026-10-28 the owner said the eight-point sequence was the work before reports. Report implementation waits for an explicit Go and the open questions in `features/006-reporting-drill-down.md`. See `doc/status.md` for verified scope. Further implementation must follow the requested scope; this guidance does not authorize publication or selection of a final license.

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

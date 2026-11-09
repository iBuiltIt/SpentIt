# Project status

## Unified Undo replacement — implemented and verified, 2026-11-09

The authorized replacement is integrated in storage and the application, with
working experimental formats **70 with rules / 69 without**. One durable file-wide
head covers financial transaction actions, acceptance and whole-import rollback.
All financial Undo entry points confirm the same action/account/count scope; native
text Undo is preserved. Save and apply is atomic and its inverse retains the rule.
Successful close expires ordinary history; failed close and abnormal recovery
preserve it. The rejected generic row-capture prototype is archived separately.

See [implementation and verification](action-undo-implementation.md) for actual
passes and limits and [raw storage evidence](../review/action-undo-replacement/).
The 50,000-row import gate has one root and zero ordinary imported-row members.
The completed 19,000-row accumulated workload retains 31,850,496 bytes, below
cf8f1a5's 32,268,288 and b4b65df's 31,977,472. Retained payload is lower too;
temporary journal peaks and small-workload fixed costs are reported separately.
Financial fault sweeps, rules-off/sanitizer integration and native synthetic text/
dirty-target Undo have passed. The final eight-mode Review recovery sweep and Help
suite passed. Four obsolete account fixtures also fail on unchanged b4b65df and
remain documented failures; this is not an all-green repository test claim.
No private financial data or manual prose/screenshots were changed. MAN-017 and
final Change History layout remain separate work; Linux/Windows are unverified.

## Unified Undo replacement design — design only, 2026-11-08

The owner requested a replacement design after the failed generic import row-capture
experiment, then authorized design and explicitly prohibited implementation in this
session. The [replacement handoff](action-undo-design.md) and
[operation inventory](action-undo-inventory.md) are written for a new agent/session.
The owner explicitly accepts retaining import-created accounts and categories.

The design keeps one newest-first file-wide action chain and reversible acceptance,
with typed user-operation restoration using existing domain snapshots. Import is
one atomic source-owned rollback, with zero ordinary edit-Undo members for new
imported rows. Added restoration data concerns actual changes to pre-existing state.
The existing production import rollback and mixed Review/edit/rule stack were
source-inspected as foundations, not represented as a finished replacement.

Visible replacement choices include definitions/settings outside ordinary Undo,
retained import setup/openings, transaction-only reversal of Save and apply, and
non-undoable older-import removal/Discard guarded by an empty active chain. These
choices are proposed explicitly for owner review; no separate implementation Go,
new storage budget or production readiness claim is implied. Acceptance reversal,
restoration lineage, command coverage and recovery remain implementation work.

The earlier design/inventory are archived as rejected history; discovery pointers
and MAN-017 now describe the replacement. Prototype code, CMake additions and raw
measurements remain untouched. Production `src/`, runtime tests and manual content/
screenshots were not changed. No agents/reviewer loop, builds, runtime tests,
private-data inspection or native application actions were performed. Documentation
verification: `git diff --check` passed; 753 relative links across 14 affected
documents resolved, and checked Undo/backlog heading anchors were valid. The
repository diff has no production `src/` changes. These are documentation/source
inspection results, not runtime correctness or performance evidence. Work stops
at design delivery.


## Undo architecture rejected — stopped for next-session rethink, 2026-11-08

The owner rejected generic per-row import Undo capture and stopped both implementer
and reviewer. Read the [authoritative rethink handoff](action-undo-rethink-handoff.md).
No replacement design, storage budget or production compaction feature is approved.
The earlier approvals and measurements below are historical evidence, not a
continuation instruction. Production code is unchanged; the experimental working
tree is retained. This handoff has no reviewer. Next work is an owner discussion
of operation-level Undo, not resuming the rejected implementation.

## Unified action Undo — broader measurements complete, budget decision pending, 2026-11-08

**Owner follow-up:** the owner declined to adopt the proposed limits pending an
explanation and measurement of compaction, ordinary-backup behavior and reportable
reusable space. Their accumulated dataset is about 19,000 transactions; the
50,000-row case is a scale probe, not a typical editing session. That case only
imports rows and closes/reopens; it does not edit all rows. Investigate disposable
synthetic copies and retain the uncompacted measurements. No storage cap or
production Compact feature has been approved.

After reviewing the measured storage excess below, the owner considered roughly
8% lasting overhead potentially acceptable and gave Go to broader prototype
measurements before a final storage limit is agreed. The investigation followed the
[recorded follow-up](action-undo-rejected-design.md#owner-follow-up--broader-storage-measurement-go-2026-11-08).
It measured after-successful-close physical/retained storage and active-session
journal/pin/sidecar peaks across representative workloads. The old no-growth stop
was lifted for this investigation; no final overhead budget, production readiness,
financial-correctness waiver or recovery waiver is implied.

The broader opt-in prototype now captures actual forward writes, records body pins,
and expires inverse/action rows and pins while collecting unreachable bodies inside
the existing successful-close transaction. A common driver compares `cf8f1a5`,
unchanged current storage and the prototype across import, repeated edits, older
import removal, linked transfers and mixed multi-session workloads. It records
simultaneous SQLite-VFS file-length peaks separately from sampled inverse/body
codec bytes and final DBSTAT payload. **All 36 unique runs passed:** three variants
for five modes at 1,000/10,000 rows, plus UUID import at 10,000/50,000 rows.
Across these cases versus current storage, retained payload grows **7.5–22.1%**,
the final physical main file **26.4–89.6%**, and simultaneous VFS peak
**27.4–117.9%**. UUID-length import sensitivity at 50,000 rows measures
**59,831,026 bytes retained payload, 105,791,488 bytes main file after close and
143,726,072 bytes simultaneous VFS peak**: respectively **+21.7%, +72.3% and
+110.6%** versus unchanged current storage. Retained payload is +4.1% versus
`cf8f1a5`; the comparator matters. Inverse/pin rows and unreferenced bodies are
zero after close, but freed pages remain a lasting physical-file cost. These are
prototype observations, not an accepted budget or final feature prediction.

The broader measurement checks pass **3/3 CTests** on macOS Release with rules:
typed capture/registry cases, VFS measurement tests, and bounded actual SQLCipher
close failure/cancellation/disk-full/interruption cases. Failed closes preserve
the complete persistent journal/pin/body/marker state; successful close expires
the prototype journal and collects unreachable bodies. This is bounded lifecycle
verification, not completed unified Undo recovery acceptance. See the
[methodology and test output](../review/action-undo-prototype/broader-storage/README.md).

Independent implementation review returned **APPROVED, zero blocking findings**
for this bounded prototype and storage evidence, independently reconciling the
30-run matrix and 810 physical phase rows plus the six UUID cases. See the
[review verdict](../review/action-undo-prototype/broader-storage/implementation-review.md).
This does not approve production unified Undo or a new owner budget. The proposed
next-stage fixed-workload gates are **1.30× retained payload, 2.00× final main-file
length and 2.50× simultaneous VFS peak** versus current `b4b65df`; every measured
case fits. They remain **unaccepted**, require full-feature remeasurement, and are
not universal session limits or permission to truncate/expire Undo early.

The common coordinator/API, complete command/payload conversion, preparing-import
protocol, full allocation/recovery coverage and UI integration remain unimplemented.
Financial, encryption/durability and recovery requirements are unchanged. Final
Change History layout and manual text/screenshots remain separate work.

### Prior prototype gate and evidence


The owner accepted one strict file-wide session action chain, reversible
acceptance, full reversal of current-session imports including import-created
setup, and undoable removal of eligible prior-session imports. The owner then
authorized design and independent review, then approved the reviewed design and
granted implementation Go for a fresh session on 2026-11-08. Use the
design-review-implement workflow for implementation and independent implementation
review. See the [historical reviewed design](action-undo-rejected-design.md).
**The initial prototype stopped at its original storage gate. Investigation has
now resumed under the follow-up above. Production Undo is unchanged; broad command
conversion and UI integration have not begun.**
The [historical conversion inventory](action-undo-rejected-inventory.md) covers 94 persistent tables,
command families and allocator/ancestry assumptions. The opt-in prototype executes
actual repository commands against SQLCipher and an isolated initialization that
splits ten immutable-body tables from active memberships. It preserves live FKs,
explicit source order and temporary body pins; it is not a shipped format.

The stable, byte-identical import-only driver was run against `cf8f1a5` and the
prototype, using the same Release toolchain, SQLCipher and OpenSSL settings and
1,000 synthetic transactions. After close/reopen, prototype physical size is
**1,683,456 versus 1,556,480 bytes (+126,976)**; retained B-tree payload is
**663,049 versus 602,151 bytes (+60,898)**. All ten payload tables have **zero**
bodies without active memberships at this checkpoint, so this excess is not
uncollected removed data. The structural-only driver contains no inverse journal:
this representation already exceeds the hard limit before action capture.
The [paired results, raw data and reproducible script](../review/action-undo-prototype/README.md#hard-storage-gate--stable-final-comparison)
record the measurements and limits. No VACUUM, padding, weaker durability or
omitted dependencies was used to obtain the comparison.

Representative 1,000-row rename, acceptance and earlier-of-two-import removal
round trips pass canonical semantic-state equality, FK and exact-balance checks.
Removal also passes the existing full verifier. Rename and acceptance reversal
fail that legacy verifier with `SQLITE_CORRUPT`; they are **not** full-verification
passes. Registry policy and action-aware verification still require conversion.
The codec/capture/registry regression CTest passed 1/1. Normal production opening
refuses the distinct prototype application ID and leaves the entire file unchanged.
Independent implementation review returned **APPROVED for the bounded prototype,
evidence and gate stop, with zero blockers for this checkpoint**; it does not
approve full unified Undo as delivered. See the
[investigation](../review/action-undo-prototype/README.md) for checks and limits.

At that checkpoint the measured representation failed the original gate; this did
**not** prove that every possible encoding was too large. The initial recommendation
was to rework the representation under that cap. The broader-measurement Go above
supersedes that stop and leaves the final storage budget undecided. Full coordinator,
prepared-import protocol, command/payload-family coverage,
compound actions, high-water allocation, recovery/fault verification and UI remain
unimplemented. Only macOS Release, rules enabled, was exercised; sanitizer,
rules-disabled, Linux/Windows and native UI checks remain unverified. Final
Change History layout and manual prose/screenshots remain separate and untouched.

Separate Designer and Reviewer agents inspected the current code and contracts.
The first review requested explicit registry/restore and version-allocation rules,
a concrete representation for removed-but-retained payloads, and atomic
Save-and-apply semantics. The revised design addresses all three; the independent
Reviewer returned **APPROVED, zero unresolved blocking design issues**.

The proposed implementation uses a durable root-action chain, typed inverse
capture, immutable payloads with active memberships and temporary Undo pins,
monotonic freshness/identity/version allocation, and one common Undo API. It
requires a new experimental format and changes to storage verifiers, command
composition, lifecycle and UI command routing. The initial prototype was subject
to the original storage acceptance, now under investigation as described above;
design review is not proof of size, performance or
recovery correctness. Deferred unrelated retention cleanup remains excluded.

Prior design-handoff verification (before this implementation prototype):
Documentation-only work updated contributor guidance, feature discovery, the release
plan and roadmap to point to the handoff. MAN-017 records future manual work as pending
implementation; manual text/screenshots are unchanged. Relative file links and
`git diff --check` were checked. No builds, runtime tests, private-data inspection
or native UI verification were performed for this design handoff.

## Reports button placement refinement — 2026-11-08

Owner follow-up: moved Duplicate report / Delete report below the name field.
Categories now orders Clear expenses, Clear income, then conditional Select all
categories, keeping both Clear buttons stationary. Account action order and all
command behavior are unchanged. Rebuilt build/SpentIt (macOS arm64 Release);
offscreen QML smoke and git diff --check passed. No new tests or financial
regression rerun for this placement-only change. MAN-016 records the deferred
manual capture update.


## Reports interaction corrections — owner follow-up, 2026-11-08

New report now keeps its label and is disabled while a new draft exists, above
the report search. Help with reports is at the bottom. Report details and
account/category count headings sit above their action rows; empty definition
action rows collapse. Clear expenses and Clear income replace Clear categories,
retaining Uncategorized and the other group even when search hides entries.

Wired all Reports search clearRequested signals to their local clearSearch
handlers. Saving and selecting a report release the prior text editor focus.
Date fields use overwrite entry with a focus-gated text binding: raw typing
updates the unsaved Custom range without formatting back into the field on each
keystroke. Calendar choices explicitly update the displayed field as well.
Existing validation and saved-date semantics remain unchanged.

Both checklists use persistent interactive thumbs with reserved content space.
Native drag verification exposed the category scrollbar overlapping the outer
workspace scrollbar at the right edge. Reserved outer-scrollbar space fixes the
hit-target conflict; native hover/press/drag-to-top now passes.

Rebuilt build/SpentIt and Reports UI tests on macOS arm64 Qt 6.12.0 Release.
Final offscreen Reports/QML CTest passed **2/2**, 10.60 s (8.96 s and 1.63 s).
Native workspaceUsability and incomeGroups passed **4/4 including setup/cleanup**,
1.986 s. Tests cover stable disabled New report, all three search X buttons,
released Save focus, selected-digit replacement, year overwrite, whole-date
replacement, selective group clearing with search-hidden categories, retained
Uncategorized, and an actual category thumb hover/press/drag. Inspected a synthetic
native checklist screenshot. The initial drag failure was resolved before the
final passing runs; a temporary test-only key API compile error was corrected.

git diff --check and changed-document relative file links passed. No private
financial data was accessed. Financial/report calculation policies are unchanged.
Windows/Linux, sanitizers and the broader release regression suite were not run.
MAN-016 records the follow-up; manual prose/screenshots remain deferred.


## Reports workspace usability — owner Go, 2026-11-08

Implemented the [approved Reports presentation](../features/006-reporting-drill-down.md#reports-workspace-usability--owner-go-2026-11-08).
Empty Reports offers New report without empty search or definition actions.
One new draft changes New report to Continue new report; Duplicate is available
only for a saved definition when no new draft exists. It copies current edits
without saving the source. Search has an explicit no-results state and retains
the independent selected editor with an outside-results notice.

Each report row has one Show report / Save and show action. Selected Save report
and Discard changes/Discard draft appear only when meaningful. The selected
heading owns Duplicate report and Delete report…. Named modal deletion offers
Delete report / Cancel, with Escape cancellation and an unsaved-edits consequence.
It persists only the selected removal, retains unrelated drafts and selects a
remaining new draft first. The leave dialog lists changed report names and uses
Save all changes / Discard all changes / Cancel.

Account/category scope precedes periods and Chart; whole-list shortcuts name
their scope. Chart and preset controls size for their full labels, and narrow
layouts stack sections. Help is stable; local refusals reach the permanent
error-colored status line. Results display the accounts used when shown.
Financial calculations, membership, category selection semantics, transaction
editing guards and Undo remain unchanged. No engine/storage/format changes.

Verification: macOS arm64, Qt 6.12.0, Release, synthetic files only.

- Rebuilt build/SpentIt, Reports UI and Help UI tests.
- Full offscreen Reports UI plus QML CTest: **2/2 passed**, 10.47 s
  (Reports 8.81 s; QML 1.66 s). Reports covers the new usability flow plus Income
  groups, editable drill-down, exact displayed totals, transaction Undo,
  calendar behavior and independent selected saves with an invalid other draft.
- New usability regression verifies empty controls, single-draft continuation,
  duplication of current unsaved edits without saving its source, no-match search,
  conditional controls, selected-only save/deletion, Cancel/Escape, remaining-draft
  selection, deletion reload and chart content width.
- Native Cocoa usability test: **3/3 including setup/cleanup**, 1.071 s.
  Inspected its synthetic screenshot at a 1000×800 logical window: complete
  chart label, periods and selected Save/Discard controls.
- Reports leave-dialog Help regression: **3/3 including setup/cleanup**, 1.267 s.
  Offscreen plugin reports unsupported raise and a font alias warning.
- Initial checks found and corrected temporary QML syntax errors. Sandboxed
  execution could not create normal application-data locks; checks were rerun
  with lock access. The first complete UI run also exposed two stale assertions
  for the retired groupOpeningDateCalendar. Updated them to assert its absence,
  retaining checks of Correct opening and transfer calendars; the final full
  Reports run passes.
- git diff --check and changed-document relative file-link checks passed.

No private financial file was accessed. Windows/Linux, sanitizers, broad release
regressions and large-report responsiveness were not tested for this UI change.
Manual prose/screenshots remain deferred under MAN-016.


## Rules documentation reconciliation — 2026-11-08

Reconciled contributor guidance, feature index, durable-rules and ledger entry
points with the implemented Rules usability and ledger Create rule contracts.
Consolidated MAN-015 around the final modal deletion and separate application
section; manual prose and screenshots remain deferred. Release planning now
records the subsequent UI Go, and the README identifies formats 68/67 and the
bounded Reports implementation. Older dated verification remains historical.

Documentation-only follow-up: changed-document relative file links and
`git diff --check` passed. No runtime tests rerun; the implementation verification
and native crash limitations below are unchanged.

## Ledger Create rule and disabled menu styling — 2026-11-08

Implemented the [approved ledger entry point](../features/004-rules-and-explanations.md#ledger-context-menu-rule-creation--owner-go-2026-11-08).
Create rule… sits below Add transaction… in the normal ledger context menu. It
requires one eligible selected transaction, writable idle state and no existing
new rule draft. The existing transaction/split chooser and prefill are reused;
the chooser now has Cancel. Multiple selected transactions never silently choose
one. Manual-only input and already-occupied creation targets retain existing
eligibility; current-value matching remains separately gated.

Right-click inspects the selected identity without expanding the transaction
editor or scrolling it into view. This is necessary because ordinary ledger
selection does not update the separately inspected editor. Create rule remains
unavailable during inspection and checks inspected/selected identity agreement.
Existing unsaved-edit guards precede selection/inspection. No engine, storage,
rule matching or financial command eligibility changed.

A shared ledger MenuItem component explicitly sets the enabled/disabled text
palette used by Qt Basic for both labels and submenu arrows. Disabled rows have
no hover highlight; duplicate and category submenus use the same presentation.
High-contrast highlight borders remain supported. Other application menus were
not changed.

Rebuilt `build/SpentIt`, Rules and navigation tests (macOS arm64, Qt 6.12.0,
rules-enabled Release). Native light/dark regression passed **4/4 including
setup/cleanup**, 3.567 s. It uses an actual right-click on a different transaction
from the earlier inspector, confirms the editor stays closed and the correct
original payee seeds the rule, checks multi-selection refusal, split chooser/
prefill, manual-only refusal, enabled/disabled text and arrow colours, and no
highlight on a disabled item. Light/dark native captures were inspected.

An initial native test run encountered two QML syntax errors in the new submenu
delegates; its fixture then dereferenced the missing window and crashed, producing
the owner's `spentit_rule_ui_tests` macOS alert. Corrected the QML and added a
missing-root test guard before rerunning. This was a temporary synthetic test
process, not the owner's SpentIt session; no private financial data was accessed.
The earlier owner's SpentIt accessibility crash remains separately qualified.

Final offscreen Rules/navigation/QML CTest run passed **3/3**, 52.98 s:
navigation 21.34 s (**22 passed, 1 opt-in diagnostics skip**), Rules 30.80 s
(**32/32 including setup/cleanup**), QML smoke 0.83 s. `git diff --check` and
changed-document relative link-target checks passed. Manual prose/images remain
deferred under MAN-015; Windows/Linux and sanitizers were not run for this UI change.

## Rules application section heading — 2026-11-08

Owner follow-up: renamed the account-operation heading to “Preview or Apply Rule”
plus the selected rule name. Increased the divider's bottom margin from 12 to 28
pixels; long headings wrap. Definition Save/Discard remain above the divider.
Rebuilt `build/SpentIt` and Rules UI tests. Existing native validation/removal and
long-list geometry checks passed **4/4 including setup/cleanup**, 2.604 s, on macOS
arm64 Qt 6.12.0 Release. Inspected the synthetic native capture showing the heading,
separation and red validation feedback. `git diff --check` passed. No financial
behaviour changed; manual capture follow-up remains under MAN-015.

## Modal rule deletion and remaining-draft selection — 2026-11-08

Owner approved replacing staged removal with a modal confirmation. Delete rule…
now asks about the named rule and offers Delete rule / Keep rule. Escape cancels;
underlying Rules controls are unavailable during the decision. Opening the dialog
changes no definition or draft and adds no Deleted badge/category summary.
Confirmation submits only the selected deletion through the existing atomic
rules-save command, using saved surrounding definitions. Unrelated unfinished
rules remain untouched; failures leave local definitions available for correction.
After successful deletion, the existing new draft is selected and its editor
revealed; otherwise selection falls to the nearest remaining rule or the empty
state. No matching, transaction application, financial or storage policy changed.

Rebuilt `build/SpentIt` and Rules tests in macOS arm64 rules-enabled Release,
Qt 6.12.0. Full offscreen Rules/QML checks passed **2/2**, 30.21 s: Rules UI
**30/30 including setup/cleanup** (28.36 s CTest), QML smoke 1.84 s. Native focused
confirmation/remaining-draft and accessibility removal tests passed **4/4 including
setup/cleanup**, 2.949 s. The native confirmation capture was inspected. Coverage
includes modal blocking of New/select, Keep and Escape cancellation, no premature
Deleted badge, preservation/selection of an incomplete draft beside another saved
rule, final empty state, persistence and unchanged earlier transaction assignments.

The earlier reported macOS accessibility crash remains unreproduced; this change
also avoids the former pending-deletion model/text update but is not proof of a
general platform crash fix. Windows/Linux and sanitizers were not run. No private
financial data was accessed. Manual follow-up remains in MAN-015; no manual prose
or screenshots changed. `git diff --check` and relative document link checks passed.

## Rules feedback and native removal crash investigation — 2026-11-08

Owner-reported follow-up: rule numbers now have an 8-pixel inset, a divider with
spacing separates definition Save/Discard from account Apply, and missing-category
validation uses the error-colored status line rather than ordinary feedback.
Validation clears on rule editing/selection or successful command completion.
No engine, storage, matching, deletion transaction or format change.

Inspected the latest owner-selected SpentIt crash report locally. Main-thread
termination is `EXC_GUARD / GUARD_TYPE_MACH_PORT / INVALID_OPTIONS` in
`_XMIGPostNotification`, through AppKit accessibility posting,
`QQuickAccessibleAttached::setName`, `setNameImplicitly`, button text update and
`QQmlListModel::setProperty`. This identifies a native accessibility notification
path during a model update; it does not establish a storage deletion failure or
prove the underlying platform cause. No private financial file was opened and no
raw crash report was copied into the repository.

The exact synthetic save/new/select/select/discard/remove sequence passed natively
before the mitigation, with Qt accessibility active (**3/3 including setup/cleanup**,
1.405 s). Therefore the owner crash is **not reproduced or proven resolved**.
A bounded mitigation gives the rule row an explicit accessible definition name,
with New/Unsaved/Removal pending in its accessible description. Visual badges
remain; accessibility is not disabled. Pending removal no longer renames that
button's accessible action. Qt's
[Accessible documentation/source](https://raw.githubusercontent.com/qt/qtdeclarative/6.12/src/quick/items/qquickaccessibleattached.cpp)
distinguishes concise control names from descriptions. Other native accessibility
notifications remain; this is not a general macOS crash fix.

Rebuilt `build/SpentIt` and the Rules tests (macOS arm64, Qt 6.12.0, Release).
Final offscreen Rules/QML CTest run passed **2/2**, 28.65 s (Rules 26.99 s;
smoke 1.65 s). Native new regression plus long-list geometry passed **4/4 including
setup/cleanup**, 2.864 s. The regression checks error color, number inset, divider
placement, stable accessible name and updated removal description, unchanged saved
rule during staged removal, durable deletion and retained removed definition after
reopen. The native validation capture was inspected. Windows/Linux, sanitizers,
external assistive-client observation and a reproduction of the original native
termination remain unverified. No claim of private-file recovery verification.

Manual changes remain deferred under MAN-015. `git diff --check` and changed-document
relative link-target checks passed.

## Navigation tests aligned with session history — 2026-11-08

Updated `acceptedManualTransferAssignment` and
`categoryMenuSavesMixedRowsAndReopens` to the accepted
[session-history contract](session-history-baseline.md). Both now verify successful
ordinary assignment Undo within the session, reapply the assignment, then close
and reopen. The complete displayed row maps (including amounts, splits, balances,
text and acceptance state) must equal their saved pre-close values. Undo eligibility
must be false after reopen; explicitly attempting the expired Undo must fail and
leave those row maps unchanged. The mixed-row case preserves accepted, new imported
and manual transaction states without treating close as acceptance.

Rebuilt `spentit_import_navigation_tests` in the macOS arm64 rules-enabled Release
configuration (Qt 6.12.0). Reran precisely those two tests with
`QT_QPA_PLATFORM=offscreen`: **4/4 passed including setup/cleanup**, 3.084 s.
Used normal application-data lock access. The build retains four existing
floating-point-to-integer warnings in the unrelated screenshot crop helper at
line 79. `git diff --check` passed. No application/financial code or manual content
changed in this follow-up. The full navigation suite, native interactions and
other platforms were not rerun; this resolves the two specific stale failures
reported below, without claiming a new full-suite pass.

## Rules workspace usability — owner Go, 2026-11-08

Implemented the [approved Rules cleanup](../features/004-rules-and-explanations.md#rules-workspace-usability--owner-go-2026-11-08).
Empty Rules offers New rule; unavailable search, removed-rule and transaction
controls disappear. The selected definition owns Save rule / Discard changes and
staged Save removal / Keep rule. Removed definitions have readable summaries and
Copy into new rule. Search reports no matches and retained selections outside the
results. Long rows omit secondary summaries at narrow widths; full definitions
remain available in the selected editor.

A separate Apply section names the selected rule and exposes the target account
picker and transaction scope. Switching accounts stays in Rules, retains all
local drafts/selection/search and invalidates preview output. The existing account
selection command is reused without the usual Rules-form reset. Optional Preview
and selected Apply rule / Save and apply rule retain existing ordered context,
category eligibility and financial protections. Undo appears only for the existing
eligible account application, independently of selected rule. The movable leave
dialog lists changed definitions in a bounded scroll area and offers Save all
changes / Discard all changes / Cancel. No engine, storage or format changes.

Verification: macOS arm64, Qt 6.12.0, rules-enabled Release, synthetic files only.

- Rebuilt `build/SpentIt`, Rules UI tests and navigation tests. Full Rules UI suite
  passed **28/28 including setup/cleanup**; final CTest Rules/QML pass was
  **2/2**, 27.23 s (Rules 25.64 s; smoke 1.59 s). The new end-to-end
  case covers empty controls, two-account preview/application, preserved incomplete
  draft and selected edit across account switches, selected-only Save and apply,
  Undo after changing rule selection, disabled-rule controls, empty search,
  pending removal cancellation/save and removed-definition copying.
- Native focused run passed **5/5 including setup/cleanup**, 5.745 s, covering that
  workflow, long-list editor/action visibility and independent draft/save/apply
  plus movable leave-dialog dismissal. Final capture run passed **4/4**, 4.617 s.
  Inspected empty, draft editor, preview, 720×560 scrolled editor and leave-dialog
  captures. Full native suite, Windows, Linux and sanitizers were not run.
- Broader navigation suite: **20 passed, 2 failed, 1 skipped** (opt-in diagnostics).
  `acceptedManualTransferAssignment` and `categoryMenuSavesMixedRowsAndReopens`
  both expect ordinary assignment Undo after successful close/reopen. Their
  assertions conflict with the accepted session-baseline expiration policy;
  the command correctly refuses later Undo. These tests were not changed, and
  no full navigation-suite pass is claimed. Guarded navigation/isolation passed.
- Initial sandboxed execution could not create application-data locks; reruns used
  normal lock access. Iteration caught QML syntax/transient-index warnings, a
  long-list viewport overflow and old dialog-label assertions; corrected before
  the passing Rules runs. Offscreen Qt emits its usual unsupported size-hint
  warnings; native focused runs had no QML warnings.

`git diff --check` and changed-document relative link-target checks passed.
Manual prose and pictures remain unchanged; [MAN-015](manual-update-backlog.md#man-015--rules-workspace-usability)
records the later batch. Current-value matching remains separately gated.

## Selected-row Review Merge correction — owner Go, 2026-11-08

Corrected the [selection contract](review-sheet.md#selected-row-merge-correction--owner-go-2026-11-08):
the sole Merge button in a group larger than two uses only checked rows and names
their count. It is unavailable until at least two are checked. The redundant
Merge selected rows button is removed. Two-row groups retain a direct Merge.
The survivor dialog shows only selected eligible candidates with full wrapped
categories for all splits, split amounts/memos, transaction memo, account, date,
payee, amount/currency and status, alongside explicit Keep this transaction
buttons. It states the merge count and the number of unchanged unselected rows.
Existing automatic-survivor policy and storage validation remain unchanged.

Rebuilt `build/SpentIt` and import UI tests (macOS arm64, Qt 6.12.0, rules-enabled
Release). Focused offscreen checks passed **10/10 including setup/cleanup** in
7.690 s: `selectedReviewMerge`, `batchReviewSurvivorSelection`,
`batchReviewWorkspace`, `unifiedReviewSheet`, and four
`reviewMultipleLinkCandidates` cases. QML smoke passed **1/1**, 1.68 s.
The synthetic four-row regression verifies that the dialog contains precisely
the checked identities, exposes full category and memo text, leaves Cancel
unchanged, retains the chosen category and both unselected identities, changes
four 6,000-unit postings to three (24,000 to 18,000), and Undo restores all four.

Those selected-pair and batch checks also passed natively; the actual two-choice
survivor screenshot was inspected. A broader native run passed 9 cases including
setup/cleanup but failed `unifiedReviewSheet` at its existing keyboard-Undo
assertion, even after explicitly focusing Review. Its offscreen counterpart
passed; native keyboard Undo is not claimed verified by this change. No shortcut
implementation was changed. An initial synthetic fixture used only one import,
which correctly produced no cross-import duplicate question; the fixture was
corrected to two imports before the passing selection checks.

`git diff --check` and changed-document link-target checks passed. No financial
policy, storage, format or private data changed. Windows/Linux and full financial
regressions were not run. MAN-014 records the deferred manual updates; no manual
text or pictures were changed.


## Review workspace cleanup — owner Go, 2026-11-08

Implemented the [approved cleanup](review-sheet.md#review-workspace-cleanup--owner-go-2026-11-08).
Review now separates supporting tools, duplicate/transfer/problem questions and
one all-account acceptance footer. Unavailable tools disappear. The Review Close
button, read-only imported-category catalog and old blocking-commit dialog are
removed; dialogs retain Close/Cancel. Warnings use the dataset acceptance snapshot
and retain required-fix versus overridable behaviour. No storage, matching,
acceptance policy or format change (68/67).

Undo import uses description rows with explicit Undo… buttons and the existing
whole-source impact confirmation. Hidden transfer suggestions is in Settings with
Close and remains available contextually beside transfer candidates. Transfers
has an account picker that stays in Review and an on-demand Add transfer form.
Mapped transfer setup now explains matching, possible counterpart creation and
file-wide scope before confirmation; this is not an exact per-posting preview.
Select all merges skips ambiguous choices; Clear merge selection affects batch
intent only. Question-kind ordering explicitly preserves ties, avoiding QML sort
reordering and retaining the batch-Merge viewport anchor.

Verification: macOS arm64, Qt 6.12.0, rules-enabled Release, synthetic files only.

- Rebuilt `build/SpentIt`, import UI tests and Help UI tests.
- Focused import UI run passed **18/18 including setup/cleanup**, 15.772 s:
  `reviewCleanup`, `mappedTransferConfirmation`, `datasetAcceptanceAndRollback`,
  `warningAndQuitAcceptance`, `undoSelectedImport`, `batchReviewWorkspace`,
  `batchReviewSurvivorSelection`, `mappedTransferImport`, `unifiedReviewSheet`,
  four `reviewMultipleLinkCandidates` rows, `unifiedReviewLinkDifference`,
  `transferManagementUi`, and `qifTransferResolution`. Covers empty controls,
  all-account acceptance, warnings and required fixes, cancel/confirm, select-all,
  ambiguous survivor protection, durable Merge/Undo, viewport and transfer picker.
- Acceptance/Review/contextual-Help walkthroughs passed **6/6 including
  setup/cleanup**, 10.270 s. Updated the walkthrough's obsolete expectation of a
  survivor prompt: the already-accepted unique-earliest-import policy selects it.
- Native Review empty/populated and batch interaction checks passed **4/4 including
  setup/cleanup**, 4.989 s on the layout checkpoint. Final native captures inspected:
  empty with/without new work, populated duplicates, 720×560 layout, Undo picker,
  transfer account picker and Hidden transfer suggestions with Close. A later
  native mapped-confirmation check initially asserted visibility before layout;
  after waiting for layout, that native check passed **3/3 including setup/cleanup**,
  1.162 s. Its explanation/Cancel/Set up transfers capture was also inspected.
- Help preparation/catalog and application QML smoke passed **3/3**, 1.97 s.
- Initial sandboxed checks could not create application-data locks and the test
  then aborted on missing state; reruns used normal lock access. No lock protection
  was changed. Iteration also caught a QML name collision, transfer navigation
  leaving Review, and unstable sort ties; corrected before the passing checks.

Manual prose and images remain unchanged; [MAN-014](manual-update-backlog.md#man-014--review-workspace-cleanup)
records the later batch. No private financial files were read. This is selected
UI coverage, not a full financial regression, sanitizer, Windows or Linux claim.


## Compact About component picker — 2026-11-08

Owner-requested reduction of vertical spacing in the third-party component list.
Rows now use four pixels of top/bottom padding and a 28-pixel minimum height,
growing with the existing font metrics. Text size is unchanged. Rebuilt the
Release application; native light-mode About check passed 3/3 including setup/
cleanup in 2.05 s, opening the actual component popup for visual inspection.
The compact list capture was inspected; all 20 current entries fit in that view.
MAN-013 records the later manual capture update. No financial behavior changed.

## About dialog and offline notices — owner Go, 2026-11-08

Implemented the [approved About design](../features/007-licensing-and-distribution.md#about-design-and-implementation-go--2026-11-08),
without the rejected extra description. The native macOS application-menu About
entry opens before or after loading a file; other platforms retain Help → About
SpentIt. The window shows the icon, tagline, version/build, exact copyright
**© 2026 Stefan Wessels**, warranty summary and Qt/LGPL acknowledgement. Version
copying includes only version/build, Qt, OS and architecture. Offline selectable
terms and third-party pages have Back/Close/Escape navigation. The agreed risk
notice is displayed with an explicit statement that the final distribution
license remains unselected; no acceptance checkbox or operative license was added.

The [notice inventory](../third_party/notices/README.md) contains full Qt LGPL/GPL
texts, Qt Base/Quick runtime-source attribution catalogs, SQLCipher/SQLite/PCRE2
and the inspected macOS transitive libraries. Package-version mismatches and
other-platform coverage limits are explicit rather than mislabeled. Final deployed
plugin/dependency inventory, source/relinking obligations, reference-data terms,
legal review, signing and notarization remain release work.

About metadata is a read-only UI object, independent of financial commands. Its
opening/closing participates in the existing focus-suspension guard. The main
window and unsaved text are retained. The local macOS bundle now embeds the full
current UI/Help resources and links Help/diagnostic transport; those stale wiring
gaps surfaced when verifying its About metadata. Bundle copyright is populated.
No financial/storage behavior or experimental format changed (68/67).

Verification on macOS arm64, rules-enabled Release, Qt 6.12.0, synthetic data:

- Rebuilt `build/SpentIt`, the Help/About test binary and local macOS app bundle.
- Native selected About and adjacent Help checks passed **9/9 including setup/
  cleanup** in 8.93 s: actual Cocoa application-menu activation, light/dark views,
  version clipboard content, offline license texts, keyboard page activation and
  Escape, unchanged unsaved transaction expression/payee and zero worker commands,
  plus Help workspace/category focus and Rules/Reports decision modality.
- Inspected native normal/narrow main, terms and notices captures in light/dark.
  This caught a clipped Close button at native font sizes; content-based minimum
  height corrected it and the final geometry assertion passed. The native menu
  test initially needed Cocoa menu validation before testing enabled state.
- The initial offscreen focused checks passed **4/4 including setup/cleanup**
  after accounting for the expected missing-file probe and permitting normal
  application-data locking. An earlier sandboxed synthetic creation was refused;
  no locking protection was weakened. Native final checks above cover the later
  layout refinements and actual menu path.
- The final registered `import.about_ui` CTest passed **1/1** in 1.44 s;
  its offscreen dark row deliberately skips (native coverage is recorded above).
  This also verifies the final platform-independent Qt-notice selection assertion.
- Help preparation/catalog and application QML smoke passed **3/3** in 1.98 s.
  The local bundle's offscreen `--smoke` also exited successfully; its plist
  contains the exact copyright. These are local-build checks, not deployed,
  signed/notarized-package acceptance.

Changed-document file/heading links passed (529 checked), notice-manifest paths
and recorded file digests verified, and `git diff --check` passed.
Manual text/screenshots remain unchanged; MAN-013 records the later update.
No private files were read. Windows/Linux interaction, complete package compliance,
and a full financial regression run are not claimed for this UI-only change.

## About copyright attribution — 2026-11-08

The owner selected **© 2026 Stefan Wessels** for the planned About dialog and
application copyright metadata. Recorded in [licensing and About requirements](../features/007-licensing-and-distribution.md#copyright-attribution--owner-decision-2026-11-08)
and the release plan. About design must be discussed before implementation;
warranty/liability wording remains under discussion. No license, application,
manual or metadata implementation changed. Documentation-only verification:
relative targets and the new heading anchor checked; `git diff --check` passed.
No runtime tests were run.

## First-release decisions and next UI phase — 2026-11-08

The owner accepted the [first-release plan](first-release-plan.md): macOS first,
existing spending workflows, current-value rule matching before release, explicit
feature/retention deferrals, current release-wide verification, batch manual upkeep
and final licensing/package/publication gates. Paid-services, hosted access,
bundles and derivative source-sharing policy are settled; exact license text is not.

The owner's usage-driven UI work comes first and is separate from the subsequent
busy/readiness audit. About is one accepted item; the remainder of the list has not
yet been supplied. [About and third-party notices](../features/007-licensing-and-distribution.md#about-and-third-party-notices--accepted-2026-11-08)
will provide application identity/version and offline access to bundled component
notices/licenses. Application license access follows exact-term selection.

This is documentation only, not implementation Go. No application, financial or
storage behavior, format, license file, package or manual content changed. Current
formats remain 68/67; existing rule inputs remain unchanged until implementation.
The release plan is the current planning baseline where older handoffs conflict.
Documentation verification: changed-document relative file/heading links and
`git diff --check` passed. No runtime tests were run for this documentation-only work.

## Bounded ruleset-save retention — 2026-11-08

Owner Go covers ruleset duplication only. The [contract](../features/004-rules-and-explanations.md#ruleset-save-retention--owner-go-2026-11-08)
keeps one current rules baseline, referenced old definitions and compact retry
fingerprints, expiring full requests and superseded snapshots atomically at
successful close. Failed/cancelled close and abnormal recovery retain session
inputs; normal open/close does not gain an audit. Formats are **68/67**; older
formats are rejected unchanged.

**Deferred and incomplete — explicit owner direction:** category history, undone
Review-link history, link-exclusion history and account/opening/currency retention
cleanup. Do not start these as an automatic follow-up; each needs a separate
owner discussion and Go. The bounded ruleset work is complete; this does not mark
all findings in the original storage review resolved.

Implemented and retained after the [paired measurements](../review/04-ruleset-retention-results.md)
met the bounded efficiency criterion. Across 32 saves, file growth fell from
159,744/679,936 bytes to zero for 32/128 rules. The 128-rule case retains 128
baseline members and zero full requests instead of 4,224 of each. B-tree payload
growth fell from 561,782 to 4,998 bytes. One-rule physical growth was zero both
before and after; retained payload growth roughly halved. Compact per-command
receipts still grow intentionally.

Verification on macOS arm64 with synthetic data:

- Rebuilt the rules-enabled Release application (`build/SpentIt`) and test targets.
  **21/21 selected Release tests passed**: rule definitions/application/category
  gating/account Apply, verification/schema/backup, Save I/O/crashes, rules/session
  retention, cross-session import Undo, committed session/marker/corruption/backup
  checks, verification cache, and full import/rules UI suites (**88/27 Qt cases**,
  including setup/cleanup).
- The new rules close sweep covered **121 write/sync positions**, alternating
  disk-full/I/O failures, reopening/retry and process exits on both sides of commit.
  Existing session-close tests also covered 105 positions and lost acknowledgement;
  activation covered 13 positions and process exits.
- New regression verifies old definitions referenced by accepted and pending
  transactions after newer saves/deletion, no resurrection, exact and conflicting
  retries, cancellation after pruning, open-session and closed-baseline backup
  restoration, restored import Undo, and explicit refusal of corrupted baseline,
  missing referenced definitions and wrong receipt domains.
- Final review added a deleted-member filter to rule evaluation and a regression
  with **128 deleted rules plus one live rule**. The final retention test passed
  in Release and ASan/UBSan. The preceding sanitizer selection passed **4/4**;
  leak detection was disabled. Rules-disabled schema and cross-session import Undo
  passed **2/2**. The broad Release selection spans the final filter rebuild;
  its dedicated final regression verifies that last change.
- All 12 paired 1/32/128-rule runs (zero/32 cycles, before/after) passed full checks
  and backup creation. Separate referenced-definition tests cover cases absent
  from the unapplied-rule size fixture. Unchanged closes preserved sizes/counts.
- Repeated the original import/edit/Undo/linked storage acceptance with identical
  driver sources against `cf8f1a5`: physical sizes and B-tree payload remain lower
  in all four workloads. Raw logs, free-page accounting and CSV summaries are in
  the measurement report. No VACUUM or storage padding was used.
- Changed-document relative links and `git diff --check` passed. Native interaction,
  Linux and Windows remain unverified. No manual text/screenshots changed; MAN-012
  records the later manual pass. Other retention cleanup remains deferred.

## Explicit import openings; legacy balance path retired — 2026-11-08

The owner authorized step 1 only after committing the OFX fixes and investigation
as `c1fbc0b`. The [caller audit and retirement contract](qif-account-imports.md#legacy-staged-balance-retirement--owner-go-2026-11-08) requires an explicit zero/reported choice for new-account mappings, rejects omitted choices before capture, removes the obsolete UI command/panel and commit-time opening publisher, and verifies only fixed creation-time choices. Formats are **66 with rules / 65 without**; older formats are rejected unchanged. Rules and other retention changes remain deferred.

Verification on macOS arm64 with synthetic data:

- Rebuilt the rules-enabled Release application and affected test/measurement targets. New zero/reported-opening regressions reject omitted choices before any capture write, refuse the retired operation, and check two successive amount-edit/close cycles, exact balances, full verification, unclean reopening, backup restoration and import Undo.
- Selected Release checks passed for mapping openings, statement Review/rollback/import Undo across sessions, committed session/recovery/marker/corruption/backup behavior, schema refusal, verification cache and incomplete-import cleanup. The full offscreen import UI suite passed (**88 cases**, including setup/cleanup) after adjusting its account-selection fixture. After the final opening-display guard adjustment, five focused UI cases passed (**7 including setup/cleanup**).
- Rules-disabled mapping/opening and cross-session import Undo checks passed **2/2**; its schema gate also passed. Final ASan/UBSan mapping/opening and cross-session import Undo checks passed **2/2** (leak detection disabled).
- The Release opening fault sweep passed in 331.17 s: capture/resume/accept/abort covered **137/149/177/85 write/sync positions** with alternating disk-full/I/O failures, retries, restored backups and interruptions before/after commit. The final display-only guard adjustment was subsequently covered by mapping and focused UI reruns.
- The two broader `accounts.groups` and `accounts.validation` checks still fail with `Choose the imported categories before accepting transactions`, matching the previously recorded category-decision fixture failures. They are not passing coverage for this change.
- The measurement driver now confirms legacy mapping refusal without capture. The equivalent fixed-opening workload passes two edits/closes, verification, backup and unclean recovery; its main-file size remains 888,832 bytes on this bounded workload.
- Changed-document relative file links and `git diff --check` passed. Normal clean-marker policy and other retention behavior are unchanged. Native interaction, Linux and Windows remain unverified. Manual text/screenshots remain deferred in MAN-011.

## Storage retention investigation — 2026-11-08

The owner authorized investigation and measurements only. The [results](../review/03-retention-investigation-results.md) record 12 synthetic 1,000-row/32-cycle workloads, zero-edit controls, B-tree accounting, and minimal balance-history reproductions. No production storage implementation changed.

- Confirmed legacy staged-balance correctness defect: the second amount-edit close prunes a draft still required by a retained balance event. Verification passes before close and fails afterward; backup creation and unclean-reopen verification also fail. Current desktop fixed-opening mapping passed the equivalent edits, backup and unclean reopen. Current amounts remained exact in both cases.
- Rule saves retain complete ruleset copies: 32 changes to one rule in a 128-rule set added 679,936 physical bytes and 561,782 B-tree payload bytes. Verification passed. Other measured configuration histories were substantially smaller; Review Link/Undo retained history but subsequent import Undo passed.
- Investigation is complete for these bounded cases. **Other retention remediation remains deferred and requires a separate owner decision.** The legacy balance path is retired under the subsequent step-1 Go above; consider ruleset retention separately as an efficiency improvement. The evidence does not establish a need for an immediate six-domain refactor.
- Added an optional measurement driver and raw synthetic results, not production behavior or mandatory passing regression tests. Full verification was explicitly invoked by the driver. No private data, storage migration, manual update, or clean-marker-policy change. See the report for reproduction and verification limits.

## OFX statement validation and incomplete storage follow-up — 2026-11-07

Implemented the owner-approved [statement header validation](../features/003-ofx-profile.md#statement-header-validation--owner-go-2026-11-07): conflicting repeated statement fields refuse import, and unreadable ledger balances produce an explicit error before mapping. Identical repeats, missing balances and extra-balance suppression retain their existing behavior. Formats remain 64/63; storage behavior is unchanged. Manual updates are deferred in MAN-010.

Verification on macOS arm64 with synthetic data:

- Rebuilt the rules-enabled Release application (`build/SpentIt`) and test targets.
- `statement.parser` passed in Release and ASan/UBSan (leak detection disabled). New cases cover SGML/XML, bank/credit-card statements, identical/conflicting/empty fields, both status-conflict orders, exact signed balance limits, malformed/grouped/over-precision/overflow balances, missing balances, extra-balance suppression and refusal of a whole multi-account file. The existing statement regression suite also passed.
- Focused Qt offscreen checks `statementHeaderRefusalBeforeMapping`, `importRoutingAndLedgerCleanup` and `automaticImportDates` passed: **5 passed, 0 failed**, including setup/cleanup. Rejected imports expose the error without destination/mapping state or account changes; a subsequent valid import opens mapping. The initial sandboxed run could not create the application-data lock; the approved rerun passed with normal locking unchanged.
- Changed-document relative file links and `git diff --check` passed. No private data was read. Native interaction, Linux and Windows were not tested.

**Incomplete — storage remediation; investigation results recorded above.**
The independent [storage review](../review/01-session-close-storage-growth.md) identifies retained rule/category, Review-link, exclusion, account-command and balance-choice histories not covered by the published repeated-payee-edit measurements. This work is not implemented or authorized by the OFX Go. Validate required dependencies and reproduce the suspected balance-history verification failure before choosing changes; preserve import Undo, retry safety and baseline verification. Existing measurements remain evidence only for their tested workloads. Do not mark this follow-up complete with the parser fix.

## Duplicate resolution performance and automatic Review — 2026-11-07

Implemented the [performance follow-up](review-sheet.md#resolution-performance-and-automatic-discovery--owner-go-2026-11-07).
Each decision still saves immediately through the existing atomic Review operation
and retains its individual session Undo. Merge storage/recognition, format 64/63,
and clean-marker verification policy are unchanged. Open and import now discover
Review questions on the worker before publishing the view; no visit to Review is
needed. The permanent ledger/report-detail status area shows **Selected: n**, with
Opening Balance excluded.

Repeated refreshes reuse unaffected validated committed drafts in a bounded,
in-memory cache. Other mutations invalidate it; transactional reads and full
verification bypass it. Review reads original comparison facts only for relevant
batches, collects accepted ordinals and transfer protection in bulk, and grouped
import refreshes collect record membership in bulk. Ledger category decoration
reads the selected account once instead of once per row.

Synthetic macOS Release, Qt 6.12.0 offscreen benchmark (diagnostics enabled), run
serially without concurrent builds/tests: **19,009 transactions**, 15,009 in the
main account, 100 retained transfer relationships and nine duplicate questions.
Three repetitions per action. The same driver/fixture was run against an isolated
`65ed402` source archive, adding only an explicit Review load after its lazy reopen
before timing decisions. Each timed run had no concurrent builds/tests:

| Action | Previous median | Updated median | Updated range | Updated interaction-ready median |
| --- | ---: | ---: | ---: | ---: |
| Merge | 4,194 ms | 981 ms | 971–1,006 ms | 995 ms |
| Undo Merge | 4,871 ms | 965 ms | 964–984 ms | 978 ms |
| Separate | 4,278 ms | 975 ms | 944–1,012 ms | 986 ms |
| Undo Separate | 4,865 ms | 946 ms | 940–947 ms | 955 ms |

The maximum measured UI scheduling gap during these decisions was 53 ms. Durable
transaction spans were 9–13 ms; grouped import readiness refresh remains
the largest cost. Merge and Separate completion times fell about 77% on this
fixture; decisions still take roughly one second. Importing the nine candidates took 3.20 s and
clean reopening, including automatic discovery, took 5.23 s. The workload checked
question counts, removal of exactly one duplicate, retained transfers, exact
balance restoration on Undo, and explicit full verification. The benchmark's
peak process memory was 546 MiB (includes Qt, fixtures and verification).

Reproduce with the rules-enabled diagnostics build:

```sh
SPENTIT_BENCHMARK_DIAGNOSTICS=1 QT_QPA_PLATFORM=offscreen \
  build/import-ui-rules-release/src/ui/spentit_import_benchmark 19000 --duplicates
```

Verification on macOS, synthetic data only:

- Rules-enabled Release application and all test targets built. Final complete
  import UI run: **87 passed, 0 failed** in 105.49 s. Initial runs exposed a queued
  editor-focus restoration racing the next edit/search action; restoration now
  respects new edits and newly focused controls while retaining focus across
  verification. Added a direct stale-restoration check. A separate 30-cycle
  new/accepted split-edit stress run and ledger review/viewport checks passed.
- Focused rules-disabled Release and ASan/UBSan storage runs each passed **4/4**:
  Review, cached ledger, ledger duplicates and batch Merge. Leak detection was
  disabled; no leak-check claim. Cache tests compare displayed rows with
  authoritative entries after Merge, Separate, Undo, unrelated edits, rollback,
  explicit verification and backup restore.
- The broader Release selection initially passed **133/140**. Its import UI
  failure was corrected and the full UI rerun above passed. The other six failing
  test groups have unchanged pre-existing expectations: `accounts.basics`,
  `accounts.validation`, `accounts.combined`, `accounts.groups`,
  `import.navigation`, and `import.help_ui`. All four account modes and the
  failing navigation/Help cases were reproduced from an isolated `65ed402` source
  archive: old rule/category setup, cross-session ordinary Undo, old survivor
  picker and calendar walkthrough expectations. They were not weakened here.
- Grouped-import I/O and crash suites each completed modes 0–3 (133, 157, 21 and
  85 injected write/sync positions), then stopped at the existing mode-4 fixture's
  missing imported-category choice. Full grouped-import recovery coverage remains
  blocked by that fixture; the 19k workload and UI mapped-import cases passed.
- Both dedicated Review failure suites passed: **2,036 injected disk-full/I/O
  positions** (524 Separate/Undo; 1,512 Merge/Link/batch/Undo) and **24 process
  interruptions**, covering both sides of durable commit. Trials reopen, verify,
  retry and restore encrypted backups. The broader sweep took 1,082.48 s; the
  Separate sweep took 194.78 s while other checks were running.
- Changed-document file links and `git diff --check` passed. Manual prose and
  screenshots remain deferred in MAN-009.

Native interaction, private-file timing, Linux and Windows remain unverified.

## Ledger duplicate review — 2026-11-07

Implemented the [owner-approved ledger interaction](review-sheet.md#ledger-duplicate-resolution--owner-go-2026-11-07).
Possible duplicates carry rose markers without replacing new/accepted row colours.
The ledger comparison bar shows direct candidates, New/Accepted labels, the row
that Merge keeps, pair-only Separate, None of these match, Show match, Next and
Undo review. The context menu exposes the same resolution actions. Review adds
Show in ledger and New/Accepted row styling. Explicit reveal clears a hiding
filter; ordinary selection preserves ledger order and neighbours. The original lazy
Review loading was superseded by automatic discovery in the follow-up above.

Automatic survivor choice retains a unique linked/manual/accepted entry, or a
unique entry from the earliest import when neither row is protected. Ambiguous
protected entries require a choice; an explicit survivor that would discard the
unique protected entry is rejected. Merge's source association/recognition writes,
monetary validation and relationship protection remain in place. No schema,
retention, clean-marker policy or normal-open audit change was made (formats 64/63).
Focused Separate uses the existing atomic Review receipt/choice and Undo machinery,
preserving other direct comparisons and clean remainders from partial Merge.

Verification on macOS, synthetic fixtures only:

- Rules-enabled Release application and relevant test targets built successfully;
  `build/SpentIt` was rebuilt. Seven selected CTest cases passed: full import UI,
  ledger duplicates, Review sheet, batch Merge, matching policy, transfer assignment
  and manual transactions. The full Qt run reported **86 passed, 0 failed**.
  After the final viewport adjustment, four focused Qt cases passed (six including
  setup/cleanup), covering filtered candidate reveal, pair/None/Merge/Undo,
  automatic survivor choice and bringing an offscreen survivor into view.
- New storage checks cover pair-only rejection, origin-wide rejection, unchanged
  balances, retained alternatives, retries, Undo, clean partial-Merge remainders,
  an earlier unaccepted survivor, and preservation of an established account link.
- Four new Separate/Undo fault scenarios passed **524** injected disk-full/I/O
  write/sync positions, **8** process interruptions before/after commit, repeated
  retries and restored-backup checks. A first UI run under sandbox restrictions
  could not create the application-data lock; passing runs used approved access
  with normal locking unchanged.
- The three storage cases (ledger duplicates, Review sheet, batch Merge) passed
  in rules-disabled Release and ASan/UBSan builds. Leak detection was disabled;
  these build trees do not include the UI.
- The synthetic offscreen light-mode ledger comparison was rendered and inspected.
  The initial Review background layout warning was corrected; the later focused
  UI run had no such warning. Document file links and `git diff --check` passed.

Manual text and screenshots remain unchanged; [MAN-009](manual-update-backlog.md#man-009--ledger-duplicate-review)
records the deferred chapters and pictures. No private data was read. Native
interaction, dark-mode visual inspection, Linux and Windows remain unverified.

## Verification-policy follow-through — 2026-11-07

Inspection confirmed the accepted encrypted-marker policy and atomic baseline
publication were already implemented by the session-history correction below.
The offline-change decision is explicitly settled in the accepted contract:
no routine hashing, with reduced offline-change detection accepted. This follow-up
does not change that decision, format 64/63, financial validation, locking,
encryption, durability or baseline retention.

Removed the unused file-hash/audit-approval publication implementation, obsolete
opening-audit state and build-generated verifier fingerprint. Recovery-sidecar
inspection remains unchanged. This removes the superseded cache mechanism rather
than leaving it available to accidentally reinstate rebuild-driven audits.
Extended the policy regression to cover an unusable/stale cache location, another
uncommitted import followed by clean close/reopen, exact balance preservation,
explicit audit counting, and full backup candidate/restore checks.

Verification: the full rules-enabled macOS Release build succeeded, including
`build/SpentIt`. The targeted CTest selection passed **6/6**: verification cache,
sessions, marker faults, close faults, corruption and backup. The close sweep
covers I/O/disk-full failures, process exits before/after commit and lost
acknowledgement; marker faults exercise failure/process-exit activation recovery.
The rules-disabled build and corresponding selection also passed **5/5**
(policy, sessions, marker faults, corruption and backup). `git diff --check` passed.
Initial sandboxed tests were blocked by the application-data lock directory;
the recorded passing rerun used approved access with locking unchanged.

Manual text/screenshots remain unchanged; MAN-008 now explicitly includes
unaccepted imports, rebuild/cache independence and offline-change limitations.
No private data was inspected. Linux/Windows, native UI interaction and sanitizers
were not rerun in this follow-up; prior measurements below remain the storage
acceptance evidence because this change adds no persisted data or schema changes.


## Session-history correction — 2026-11-07

The correction replaces `f8a1f4b`'s permanent ordinary-revision layer in place.
`cf8f1a5`, its immediate parent, is the storage comparator. Unrelated changes,
including `0edf528`, `68b73f5` and `9fa2166`, remain intact; no reset, revert or
Git-history rewrite was used. Experimental formats are **64 with rules / 63
without**; previous formats are rejected unchanged with no migration.

Current payloads now form the baseline. `snapshot_facts` and allocation-origin
tombstones are removed. Close deletes obsolete ordinary snapshots, operation
records, monetary summaries and group memberships, retaining only current state,
original/acceptance/duplicate references and necessary relationship/import-Undo
inputs. Repeated linked edits no longer preserve a restoration chain. Pending
transfer proposals retain the specific predecessor needed by source rollback.
Compact request-ID/domain/digest receipts reject conflicts and prevent re-execution
without preserving transaction history. Composite-key payload tables use WITHOUT
ROWID storage rather than duplicating keys in an extra B-tree.

Successful close atomically expires ordinary history and publishes the baseline and
encrypted clean marker. Failed/cancelled closes and crashes retain the recoverable
session. Normal opens/closes run neither a full audit nor a full-file hash, including
after edits and imports left uncommitted. Abnormal shutdown and backups/restores
retain full checks. Settings **Check file** runs the explicit audit without ending
Undo. Full verification validates retained baseline state/dependencies and subsequent
session changes; it does not replay expired ordinary monetary history.

**Storage acceptance passed** on all three identical 1,000-row synthetic workloads
and an additional matched-transfer import-Undo workload:

| Workload | Original physical bytes | Corrected physical bytes | Original retained payload | Corrected retained payload |
|---|---:|---:|---:|---:|
| Import only | 1,556,480 | 1,503,232 | 602,151 | 569,698 |
| 32 edits / four closes | 1,855,488 | 1,777,664 | 822,085 | 760,219 |
| Cross-session import Undo | 2,146,304 | 2,039,808 | 602,202 | 569,752 |
| Matched transfer import Undo | 2,519,040 | 2,375,680 | 1,314,653 | 1,216,681 |

All intermediate measured points also pass. The [comparison report](session-storage-comparison.md)
separates live pages, reusable freelist pages and unused B-tree space, documents
reproduction and the rejected implementation's overhead. No VACUUM, padding or
weakened encryption/durability was used. These are synthetic measurements, not an
explanation of the owner's private file's reported 23% increase.

Executed verification on macOS arm64:

- Release build succeeded, including the application, storage and UI targets.
- Final lifecycle/dependency selection: **15/15 passed**, covering session expiry,
  pending imports, exact financial state, originals/duplicate dependencies, bounded
  ordinary and linked retention, rule retries, retained import Undo, corruption,
  clean-marker policy and close failure/crash boundaries. Close covered 105
  write/sync failure positions, both commit-side process exits and lost
  acknowledgement; activation covered 13 failure/process-exit positions.
- Rules-disabled lifecycle selection: **7/7 passed**; the final currency/deletion
  cleanup rerun passed **4/4**. Earlier schema-gate, marker-fault and close-fault
  checks also passed.
- ASan/UBSan lifecycle selection: **8/8 passed**; final cleanup rerun **5/5 passed**,
  including retained import Undo, currency/deletion pruning, provenance and retry
  conflicts. Additional currency/account, corruption and bulk/reapply checks passed in the preceding sanitizer selection.
- Final targeted offscreen UI: **9/9 passed** including Qt setup/cleanup: transfers,
  linked deletion/close, normal quit/open, failed close, cancelled recovery and
  explicit Check file preserving Undo and bulk interaction/close expiry. Bulk
  lost-acknowledgement/error recovery passed separately (**3/3**).
- Broader Release financial/rule selection: 57/58 passed in the initial sweep;
  the cross-session proposal predecessor failure was fixed and passed the final
  Release, rules-disabled and sanitizer reruns. This includes disk-full/I/O and
  process-exit tests for edits, imports, rules, categories and bulk commands.
- Relationship/account/report/backup selection: 19 ordinary cases passed. The two
  transfer fault sweeps exposed an outdated one-sequence-step test assumption for
  atomic bank-match completion (two records) and source discard (no financial step).
  The assertion now compares against the completed operation's sequence. All 15
  transfer scenarios passed both I/O and process-exit sweeps across the full run's
  successful cases 0–7 and the final cases 8–14 rerun, including before/after commit
  boundaries and retries. The default tests still run all 15 cases.

Testing found and corrected sequence high-water reuse after rollback, missing
proposal predecessors, stale legacy bulk/reapply format gates, duplicate-target
ordering, deletion Undo foreign-key cleanup, deletion subcommand retention and
obsolete currency-conversion records. Tests were updated for current
formats, the existing 46 starter categories, and session-only bulk history.

Limits: no Linux/Windows build or native interactive platform walkthrough was run;
offscreen Qt tests are not a native window-system certification. An earlier broad
UI run also had Help calendar/typing/focus failures; those unrelated interactions
were not rerun or certified by this storage task. Offline-change detection is reduced by the accepted marker policy. Files retain reusable free
pages and compact completion receipts grow with commands. No private dataset was
read. Manual prose/screenshots remain unchanged; MAN-008 records the deferred work.
See the [design and operation-validation inventory](session-history-baseline.md)
and [startup policy](fast-startup.md). Earlier dated entries below describe the
superseded implementation and are not current completion claims.

## Session-history storage review — 2026-11-07

The owner reports a dataset size increase from approximately 65 MB to 79 MB
following this change. No private dataset or table/page breakdown was inspected;
the precise contribution of live data, indexes and reusable pages is unmeasured.
Code review confirms that snapshot facts persist for every revision, alongside
allocation-origin records and retry receipts. Close deletes eligible payloads but
does not physically shrink the file. A fresh import can therefore gain bookkeeping
without removing much ordinary edit history.

The completion claim below was too broad: session history expiry, retry protection
and dependency-based payload pruning are implemented and tested, but the verifier
still relies on permanent per-revision financial facts and historical monetary
continuity. It has not fully moved to current-baseline verification with only
necessary retained dependencies. This architectural limitation and representative
bulk-import storage overhead remain unresolved. No corrective implementation was
requested in this exchange; the owner requested committing the current work.

## Session history baseline — 2026-11-07

Implemented the accepted [session-only Change History scope](session-history-baseline.md)
under owner Go. Current experimental formats are **62 with rules / 61 without**;
earlier formats, including intermediate 60/59, are rejected unchanged without migration.
Every successful File → Close file, document switch or quit publishes the current
state as the baseline and expires ordinary history. Failed/cancelled closes and
crashes retain the recoverable journal. Closing does not accept pending imports.

The close transaction now computes snapshot dependencies, prunes unreferenced
payloads and their allocation/ownership/provenance children, expires the session
journal and Review stack, and advances the encrypted clean generation atomically.
It retains originals, acceptance references, current heads, duplicate recognition
(including removed identities), transfer/Review restoration dependencies and eligible
source-scoped import Undo. Allocation-origin tombstones prevent rebinding deleted
split identities. Financial/conflict generations are not reset. No full close audit,
full-file hash, VACUUM, baseline payload copy or physical shrink is introduced.
Unchanged closes use the indexed unfinished-facts check.

Direct corrections, inline edits and selection edits persist exact request
fingerprints and durable completion identities. Expired retries report already
completed and require refreshing current data; changed content is rejected and
cannot execute again. Selection-request text is removed when the fingerprint is
sufficient. Current values are never presented as an expired historical result.
Existing compact configuration/identity receipts and necessary financial references
remain. A committed close with lost acknowledgement is resolved from its generation;
retry cannot publish a second generation. Finalized connections refuse mutations.

Full verification validates the retained baseline payloads, source/provenance and
financial relationships against immutable mutation-time snapshot facts. It checks
new session transitions from that baseline; expired ordinary field transitions no
longer depend on removed snapshots. Compact monetary ordering/totals, acceptance,
currency and relationship records still reconcile independently. Retained scalar
facts/receipts can grow; this is not a claim of constant database size or secure
erasure of old backups. The historical duplicate-target lookup includes retained
earlier-session revisions, independently of user-visible history eligibility.

The broader verification-policy rollout remains separate. Normal open still uses
the existing cache/audit policy and can audit even after clean close; unclean sessions
always require recovery verification. Close no longer publishes cache approval.
The welcome authentication probe does not start a history session. Backup/restore
checks remain. No new Check file UI is included. The accepted encrypted-marker,
reduced offline-detection policy is settled for the follow-up; do not ask it again.

Verification on macOS:

- Rules-enabled Release application built. A 24-test focused run passed (227.85 s),
  including committed editing/history/dependencies/exact arithmetic, validation,
  backup, legacy-format refusal, I/O/process-crash sweeps and the new session tests.
- After the final pruning/verifier changes, all 13 focused session/currency/rule,
  cross-session import-Undo, close-policy and QML checks passed (27.49 s). Another
  12 financial workflow checks passed (40.64 s): manual entries, selection edits,
  Review batch, transfer assignment/management/imports/splits/QIF and reports.
- All nine selected rules-disabled Release checks passed (24.23 s), including
  session failures, cross-session import Undo and schema rejection. All nine
  selected ASan/UBSan checks passed (49.21 s), including session recovery/pruning,
  import Undo and rule application after baseline publication.
- Session tests cover clean/failed/cancelled close, multi-generation expiry,
  original values, pending-import retention, changed-content retry rejection,
  removed-entry reimport, backup restore, corruption of retained payloads/compact
  facts, and zero full-audit calls at close. Import-Undo checks cover shared and
  duplicate-only sources, multi-account imports, generated counterparts, Review
  Link reversal, prior arrangements, stale previews and restored backups.
- Dedicated fault sweeps exercise session activation and close with alternating
  disk-full/write errors, abrupt exits, both close commit boundaries and lost
  acknowledgements. The close sweep additionally asserts all-or-nothing payload
  pruning and journal expiry. Repeated-session tests retain only original/current
  ordinary entry payloads after eight edits per generation, over four generations;
  the measured synthetic close times were 17.22, 13.81, 9.46 and 11.51 ms, with
  file sizes 954,368 / 954,368 / 954,368 / 962,560 bytes. These small synthetic
  measurements are not large-file performance claims. Strengthened pruning-fault
  reruns passed in Release, rules-disabled Release and ASan/UBSan.

Final verifier review also retained financial preview/dependency-token validation
for expired compact records and added a tampered-preview regression. The final
reruns passed 10/10 Release checks (32.38 s), 4/4 rules-disabled checks (9.77 s)
and 5/5 ASan/UBSan checks (23.49 s). `git diff --check` and local documentation
link-target checks passed; the Release application was rebuilt.

Tests use synthetic data and required access to application-data locks. Native
interactive close/switch/quit, large private datasets, Linux and Windows were not
verified in this work. Manual text/screenshots remain unchanged; deferred chapters,
pictures and retry wording are recorded in [MAN-008](manual-update-backlog.md#man-008--session-change-history-and-close-recovery).

## Manual-transaction test wait — 2026-11-06

Corrected `manualTransactionEntry` under owner Go to wait for the second deletion
preview dialog before invoking Delete. The preceding delete and Undo already
succeeded; the test previously attempted its second deletion while the preview
worker was busy, so the command was rejected and the restored row survived reopen.
This was test synchronization, not an Undo/storage failure. Production code and
the selection-edit History UI remain unchanged; no manual update is needed.

Rebuilt the rules-enabled Release navigation-test target. The focused offscreen
`manualTransactionEntry` run passed on macOS (3/3 including setup/cleanup,
1787 ms), with application-data lock access. The existing float-to-int warnings
in screenshot test code remain. `git diff --check` passed. The full navigation
suite and other platforms were not rerun for this test-only correction.

## Navigation and Settings cleanup — 2026-11-06

Owner-approved layout follow-up: the file path now sits directly below Current
file, with a smaller gap before Date format. Equal explicit section spacers
separate Window, File settings and Backup and recovery. Removed the Settings
Close file button and close-before-opening note; File → Close file remains.
Existing navigation and manual-walkthrough test actions now use that menu entry.
No manual text or screenshots were changed; MAN-007 includes the deferred work.

Follow-up verification: rebuilt the Release application, navigation tests and Help
UI tests. `import.qml` passed (1.58 s); the focused offscreen
`guardedNavigationAndIsolation` run passed (3/3 including setup/cleanup, 1542 ms),
including closing through the File menu. Help walkthroughs were compiled but not
rerun or used to recapture pictures. The compiler reported existing float-to-int
warnings in navigation-test screenshot code. `git diff --check` passed; native
visual inspection and other platforms remain unverified.

Implemented under owner Go: unavailable toolbar Back/Forward buttons now render
at 45% opacity, reinforcing the existing disabled palette and destination/busy
checks. Settings no longer has the bottom Return to account button; sidebar
accounts and toolbar navigation remain available. No financial behavior or format
changed. Manual work is deferred as [MAN-007](manual-update-backlog.md#man-007--navigation-availability-and-settings-cleanup).

Verification on macOS Release: rebuilt `spentit_import_experiment` and
`spentit_import_navigation_tests`; `import.qml` passed (1.52 s).
The first navigation run could not create application-data locks under the
sandbox. Rerunning with that access completed in 19.70 s: 21 passed, one failed,
one opt-in diagnostics case skipped. `guardedNavigationAndIsolation`,
`ledgerViewportSurvivesCategoryNavigation` and `appearanceSwitching` passed.
`manualTransactionEntry` failed at its existing line 319 assertion: after a second
delete attempt and reopening,
`occurrenceIndex(batch,0)` was 1 instead of -1. This manual-entry failure was not
investigated or changed in this narrow presentation work; the full suite is not
claimed passing. Native visual inspection and Linux/Windows remain unverified.
`git diff --check` passed. The local `build/SpentIt` application is rebuilt.

## 50k audit profiling — 2026-11-06

The prior improvements are committed as `0edf528`. A committed-build synthetic
50k run reproduced the full audit at 74.41 s: committed replay 29.76 s and
category/provenance 27.99 s (78% combined). Observation-only instrumentation
confirmed source-row and snapshot caches saturate their byte budgets: 268,192
source reconstructions for 50k rows and 360,564 snapshot reconstructions for 150k
distinct revisions. Full-scan work scaled with rows; repeated targeted reads
drove most of the excess work. All profiled financial assertions passed.

The owner subsequently authorized implementation. Transaction-local source reuse,
a stable/rolling snapshot cache, ordered snapshot/metadata reads and audit-only
currency timelines are implemented; see the [follow-up report](audit-50k-profile.md).
Whole-cache LRU was measured and rejected because sequential passes increased
reconstructions. All integrity, exact replay, ownership and financial validation
remain enabled; cancellation, durable transaction boundaries and Undo are unchanged.
Final serial synthetic Release measurements: the 15k full-audit median fell from
11.59 s to 9.68 s (three runs, 16.5% faster), and the 50k audit fell from 73.77 s
to 57.97 s (single runs, 21.4% faster). Final 15k Delete/Undo medians were
8.15 / 6.96 s; Undo was 3.6% slower than the prior 6.72 s median. The 50k
Delete/Undo measured 28.39 / 21.82 s. The final counter run confirmed 31.4% fewer
source reconstructions and 28.9% fewer SQL step calls with unchanged full-scan
validation work. Final 20k/36k audits passed at 13.56 / 30.97 s, and five
Delete/Undo/audit cycles on 5k rows passed as history grew to 55k snapshots.
Large-history audit growth is still nonlinear.

Formats remain 58/57. Verification includes 36 selected Release checks before the
cache-policy adjustment, ten focused final Release checks, six final rules-disabled
and six final ASan/UBSan checks, and a final Release byte-budget eviction fixture
covering cancellation/retry, reopened Undo and historical corruption rejection.
The earlier whole-cache LRU eviction fixture also passed ASan/UBSan. The Release
application is rebuilt; native large-selection interaction and Linux/Windows
remain unverified. No manual changes are needed for this internal optimization.

## Undo/audit performance follow-up — 2026-11-06

Implemented the first two proposed improvements under owner Go: one bounded,
append-aware revision-to-history lookup shared by currency/category validation,
and reuse of already-read snapshots, source/member facts and committed-history
cursor fields. Rollback/cleanup invalidate the lookup; missing/ambiguous history,
currency changes, dependencies, ownership, exact balances and strict Undo remain
validated. Cancellation and durable transaction boundaries are unchanged. Format
remains 58 with rules / 57 without.

Serial synthetic macOS Release benchmarks on the final build (three 15k runs):

| Operation | Prior median | New median | Speedup |
|---|---:|---:|---:|
| Delete | 9.70 s | 8.28 s | 1.17× |
| Undo | 18.89 s | 6.72 s | 2.81× |
| Full audit | 64.81 s | 11.59 s | 5.59× |

All financial assertions passed. The proposed under-5-second Undo and
under-10-second audit targets were not reached. A 50k-row scaling check before
the final statement-lifetime adjustment passed at 29.48 / 22.64 / 73.77 s;
audit growth at that size remains nonlinear. No transfer-heavy speedup is claimed.
See the [full benchmark, implementation and verification report](undo-audit-profile.md)
for ranges, counters, memory, deeper history and remaining limits.

Verification includes 35 selected storage checks (one stale cancellation-test
trigger corrected and rerun), four UI/import-rollback checks, rules-disabled and
ASan/UBSan checks. New regressions reject missing/duplicate revision history and
verify cancellation/retry with unchanged file bytes. The final dependency-plan
lifetime adjustment passed eight focused Release checks and two each with rules
disabled and ASan/UBSan. The rules-enabled Release application was rebuilt.
Linux/Windows and native large-selection interaction remain unverified; manual
text/screenshots are unchanged because no user-visible workflow changed.

## Linked deletion and large-selection performance — 2026-11-06

Implemented under owner Go with the accepted defaults. Ledger Delete now prepares
both choices on the worker and shows per-account counts and exact currency-specific
balance changes. **Delete selected only** is the default and clears kept independent
counterpart allocations to Uncategorized. **Delete linked entries too** removes
only linked allocations from split counterparts; unrelated splits survive with
an exactly adjusted effective total. Generated drafts follow existing disposal
policy. Manual/generated postings, including subsequently deleted retained local
postings, use the same atomic command and group Undo. Cancellation remains enabled
until the durable commit gate. See the [contract](ledger-editing.md#linked-selection-deletion-and-large-operation-performance--owner-go-2026-11-06).

Deletion records explicit replayable transfer transitions. A transfer with local
money in two accounts uses two monetary history entries in one database transaction;
Undo reverses both stages and counterpart row changes. Retry compares the selection,
generation and counterpart policy. Pending match proposals and legacy attached-import
relationships remain guarded rather than silently deciding source ownership.
Experimental schema remains 58 with rules / 57 without; no migration or existing
history reinterpretation was introduced.

Performance changes: reuse bounded statement preparation across bulk commands and
Undo; initialize an exact running balance once per account instead of summing its
ledger for each row; reuse transfer Undo's relationship set/balances; start historical
currency/category lookups from their sparse operation tables; use complete Review
member keys for strict Undo; and cache draft-Undo membership only within an audit,
with a 65,536-member / 16 MiB bound and a query fallback. Full history, transfer,
provenance and financial validation remain enabled.

Measured on synthetic macOS Release data (wall times are workload measurements,
not UI-response guarantees; some runs overlapped other checks/builds):

- The same 4,100-row mixed accepted/new selection regression, including cancellation,
  retries, rename/restore/category/delete, Undo and a reopened full audit, completed
  in **147.39 s**, versus the earlier **465.11 s** measurement recorded below.
- A separate 15,000-row accepted-account workload completed deletion in **9.00 s**,
  Undo in **18.06 s**, and full audit after both operations in **59.10 s**. Every row
  and the exact balance were checked before/after Undo. This workload has ordinary
  transactions; linked-deletion correctness is covered separately. The audit still
  takes substantial time and is not advertised as instantaneous.
- An intermediate 15,000-row build before operation-scoped statement reuse measured
  22.73 / 86.48 / 63.88 s for deletion / Undo / audit; this is an intermediate
  comparison, not an untouched pre-change baseline.

Verification:

- Rules-enabled and rules-disabled linked-deletion tests passed: both choices,
  accepted/new independent postings, unrelated counterpart splits, manual/generated
  postings from either side, deleting a retained local posting later, stale requests,
  cancellation after link writes, durable retry, exact balances and reopened Undo.
- The same linked-deletion suite passed ASan/UBSan with
  `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1:abort_on_error=1`
  and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, with no diagnostic.
- The focused dialog test passed offscreen (3/3, 1233 ms) and native cocoa
  (3/3, 1611 ms): default, preview, Cancel, both choices and session Undo.
- The final selected Release CTest run passed **21/21 in 304.26 s**: linked
  deletion/recovery, selection edit/recovery, editor, Review and batch Review,
  transfer assignment/release, transfer storage basics/management/splits/manual
  evidence/validation, account lifecycle behavior/boundaries, category history,
  rule account Apply/recovery, full import UI and QML. The UI suite reported
  **84 passed, 0 failed, 1 skipped** (`bulkInteractions`: the legacy bulk format is not enabled).
- The final linked-deletion recovery sweep covered **153 / 155 / 147 / 145**
  injected write positions for keep/delete-counterparts and their Undo cases
  (600 total), alternating disk-full and I/O write errors. Each trial reopened,
  checked both accounts and receipt state, retried twice, and passed full audit.
  All eight before/after durable-commit crash cases passed. Existing selection
  fault sweeps also passed all four modes, 193 positions each.
- `git diff --check` passed. The full Release build completed and rebuilt
  `build/SpentIt`. No personal dataset
  was used. Linux/Windows interaction and performance remain unverified.

Manual text/screenshots were deferred to [MAN-006](manual-update-backlog.md#man-006--delete-linked-selections).

## Transaction selection shortcuts — 2026-11-06

Implemented under owner Go: platform Select All selects every transaction in the
current filtered ledger, including offscreen rows, excluding Opening Balance.
Command+A / Command+Shift+A work on macOS; Ctrl+A / Ctrl+Shift+A are the corresponding
Windows/Linux bindings. Deselect also accepts Qt's platform binding where present.
Keys are handled only with the list or one of its row delegates focused, including
when Deselect has cleared the current index. Text inputs and modals keep their own
keyboard handling. Selection preserves unsaved editor text and session Undo;
Select All and Deselect count as explicit search-result choices when clearing search.

The existing `select` command now accepts `mode: all` / `mode: none`; these operate
on the worker's filtered identity list, retain inspection independently and use
existing presentation/diagnostic paths. No financial/storage or schema changes.
See the [contract](search-ledger.md#selection-shortcuts--owner-go-2026-11-06).
Manual text and pictures remain deferred under
[MAN-005](manual-update-backlog.md#man-005--selection-keyboard-shortcuts).

Verification: synthetic data, macOS arm64, Qt 6.12.0, rules-enabled Release.

- Rebuilt `spentit_import_ui_tests` and `spentit_import_experiment`; `build/SpentIt`
  includes the shortcuts.
- `QT_QPA_PLATFORM=offscreen spentit_import_ui_tests selectionShortcuts largeSelection`:
  **4/4 including setup/cleanup, 8526ms**.
- Same tests with `QT_QPA_PLATFORM=cocoa`: **4/4, 9269ms**. Uses real Qt key events
  to check list/row focus, select/deselect with 4,100 rows, filtered-result choices,
  empty/opening-only lists, exclusion of Opening Balance, search/payee text
  selection, unsaved edits, modal focus and retained session Undo.
- Full Release `ctest --test-dir build/import-ui-rules-release -R
  '^import\.(ui|qml)$' --output-on-failure -j 2`: **2/2 passed, 107.30s**.
- `git diff --check` and 312 relative documentation file targets passed.

Initial focused tests exposed ListView focus landing on a delegate rather than
on the list itself, and retaining row focus after clearing the current index;
these cases are fixed and covered by the final passing runs. Windows/Linux,
rules-disabled UI and sanitizer checks were not run for this presentation change.


## Larger transaction selections — 2026-11-06

Implemented under owner Go: removed the 4,096-row ceiling from ledger range/toggle
selection and search-selection restoration, selected rename/restore/delete,
category/account transfer assignment, and Review decisions/batch Merge. Removed
Review's related 2,048-group command and Undo-reader ceilings. Nonempty/duplicate,
account, revision and relationship checks remain. Existing atomic transaction and
commit-seal boundaries, retry receipts and group Undo are unchanged; command
preparation now also checks cancellation while encoding large selections.

Formats remain **58/57**. The inactive format-52 legacy bulk API and its schema
bounds are untouched; current documents use the selection commands above.
Unrelated parsing, regex and duplicate/transfer candidate-search limits remain.
See [contract](search-ledger.md#selection-size--owner-go-2026-11-06).
Manual text/screenshots are deferred under [MAN-004](manual-update-backlog.md#man-004--larger-transaction-selections).

Verification (synthetic files, macOS arm64, Qt 6.12.0):

- Rebuilt rules-enabled Release `spentit_review_sheet_tests`,
  `spentit_import_ui_tests`, and `spentit_import_experiment`; `build/SpentIt`
  includes the change.
- Offscreen `spentit_import_ui_tests largeSelection`: **3/3 including
  setup/cleanup, 5881ms**. Selects 4,099 rows by range, toggles the 4,100th,
  and restores all identities after search with a changed sort.
- Release `ctest -R '^import\.(ui|qml)$'`: **2/2 passed, 117.37s**. UI
  reports **82 passed, 0 failed, 1 skipped** (inactive legacy bulk format).
- Release `import.large_review_batch`: **passed, 72.87s**. Covers 2,050 groups /
  4,100 rows, cancellation before the final group, exact balances, retry,
  restored backup reopening and complete group Undo.
- Release `ctest -R '^import\.(large_review_batch|selection_edit|selection_edit_faults|transfer_assignment|transfer_assignment_faults|review_batch)$'`:
  **6/6 passed, 226.13s**. Includes existing disk-full/I/O sweeps and process
  interruptions before/after the durable commit boundary for selected edits,
  their Undo, and transfer assignment.
- Final Release `ctest -R '^import.large_selection$' -V`: **1/1 passed,
  465.11s**. Uses 4,100 rows (2,050 accepted / 2,050 new); checks cancellation
  after 4,096 writes, unchanged rows/receipts on rollback, identical retry,
  invalid-last-member rejection, rename/restore, category ownership rollback and
  Undo, delete/Undo, exact balance and row counts, then closes/reopens for a full
  history audit. The test timeout is 1,800s for this deliberate stress workload.
- Rebuilt rules-disabled Release `spentit_review_sheet_tests`. Final
  `import.large_selection`: **1/1 passed, 427.93s**; `large_review_batch`,
  `selection_edit`, `transfer_assignment`, and `review_batch` separately passed
  (75.72s, 1.38s, 1.35s, 2.72s). Initial large mixed runs were intentionally
  terminated while simplifying redundant full test audits; the final test keeps
  all mutation checks and one full reopen audit. Production auditing is unchanged.
- `git diff --check` and changed-document relative file-target checks passed.

The first sandboxed UI/storage attempts could not access application-data locks;
authorized reruns use that directory. No private data was used. Sanitizers and
Windows/Linux checks were not run for this change. Large account-transfer creation
and arbitrarily large responsiveness are not established by these tests. Native
sampling of these synthetic test processes observed expensive large Undo
(`restore_review_answer` / `write_review_row` / `committed_for`) and full category /
committed-history auditing. The seven-to-eight-minute stress-test durations cover
many commands plus reopening, not one UI operation. This change removes fixed
selection ceilings; it does not optimize those existing paths.


## Shutdown progress modal — 2026-11-06

Implemented the owner's follow-up Go: **Closing SpentIt…** is a compact modal
with the shared high-contrast rotating indicator and current shutdown phase.
It appears after unsaved-edit and acceptance decisions, leaves the main window
and status feedback visible, has no buttons, and blocks Escape/outside dismissal.
Checking failure replaces it with Return / Quit anyway; Quit anyway shows
progress again. Successful shutdown retains the existing check/cleanup boundary.
The toolbar and dialog now share one inline spinner component with unchanged
colours and animation. Financial/storage behavior is unchanged.

See [feature 014](../features/014-busy-feedback-and-progress.md#shutdown-progress-dialog--owner-go-2026-11-06)
and the [close contract](fast-startup.md). Manual text and images remain deferred
under [MAN-003](manual-update-backlog.md#man-003--visible-shutdown-progress).

Verification: macOS arm64, Qt 6.12.0, rules-enabled Release, synthetic files only.

- Rebuilt `spentit_import_experiment`, `spentit_import_ui_tests` and
  `spentit_import_navigation_tests`; `build/SpentIt` includes the dialog.
- Native `spentit_import_ui_tests visibleShutdownProgress cachedStartupReviewAndQuit
  closingApprovalFailure windowPositionSettingsAndQuit warningAndQuitAcceptance`:
  **7/7 including setup/cleanup, 4585ms**. The delayed-close case verifies modality,
  matching status text, no buttons, Escape/outside blocking, repeated-close
  suppression, final exit and differing rendered spinner frames. Recovery checks
  verify progress dismissal on failure and reappearance on Quit anyway.
- The same focused offscreen tests passed **7/7 in 3939ms**.
- Native `spentit_import_navigation_tests activityAnimation` passed **4/4 in
  33697ms**, including rendered animation in light and dark themes.
- QML smoke and `git diff --check` passed; changed-document relative file targets
  were checked. The build retains pre-existing float-conversion warnings in the
  navigation animation test; the new frame crop uses explicit rectangle rounding.

The focused presentation checks do not establish responsiveness on a large real
file. No private data was used. The full UI suite, sanitizers and Windows/Linux
checks were not repeated for this modal-only follow-up.

## Visible shutdown progress — 2026-11-06

Implemented the owner's correction and Go: keep windows visible until graceful
shutdown finishes. Quit and Quit anyway retain the main window, rotating busy
indicator and worker status messages. Close starts with **Closing file…**, reports
existing verification phases and returns to closing before releasing the file.
Repeated close requests are ignored while closing. Checking failure leaves the
window visible with the existing Return / Quit anyway decision. Successful close
now waits asynchronously for queued ledger-snapshot destruction on the serial
worker pool before clearing busy and allowing the windows to close.

Existing unsaved-edit/acceptance decisions and storage verification remain in
place. No schema or financial-command change. The [startup/close contract](fast-startup.md)
supersedes its earlier hide-first direction. Manual text/pictures were left alone;
[MAN-003](manual-update-backlog.md#man-003--visible-shutdown-progress) records the
later manual pass.

Verification: macOS arm64, Qt 6.12.0, rules-enabled Release, synthetic data only.

- Rebuilt `spentit_import_experiment` and `spentit_import_ui_tests`; `build/SpentIt`
  contains the change.
- Final `ctest --test-dir build/import-ui-rules-release -R
  '^(import.ui|import.qml)$' --output-on-failure`: **2/2 passed in 97.72s**.
  Import UI reported **81 passed, 0 failed, 1 skipped** (disabled bulk format).
- Native `spentit_import_ui_tests visibleShutdownProgress cachedStartupReviewAndQuit
  closingApprovalFailure windowPositionSettingsAndQuit`: **6/6 including
  setup/cleanup in 2500ms**. The controlled delayed-close case checks ongoing
  visible progress, active/visible indicator, disabled import, repeated close
  suppression and final close. Other cases cover approval publication, checking
  failure, Quit anyway and saved window position.
- `git diff --check` and changed-document relative file-target checks passed.

Initial sandbox tests could not create application-data locks; authorized reruns
used the required lock directory. The first broader run passed shutdown cases but
failed `repeatedSplitRestoreEnter(committed)` with a date-validation error; the
final complete rerun passed without a split-editor change. No claim of fixing
that intermittent test is made. These checks verify native window/progress state,
not frame-by-frame animation under a large real dataset. Windows/Linux and
sanitizer runs were not performed for this change.


Last updated: 2026-11-06

## Import progress dialogs and date pickers — 2026-11-06

Implemented under owner Go. Submitting account mappings hides the mapping form
and shows a compact **Importing transactions…** modal. Apply Selected and Use
Uncategorized hide the category checklist and show **Applying imported categories…**.
Both expose the existing Cancel operation, disable repeat requests while Cancelling,
and show Finishing when cancellation is unavailable. Escape/outside dismissal is
blocked. Existing bottom status and title-row animation bindings are unchanged.
Still-pending mapping/category choices survive failure/cancellation; successful
completion advances to the next decision. Financial commands and cancellation
boundaries are unchanged; formats remain **58/57**.

The shared calendar now covers import opening dates, opening corrections, legacy
reported-balance review and manual transfers as well as existing transaction/report
fields. Calendar choices update editable values without submitting commands.
Date-format pattern settings remain text-only. Manual prose and images are
unchanged; [MAN-002](manual-update-backlog.md#man-002--import-progress-and-date-calendars)
records the deferred work. The broader busy/readiness audit remains pending.

Verification on macOS arm64, Qt 6.12.0, rules-enabled Release, synthetic files only:

- Rebuilt `spentit_import_experiment` and `spentit_report_ui_tests`; `build/SpentIt`
  contains the change.
- Final `ctest --test-dir build/import-ui-rules-release -R
  '^(import.reports_ui|import.qml)$' --output-on-failure` passed **2/2 in 8.23s**.
  UI cases cover immediate form-to-progress transitions, invalid-date failure
  with retained mappings, cancellation at admission for mapping and both category
  actions, cancellation feedback, successful retry, completion, and leap-day
  propagation through all added calendars without starting a command. Existing
  report/calendar tests also pass. No QML warnings in the new cases.
- Native `spentit_report_ui_tests importProgress reportsAndCalendar` passed
  **5/5 including init/cleanup in 3554ms**. Existing calendar keyboard tests cover
  day selection and Escape navigation. The new calendar checks exercise signal
  integration, not full keyboard/pointer interaction in every enclosing dialog.
- `git diff --check` and relative Markdown file-target checks passed.

Initial sandboxed tests could not create application-data locks; authorized reruns
used temporary synthetic documents. An initial tiny-fixture cancellation raced
with completed work; the test now requests cancellation synchronously at admission
before worker execution. A subsequent full run exposed test-settings leakage of a
removed temporary startup path; per-test settings reset fixed fixture isolation.
The final checks above ran after those test corrections.

No new storage/financial behavior or schema changes, private-data tests, broad
busy-state audit, large-import animation/latency measurements, dark-theme checks,
Linux/Windows checks or full-suite runs are claimed. Existing rotating-indicator
verification remains as recorded in the earlier visual-first checkpoint.

## Manual update backlog — 2026-11-06

The owner requested deferred, accumulated manual maintenance. The
[manual update backlog](manual-update-backlog.md) records outstanding Income
grouping screenshots, picker/example checks and the Saving… explanation, while
distinguishing the prose already updated in `90d4a73`. AGENTS and the Help handoff
link to it and direct future feature work to log manual impacts for a later batch.
No manual content or screenshots changed in this documentation-only step.

## Income checkbox refresh correction — 2026-11-06

Owner reported an unchecked intermediate row and a roughly four-second delay.
The original handler rebound the control to its old stored value, submitted
preview and apply as separate worker commands, and invalidated all transaction
presentation caches. Category storage also ran the general import-review refresh.

Income toggles now validate/save in one worker command, display a partial check
with **Saving…** until completion, and publish the saved check/order together.
Only category presentation is refreshed. Cached ledger/search, balances and
Review questions remain valid because this flag changes none of their inputs.
Review/rollback generation tokens and Undo availability are updated; other
category actions still use their existing refresh path. Storage retains the same
atomic transaction, mutation-generation advance, before/after commit observation,
revision checking and durable outcome handling, while retaining only previously
current Review caches. Rollback invalidates speculative caches as before.
Formats stay **58/57**; no migration or new format change.

The synthetic 4,000-pending-row UI workload reproduced the prior unchecked state,
**383ms**, two completed commands and one ledger reset. With the correction it
passed at **57ms**, one completed command and zero ledger resets (Qt offscreen,
macOS arm64 Release). Timing includes the test's event polling; this is not a
measurement of the owner's file. It asserts the immediate pending indicator,
final check/order, unchanged Review questions/balances and current generation
tokens. The focused workload plus sorting pass reported 4/4 including init/cleanup
in 7023ms. Existing financial/storage correctness is not inferred from timing.

Verification for this correction:

- Release Income persistence, pending Review-cache/Undo correctness, all counted
  I/O fault positions and crash/pre-/post-commit checks passed **4/4 in 61.41s**.
  The new Review test includes nonempty duplicate questions, failed-before-commit
  rollback, saved-after-commit outcome lookup and reopen verification.
- Full category UI, report UI and QML smoke passed **3/3 in 29.26s**.
- Native workload/sorting checks passed **4/4 including init/cleanup in 6894ms**;
  the native 4,000-row toggle also measured **57ms**, one command, zero resets.
- Rules-disabled Income persistence and Review-cache tests passed **2/2 in 1.20s**.
- ASan/UBSan Income persistence, Review-cache, I/O and crash sweeps passed
  **4/4 in 145.16s**, with no sanitizer diagnostic. The same sanitizer environment
  listed in the initial grouping checkpoint below was used.
- `git diff --check` and 376 relative Markdown file-target checks passed.

No private dataset was opened or altered. Linux/Windows and timings on the owner's
file remain unverified. The rules-enabled Release `build/SpentIt` was rebuilt.

## Category income grouping — 2026-11-06

Implemented under owner Go. Categories has an Income checkbox before Name;
new categories default off. The flag is an atomic, revision-checked category
history edit, retained through rename/archive/reopen/backup restore. Default
workspace and category-picker ordering is Expenses A–Z, Income A–Z, Accounts
A–Z. Other workspace sorts apply within ordinary groups. Explicit flag changes
remove the affected row's visit-addition bottom placement and retain selection.

Reports has tri-state Expenses and Income group checkboxes, with Uncategorized
separate. Group selection covers the whole current group even under search,
without changing the other group. Saved reports keep explicit category IDs when
a flag changes. Report arithmetic, signs, result sorting and transfer policy are
unchanged. The category/report manual text now explains the controls; existing
manual screenshots were not regenerated. See the [accepted contract](../features/010-categories-and-selection.md#income-grouping--owner-go-2026-11-06).

Experimental formats are **58 with rules / 57 without**; earlier formats are
rejected unchanged without migration. The local rules-enabled Release application
was rebuilt at `build/SpentIt`.

Verification on macOS arm64, Qt 6.12.0, synthetic files only:

- Rules-enabled Release CTest storage selection passed **10/10 in 82.88s**:
  `import.reports`, `category.income`, `category.income_io`,
  `category.income_crashes`, catalog, names, assignments, history, backup and
  validation. The new Income tests exercise default/set/clear, rename/archive,
  reopen/backup restoration, stale revisions, identical/conflicting retries,
  unchanged balances/source data, corrupt flag history and old-format rejection.
  Both failure sweeps cover every counted write/sync position; crash checks also
  interrupt immediately before and after durable commit.
- Rules-enabled Release CTest UI/schema selection passed **4/4 in 29.29s**:
  `rulestore.schema`, `import.reports_ui`, `import.category_ui`, `import.qml`.
  After final search/scroll test refinements, Reports UI and QML smoke passed
  again **2/2 in 7.26s** against the final application resources.
  A subsequent targeted Rules `sharedCategoryCompletion` passed **3/3 including
  init/cleanup in 1247ms**. Category tests cover grouping in each workspace sort,
  ledger-picker order, immediate visit-addition movement and retained selection.
- Rules-disabled Debug category Income and schema gate passed **2/2 in 10.50s**.
- Rules-enabled ASan/UBSan category Income plus full Income I/O and crash sweeps
  passed **3/3 in 150.99s**, with no sanitizer diagnostic. Invocation used
  `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1:abort_on_error=1`
  and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
- Native Categories `incomeSorting` passed **3/3 including init/cleanup in
  1947ms**. Final native Reports `incomeGroups` passed **3/3 including
  init/cleanup in 1684ms**, covering partial-state feedback, keyboard activation,
  search-hidden members, pointer activation after scrolling, and saved-selection
  stability. Synthetic captures of both workspaces were visually inspected.
- `git diff --check` and **376 relative Markdown link file-target checks** passed.

Initial sandboxed storage/UI tests could not create required application-data
locks; authorized reruns passed. An initial report assertion compared category
ID order despite save-time canonical sorting; it was corrected to compare the
selection. A native pointer test initially scrolled before restored search rows
had laid out; its final setup waits for layout. No financial behavior was changed
for these test corrections. Full-suite, Linux, Windows, spoken accessibility and
large-category performance checks were not run for this change.

## Help keyboard and appearance closeout — 2026-11-06

Under the owner's Go, Help now has an explicitly tested keyboard path through
navigation, contents/search results, text sizing, find, article links and pictures.
The pass found unreachable topic/result delegates; they now accept Tab focus,
show a focus border and scroll the focused row into view. Article Tab/Shift+Tab
selects links and pictures; Enter activates them through the existing allowlist.
Unmodified arrow/Page/Home/End keys scroll the article. Picture controls and the
pan viewport participate in Tab navigation; closing a picture restores article
focus. Both reading and picture viewports show focus borders.

Narrow Contents closes on Escape or topic/result selection and returns focus to
the article. Find reveals its field when Contents is open. A **Close Help** button
provides a keyboard path back to work, including beside Rules/Reports decision
dialogs without using those dialogs' Escape action. The keyboard reference now
explains these controls. No financial command, storage, schema or theme colors
changed.

The new tests use actual key events after setup, without programmatically moving
focus between Help controls. They cover wide/narrow windows in light/dark modes,
forward/reverse traversal, topic/result activation, history, all text-size
controls, find next/previous, article scrolling, forward/reverse links, picture
opening/zoom/fit/pan/closing, welcome-screen entry, traversal to the last of
seventeen Contents topics, and return to the focused Stay button beside both
Rules and Reports. Setup uses a nonexistent temporary welcome path and synthetic
documents. Dark mode is requested only in the test process; system settings are
unchanged. Offscreen Qt does not implement color-scheme overrides, so its two
dark cases explicitly skip and run separately on native Cocoa.

Verification on macOS, rules-enabled Release, Qt 6.12.0:

- Rebuilt the application and Help/catalog/Rules test targets; `build/SpentIt`
  contains the keyboard changes and updated reference.
- Final native `keyboardHelp keyboardContextAndModal` passed **8/8 including
  init/cleanup in 19011ms**, with light/dark wide/narrow cases and both decision
  dialogs. Native fixture captures of articles, search, tables, links, focus and
  pictures were visually inspected. No capture was added to the shipped manual.
- Full preparation/catalog/QML/Help UI/Rules UI CTest regression passed **5/5
  in 499.44s**. Help UI reported **56 passed, 0 failed, 2 skipped in 473811ms**;
  the skips are the two explicitly native-only dark rows. Rules UI passed
  27/27 including init/cleanup. This full run precedes final test-only
  null-focus guards and stronger result/Enter assertions; application code was
  unchanged, and the refined focused tests were then rerun separately.
- Final refined focused offscreen checks passed **6 with 2 expected dark-mode
  skips in 9357ms**; the refined native pass above covered all eight checks.
- `git diff --check` and 272 relative file-link target checks passed.

Initial checks exposed the missing delegate keyboard focus. Test refinements
wait for ListView layout, initial welcome probing and native window activation;
the temporary welcome path avoids probing a default user location. A first
broad run overlapped a rebuild and was stopped rather than counted. A concurrent
native attempt failed and crashed in the test process; its cause is not proven.
Subsequent assertions guard temporarily absent focus items rather than
null-dereferencing them. Final verification runs are serial against unchanged
executables; the test-process crash is not reported as a user-file failure.

Reproduction:

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests spentit_rule_ui_tests -j 2
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests keyboardHelp keyboardContextAndModal
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
```

The standalone walkthrough and screen-reader checks remain closed as skipped by
the owner's decision. Linux/Windows remain unavailable and unverified. This is
bounded native/offscreen keyboard and visual verification, not a claim of
screen-reader accessibility or all-platform support. No private financial data
was logged or captured; visual checks use only bundled synthetic Help fixtures.

## Help closeout scope — owner direction 2026-11-06

The owner closed the standalone-app walkthrough item by choosing to skip it.
The app was launched, but the computer-use tool could not attach to the standalone
executable and native Accessibility access was unavailable. This item is closed
as **skipped by owner**, not a passing standalone verification. No financial data
was read, logged or captured during that attempt.

The owner also chose to skip screen-reader verification. Linux/Windows checks
remain unverified and unavailable in this environment. These limits do not imply
verified accessibility or portability.

A source audit of `tests/ui/help_ui_tests.cpp` confirms only partial keyboard
coverage: image arrow/Escape behavior, Help Escape, a dialog Help shortcut,
focus return and modal protection. Existing tests also use pointer clicks,
programmatically assigned focus/text and direct navigation calls. They do not
establish an end-to-end keyboard-only path through all Help controls. Full Tab/
Shift+Tab traversal, keyboard result/link/image activation, scrolling, find,
narrow navigation and return focus still need a focused test pass. Dark styling
exists in the shared theme, but the Help suite has no explicit dark-appearance
case. Keyboard-only and dark-appearance verification are the remaining proposed
local closeout checks. No new runtime verification was performed for this audit.

## Help search excerpts — 2026-11-06

Under the owner's Go, bundled search text now removes heading, emphasis, list,
quote and table formatting while retaining link labels and image descriptions.
Code examples and escaped punctuation retain their literal symbols, including
regex asterisks, underscores, pipes and backslashes. Article Markdown is unchanged.
Excerpts extend the existing 40-character context on each side to whitespace
boundaries, so long words, punctuation and combining characters stay together.
The matching text remains highlighted by the existing escaped-text UI; title and
keyword ranking and topic navigation are unchanged.

Verification on macOS, rules-enabled Release, Qt 6.12.0:

- Rebuilt `spentit_import_experiment`, `spentit_help_tests` and
  `spentit_help_ui_tests`; `build/SpentIt` contains the change.
- `python3 tests/ui/help_prepare_tests.py` passed 9/9. The added case checks
  formatting removal, linked image descriptions and preservation of code symbols.
- Preparation, catalog and QML CTest checks passed 3/3 in 1.70s. New catalog
  coverage checks both excerpt edges, long tokens, accented/combining text,
  case-insensitive matching and keyword-only fallback.
- Focused offscreen Help passed 12/12 including init/cleanup in 9772ms: sample
  search/highlighting and all nine quick-reference lookup/navigation cases.
- The same focused native Cocoa tests passed 12/12 in 184829ms with desktop
  access. The initial sandboxed attempt aborted before showing a window because
  Qt reported no screens available; the authorized native rerun passed.
- `git diff --check` and 265 relative file-link target checks passed.

No private data was used. Full Help/Rules suites, standalone-app walkthrough,
accessibility, dark appearance and Linux/Windows were not rerun for this change;
viewer focus and modality code were not changed.

Reproduction:

```sh
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.qml)$' --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests sampleViewerBeforeAFileIsOpen quickReferenceLookup
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests sampleViewerBeforeAFileIsOpen quickReferenceLookup
```

## Contextual Help — 2026-11-06

Under the owner's Go, Help now opens the existing manual from the welcome
screen, workspaces, 33 application dialogs, Rules/Reports leave windows and
problem messages. **SpentIt Help** retains the current reading; **Help for this
screen**, workspace/dialog **Help**, and the platform Help shortcut select the
current task. Ledger **Search help** opens examples. Error links open instructions
without retrying a command. Native file choosers are unchanged.

Targets are fixed bundled identifiers, not financial values or error payloads.
Referenced sections have explicit stable anchors. CMake watches the two QML
sources and validates every literal context target before bundling; preparation
tests reject missing topics, missing sections and outside paths. The viewer
retains its runtime allowlist and missing-target explanation.

Native tests exposed focus-loss behavior that first normalized an unfinished
amount expression and then started a category preview while opening Help.
The Help/menu focus guard now suspends amount normalization, category-completion
reset and category autosave while reading Help. It accounts for Qt treating a
transient parent as active while Help has input, restores the preceding editor
and keeps normal explicit saves working. Rules and Reports remain
`Qt.ApplicationModal`, with Help in the same process and existing transient-parent
handling. No financial command, schema or storage policy changed.

Verification on macOS, rules-enabled Release, Qt 6.12.0:

- Rebuilt the application, Help UI/catalog and Rules UI targets; `build/SpentIt`
  contains contextual Help.
- The full five-check CTest selection passed **5/5 in 498.35s**, including Rules
  UI, Help UI, preparation, catalog and QML smoke. Full offscreen Help passed
  **52/52 including init/cleanup in 472087ms**.
- Final target corrections (transfer Link versus duplicate Merge, bulk changes
  and Restore imported values) and a welcome-test setup correction were followed
  by another rebuild. Final preparation/catalog/QML checks passed **3/3 in
  1.99s**, and the focused offscreen context checks passed **5/5 including
  init/cleanup in 5447ms**.
- Final full native Help passed **52/52 including init/cleanup in 525002ms**.
  This includes both continuous walkthrough formats and all three new context
  checks, plus the existing Rules/Reports modality and reading/lookup checks.
- Native unsaved-dialog and contextual article captures were visually inspected.
  No screenshots were added to the shipped manual. `git diff --check` and
  467 relative file-link target checks passed. The manual now explains the
  Help entry points; existing screenshots precede these additional controls.

The context checks exercise actual QML pointer clicks, a dialog Help shortcut,
all workspace targets, unsaved transaction text and amount expressions, a category
draft, account rename, Rules/Reports leave dialogs and genuine search/open errors.
They compare model state, preserve drafts and decisions, check return focus and
confirm that explicit category Save via Enter still works. All 33 instantiated
dialog targets resolve. This is a synthetic QML harness, not a standalone-app
walkthrough or a claim that every dialog was clicked.

Initial restricted tests lacked application-data lock access; authorized runs
used the required access. A draft-array assertion was corrected to compare QML
array contents. A welcome error check initially raced the screen's delayed probe;
it now edits the visible path field and waits for that probe. One native menu
attempt aborted the test harness: macOS native menu items have no QQuickWindow
for QtTest mouse events. That unsupported pointer subcase now runs offscreen
only; the native suite checks workspace/dialog actions and focus return. This
was a test-process crash with temporary synthetic files, not a SpentIt user-file
crash. Native system-menu clicking remains unverified.

Reproduction commands:

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests spentit_rule_ui_tests -j 2
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests contextHelpPreservesWorkspaceAndEditor contextHelpPreservesCategoryDraft contextHelpDialogAndErrorActions
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.qml)$' --output-on-failure
```

No private data was used. Native chooser interaction, standalone-app walkthrough,
complete keyboard-only/screen-reader use, dark appearance and Linux/Windows
remain unverified. The search-excerpt roughness observed at this checkpoint is
addressed by the subsequent search-excerpt change above.

## Whole-manual continuity and acceptance replay — 2026-11-06

Under the owner’s Go, all seventeen topics were audited for continuity,
terminology, links, decisions and screenshot consistency. Independent review
visually checked all 38 shipped screenshots against their captions and nearby
instructions; no amount/caption mismatch was found. The introduction and Review
now explain that pictures show different stages, so a reader keeps correct
entries rather than changing them to match a picture. Reports specifies the two
February purchases and links to entry instructions. The outline no longer says
the quick references are pending. No shipped screenshots changed.

The new continuous QML walkthrough starts with an empty password-protected file
and starter categories. It creates accounts/openings, imports the purchase and
refund, explicitly keeps the later-imported duplicate during Merge, links the
payment, adds and categorizes the Corner shop split, checks search/error
retention, saves a rule with no eligible existing matches, accepts January work,
and imports February work to exercise that saved rule. It verifies January
56.18 and February 70.00, the 12.00 split contribution, date-edit/Undo changes,
and a restored backup with exact balances, saved reports and two new entries.
Both YYYY-MM-DD/comma-dot and DD/MM/YYYY/dot-comma cases are covered. Additional
lookup rows click from backup, split and stale-report messages to their tasks.

This uses actual QML forms, pointer clicks, Enter keys and injected text/combo
values. File-picker paths are submitted through model commands; native chooser
typing and a manual standalone-app session are not claimed. Initial harness
failures were corrected: delayed category-editor creation, selected-account
control naming and updating the welcome path to match the submitted backup.

The walkthrough exposed a real current-format verification defect: Merge can
point an earlier batch’s duplicate at a later unaccepted survivor, then dataset
acceptance publishes both atomically. Replay wrongly required the target to
have appeared earlier in row/batch order. The owner separately gave Go to fix
and verify it. Replay now accepts an included target from the same publication
or dataset-acceptance parent, retaining normal snapshot/reference/financial
checks and rejecting a target from an independent future acceptance. No stored
transactions, schema, matching policy or acceptance ordering changed. See the
[atomic acceptance explanation](import-commit-discussion.md#atomic-acceptance-replay--correction-2026-11-06).

The deterministic storage regression first failed with “Acceptance precedes its
duplicate target” (0.33s), then passed after the correction. It covers a later
batch survivor, a later same-batch ordinal through explicit ledger Merge, and
crossed batch dependencies. Each checks verify/reopen/restored backup, idempotent
retry, write/sync failures and abrupt exits before/after durable commit. A
separate future legacy publication still fails verification. An initial
same-batch fixture incorrectly expected an automatic suggestion; it was changed
to explicit Merge without widening matching.

Release results: the focused continuous offscreen walkthrough passed
4/4 including init/cleanup in 22844ms. The storage Review/acceptance selection
passed 3/3 in 139.52s, with acceptance-order checks taking 133.21s and sweeping
165 write/sync failure positions in each of three scenarios. Related review
state, committed basics/history/review/dependencies/exact/backup/validation,
Help preparation and catalog checks passed 10/10 in 17.42s. Rules-disabled
Release acceptance-order checks passed 1/1 in 117.95s, with 157 failure positions
per scenario.
ASan/UBSan Debug acceptance-order checks passed 1/1 in 336.77s, with 165
failure positions per scenario and no sanitizer diagnostics.

The first full native Help run passed 47 cases and failed two new lookup cases
in 543267ms. Both continuous scenarios, all existing chapter walkthroughs,
reading, scrolling and Rules/Reports modal Help passed. The failures were test
clicks below the Search viewport. The helper now scrolls Search with wheel events, waits for movement to stop,
checks click bounds, and sends the actual click; Contents is positioned at the
intended row. Intermediate reruns exposed delegate-layout changes and a click
consumed while the list was still moving. A diagnostic capture and moving=true
trace confirmed the latter; the final focused native pair passed 4/4 including
init/cleanup in 6974ms; the same offscreen pair passed 4/4 in 41693ms.
Final verification passed all five CTest checks (Help UI, Rules UI, QML smoke,
Help preparation and catalog). The full offscreen Help suite passed 49/49 in
89152ms. All nine native lookup cases then passed, 11/11 including init/cleanup
in 13688ms.
The expanded 49-case offscreen suite has a 600-second timeout instead of the
old 120-second prototype allowance; no application timing or behavior changed.
Native report/restored-backup captures for both formats were inspected and
independently reviewed. They preserve balances and all four category amounts.

Reproduction commands (the targets were rebuilt in several invocations):

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests spentit_rule_ui_tests spentit_review_sheet_tests spentit_import_review_state_tests spentit_committed_tests -j 2
ctest --test-dir build/import-ui-rules-release -R '^import\.(acceptance_order|review_sheet|review_batch)$' --output-on-failure
ctest --test-dir build/import-ui-rules-release -R '^(import\.review_state|committed\.(basics|history|review|dependencies|exact|backup|validation)|help\.(prepare|catalog))$' --output-on-failure
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests quickReferenceLookup
ctest --test-dir build/category-rules-off -R '^import\.acceptance_order$' --output-on-failure
ctest --test-dir build/category-sanitized -R '^import\.acceptance_order$' --output-on-failure
```

The rules-off and sanitized test targets were rebuilt before those runs.
Temporary walkthrough captures used `SPENTIT_GUIDE_CAPTURE`; none were bundled.

No private data was used. Full keyboard-only/screen-reader, dark appearance,
standalone-app and Linux/Windows verification remain unclaimed. Application-wide
context links remain separate work requiring their own Go. Search excerpts can
still show Markdown markers or start/end mid-word; this existing presentation
roughness was observed during lookup capture and is a separate polish item.

## Messages and problems quick reference — 2026-11-05

The glossary is committed as `4c12a06`. Under the owner’s next Go,
[Messages and problems](../manual/messages-problems.md) is bundled and was accepted for commit on 2026-11-06. It covers file access and backups, imported records, split totals,
search/rules, stale reports and saved/uncertain changes, with exact message
excerpts and links to supported next steps. Recovery links back to the reference.
Help now bundles seventeen topics and 38 images; no application behavior or
screenshots changed.

A supporting agent checked the current message sources and existing tests;
`import.search` passed 1/1 in 0.01s. Independent prose review clarified that
quoted text can be an excerpt and confirmed the actions and linked instructions.
The catalog check now expects both topics containing “unlock” and checks lookup
of “Unclosed search quote”. The shared reader checks cover the new topic and
actual Search-result/article-link/Back/Contents clicks.

Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
`spentit_help_tests`; `build/SpentIt` includes the reference. Focused native
reading/lookup passed **4/4 including init/cleanup in 43345ms**. Normal, section
and 500-pixel/150% captures were visually inspected and independently reviewed.
Those captures precede the small “excerpt” wording correction; final regression
uses the rebuilt text. The initial restricted native launch failed because the
window server exposed no screens; the run with required access passed.

The final full native Help suite passed **44/44 including init/cleanup in
415871ms**, covering all reading/lookup cases, synthetic chapter walkthroughs,
reverse scrolling and Rules/Reports modal Help.

The five-check CTest selection passed Rules, preparation, catalog and QML
(4/5, 146.54s total). Full offscreen Help hit the previously recorded 120-second
timeout during the existing acceptance reading case, after earlier reading
cases passed; no assertion failure preceded the timeout. The final focused
offscreen reading/lookup rerun passed **4/4 including init/cleanup in 20732ms**.

Commands used the rules-enabled Release tree:

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests -j 2
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests createOpenChapterReading:messages-problems quickReferenceLookup:messages-problems
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests createOpenChapterReading:messages-problems quickReferenceLookup:messages-problems
```

Focused reading used `SPENTIT_GUIDE_READING_CAPTURE` for temporary captures;
none were added to the manual. Preparation validates bundled links, anchors
and images. `git diff --check` and 319 relative file-link target checks across
the touched documentation passed.

No private file was opened. These checks use the actual QML in a synthetic
native harness, not a standalone-app walkthrough. Full keyboard-only,
screen-reader, dark appearance and Linux/Windows verification remain unclaimed.
The next proposed step is the complete synthetic-example
walkthrough and whole-manual consistency check. Application-wide context links
remain separate work requiring their own Go.

## Words used in SpentIt glossary — 2026-11-05

The owner accepted Search examples and requested its commit (`6b1333c`), then
gave Go for [Words used in SpentIt](../manual/words.md). The glossary is bundled
and is owner-accepted for commit. It defines accounts, balances, categories, files and
backups, imports, ledger, Merge/Link, new/accepted, payee/memo, refunds/net
spending, reports, rules, splits, transactions and transfer postings, with
links to the existing instructions. Independent review added the Transaction
entry. No new screenshots or application behavior changed; Help now bundles
sixteen topics and 38 images.

Supporting verification checked current engine/storage behavior and the
existing manual. `engine.reports` passed 1/1 in 0.02s for report signs,
currencies, selected splits and exact reconciliation. No new financial
workflow was introduced. Reader checks add glossary navigation and actual
Search-result/article-link/Contents clicks to the existing shared tests.

Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
`spentit_help_tests`; `build/SpentIt` includes the glossary. Focused native
reading/lookup passed **4/4 including init/cleanup in 43370ms**. Normal and
500-pixel/150% text captures were visually inspected and independently reviewed.
Search-result, article-link, Back and Contents clicks reached the glossary and
opening-balance instructions. The harness uses actual QML with no open file;
it is not a manual standalone-app walkthrough.

The five-check CTest selection passed **5/5 in 101.53s**, including full
offscreen Help in 76.81s, Rules UI, preparation, catalog and QML. The final
full native Help suite passed **42/42 including init/cleanup in 380983ms**,
covering all chapter reading/walkthrough cases, five lookup rows, reverse
scrolling and Rules/Reports modal Help. No failures occurred in this pass;
the previously recorded intermittent offscreen timeout did not recur.

Commands used the rules-enabled Release tree:

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests -j 2
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests createOpenChapterReading:words-used quickReferenceLookup:words-used
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
```

Focused reading used `SPENTIT_GUIDE_READING_CAPTURE` for temporary captures;
none were added to the manual. `git diff --check` and 295 relative file-link
target checks passed. Preparation validates bundled links, anchors and images.

No private file was opened. Full keyboard-only/screen-reader, dark appearance,
standalone-app and Linux/Windows verification remain unclaimed. The next
proposed reference is Messages and problems; it and application-wide context
links require their own Go.

## Search examples quick reference — 2026-11-05

The owner accepted Keyboard and mouse and requested its commit (`8c3c830`),
then gave Go for [Search examples](../manual/search-examples.md). The new
reference is bundled and is owner-accepted for commit. It covers literal words/phrases,
category/date filters, whole-transaction totals, regex and original-record
search, clearing and errors. Under the owner's follow-up permission, the shared
pattern table moved from Rules; Rules retains a link and its PayPal/template
walkthrough. No new screenshots or financial behavior changed. The bundle has
fifteen topics and 38 images.

Supporting verification checked the parser, current query fields and focused
synthetic tests. Independent review checked prose, links, table reuse and
rendering. Amount-search examples were omitted because current search uses
ungrouped decimal-point amounts independently of display formatting.

A real click below the moved table exposed stale Help scroll height after
synchronous Markdown formatting: the link was at y3236 while content height
was 2588. A coalesced deferred layout refresh after assigning the topic, before
restoring scroll, updates height to 3889 and makes the link reachable. The
regression asserts full document height and clicks the rendered link. Reverse
scrolling checks now follow the shared table into Search examples. Help stays
in one process, and Rules/Reports modality is unchanged.

The new walkthrough uses the synthetic Market purchase/refund and Corner shop
split. Setup/import/category resolution use model commands; searches use the
actual search field and Enter, and Escape clears it. It checks words, phrases,
category/date combinations, whole-field/prefix regex, original text, error
retention, the full -20.00 total for a Household split match and unchanged
-56.18 account balance. Early harness runs failed on category-dialog setup,
partially visible Contents clicks and the stale height defect; those failures
were corrected and are not counted as passing runs.

Supporting Release checks passed `import.search` 1/1 (0.02s),
`combinedSearchAndSelection` 3/3 (967ms), and `configuredDatesAndCategories`
3/3 (854ms), with Qt counts including init/cleanup. Initial restricted model
setup failed application-data lock access; the authorized rerun passed.

Rebuilt the application, Help UI/catalog and Rules UI targets in the rules-enabled
Release tree. `build/SpentIt` includes the new reference and layout fix. Focused
native reading, both actual Rules/Search cross-links, search walkthrough and
reverse scrolling passed **7/7 including init/cleanup in 81310ms**. Normal,
table and 500-pixel/150% captures were visually inspected and independently
reviewed. These captures used the initial deferred callback; independent review
then changed it to a named callback to coalesce repeated topic requests.

The final full native Help suite passed **40/40 including init/cleanup in
89603ms**, including every chapter, all four lookup rows, reverse scrolling,
walkthroughs and Rules/Reports modal Help. The five-check CTest selection passed
Rules, full offscreen Help (95.87s), preparation and QML (4/5, 120.85s total);
catalog failed an old refund-search count because the new reference also
contains that word. After updating the expected count, preparation/catalog/QML
passed **3/3 in 1.16s**. The prior intermittent offscreen timeout did not recur.

Commands:

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests spentit_rule_ui_tests -j 2
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests createOpenChapterReading:search-examples quickReferenceLookup:search-examples quickReferenceLookup:rules-pattern-reference searchExamplesWalkthrough longChapterScrollRendering
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
```

Focused captures used `SPENTIT_GUIDE_READING_CAPTURE` with temporary paths;
no captures were added to the manual. The synthetic harness uses actual QML;
it is not a manual standalone-app walkthrough. `git diff --check` and 283
relative file-link target checks passed. Preparation validates bundled anchors
and images. The final targeted rerun selected `help.prepare`, `help.catalog`
and `import.qml`.

No private data was used. Full keyboard-only/screen-reader, dark appearance,
standalone-app and Linux/Windows verification remain unclaimed. Next proposed
reference: Words used in SpentIt; remaining references and context links need
their own Go.

## Keyboard and mouse quick reference — 2026-11-05

Under the owner's Go, [Keyboard and mouse](../manual/keyboard-mouse.md) is
bundled after Where do I find…? and is owner-accepted for commit. It covers selection,
editing and category completion, calendar keys, search/navigation and Help.
Supporting implementation verification and independent prose/render review
followed the accepted workflow. Review added the category Shift+Tab caveat.
No application behavior or screenshots changed; the bundle now has fourteen
topics and 38 images. Where do I find…? is committed as `2ef8158`.

Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
`spentit_help_tests`, updating `build/SpentIt`. Focused native reference
reading and actual Search/article-link/Contents clicks passed 5/5 including
init/cleanup in 63069ms. The final wording's focused offscreen rerun passed
5/5 in 2470ms. Normal and 500-pixel/150% views were visually inspected and
independently reviewed. The first captures preceded the one-sentence category
clarification.

The five-check CTest selection initially passed Rules UI, full offscreen Help,
catalog and QML (4/5, 85.46s); preparation failed because its expected topic
inventory still had thirteen entries. After updating that inventory,
preparation/catalog/QML passed 3/3 in 1.00s. Full offscreen Help passed in
61.20s; the previously recorded intermittent timeout did not recur in this run.

Supporting existing offscreen checks passed: navigation `textBoundaries`
5/5 in 184ms; import UI `accessibilityAndKeyboard qmlEditorNavigation` 4/4 in
1526ms; reports `reportsAndCalendar` 3/3 in 1722ms (counts include init/cleanup).
The first restricted import test failed application-data lock setup; the
rerun with required access passed. These are focused synthetic checks, not
full keyboard-only or accessibility verification.

The final full native Help suite passed **36/36 including init/cleanup in
366517ms**, covering all chapter walkthroughs, reverse scrolling and Rules/Reports
modal Help. Final normal/narrow reference captures were inspected after the
wording change. These checks use actual QML in a native macOS Qt 6.12.0 harness,
not a manual standalone-app walkthrough. `git diff --check` and 275 relative
file-link target checks passed; preparation validates bundled anchors/images.

Commands used the rules-enabled Release tree:

```sh
cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests -j 2
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests createOpenChapterReading:keyboard-mouse quickReferenceLookup
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
```

Native runs used `SPENTIT_GUIDE_READING_CAPTURE` for temporary reading captures;
none were added to the manual. After correcting the preparation inventory,
the targeted CTest rerun selected `help.prepare`, `help.catalog` and `import.qml`.

No private file was opened. Full keyboard/screen-reader, dark appearance,
standalone-app and Linux/Windows verification remain unclaimed. The next
proposed reference is Search examples; remaining references and application-wide
context links require their own Go.

## Where do I find…? quick reference — 2026-11-05

The owner gave Go for the first quick reference. [Where do I find…?](../manual/where-to-find.md)
is bundled after the twelve main chapters and is owner-accepted for commit. It groups
file/settings, account, transaction/category, Review/rules/report and Undo/problem
entry points, with links to the existing task instructions. No new screenshots
or application behavior changes were needed. Supporting code verification and
independent prose/render review followed the accepted Help workflow.

Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
`spentit_help_tests`; `build/SpentIt` bundles thirteen topics and 38 images.
Native focused reading/lookup passed 4/4 including init/cleanup in 40840ms.
Normal and 500-pixel/150% text captures were visually inspected and independently
reviewed. Search and an actual article-link click reached the reference and
backup instructions, with Back returning to the reference. Full native Help
passed 34/34 including init/cleanup in 71257ms, including the existing walkthroughs,
reverse scrolling and modal Help. After that run, review strengthened the
Contents check to navigate from Home before clicking the reference; its focused
offscreen rerun passed 3/3 in 901ms and native rerun passed 3/3 in 1100ms.

The initial restricted native run had no available screens. The first restricted
CTest run passed preparation/catalog/QML but failed Rules and Help walkthroughs
because application-data locks were unavailable, with subsequent test aborts.
These attempts are not passing verification. With the required access, the
five-check CTest selection passed preparation, catalog, QML and Rules (4/5,
144.26s total), but full offscreen Help hit its existing 120-second timeout
during Rules reading. This known intermittent limit remains unresolved; no
viewer timing changes were made. The new focused lookup and full native passes
above do not turn that timeout into a passing offscreen suite. No private file was opened. Full keyboard/screen-reader,
dark-mode, standalone-app and Linux/Windows verification remain unclaimed.
The next proposed reference is Keyboard and mouse; remaining references and
application-wide context links require their own Go. The owner accepted the reference and requested its commit.

See the [Help handoff](help-prototype-handoff.md) for the continuation workflow.

## Busy indicator manual note — 2026-11-05

Under the owner's Go, chapter 2's [Find your bearings](../manual/create-open-file.md#find-your-bearings)
now explains that some actions take time, the spinning arc at the top right
shows when SpentIt is busy, the bottom status line describes the activity, and
the reader should wait before retrying. This replaces the shorter status-line
sentence; no screenshots or application behavior changed.

Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
`spentit_help_tests`; `build/SpentIt` includes the approved paragraph.
`ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog)$' --output-on-failure`
passed 2/2 in 0.30s, validating the bundle, links/images and catalog.
`git diff --check` passed. Native interaction checks were not repeated for this
wording-only change.

## More noticeable activity indicator — 2026-11-05

Implemented under the owner's narrow visual-first Go. The title-row indicator
is now a 36-pixel ring with a five-pixel blue arc, continuously rotating every
850 ms using a render-thread animator. Light/dark theme colours provide stronger
contrast than the previous pale dots. No extra activity text was added. It retains
the current worker busy binding; broader readiness and import-dialog work in
[feature 014](../features/014-busy-feedback-and-progress.md) remains outstanding.

Rebuilt rules-enabled Release `build/SpentIt` and navigation tests without compiler
warnings/errors. Native macOS `spentit_import_navigation_tests activityAnimation`
passed 4/4 including init/cleanup, 39.817 s. Tests isolate the actual toolbar
control with sustained running state, compare three rendered frames per theme,
and check idle/stopped hiding. Captured light/dark frames were visually inspected.
This proves actual rendered movement in the synthetic test, not uninterrupted
animation during the owner's large account/import workload.

Offscreen `import.navigation` and `import.qml` passed 2/2, 59.31 s. The navigation
timeout was raised from 60 to 90 seconds to accommodate the two new rendered-frame
cases (the expanded suite took 57.76 s). The first sandboxed native attempt had
no available screens; the rerun used macOS window access. No private file was
opened. Linux/Windows and heavy-workload busy-state coverage remain unverified.
`git diff --check` and changed-document relative file-link checks passed.

## Help chapter 12 — 2026-11-05

Under the owner's Go, [Look after your work and resolve remaining problems](../manual/backup-recovery.md)
is bundled with two screenshots and is owner-accepted for commit. Supporting verification,
one primary writer/capture owner and independent review followed the accepted
workflow. The chapter covers backup creation/completion, checking and recovering
through a fresh working copy, Settings, password/format limits, opening/write/
uncertain-outcome problems, interrupted imports, quit-check choices and a linked
problem index. The final continuity review clarified chapter 10's pre-payment
example. No application or financial behavior changed.

A new protected-file walkthrough follows Market -42.18, refund +6.00 and the
36.18 payment from an empty file through a 36.18 spending report and recovery.
It verifies balances 963.82 / 0.00, new transfer entries, the same password,
existing-destination refusal, unchanged current-file path after saving copies,
and the restored report. Focused offscreen passed 3/3 including init/cleanup in
5371ms; native capture passed 3/3 in 7266ms. Both source images were visually
inspected and independently reviewed.

Supporting backup/cancellation/recovery checks passed 3/3 in 10.81s; focused
model recovery/outcome/formatting/window checks passed 8/8 in 10851ms including
init/cleanup. The initial restricted recovery run could not create required
application-data locks; the authorized rerun passed. These are selected checks,
not a full new financial fault sweep. The documented Help regression passed
5/5 in 81.77s, including full offscreen Help in 56.59s. Earlier intermittent
offscreen timeouts did not recur in this run; no viewer timing change was made.

Rebuilt `build/SpentIt` and both Help test targets: twelve chapters and 38 images.
Full native Help passed **32/32 including init/cleanup in 280850ms**, including
all chapter walkthroughs, image clicks, navigation, reverse scrolling and modal
Help. Chapter 12's normal, inline-image and 500-pixel/150% text views were
visually inspected and independently reviewed. Actual Search and Contents
clicks reach the new chapter. Preparation validates bundled links/images;
changed-document relative file links and `git diff --check` passed.

The [handoff](help-prototype-handoff.md#chapter-12--2026-11-05) records commands,
harness corrections and capture basis. This uses real QML with synthetic data;
the OS backup picker is opened/cancelled, with destination submission through
the model. No manual standalone-app walkthrough, full keyboard/screen-reader,
dark-mode or Linux/Windows verification is claimed. The main twelve chapters
are written; chapter 12 is owner-accepted for commit. Quick
references and application-wide context links still require their own Go.

## Help chapter 11 — 2026-11-05

The owner accepted chapter 10, committed as `b1c55fc`, and gave Go for
[Answer questions with reports](../manual/reports.md). Chapter 11 is bundled
with four synthetic screenshots and owner-accepted for commit. Supporting
verification, one primary writer/capture owner and independent review followed
the documented chapter workflow. The chapter covers report setup, selection,
periods/calendar, comparisons, signs/currencies, graphs, partial-split details,
editing/Undo, saved-report maintenance and refresh/problems. No financial or
Help viewer implementation changed.

The synthetic walkthrough verifies January 56.18 versus February 70.00, a
20.00 purchase contributing only its 12.00 Groceries split, refreshed period
totals after a date edit and Undo, graph selection and report leave choices.
Offscreen walkthrough passed 3/3 including init/cleanup in 4549ms; native
capture passed 3/3 in 6244ms. Report engine/storage/UI selection passed 3/3 in
16.62s, including exact reconciliation and existing report-save recovery checks.
All four source images were visually inspected and independently reviewed.
The Graph field's existing clipped label and below-window Save controls remain;
the chapter gives full labels and tells the reader to scroll.

Rebuilt the application and both Help test targets; `build/SpentIt` bundles
11 chapters and 36 images. Full native Help passed 30/30 including init/cleanup
in 271744ms, covering image clicks, reading/navigation, 500-pixel/150% text,
existing walkthroughs, reverse scrolling and modal Help. Normal/inline-image/
narrow Chapter 11 views were visually inspected. Preparation, catalog, QML and
Rules UI passed (4/5 checks); full offscreen Help again hit the existing
120-second limit during Rules reading (144.78s total selection). That timeout
remains unresolved. Search, bundled links/images, relative links and
`git diff --check` passed. The
[handoff](help-prototype-handoff.md#chapter-11--2026-11-05) records commands,
capture basis, harness corrections and limitations. This uses actual QML in a
native synthetic harness, not a manual standalone-app walkthrough. Dark-mode,
complete keyboard/screen-reader and Linux/Windows checks remain unverified.
Chapter 12 and context links require their own Go; the owner accepted chapter 11 and requested its commit.

## Help chapter 10 — 2026-11-05

Under the owner's Go, [Accept transactions — now or later](../manual/accept-transactions.md)
is bundled with three synthetic screenshots and owner-accepted for commit. The
accepted workflow used supporting implementation/test verification, one primary
writer/capture owner and independent review. The chapter covers all-account
acceptance, warnings/blockers, failed imported records, quit choices, saved new
transactions and the differences between accepting, editing and removal.
No financial or Help viewer behavior changed.

Rebuilt the three application/Help targets; `build/SpentIt` contains ten chapters
and 32 images. Final full native Help passed 28/28 including init/cleanup in
242679ms. Chapter 10 checks navigation, actual image clicks/enlargement/return,
and its comparison table at 500-pixel width and 150% text. Source screenshots
and normal/inline/table/narrow rendered views were visually inspected.
The focused source walkthrough passed offscreen 3/3 in 3054ms and native capture
3/3 in 4070ms. Supporting acceptance/outcome UI checks passed 5/5 in 3816ms;
storage acceptance/rollback/import-Undo/manual selection passed 4/4 in 50.55s.

The documented CTest selection passed rules UI, catalog and QML, but full
offscreen Help hit its existing 120-second limit. Preparation initially found
an outdated chapter/image count; after correction, preparation/catalog passed
2/2 in 0.37s. The full offscreen timing limit remains unresolved. Relative file
links and `git diff --check` passed. The existing quit dialog shortens the
“Quit without accepting” label; the chapter states its full text.

The [handoff](help-prototype-handoff.md#chapter-10--2026-11-05) records commands,
capture basis, harness corrections and verification limits. All data was
synthetic; no manual standalone-app walkthrough, full financial fault sweep,
dark-mode, complete keyboard/screen-reader or Linux/Windows verification is
claimed. Chapter 11 and context links require their own Go; the owner accepted chapter 10 and requested its commit.

## Help regex wording clarification — 2026-11-05

The owner's three approved cheat-sheet clarifications are bundled in
`build/SpentIt`: “0 through 9”, the explicit Market-and-space match in
Market 1234, and the simpler `CARD[0-9]{4}$` example. Application/Help targets
rebuilt; preparation/catalog passed 2/2 in 0.23s and `git diff --check` passed.
This is a wording-only update; native interaction tests were not repeated.

## Help long-chapter rendering correction — 2026-11-05

The owner's reverse-scroll blank-text report was reproduced with native pixel
comparisons. Help bypasses Qt 6.12's viewport-limited text rendering for articles
containing tables, retaining Flickable clipping. Paragraph formatting is batched,
unchanged formats are skipped, and deferred layout finishes on the GUI thread.
The tradeoff is a complete scene graph for the finite table article. No financial
behavior, manual content or modal-dialog protection changed.

The local Rules selection measurement improved from 283ms to 153–161ms including
a fixed 100ms settling wait. The new regression checks visible initial text,
reverse-scroll pixels, wheel input, Contents navigation and saved nonzero scroll
at normal and narrow/larger-text sizes. See the
[reproduction and correction record](help-prototype-handoff.md#long-help-chapter-rendering-correction--2026-11-05).

Rebuilt `build/SpentIt` and Help tests. Final full native Help passed 26/26
including init/cleanup in 253250ms; the final focused offscreen pixel test passed
3/3 in 23312ms. The five-check regression passed rules UI, preparation, catalog
and QML, but the full offscreen Help test again exceeded 120 seconds after the
chapter-reading checks. That suite timing limit remains unresolved. Full-native
Rules selection timings varied to 197–220ms including the fixed settling wait;
the earlier focused timing is not a general latency guarantee. Independent review,
relative file links and `git diff --check` passed. Linux/Windows and screen readers
remain unverified.

## Help chapter 9 regex reference — 2026-11-05

Under the owner's follow-up Go, the chapter now ends with a common-pattern
cheat-sheet, worked PayPal/eBay capture cleanup and bank-reference/template
examples. Temporary probes against the actual regex library verified all example
patterns and results. Independent review checked accuracy and novice wording.
All three application/Help targets rebuilt. Native Help passed 25/25 in 66837ms
including init/cleanup; normal and narrow/larger-text table captures were visually
inspected. CTest passed regex, rules UI, preparation, catalog and QML checks,
but full offscreen Help timed out at 120 seconds both alongside the native run
and on an isolated retry; all chapter-reading cases passed before each timeout.
That offscreen suite timing issue remains unresolved. No financial/viewer behavior
or source screenshots changed. See the
[follow-up record](help-prototype-handoff.md#chapter-9-regex-cheat-sheet-follow-up--2026-11-05).

## Help chapter 9 — 2026-11-05

Under the owner's Go, [Use rules for repeated categorization](../manual/rules.md)
is bundled with three synthetic screenshots and owner-accepted for commit. The
accepted workflow used supporting verification, one primary writer/capture owner
and independent review. It covers category-gated rules, original imported/current
manual matching, split-first order, selected Save/Apply, scope, automatic import
use, Undo and leave choices. Review corrected the preview capture and clarified
eligible matches, scenario continuity and selection before saving. No financial
or Help viewer implementation changed.

The three rules-enabled Release application/Help targets rebuilt to `build/SpentIt`,
with nine chapters and 29 images. The five-check Help regression passed 5/5 in
60.71s. Native Help passed 25/25 including init/cleanup in 194615ms.
Chapter 9 checks previous/next and section navigation, all three actual image
clicks/loading/Actual size/close/scroll return, and 500-pixel reading at 150% text.
A separate native `createOpenChapterReading:rules` capture run passed 3/3 in
21650ms; normal, inline-image and narrow/larger-text rendered views were visually
inspected. Focused Rules UI passed 8/8 including init/cleanup in 6330ms; engine/storage
selection passed 6/6 in 5.05s. The source walkthrough first passed offscreen 3/3
in 3246ms; the final native capture passed 3/3 in 4842ms. It checks preview,
Save and apply, Undo, saved-rule Apply, category eligibility and leave Stay/Discard.
The first preview capture clipped the matching row despite a passing walkthrough;
visual review caught it and the corrected harness asserts row visibility.
All three final images were inspected and independently reviewed.

The [handoff](help-prototype-handoff.md#chapter-9--2026-11-05) records commands,
synthetic examples and capture basis: actual QML in a native macOS Qt 6.12.0 Basic
light harness, temporary files/settings, 1440 × 900 logical pixels at 2×, dialog
captured separately. This is not a manual standalone-app walkthrough. Search and
bundled links/images passed; relative file links and `git diff --check` passed.
Dark appearance, full keyboard/screen-reader use, Linux and Windows remain
unverified. Chapter 10 and application-wide context links require their own Go.

## Help chapter 8 — 2026-11-05

The owner accepted chapter 7, committed as `2b9c41e`, and gave Go for chapter 8.
[Organize spending with categories](../manual/categories.md) is bundled with
four screenshots and is owner-accepted for commit. The documented workflow used supporting
verification, one primary writer/capture owner and independent review. The chapter
covers independent category names, management, inline completion, selected/split
assignment, account destinations and eligible Undo. Review corrected a report
selection claim and the synthetic opening balance. No financial or Help viewer
implementation changed.

The three application/Help targets rebuilt to `build/SpentIt`, with eight chapters
and 26 images. The five-check Help regression passed 5/5 in 55.79s; native Help
passed 23/23 including init/cleanup in 184933ms with no further crashes. Chapter 8
checks cover all four image clicks/loading/enlargement/return, navigation and
500-pixel reading at 150% text. Search and bundled links/images passed; relative
file links and `git diff --check` passed.

The source capture walkthrough passed 3/3 in 6578ms; the repaired harness first
passed offscreen 3/3 in 5194ms. Initial test-harness failures caused macOS crash
dialogs noticed by the owner: incorrect account-selection setup, missing dropdown
lookup and reading a selection cleared by saving. The harness assumptions were
corrected and checked offscreen before native captures resumed. All runs used
isolated synthetic files/settings. All four final source images and normal/inline-image/narrow rendered views were
inspected. The separate native reading capture passed 3/3 in 23703ms.

Supporting category UI, navigation/assignment and storage checks passed 6/6
in 13951ms, 5/5 in 2623ms and 5/5 in 10.40s. QtTest counts include init/cleanup;
the scrolling test retained two mouse-outside-window warnings. See the
[handoff](help-prototype-handoff.md#chapter-8--2026-11-05) for exact commands,
results and capture basis: actual QML in a native macOS Qt 6.12.0 Basic light
harness, 1440 × 900 logical pixels at 2×, not a manual standalone-app walkthrough.
The existing “subcategory” hint and source-0/source-1 split labels are recorded
presentation issues, preserved in screenshots. Dark appearance, full keyboard
and screen-reader use, Linux and Windows remain unverified. Chapter 9 and context
links need their own Go.

## Help chapter 7 — 2026-11-05

Under the owner's Go, [Add and correct transactions](../manual/add-correct-transactions.md)
is bundled with four synthetic screenshots and is owner-accepted for commit. Chapter 6
is accepted and committed as `a8a2739`. The documented workflow used supporting
verification, one primary writer/capture owner and independent review. Corrections
clarify category-saving for new versus existing entries, Restore routes, deletion
confirmations and example continuity. No financial or Help viewer behavior changed.

The three rules-enabled Release application/Help targets rebuilt to `build/SpentIt`.
The five-check Help regression passed 5/5 in 127.29s. Native Help passed 21/21
including init/cleanup in 30784ms. Chapter checks exercise creation, calendar
selection, balanced splits, mismatch Keep editing, unsaved Stay/discard and saved
correction Undo; image clicks/loading/enlargement/return, section navigation and
500-pixel width at 150% text also pass. The refreshed source capture run passed
3/3 in 4627ms. A rendered-reading capture run passed 3/3 in 22192ms. All four
source screenshots and normal/inline-image/narrow rendered views were visually
inspected. Focused
offscreen editing, manual-entry and committed-correction checks passed 10/10,
3/3 and 5/5, respectively. Relative links and `git diff --check` passed.

The [handoff](help-prototype-handoff.md#chapter-7--2026-11-05) records commands,
results and capture basis: actual QML in a native Qt harness, isolated synthetic
files/settings, macOS Qt 6.12.0 Basic light theme, 1440 × 900 logical pixels at 2×.
This is not a manual standalone-app walkthrough. Dark chapter appearance, complete
keyboard/screen-reader operation, Linux and Windows remain unverified. Chapter 8,
Organize spending with categories, and context links need their own Go.

## Help chapter 6 — 2026-11-05

The owner accepted chapter 5, committed as `bbd3a8b`, and gave Go for chapter 6.
[Review duplicates and link transfers](../manual/review-duplicates-transfers.md)
is bundled with four synthetic screenshots, accepted by the owner for commit. The accepted
workflow used supporting verification, one primary writer/capture owner and an
independent reviewer. No financial behavior or Help viewer implementation changed.
The chapter explains Merge versus Link, survivor/manual-entry choices, batch
selection, generated counterparts, differences, exclusions and eligible Undo.

The three rules-enabled Release application/Help targets rebuilt to `build/SpentIt`.
The documented five-check Help regression passed 5/5 in 103.21s. Native Help passed
19/19 including init/cleanup in 45829ms. New checks exercise actual Merge/No/Undo,
exact Link, transfer-arrow navigation, amount-difference Cancel/Link/Undo, all four
image clicks/loading/enlargement/return, section navigation and narrow reading at
150% text. The initial native capture walkthrough passed 4/4 in 7326ms; a separate
rendered-reading capture passed 3/3 in 2542ms. Source images and normal/narrow Help
captures were visually inspected. Focused offscreen Review checks passed 12/12 in
9929ms, and policy/state/manual-transaction checks passed 3/3 in 42.62s. Relative
file-link checks and `git diff --check` passed.

The [handoff](help-prototype-handoff.md#chapter-6--2026-11-05) records commands,
scenarios and limits. Captures use actual QML in a native Qt harness, isolated
synthetic files, macOS Qt 6.12.0 Basic light theme, 1440 × 900 logical pixels at 2×.
This is not a manual standalone-app walkthrough. Dark chapter appearance, full
keyboard/screen-reader operation, Linux and Windows remain unverified. Chapter 6
is owner-accepted for commit; chapter 7 and context links need Go.

## Help chapter 5 — 2026-11-05

Under the owner's Go, [Find your way around transactions](../manual/find-transactions.md)
is bundled with three synthetic screenshots. The documented workflow used a
verification agent, one primary writer/capture owner, and an independent reviewer.
The chapter covers ledger reading, signs and balances, selection versus editing,
search/sort/clear, splits, navigation and Review orientation. No financial behavior
or Help viewer implementation changed. The owner accepted chapter 5 and requested its commit, then gave Go for
chapter 6. Context links remain separate.

The rules-enabled Release application and Help targets rebuilt to `build/SpentIt`.
The five-check Help regression passed 5/5 in 38.95s. Native Help passed 16/16
including init/cleanup in 73435ms, including Chapter 5's three real image clicks,
image loading/enlargement/return, section navigation, and 500-pixel width at 150%
text. Source images and normal/narrow rendered chapter views were visually
inspected. The standalone synthetic capture walkthrough passed 3/3 in 5115ms. Focused
offscreen search/selection/Review checks passed 7/7 in 6539ms, and navigation
checks passed 6/6 in 5994ms. Counts include init/cleanup. Relative file links and
`git diff --check` passed.

See the [handoff](help-prototype-handoff.md#chapter-5--2026-11-05) for commands,
initial harness failures, capture basis and limits. Captures use real QML in a
native Qt harness with temporary synthetic data: macOS Qt 6.12.0 Basic, light,
1440 × 900 logical pixels at 2×. This is not a manual standalone-app walkthrough.
Dark chapter appearance, complete keyboard/screen-reader operation, Linux and
Windows remain unverified.

## Help chapter 4 — 2026-11-05

The owner accepted chapter 3, committed as `467a20e`, and gave Go for chapter 4.
[Bring in transactions](../manual/import-transactions.md) is now bundled and owner-accepted,
with five real-screen synthetic screenshots covering destination, account
mapping, category choices, imported transactions and whole-import Undo.
Verification and independent review agents supported the primary writer/capture
owner under the documented chapter workflow. No financial behavior or Help viewer
implementation changed.

The rules-enabled Release application and Help targets rebuilt to `build/SpentIt`.
Focused offscreen import checks passed 13/13 including init/cleanup in 11735ms:
routing, ambiguous dates, opening choices/invalid reported balances, multi-account
and mapped-transfer imports, amount/record repair, cancellation/mapping persistence,
selected import Undo and outcome failures. The native synthetic import walkthrough
passed 3/3 in 5171ms. It checks both a repeated import without balance changes and
whole-import Undo preserving an earlier salary import and account openings.
The documented five-check Help regression suite passed 5/5 in 35.27s. Native
Help passed 14/14 including init/cleanup in 72681ms. All five chapter images
were clicked, loaded, enlarged and closed with scroll return checked. Navigation,
section links, QFX search, normal width and 500-pixel width with 150% text passed.
Source screenshots and normal/image/narrow rendered chapter captures were
visually inspected. Relative file links and `git diff --check` passed.

The [handoff](help-prototype-handoff.md#chapter-4--2026-11-05) records reproducible
commands and capture details. Screenshots use macOS Qt 6.12.0 Basic, light theme,
1440 × 900 logical pixels at 2×, and temporary synthetic data. The native file
picker is bypassed; mapping fields/choices are supplied through Qt, followed by
real mouse clicks on Import, Apply Selected and Undo. This is not a manual
standalone-app walkthrough. Native file-picker use, dark chapter appearance,
complete keyboard/screen-reader operation, Linux and Windows remain unverified.
The owner accepted chapter 4 and requested its commit. Chapter 5 and context
links need their Go.

## Help chapter 3 — 2026-11-05

Under the owner's chapter-3 Go, [Set up your accounts](../manual/accounts.md)
is bundled after chapters 1 and 2, with three real-screen synthetic screenshots.
The accepted workflow used a verification agent, one writer/capture owner, and
an independent review agent. The review's deletion and archive guidance findings
were resolved. Account behavior and the Help viewer implementation are unchanged.

The chapter covers creation, opening balances, account selection/balances,
reordering, retained names, rename/archive/reactivate/delete, currency reassignment,
and eligible Undo. `build/SpentIt` was rebuilt with the new chapter for owner
review. The [handoff](help-prototype-handoff.md#chapter-3--2026-11-05) records the
capture inventory, reproducible commands, workflow and verification limits.

The documented five-check Help regression selection plus `import.ui`,
`import.navigation`, and `import.category_ui` passed 8/8 in 162.87s.
The three application/Help build targets succeeded. Relative file-link checks
and `git diff --check` passed.
Native Help passed 12/12 including init/cleanup in 49542ms on macOS Qt 6.12.0.
Real mouse and keyboard events exercised account creation, switching, opening
editing, menus, currency reassignment/Undo and archive/reactivation. Help checks
cover navigation, section links, actual image clicks, loaded images, Actual size,
scroll return, normal width and 500-pixel width with 150% text. All three source
images and rendered normal/narrow chapter views were inspected.

This is a native Qt harness walkthrough using temporary synthetic data, not a
manual standalone-app walkthrough. Dark chapter appearance, complete keyboard-only
use, screen readers, Linux and Windows remain unverified. Chapter 3 was then
accepted and committed; chapter 4 received Go, as recorded above.

## Manual continuation workflow — 2026-11-05

Documented the owner's accepted approach: sequential chapters with one primary
writer, supporting verification and independent review agents, real interaction
checks, and owner review before continuing. Chapter 3, Set up your accounts, is
the next fresh-session task at that handoff; its subsequent Go and implementation
are recorded above. The
[handoff](help-prototype-handoff.md#continuing-the-manual--2026-11-05) lists the
reading context and links to the full workflow. This is documentation-only work;
relative file links and `git diff --check` were checked, with no runtime changes.

## Help picture clicks — 2026-11-05

Fixed the owner's report that chapter screenshots did not open when clicked.
Qt Markdown had dropped the image anchor; the document formatting pass now
restores each allowlisted image's `help-image:` link. The previous tests called
the viewer function directly and did not establish mouse-click behavior.
The replacement regression reproduced the failure and now clicks all three
rendered chapter screenshots, verifies image loading, and checks return position.

Rebuilt `build/SpentIt` and Help tests in rules-enabled Release on macOS,
Qt 6.12.0. The five-check regression suite passed 5/5 in 27.77s; native Help UI
tests passed 10/10 including init/cleanup in 24346ms. `git diff --check` passed.
Linux, Windows, and a manual application walkthrough remain unverified.
See [the correction](help-prototype-handoff.md#picture-click-correction--2026-11-05).

## Help chapter 2 — 2026-11-05

The owner's Go for the next chapter is implemented. Help now bundles
[Create or open your file](../manual/create-open-file.md) after the introduction,
with three synthetic screenshots of setup, the first workspace, and unlocking.
The owner's concise/plain-English writing direction is recorded in the language
guide. Full-size screenshots exposed inline overflow; images now fit the article
without stretching or losing their full-size source. Contents selection colors
are explicit so the selected chapter remains readable. Manual source/image
changes regenerate the bundle during the build.

Rules-enabled Release on macOS, Qt 6.12.0: application and Help targets rebuilt
successfully to `build/SpentIt`. The five-check Help regression command passed
5/5 in 27.90s; native Help UI tests passed 10/10 including init/cleanup in
24211ms. New checks exercise create/close/wrong-password/reopen and chapter
navigation, section linking, three image sources, image-view return, and narrow
reading at larger text sizes. Source screenshots and normal/narrow rendered
captures were visually inspected. `git diff --check` passed.

The walkthrough uses the real QML screens in the Qt harness with isolated
synthetic data. Native file-picker interaction, a manual `./build/SpentIt`
walkthrough, full keyboard/screen-reader checks, dark chapter appearance, Linux,
and Windows remain unverified. See [the chapter record and image inventory](help-prototype-handoff.md#chapter-2--2026-11-05).

## Approved Help introduction voice — 2026-11-05

The owner approved the replacement chapter and requested installation in
`manual/index.md` and a rebuilt application. The chapter now begins with a
familiar end-of-month question, walks through purchase/refund/card-payment
sections, and introduces categories and reports through the reader's questions.
The approved text is bundled into `./build/SpentIt`; viewer behavior is unchanged.
Existing introduction checks now use the new heading, one list, and
whitespace-normalized opening text to tolerate Qt Markdown line wrapping.

Verification on macOS, rules-enabled Release, Qt 6.12.0:

- Built `spentit_import_experiment`, `spentit_help_ui_tests`, and
  `spentit_help_tests`; the application linked to `build/SpentIt`.
- The five-check Help regression command documented in
  [the handoff](help-prototype-handoff.md#build-and-check) passed 5/5 in 26.45s.
- Native `spentit_help_ui_tests` passed 8/8 including init/cleanup in 2764ms.
- Initial sandbox runs could not access the display or application-data lock
  directory; reruns with that access passed. The opening-text assertion also
  needed whitespace normalization before the final passing runs.

No manual visual walkthrough, Linux, or Windows check was performed. Restart
an already-running application to load the new bundled chapter.

## Help introduction reading — 2026-11-05

After reading the rendered introduction, the owner asked for space between
pieces and for plainer, more human wording. Qt Markdown was leaving every
block margin at zero, so paragraphs and headings sat together. The viewer now
adds paragraph, list, and heading space, opens the line height slightly, and
stops the article line at 36 times the text size. The chapter was rewritten in
short pieces, starting from “Where did the money go?” The same facts remain:
the household example, Import…, + New transaction, Accept transactions,
transactions not yet accepted, and a recorded transfer left out of report
spending.

Rules-enabled Release on macOS, Qt 6.12.0. Rebuilt `./build/SpentIt`.

- `ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure` passed 5/5, 26.01s. `import.help_ui` is offscreen and passed in 1.98s.
- `./build/import-ui-rules-release/src/ui/spentit_help_ui_tests` with no `QT_QPA_PLATFORM` passed. QtTest reported 8 passed, 0 failed, 2878ms, including init and cleanup. The introduction check requires a gap after the 42.18 line, a larger gap before “Two ways to begin”, two lists, and a shorter article line in a 1400-pixel window. The sample viewer check and the Rules and Reports leave-dialog checks still passed.

Linux, Windows, and a person looking at the rewritten page in `./build/SpentIt`
were not part of this run. Reopen the application to see it. Details are in
[the handoff](help-prototype-handoff.md).

## Help introduction — 2026-11-05

The owner gave Go for the first real chapter after the viewer prototype. Help
now opens **Understand your spending** from `manual/index.md`. Sample topics
moved to `tests/fixtures/help/` and stay out of the Help window. The chapter
uses one household example: Chequing, a credit card, and Cash; a 42.18 Market
purchase on 2026-01-15; a 6.00 refund on 2026-01-20; and a 36.18 card payment
from Chequing on 2026-01-28. It names Import…, + New transaction, and Accept
transactions. It states that transactions not yet accepted stay in the file and
count in balances and reports, and that a recorded transfer is left out of
report spending. No application screenshot and no context-help actions were
added. Further chapters still need their own Go. The Rules and Reports leave
dialogs stay `Qt.ApplicationModal`.

Rules-enabled Release on macOS, Qt 6.12.0. Reconfigured
`build/import-ui-rules-release` and linked `./build/SpentIt`.

- `ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure` passed 5/5, 26.24s. `import.help_ui` is offscreen and passed in 2.21s.
- `./build/import-ui-rules-release/src/ui/spentit_help_ui_tests` with no `QT_QPA_PLATFORM` passed. QtTest reported 8 passed, 0 failed, 2615ms, including init and cleanup. The new check opens the introduction with no file, hides the sample banner, and shows `42.18` and Accept transactions. The sample viewer check and the Rules and Reports leave-dialog checks still passed.

Linux, Windows, and a person clicking through `./build/SpentIt` were not
checked. The Cocoa test calls `openHelp()` and does not click the Help menu
item. Details are in [the handoff](help-prototype-handoff.md).

## Busy feedback work item documented — 2026-11-05

Added [feature 014](../features/014-busy-feedback-and-progress.md) as a separate
work item alongside the in-flight Help work. It records the owner's account-switch
and large-import observations, accepted global busy feedback and progress-dialog
requirements, current code findings, proposed behavior, open implementation details
and mandatory verification. The reported delays and input size are owner observations,
not measured results. Implementation is not authorized by this documentation request
and has not started. Feature index and roadmap link the work item.

Documentation-only verification: `git diff --check` and relative file-link checks
for the changed documents passed. No runtime tests were required or run.

## Workspace titles and ledger defaults — 2026-11-05

Implemented under owner Go. The toolbar title follows the visible workspace,
including Back/Forward, and shows account/currency on the ledger. The ledger
creation button now reads **+ New transaction**. Accounts without a valid saved
sort choice default to date descending (newest first); existing saved choices,
including ascending dates, remain unchanged. Chronological balance calculation
is unchanged. See [navigation](../features/005-transaction-sheet-navigation.md)
and [manual transactions](manual-transactions.md).

Rules-enabled Release on macOS: rebuilt `build/SpentIt` and both UI test binaries
without compiler warnings/errors. CTest selection `import.ledger`, `import.search`,
`import.ui`, `import.navigation`, `import.qml` passed **5/5**, 117.04 s.
Offscreen UI checks passed 80 cases with the existing bulk-format skip; navigation
passed 20 cases with its opt-in diagnostics skip. Coverage includes newest-first
ordering, saved ascending choices after reopening, independent account settings,
Back/Forward titles, button copy, search restoration and unchanged balances under
sorting. Earlier fixture failures assumed ascending positions; those interaction
fixtures now request that order explicitly, while the default-sort fixture uses
an opening date before its sample transactions.

The initial sandboxed run could not access application-data locks; verification
was rerun with that access. No private dataset was used. Native visual inspection,
Linux and Windows were not checked for this change. `git diff --check` and changed
document relative-link checks passed.

## Help viewer prototype — 2026-11-05

The owner gave Go for the viewer prototype only. Sample topics, the Help
window, bundled-link checks, and Help beside the Rules and Reports leave
dialogs are in the rules-enabled Release build launched as `./build/SpentIt`.
Real chapters and application-wide context links were not started. The dialogs
stay `Qt.ApplicationModal`. Help is a transient child of the open dialog and
returns to the main window before that dialog hides. Escape on the Rules
dialog still means Stay.

macOS Qt 6.12.0, rules-enabled Release:

- `ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure` passed all 5 tests (26.31s). `import.help_ui` is offscreen.
- `./build/import-ui-rules-release/src/ui/spentit_help_ui_tests` with no `QT_QPA_PLATFORM` passed 7 checks (2.26s). Before the Rules dialog, a Z key appended `z` to the rule expression. With the dialog open, the same key left `^Sample help$` unchanged. A click on Larger text changed the Help text scale. Closing Help left the dialog open and returned Qt focus to Stay. Stay left Help open.

Qt Markdown still has no heading anchors; the viewer scrolls to heading text.
Wide sample tables wrap inside the article. Linux, Windows, and a person
clicking through the running application were not checked. Details and the
next-agent notes are in [the handoff](help-prototype-handoff.md).

## Help system design refinements — 2026-11-05

Incorporated the owner's corrections into the
[help-system design](../features/013-help-system.md). Help must stay usable beside
the application-modal Rules Save / Discard / Stay window, return focus there
when it closes, and leave that dialog's block on the rest of SpentIt unchanged.
If that cannot work cleanly in one process, the prototype stops and reports the
tradeoff. The first Go, given later the same day and recorded above, is a viewer prototype only:
sample content, navigation, search and highlighting, image enlargement, keyboard
use, that dialog, and build-time plus runtime bundled-link checks, launched with
`./build/SpentIt`. Real chapters are written and reviewed incrementally from one
stated synthetic example. Qt Markdown is tested before any real chapter; a
shortfall is reported with a proposed embedded-HTML approach before a new
renderer is added. This section records that design incorporation. The viewer
prototype section above supersedes the earlier statement that no viewer had
been implemented. Verification for this documentation pass: `git diff --check`
and relative file-link checks for the feature, feature index and status
document passed. No runtime tests were required or run for this
documentation-only change.

## Help system design — 2026-11-05

Added the [help-system proposal](../features/013-help-system.md) under the owner's
request for a design document. It records the embedded/offline reading experience,
nontechnical introduction and encounter-based manual, responsive layout, images,
search, context links and proposed QML implementation. Authored content belongs
in `manual/`, with `manual/images/`; viewer code is proposed under `src/ui/help/`.
No manual chapters, screenshots, viewer or packaging were implemented when this
design note was written. The viewer prototype section above records the later
implementation. The renderer limits remain in the handoff. The refinements
above supersede the earlier “awaiting owner Go” scope for the first prototype.
Verification: `git diff --check` and relative file-link checks for the new feature,
feature index and status document passed. No runtime tests were required or run
for this documentation-only change.

## Compact reports, grouped bars and editable drill-down — 2026-11-04

Implemented under the owner's follow-up Go. See the
[updated contract](reports-calendar-handoff.md#compact-results-and-editable-report-ledger--owner-go-2026-11-04).
Charts now precede one 34-pixel-row category table with ledger-style alternating
backgrounds and category swatches. All categories extend the report's outer
vertical scroll; the table/chart areas scroll horizontally only. Repeated chart
category legends were removed. Saved graph choices add **Bar — grouped periods**,
with adjacent bars in configured period order, stable category hues, period
shades, date labels and exact-value hover labels. Pie and separate bars remain.

Report drill-down reuses the actual ledger and its persistent inline editor,
with Account and In report columns. The opened transaction identity set remains
fixed, with one row per transaction and marked contributing splits. Current
allocations determine each included amount. Saved corrections, category changes
and Undo refresh the report; transactions moved outside its filters remain visible
as “No longer included”. Search only narrows this fixed set. Back uses the ordinary
unsaved-edit guard and restores the account filter/sort. Removed transactions
leave the list until restored by Undo. No opening or running balance is presented
for this multi-account result.

Existing account-scoped commands retain their financial/history boundaries.
Individual edits route to the correct account. Bulk corrections/category edits
require a single account and reject mixed selections before mutation. Merge/Link
and transaction creation remain in the ordinary account ledger. Automatic report
refresh failure leaves a visible stale result and recovery instruction; it does
not retry a saved edit. Large-report responsiveness has not been benchmarked.

The saved graph value changes experimental formats to **56 with rules / 55
without**. Old formats, including 54/53, are rejected unchanged without migration.
No private financial dataset was used. Only synthetic fixtures and synthetic UI
captures were used for verification; supplied private screenshots were not added
to project files, and their local temporary copies were removed.

Verification on macOS arm64, AppleClang 21, Qt 6.12:

- Release report storage checks pass, including grouped-choice persistence,
  identical retries, pre/post-commit interruption, restored backups and the
  existing write/sync fault sweep. Exact report engine checks pass, including
  equality of grouped/separate values and category/period order.
- Rules-disabled Release report engine, storage and old-format gate: **3/3
  passed**, 19.86 s. ASan/UBSan report engine/storage: **2/2 passed**, 26.96 s.
- Existing Release ledger editor, account lifecycle behavior/boundaries,
  rules/schema, statement review, navigation, committed UI and QML checks passed
  across the recorded selections. The 9-case and later 7-case selections each
  initially had a report UI fixture failure; these are not clean full-suite runs.
- Expanded synthetic report UI checks exercise cross-account editing of accepted
  and new entries, retained membership after a date edit, report refresh/Undo,
  deduplicated split rows, partial-split category changes and Undo, group rejection,
  unsaved Stay, source/current-value search, long-table height and grouped-choice
  reopening. Final native Cocoa execution passed **4 Qt cases**, 6.020 s,
  with no QML warnings. Synthetic native grouped-chart and editable-ledger
  screenshots were visually inspected. Final offscreen `import.reports_ui` and
  `import.qml` passed **2/2**, 5.89 s, after the last chart and test refinements.
- Initial sandboxed storage/UI execution could not access persistent app-data
  locks. Those checks were rerun with the required access; locking was unchanged.
  Test refinements corrected an omitted import-account argument, automatic category
  Apply handling, a QML invocation argument and source-query token spacing. A real
  refresh-path issue (the completion snapshot has no operation context) and a
  transient split-marker binding warning were fixed and retested.

`build/SpentIt` was rebuilt. Final build logs had no compiler warnings/errors;
`git diff --check` and changed-document relative file-link checks passed.
Linux/Windows, native accessibility and representative
large-data UI timing remain unverified. No broader storage fault-suite claim is
made for this follow-up.

## Imported-category assignment performance — 2026-11-04

Implemented under owner Go after profiling the Apply selected category decision.
`resolve_import_categories` now creates the existing bounded `ScopedStatements`
operation scope before its transaction; `finish_import_categories` also establishes
that scope for automatic completion during import preparation. Nested scopes reuse
the outer cache. This avoids rebuilding account aliases and reparsing grouped
source files for every allocation, and reuses SQL plans. Existing alias-signature
invalidation and per-operation destruction remain unchanged. No financial policy,
transaction boundary, verification step, format or migration changed.

Diagnostics appends the `resolve_import_categories` action and three fixed timing
codes for total resolution, assignment and automatic rules. The UI now reports this
command's own saved outcome and delivered generation. No financial text is added.

Synthetic benchmark: macOS arm64, AppleClang 21, rules-enabled Release storage,
diagnostics enabled in both binaries, encrypted temporary datasets, two accounts,
one automatic rule, ordinary imported and deliberately cleared categories. Three
serial before/after pairs per size; no competing builds/tests during measurements.
Each process creates a new dataset; OS caches were not flushed. Correctness checks
cover every row's exact amount, selected category/rule result, retry, acceptance,
full verification and reopening. These are storage timings, not native UI latency
or a prediction for the owner's private dataset.

| Total rows | Before resolution median (range) | After median (range) | SQL preparations before → after |
|---|---|---|---|
| 1,000 | 2.219 s (2.156–2.284) | 0.554 s (0.544–0.562) | 67,456 → 71 |
| 4,000 | 14.259 s (13.810–15.369) | 2.185 s (2.180–2.268) | 269,456 → 71 |

Reproduce with `tests/storage/spentit_rule_store_tests category_scale 500` or
`category_scale 2000` under the configured build directory (argument is rows per
account); each invocation creates and removes its own synthetic dataset.

Measured binary SHA-256: before
`6f6dae0d0e181c5e0993afd4f1b0db799b72273e5c9fb89eb6f7c21fcb52d793`;
after `e5cf42da90703790e2cc18238bbe3e7942df9b6ce3084d0b24cb5cf77120f396`.
The final test binary adds deterministic preparation-count and timing-span
assertions, plus same-connection retry after cancellation following a written row;
production behavior is unchanged from the measured revision.

Verification on the final code, synthetic fixtures only:

- Rules-enabled Release: **10/10 CTest checks passed**, 181.06 s, with
  `SPENTIT_TEST_DIAGNOSTICS=1 QT_QPA_PLATFORM=offscreen`. Selection:
  `diagnostics.collector`, `transferstore.basics/imports/qif`,
  `rulestore.category_scale/category_gate/category_multi/category_faults/application`,
  and `import.ui`. The UI runner passed 80 Qt cases, skipped its existing
  unavailable `bulkInteractions` format case, and reported no failures. Its new
  assertions verify the category command's own successful saved outcome, GUI
  delivery and resolution span.
- Category fault sweep: **185 write/sync positions**, alternating disk-full/I/O
  errors, pre/post-commit process interruption, cancellation, reopen/retry and
  same-connection retry after a written row was rolled back; Release 80.07 s.
- Rules-enabled Debug ASan/UBSan (`build/category-sanitized`): **5/5 passed**,
  213.73 s: diagnostics collector and category scale/gate/multi/faults. Fault sweep
  202.46 s; no sanitizer diagnostic. Environment:
  `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1`,
  `UBSAN_OPTIONS=halt_on_error=1`. This is storage/collector coverage, not a
  sanitized Qt application run.
- Rules-disabled, diagnostics-disabled Release (`build/category-rules-off`):
  storage/schema-gate and collector targets built; **2/2 passed**, 8.43 s.
  This is not a rules-disabled UI or full import-suite claim.
- Final rules-enabled Release application and diagnostics reader rebuilt.
  `git diff --check` and edited-document relative file-link checks passed.
  The full application suite was not run.
The existing private-data application session was not restarted or used as a test
fixture. Linux/Windows and native UI retiming remain unverified.

## Reports and shared calendar — 2026-11-04

Implemented under the owner's subsequent Go for the initial reporting/calendar
scope in the [handoff](reports-calendar-handoff.md). Reports has a searchable list,
selected editor, current account/category selections, moving presets and fixed
custom periods, comparison rows, selected Save/Discard, Duplicate, deletion and
Save/Discard/Stay navigation. Independent drafts survive selecting/showing another
report. Save-all validates every definition before one atomic save.

Exact C++ allocation aggregation produces direct category rows, per-period totals
and matching drill-down from one immutable snapshot. It includes current edited
new/accepted entries, refunds and income, excludes openings and transfer principal,
and includes ordinary transfer fees. Currency definitions are kept separate.
Results mark themselves stale after dataset changes and require Refresh before
new drill-down. Imported records needing repair are visibly identified. Selected
entries that no longer exist cause an explicit error rather than broader scope.

Results use fixed category/period headers, horizontal scrolling, identity-stable
colours, separate charts per period, and shared signed bar scales within each
currency. Negative pie periods retain their table and explain the Bar alternative.
The renderer uses the existing Qt Quick Canvas dependency; no external chart
package was adopted. Only graphical coordinates use floating point.

The shared day/month/year calendar supports Sunday-first weeks, today/selection
markers, keyboard navigation and Enter, two-stage Escape, outside dismissal,
virtualized years 0001–9999, and date-format-aware text fields. Browsing does not
change a field; selecting a transaction date changes its unsaved editor only.
Report date edits switch the stored definition to Custom. The modal Reports leave
window explicitly restores the application window's focus on dismissal.

New encrypted report tables advance the experimental formats to **54 with rules /
53 without**. Earlier formats, including 52/51, are rejected unchanged, without
migration. Revision-checked saves and identical retries are atomic. No existing
user dataset was opened, migrated or modified by this work.

Verification on macOS arm64, AppleClang, Qt 6.12.0, synthetic fixtures:

- Final rules-enabled Release reporting set: `engine.reports`, `import.reports`,
  `import.reports_ui`, `import.qml`: **4/4 passed**, 14.34 s. The storage test sweeps
  **49 write/sync fault positions** with disk-full/I/O failures and verifies whole
  old/new images plus retries. It also covers pre/post-commit exceptions, process
  exit on both sides of commit, reopening, backup restore, current edited amounts,
  accepted/new rows, single/both-account transfer principal, fees and cancellation.
- Engine tests cover inclusive/overlapping ranges, leap-year comparisons, year
  bounds, complete-month presets, refund/income signs, direct colon-category rows,
  separate currencies, zero rows, missing selections, date errors, exact cell
  reconciliation and overflow with cancelling signed extremes.
- UI tests cover creation, selected saves with an unrelated incomplete draft,
  failed save-all atomicity, Stay, persistence after reopening, typed dates becoming
  Custom, snapshot drill-down, stale refresh, positive/negative pie periods,
  unsaved transaction calendar selection, keyboard Return/Escape and popup bounds.
  The offscreen run emitted no QML warnings. Native Cocoa execution of the
  report/calendar test passed all 3 Qt cases (including setup/cleanup), 2.115 s.
  Synthetic native screenshots
  of the workspace, result and calendar were visually inspected. After the final
  list-label/preset refinements, `import.reports_ui` and `import.qml` passed again
  (2/2, 3.82 s).
- Existing Release regressions passed: `import.editor`, `ledger.balance`,
  `transferstore.basics`, `transferstore.imports`, `transferstore.splits`,
  `transferstore.manual_evidence`, `rulestore.schema`, `import.manual_transactions`,
  and `import.navigation`. The combined 13-case run initially exposed a report UI
  test failure; that test was corrected/fixed and the final report set above passed.
  Separately, existing `amountFormatting` and `immediateEditor` Qt tests passed
  (4 Qt cases including setup/cleanup, 3.635 s).
- Rules-disabled Release builds of the app and report tests succeeded. The engine,
  storage and old-format rejection gate passed; after the shared UI fixes, final
  `import.reports_ui` and `import.qml` passed **2/2**, 3.21 s.
- Final builds emitted no compiler warnings. `git diff --check` and relative
  Markdown file-link checks passed.
- ASan/UBSan engine report and date-format tests passed; the final expanded report
  test passed again without sanitizer diagnostics. This is engine coverage, not
  a sanitized run of the full storage/UI application.

Linux/Windows builds and interactions, spoken accessibility, long-running report
workload responsiveness, and the complete application regression suite remain
unverified. The optional Hide zero rows and chart-hover cross-highlighting are
not implemented. Printing/exports, fiscal periods, conversion, inclusive group
subtotals and time-series remain outside the authorized initial scope. Feature 006
remains active for the remaining platform/verification work.

## Reports and calendar design handoff — 2026-11-04

The owner authorized the [next-agent handoff](reports-calendar-handoff.md).
It records the accepted Rules-style Reports workspace, one Pie or Bar chart per
period, and shared day/month/year calendar picker. The owner subsequently accepted
all five policy groups: net spending/refunds/income, moving and fixed periods,
direct category rows, separate currency results, and selection/inclusion rules.
Chart dependency choice remains an investigation.
Documentation Go does not authorize implementation. No runtime, schema or
dependency changed. Verification: `git diff --check` and relative Markdown file
link checks passed for the edited documents. No runtime tests were required or run.


## Private document lock directory — 2026-11-04

Owner-approved session locks now use private application-data storage for
private local datasets and backup candidates. Path reservations cover creation;
file-identity locks cover existing documents, renames and overwrite. Locks stay
held until after SQLCipher closes and are released by the OS after a crash.
Shared/unsupported dataset locations and existing adjacent locks retain the
sidecar mechanism. Existing sidecars are not automatically removed. See the
[locking contract](document-locking.md), including the requirement to retire
older binaries before one-time legacy cleanup. No format or journal change.

The platform directory is resolved inside storage using native APIs/XDG rather
than introducing a Qt dependency. Missing, unsafe or unwritable lock storage
fails the operation. Source and backup paths resolve their parent once, before
locking/I/O; dataset symlinks remain rejected. Windows/Linux branches exist but
were not compiled or executed on this macOS host.

Verification uses synthetic data, Apple clang arm64, macOS, rules-enabled
Release in `build/import-ui-rules-release`:

- Application, storage-test and new document-lock test targets rebuilt without
  first-party compiler warnings. `git diff --check` and edited-document relative
  link target checks passed.
- Eight focused CTest cases passed on the final build in 48.98 s: `import.document_locks`,
  `import.local_locks`, `import.replacement`, `import.basics`, `import.backup`,
  `import.backup_publication`, `import.backup_cancellation`, and `import.qml`.
  The replacement case initially stopped at its stale 41-category expectation;
  that case now checks the current 46-category starter catalog and passes its
  disk-full/I/O and before/after creation-interruption checks.
- The full `import.io` and `import.crashes` sweeps passed in 364.01 s and
  365.60 s during implementation. Together with `import.replacement`, that
  selection passed 3/3 in 733.67 s. The sweeps used builds before the final
  parent-path normalization and legacy-sidecar permission follow-ups; those
  follow-ups are covered by the final focused run above.
- The standalone lock implementation and tests were compiled with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` and passed without a
  sanitizer diagnostic. This is lock-mechanism coverage, not a sanitized build
  of the complete storage application.
- Lock tests cover competing processes before creation and during a session,
  rename and parent aliases, force-kill/reopen, legacy-held locks, shared-mode
  fallback, invalid directories, permissions and lock-file symlinks. Integration
  checks cover unchanged bytes after refused overwrite, failed authentication,
  reopening and restoring a backup, and no adjacent locks for private local
  source/backup files.

Native Windows/Linux, network shares, cross-user sessions and other filesystem
variants remain unverified. The full application suite and full backup fault
sweeps were not run. Existing user datasets/sidecars were not changed.

## Light icon geometry — 2026-11-04

Owner-approved regeneration and deterministic cleanup replaced the light icon's
cropped tile with a complete rounded white tile and equal transparent margins.
All seven light PNG sizes, the light ICNS and Windows ICO were regenerated.
The dark artwork is unchanged. The retained source and macOS reproduction script
are documented in [asset maintenance](../assets/images/README.md).

Verification: all seven PNG dimensions, transparent outer margins and opaque
center pixels passed programmatic checks. Original/new 128 px composites were
visually inspected against light, dark and blue backgrounds. The rules-enabled
Release application rebuilt successfully; `import.qml` passed 1/1 (1.66 s).
All 11 ICNS and five ICO embedded representations exactly match the new PNGs.
Packaged applications and native Dock
appearance have not been refreshed or inspected in this checkpoint.

## Native Windows MSVC build and this machine's WSL — 2026-11-03

The rules-enabled Release application configures, compiles, and opens its
window with MSVC on this Windows host. The same checkout was built with GCC in
this machine's WSL Ubuntu, in a separate directory. Commands and host versions
are in [Windows development setup](windows-development-setup.md). The earlier
owner-reported Linux hosts in the note below are unchanged.

Windows, registry product name `Windows 10 Pro`, display version 26H2 build
26300.9818, Visual Studio 2026 toolset 14.44 (`cl` 19.44.35229), CMake
4.3.1-msvc1, Ninja 1.13.2, Qt 6.8.3 `msvc2022_64`, static PCRE2 10.49, vcpkg
OpenSSL 3.5.2:

- `build\SpentIt.exe` stayed up with window title `SpentIt`. After directory
  publication used write access, UI tests created, reopened, saved, and
  committed documents.
- `ctest` registered 40 tests because `tests/storage` returns immediately on
  Windows. Those tests call `fork`. **39 passed, 1 failed**, real time 208.00 s.
  The failure is `import.ui` / `ImportUiTests::accessibilityAndKeyboard`: under
  `QT_QPA_PLATFORM=offscreen`, the unsaved-changes Save button has no accessible
  interface. Details are in the setup document.
- Final first-party `/W4` rebuild logs had no warnings. SQLCipher stays `/W0`.
  Sanitizer configuration is refused on Windows. Packaging was not run. macOS
  was not rebuilt. A user-local OpenSSL 3.5.9 was not installed; the linked
  library is the existing vcpkg 3.5.2.

WSL Ubuntu, checkout on DrvFS (`/mnt/c`), build directory
`build/wsl-import-ui-rules-release`, Qt 6.8.3 `gcc_64`, Ubuntu OpenSSL 3.0.13.
Storage-test temporary files were on `/tmp`.

- `build/SpentIt` opened a WSLg Wayland window. Mesa/ZINK lines matched the
  class already recorded for the Linux guide. The application link was
  warning-clean.
- `storage.crashes` passed in 12.97 s, `storage.io` in 22.41 s,
  `storage.backup_crashes` in 66.91 s, and `storage.backup_failures` in 97.68 s.
- `transferstore.io` failed in 225.61 s and `transferstore.crashes` in
  229.37 s, both at `tests/storage/transfer_store_tests.cpp:368` during
  propose-match fault injection (operation 5), after operations 0–4 had
  printed their position counts. An isolated rerun had not been done.
- `import.review_sheet_faults` hit its 1200 s timeout.
- `accounts.io` passed in 126.98 s and `accounts.mapping_openings` passed in
  3.11 s. `accounts.basics` failed in 0.57 s with `Choose a category for every
  rule`. `accounts.validation` (11.54 s) and `accounts.groups` (0.62 s) failed
  with `Choose the imported categories before accepting transactions`.
  `accounts.group_io` (305.68 s) and `accounts.group_crashes` (304.19 s)
  printed that same message on mapped fault sweep kind 4, after group
  operations 0–3. `accounts.combined` (0.56 s) and `accounts.combined_io`
  (0.84 s) failed with `Clear categories in the ledger before running rules
  again`. These are the category-gate checks from `e350ebd`. The account tests
  still use the earlier save and accept calls. The Windows-session Unix diff
  is warning line-breaks, identifier renames, and Windows-only branches.
- `accounts.crashes` passed in 127.76 s and
  `accounts.mapping_opening_faults` in 657.45 s. The statement suites through
  `statement.faults` passed, including `statement.generated_faults` (423.24 s),
  `statement.import_undo_faults` (536.65 s), `statement.reimport_faults`
  (262.30 s), `statement.rollback_faults` (149.38 s), and
  `statement.acceptance_faults` (128.11 s).
- `import.replacement` failed in 0.89 s at `tests/storage/import_tests.cpp:518`.
  A rejected create through a symlink changed the original file bytes.
- `import.io` passed in 753.41 s and `import.crashes` in 759.12 s.
  `import.backup_failures` and `import.backup_crashes` each hit the 2400 s
  timeout. Backup publication, cancellation, validation, opening, and
  cancellation then passed. The committed suites through `committed.crashes`
  passed, including `committed.io` (305.52 s) and `committed.crashes`
  (314.51 s).
- `category.setup` failed in 0.43 s at `tests/storage/category_tests.cpp:65`.
  The test requires 41 starter categories. The starter list in
  `src/storage/category_store.inc` has 46 names; the five `Utils:` entries are
  the difference. `category.catalog` through `category.validation` passed.
- `category.io` passed in 330.65 s and `category.crashes` in 337.06 s. The
  rule-store cases through `rulestore.io` passed, including
  `rulestore.category_faults` (158.01 s) and `rulestore.io` (184.22 s).
- `rulestore.bulk` failed in 1.82 s. `tests/storage/bulk_cases.inc:22` expects
  a bulk preview of a pending row to fail with text containing `committed`.
  The failure text was `Select an accepted transaction that has not been deleted.`
- `rulestore.crashes` passed in 188.32 s, `rulestore.bulk_io` in 312.59 s,
  `rulestore.bulk_crashes` in 318.52 s, and `rulestore.account_faults` in
  226.34 s. `rulestore.reapply` and `rulestore.reapply_faults` passed in 2.21 s
  and 3.14 s. `import.manual_transactions` passed in 72.42 s.
- `import.navigation` failed in 23.45 s: 18 passed, 2 failed, 1 skipped. Both
  failures are `QMetaObject::invokeMethod(..., "popup")` in the binary compiled
  for this suite. Qt 6.8 exposes only `popup(QQmlV4FunctionPtr)`. The current
  source uses `showPopup`, and the rebuilt Windows `import.navigation` passed.
- `import.ui` failed in 150.57 s: 80 passed, and
  `accessibilityAndKeyboard` left `guardAccessible` null at
  `tests/ui/import_model_tests.cpp:1583` in that binary. That is the same
  offscreen Save-button failure as Windows. `import.benchmark_smoke` failed in
  10.65 s because commit still required the imported-category choices. The
  current benchmark source submits those choices, and the rebuilt Windows test
  passed. `import.recovery_ui` (2.79 s), `import.committed_ui` (24.32 s),
  `import.category_ui` (18.98 s), `import.rules_ui` (31.05 s), `import.qml`
  (0.70 s), and `import.group_benchmark_smoke` (3.40 s) passed.
- The finished WSL `ctest` was **141 passed, 18 failed, out of 159**, real
  time 8628.74 s. The WSL test compile warned on Qt `QTRY_*` conversions. The
  Unix warning flags were not weakened. An isolated rerun of the failures was
  not done.

## Alternating uncommitted ledger rows — 2026-11-03

Implemented the owner's follow-up Go: uncommitted rows alternate existing pale
yellow `#fff6de` with lighter cream `#fffdf2`; dark mode uses warm `#3d3524`
and `#49402c`. Expanded splits share the parent stripe and selection retains
priority. See [the palette contract](../features/005-transaction-sheet-navigation.md#system-light-and-dark-palettes--owner-go-2026-11-03).

Verification on macOS arm64, Qt 6.12.0, synthetic data:

- Rebuilt rules-enabled Release with `cmake --build build/import-ui-rules-release -j 2`;
  `build/SpentIt` contains the change.
- Parameterized the existing stripe checks for committed/uncommitted rows,
  sorting, filtering, expanded splits and controls; palette contrast includes
  the new draft alternate colour.
- Native `ledgerStripes appearanceSwitching`: **5 passed** including setup/cleanup,
  3.826 s. Light/dark uncommitted screenshots visually inspected.
- CTest `import.qml`: **1/1 passed**, 1.69 s.
- Initial uncommitted test expected a draft colour for the opening-balance row;
  corrected that expectation before the final passing run.
- Changed-document relative links and `git diff --check` passed.

Windows/Linux, rules-disabled builds and the full suite were not run. Existing
unrelated linked-split editor changes were preserved.

## Alternating ledger rows and aligned controls — 2026-11-03

Implemented the owner's Go for alternating ordinary transaction backgrounds:
light blue-grey/white and two dark shades, assigned by displayed row index.
Expanded saved splits share their transaction's stripe; selection and draft
colours retain priority. Transfer arrows and split controls use transparent
resting backgrounds, themed hover/pressed surfaces and keyboard-focus outlines.
A fixed arrow slot and left-aligned split label remove the transfer-dependent
indent. See [the palette contract](../features/005-transaction-sheet-navigation.md#system-light-and-dark-palettes--owner-go-2026-11-03).

Verification on macOS arm64, Qt 6.12.0, synthetic data:

- Rebuilt rules-enabled Release with `cmake --build build/import-ui-rules-release -j 2`;
  `build/SpentIt` contains the change.
- New `ledgerStripes` checks alternating rendered colours through ascending and
  descending sorting and filtering, fixed split-label alignment, transparent
  resting background, hover/pressed states, keyboard focus and split expansion.
- Native `ledgerStripes appearanceSwitching`: **4 passed** including setup/cleanup,
  2.651 s; both light and dark modes checked and screenshots visually inspected.
  Contrast checks now include the alternate row colour.
- Offscreen `qifTransferResolution readOnlySplitViews transferManagementUi`:
  **5 passed** including setup/cleanup, 5.992 s. Includes transfer-control
  transparency/non-overlap and existing counterpart navigation and split viewing.
- CTests `import.navigation` and `import.qml`: **2/2 passed**, 18.89 s.
- Changed-document relative links and `git diff --check` passed.
- Initial stripe test needed a single QIF header and an explicit return to the
  beginning of the virtualized list before inspecting delegates; corrected before
  the final passing runs.

Windows/Linux, rules-disabled builds and the full suite were not run. Existing
unrelated linked-split editor changes were preserved.

## Text-field Home and End — 2026-11-03

Implemented the owner's Go for shared Home/End cursor movement and Shift
selection across editable single-line fields, including search, completion,
passwords, rules, dialogs and ledger/split editors. A forwarded key handler
preserves field-specific key handlers; modified Control/Alt/Meta shortcuts retain
Qt behavior. See [the contract](ledger-editing.md#text-field-home-and-end--owner-go-2026-11-03).

Verification on macOS arm64, Qt 6.12.0, synthetic text:

- Rebuilt rules-enabled Release using `cmake --build build/import-ui-rules-release -j 2`;
  `build/SpentIt` contains the change.
- `textBoundaries` tests plain, search and category-completion fields: movement,
  selection-anchor reversal, deletion, replacement, collapsing selection, empty
  input, Left and Escape. Offscreen **5 passed** including setup/cleanup (0.165 s);
  native macOS **5 passed** (0.517 s).
- CTests `import.navigation`, `import.category_ui`, `import.rules_ui`, `import.qml`:
  **4/4 passed**, 75.60 s.
- Initial test compilation used an unavailable QWindow `keyClicks` overload;
  corrected to `keyClick` before the passing runs. Initial sandboxed native run
  could not access a screen; rerun with permitted window access passed.
- Changed-document relative links and `git diff --check` passed.

Windows/Linux, rules-disabled and synthetic-sheet builds, physical keyboard/mouse
interaction and the full suite were not run. Read-only multiline displays retain
Qt navigation. Existing unrelated transaction-origin changes were preserved.

## Transaction-row arrows for split transfers — 2026-11-03

Implemented the owner's Go to show a transfer navigation arrow on a split
transaction's main ledger row, including while collapsed. It chooses the first
linked allocation in displayed split order, skipping ordinary categories such as
fees. The split toggle leaves space for the arrow. Expanded saved splits retain
their own exact-counterpart arrows; existing destination-account tooltips and
navigation guards are reused. No transfer ownership or financial data changes.
See [the contract](../features/008-transfer-matching-and-lifecycle.md#match-navigation).

Verification on macOS arm64, Qt 6.12.0, synthetic data:

- Rebuilt rules-enabled Release with `cmake --build build/import-ui-rules-release -j 2`;
  `build/SpentIt` contains the change.
- Offscreen `qifTransferResolution transferManagementUi`: **4 passed**, 0 failed
  including setup/cleanup, 3.784 s. The fee-first split case checks the collapsed
  row arrow, destination description, non-overlap with the toggle, navigation to
  the counterpart, and retained expanded-split navigation. Existing unsplit
  incoming/outgoing navigation also passes.
- Offscreen `firstSplitTransferNavigation`: **3 passed**, 0 failed including
  setup/cleanup, 0.861 s. Synthetic navigation metadata checks multiple linked
  allocations in both display orders, skipping an ordinary fee, one link and no
  links. This is a selector test, not a new multi-account transfer storage test.
- Final synthetic collapsed-split screenshot visually inspected: arrow and split
  toggle are both visible on the transaction row.
- CTest `import.qml`: **1/1 passed**, 1.75 s.
- Initial new test assertions needed correction to handle QVariantMap as well as
  QJSValue returns; the final runs above include that correction.
- Changed-document relative file links and `git diff --check` passed.

Native mouse interaction, dark-mode capture, rules-disabled builds, Linux/Windows
and the full suite were not run. Existing linked-split editor work was preserved.

## New-account sidebar scroll fix — 2026-11-03

Implemented the owner's Go to replace the **+ New Account** handler's unconditional
scrollbar `position=1` with a bounded reveal of the inline account-name field.
Deferred layout/viewport changes also clamp the sidebar position, including when
the form is removed. Account creation and drag ordering are unchanged. The faulty
handler predates drag reordering (Git blame: 2026-10-28). See
[the contract](../features/005-transaction-sheet-navigation.md#new-account-sidebar-scrolling--owner-go-2026-11-03).

Verification on macOS arm64, Qt 6.12.0, synthetic data:

- Added short-list (1 account) and scrolling-list (16 accounts) regression cases
  that open the form, check focus/visibility/scroll bounds, cancel with Escape,
  reopen and create with Enter, then check bounds and account count.
- Before the fix, the scrolling-list case failed because the field was outside
  the viewport; short-list passed (3 passed, 1 failed including setup/cleanup).
- Rebuilt rules-enabled Release using
  `cmake --build build/import-ui-rules-release -j 2`; `build/SpentIt` contains the fix.
- Offscreen `newAccountScroll sidebarAccountDrag sidebarAccountDragScroll`:
  **6 passed**, 0 failed including setup/cleanup, 7.803 s.
- Native `newAccountScroll`, with permitted window access: **4 passed**, 0 failed
  including setup/cleanup, 2.064 s.
- CTest `import.qml`: **1/1 passed**, 1.44 s.
- Changed-document relative file links and `git diff --check` passed.

Native tests invoked the New Account button's signal and used keyboard events;
physical mouse interaction, archived-account variants, viewport-resize interaction,
Linux/Windows, rules-disabled builds and the full suite were not exercised.
Existing transaction-origin changes were preserved.

## Stronger committed-row background — 2026-11-03

Implemented the owner's Go to change light-mode ordinary ledger rows from
`#f3f5f7` to `#e6ebf0`, separating them more clearly from the `#f6f7f9` window.
Selected/draft colours and the dark palette are unchanged. See
[the palette contract](../features/005-transaction-sheet-navigation.md#system-light-and-dark-palettes--owner-go-2026-11-03).

Verification: rebuilt rules-enabled Release with
`cmake --build build/import-ui-rules-release -j 2`; `build/SpentIt` contains the
change. Existing offscreen `appearanceSwitching` light-mode checks passed
**3/3 including setup/cleanup**, 0.856 s, including ledger row colour and text
contrast. Primary text on the new row background has **13.29:1** contrast.
Changed-document relative file links and `git diff --check` passed. Native visual
inspection, dark-mode rerun, other platforms and the full suite were not run for
this one-colour adjustment.

## Readable dropdown and menu highlights — 2026-11-03

Implemented the owner's follow-up Go for the light-palette contrast defect.
Qt Basic item delegates used white highlighted text over the pale `palette.light`
surface. Shared dropdown delegates now use explicit themed plain-text labels;
menu/delegate light and midlight roles use blue selection and pressed surfaces.
Amount-format and import-Undo account dropdowns now use that shared component.
Highlighted buttons explicitly inherit themed bright-text colour. Text-field
selection retains its existing white-on-blue pair. No data-format change.
See [the palette contract](../features/005-transaction-sheet-navigation.md#system-light-and-dark-palettes--owner-go-2026-11-03).

Verification on macOS arm64, Qt 6.12.0, synthetic data:

- Rebuilt rules-enabled Release with `cmake --build build/import-ui-rules-release -j 2`;
  `build/SpentIt` contains the fix.
- CTests `import.navigation` and `import.qml`: **2/2 passed**, 19.91 s.
- Extended `appearanceSwitching` checks actual dropdown/menu text and background
  properties for highlighted/pressed states and a highlighted sidebar button,
  requiring at least 4.5:1 contrast. After adding keyboard Down selection,
  final offscreen light run: **3 passed**, 0 failed, 0.493 s including setup/cleanup;
  native light → dark → light: **3 passed**, 0 failed, 1.412 s.
- Inspected final native light/dark dropdown captures. Light mode shows dark text
  on pale blue; dark mode shows light text on darker blue.
- Offscreen `mappingOpeningChoices amountFormatting importUndoOutcomes`:
  **5 passed**, 0 failed including setup/cleanup, 4.391 s.
- Initial sandboxed native attempt failed because no screens were available;
  native verification then passed with permitted window access.
- `git diff --check` and changed-document relative file-link checks passed.

Full application suite, mouse-hover coverage of every menu, rules-disabled builds,
Linux and Windows were not run. Existing transaction-origin changes were preserved.

## Saved amount formatting — 2026-11-03

Implemented the owner's Go for **Settings → File settings → Use amount formatting**
and the `1,234.56`, `1.234,56`, `1 234,56` selector. New files default on using the
creation-time currency's product default; the user-selected style applies across
accounts and persists in the encrypted file. Off retains the choice and uses
ungrouped decimal-point numbers. Each amount retains its currency scale. Shared
exact formatting covers the ledger, sidebar, editors, Review, rule match amounts
and history. Sidebar space-grouped numbers stay together when the label wraps.

User entry and arithmetic accept ungrouped or correctly grouped numeric literals
in the active style. Invalid grouping, precision and arithmetic errors reject the
edit. Imported-file and stored-decimal parsing are unchanged. Preference updates
are atomic and leave financial rows, account revisions and ledger generation
unchanged. Backups retain the preference. Formats are **52 with rules / 51 without**;
previous formats reject unchanged without migration. See
[the contract](ledger-editing.md#amount-formatting--owner-go-2026-11-03).

Verification on macOS arm64 with synthetic data:

- Rebuilt rules-enabled Release `build/SpentIt` with
  `cmake --build build/import-ui-rules-release -j 2`.
- Release selected CTests `engine.amount_expression`, `engine.amount_format`,
  `accounts.lifecycle_formatting`, `accounts.lifecycle_formatting_boundaries`,
  `rulestore.schema`: **5/5 passed**, 6.66 s.
- Formatting tests cover signed 64-bit extrema, currency scales 0/2/3/4,
  grouped/ungrouped arithmetic, malformed grouping, overflow/precision protections,
  on/off persistence, EUR creation defaults, restored backup and unchanged money,
  revisions and ledger generation. The preference boundary test counted 13
  write/sync positions; it injected disk-full/I/O at positions 1, 7 and 13,
  retried after reopening, and killed processes immediately before/after commit.
  This is selected fault coverage, not a sweep of all 13 positions.
- Offscreen focused amount formatting/expression/remainder/split editor checks:
  **6 passed, 0 failed** including setup/cleanup, 3.412 s. They cover formatted
  manual-transaction save/Undo, opening edits, exact split balancing, restart,
  checkbox and selector interaction. The final sidebar-layout check reran
  `amountFormatting`: **3 passed, 0 failed**, 1.815 s. Visually inspected its
  rendered Settings capture, including the space-grouped sidebar balance.
- Full `spentit_import_ui_tests` attempt: **80 passed, 1 failed**, 114.122 s.
  The sole failure expected the old `-1637.00` Review display. Updated it and its
  later Undo expectation to grouped text; rebuilt `batchReviewWorkspace` rerun:
  **3 passed, 0 failed**, 1.842 s. The whole executable was not repeated after
  that expectation-only correction. An earlier focused split-editor expectation
  was likewise updated from `1012.00` to `1,012.00` before its passing rerun.
- Final build after clearing stale formatted preview/receipt text: offscreen
  `amountFormatting batchReviewWorkspace` **4 passed, 0 failed**, 3.320 s.
- Release CTests `import.committed_ui`, `import.category_ui`, `import.rules_ui`,
  `import.qml`: **4/4 passed**, 69.72 s.
- Rules-disabled Release: preference behavior/boundaries and `import.schema_gate`
  **3/3 passed**, 10.13 s; rules-disabled GUI was not rebuilt or exercised.
- ASan/UBSan engine amount-format and expression tests: **2/2 passed**, 0.60 s,
  with `ASAN_OPTIONS=detect_leaks=0` and
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; no sanitizer diagnostic.
- Changed-document relative file links and `git diff --check` passed.

Native mouse interaction, dark theme, full storage fault sweeps, storage/UI
sanitizers, Linux/Windows and the full project suite were not run for this change.
Existing uncommitted transaction-origin editor changes were preserved.

## Linked split account display in the editor — 2026-11-03

Implemented the owner's Go to display the live counterpart account in the
transaction editor, matching the saved ledger split view. Both independent
endpoints and generated postings use the current account name plus `[Account]`,
even when the stored split category is blank. Display metadata marks linked
allocations as transfers, excluding them from automatic remainder selection and
its label. Stored category values and existing editing restrictions are unchanged;
released, removed and unresolved links do not supply this override. No format
change. See [the contract](transaction-origin-handoff.md#implemented-transaction-behavior).

Verification on macOS arm64 with synthetic data:

- Rebuilt rules-enabled Release `build/SpentIt` using
  `cmake --build build/import-ui-rules-release -j 2`.
- Offscreen `spentit_import_ui_tests reviewMultipleLinkCandidates
  qifTransferResolution transferManagementUi splitAmountEditing`: 9 passed,
  0 failed (including setup/cleanup), 9.642 s, no QML warnings.
- Extended checks cover both independent endpoints, the accepted imported split
  with blank underlying category, visible editor text, exclusion from the remainder
  label, unchanged category data, explicit QIF transfer resolution and a generated
  counterpart. Existing transfer-management and split-amount cases also pass.
- Visually inspected an offscreen two-split editor capture: counterpart account is
  visible on the linked split and the remainder label appears only on the other
  eligible split.
- `git diff --check` and changed-document relative file-link checks passed.

Native mouse interaction, dark theme, rules-disabled builds, sanitizers,
Linux/Windows and the full project suite were not run for this follow-up.

## Transaction and split rule actions — 2026-11-03

Implemented the owner's Go for separate **Create rule…** and **Edit rule…**
buttons. Transaction (payee) and imported splits (memo) are separate targets.
Create offers targets without an editable applied rule; Edit offers those with
one. One choice opens directly, multiple choices open a labelled chooser, and
no choices disables the button. Split choices include amount and memo. Individual
split creation buttons are removed. Deleted rules permit creation again; splits
without imported source fields cannot seed split-memo rules.

Attribution includes the applied definition's saved scope, so a transaction rule
that categorized a split occupies the transaction target. A new transaction rule
on a split transaction uses the original payee and leaves category selection to
the user. Existing split-memo prefill, unsaved-edit guards, attribution and
category gates remain. No persisted format or rule-evaluation policy changes.
See [the contract](../features/004-rules-and-explanations.md#transaction-rule-editor--owner-go-2026-11-01).

Verification on macOS arm64 with synthetic data:

- Rebuilt rules-enabled Release with `cmake --build build/import-ui-rules-release -j 2`;
  `build/SpentIt` includes the change.
- Selected CTests `^(import\.(rules_ui|qml)|rulestore\.(category_gate|category_multi|match_preview))$`:
  5/5 passed in 41.90 s. The full rules UI suite reported 27 passed, 0 failed,
  including setup/cleanup; no QML warnings were reported.
- Target tests cover unsplit/split combinations, transaction and split attribution,
  deleted definitions, no imported split source, direct versus chooser routing,
  and original-payee transaction prefill without an arbitrary split category.
  An imported two-split case applies a split rule and transaction rule, checks their
  saved scopes and opens the remaining split's memo through Create.
- `spentit_committed_ui_tests addThenRemoveEmptySplit` passed (3 including
  setup/cleanup, 1.291 s), with obsolete split-button expectations removed.
- Visually inspected the offscreen imported two-split editor capture: Create/Edit
  share the Add split row and attribution remains below it.
- `git diff --check` and changed-document relative file-link checks passed.

Native mouse interaction, dark-theme capture, rules-disabled builds, sanitizers,
Linux/Windows and the full project suite were not run for this follow-up.

## Review candidate defaults and grouping — 2026-11-03

Implemented the owner's Go for a unique same-date candidate default, automatic
unique exact opposite-amount split selection whenever a candidate is selected,
and clearer question boundaries. Missing or ambiguous exact split pairs show
**Select split**; changing candidates refreshes both allocation choices. Manual
split choices remain available and Yes still confirms through the existing
revalidated command. Each question has a solid separator, alternating themed
background and spacing; a compact labelled split selector belongs to its row.
See [the interaction contract](review-sheet.md#user-interaction).

The defaults use existing eligible candidates and exact Money values in storage;
no floating-point amount comparison, new matching eligibility, persistence change
or format bump is introduced. Formats remain 50/49.

Verification on macOS arm64, synthetic data:

- Rebuilt rules-enabled Release with `cmake --build build/import-ui-rules-release -j 2`;
  the local application is `build/SpentIt`.
- Offscreen `spentit_import_ui_tests reviewMultipleLinkCandidates unifiedReviewSheet
  unifiedReviewLinkDifference`: 8 passed, 0 failed (including setup/cleanup), 7.873 s.
  Four candidate cases cover unique/no/tied exact dates, mixed-sign splits,
  ambiguous exact splits, manual split override, switching away/back, and confirmed
  linking using the refreshed choice. Existing Merge/Undo and amount-difference
  confirmation cases also pass. No QML warnings in the final run.
- Selected Release CTests `^import\.(review_sheet|review_batch|review_policy|review_state|qml)$`:
  5/5 passed, 17.89 s.
- Visually inspected an offscreen rendered two-question Review view: distinct
  backgrounds and separator, compact indented split selection, aligned columns.
- `git diff --check` and changed-document relative file-link checks pass.

An initial UI run exposed a missing QML handler separator, fixed before final
verification. The mixed-sign fixture initially depended on new-row pair orientation;
accepted candidate rows now make its orientation deterministic. An overlapping
intermediate build produced stale Qt test metadata; a fresh build resolved it.
Native mouse interaction, dark-theme capture, sanitizers, rules-disabled builds,
Linux/Windows and the full suite were not run for this presentation follow-up.

## Uniform transaction origins — 2026-11-03

Implemented the owner's Go for the transaction/category/transfer cleanup in the
[contract](transaction-origin-handoff.md). Rule matching remains unchanged and its
current-versus-original input decision remains open.

The category menu now submits transaction/allocation identities, category and an
expected ledger generation. Storage derives the facts, shares canonical editor
validation, preserves unresolved duplicate decisions and saves a mixed selection
with one group Undo. Exact retries compare the original command rather than loading
changed rows. Account assignment retains its complete atomic transfer command.

Transfer history explicitly distinguishes saved independent ledger entries from
transfer-local postings. Saved manual rows receive authoritative fact/revision
validation, are counted once, support accepted account assignment and can link to
other manual or imported postings. Either independent side can release its category
or retarget to another account. Category, date, amount and removal changes use the
shared financial/history paths; independent and accepted counterparts remain, while
only disposable generated drafts are removed. Legacy local-manual transfer
accounting remains distinct. New linked-row editing/deletion restrictions remain
shared across origins; generated-side editing is still restricted.

Formats advance to **50 with rules / 49 without**, including the new transfer
ownership bit and category selection revision in encrypted history. Previous
experimental formats are rejected unchanged, with no migration. Creation origin
comes from stored metadata rather than presence of an original amount, so repaired
imports are not mistaken for manual entries. Source-specific capabilities retain
their existing association requirements; no new automatic matching or rule input
semantics were added.

Verification uses synthetic fixtures on macOS arm64:

- Rebuilt the rules-enabled Release application with
  `cmake --build build/import-ui-rules-release -j 2`; output is `build/SpentIt`.
- Rules-enabled Release: 27/27 selected CTests passed in 36.13 seconds. Selection:
  `transfers.*`, `transferstore.{basics,imports,management,splits,manual_evidence,qif,validation}`,
  `import.{transfer_release,transfer_assignment,editor,selection_edit,review_policy,review_sheet,review_batch,generated_batch,navigation,qml}`,
  `statement.import_undo` and `rulestore.schema`. The final expanded
  `import.manual_transactions` passed separately in 35.19 seconds.
- Rules-disabled Release: 9/9 selected CTests passed in 31.56 seconds:
  `import.{manual_transactions,transfer_release,transfer_assignment,editor,selection_edit,schema_gate}`
  and `transferstore.{management,manual_evidence,validation}`. The final expanded
  manual suite passed again in 32.63 seconds.
- ASan/UBSan: 12/12 selected CTests passed in 79.69 seconds:
  `import.manual_transactions`, `transfers.*` and
  `transferstore.{management,manual_evidence,validation}`. Options:
  `ASAN_OPTIONS=detect_leaks=0:quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1`
  and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The final expanded manual
  suite passed again in 99.58 seconds. No sanitizer diagnostic was reported.
- The manual suite covers all four manual/imported pairings with neither, either
  or both sides accepted; new/accepted account assignment; both-side category
  release, retargeting, date correction, deletion and Undo; mixed-sign/positive/zero
  split transfers; accepted generated-counterpart preservation on amount edits;
  bounded manual Review Link; import undo preserving a manual counterpart; reopen,
  exact/changed retries, forged/stale snapshots and cross-currency rejection.
- Recovery covers cancellation after partial in-transaction work, sampled disk-full
  and write/sync failure positions, exact retries after reopening, and child exits
  immediately before/after durable commit. These are sampled new fault checks,
  not an exhaustive transfer fault sweep.
- Offscreen navigation includes the new accepted-manual account assignment,
  reopen and Undo case, alongside mixed category-menu and viewport checks.
  Both current-format schema gates passed. `git diff --check` and changed-document
  relative file-link checks passed.

One initial Release regression exposed the old bulk API's hardcoded format gate;
it now recognizes format 50. The rules-disabled transfer test also expected the
rules-enabled bulk refusal text; it now checks the appropriate refusal for that
configuration. Both reruns passed. Native mouse interaction, Linux/Windows,
leak detection and the full suite were not run for this checkpoint.

## Next-session transaction-origin audit — 2026-11-03

The [handoff](transaction-origin-handoff.md) records read-only findings and proposed
cleanup. Synthetic repository probes reproduced account assignment failure for
accepted transactions of both origins; new manual/imported variants succeeded.
Two-manual-endpoint linking is restricted in inspected code. Rule matching retains
its existing origin-dependent semantics pending an explicit decision. No broader
cleanup was implemented during the audit; the category fix below is separate.

## Manual transaction context-menu categories — 2026-11-03

Fixed the owner's reported `Invalid category assignment` failure under Go.
The ordinary category branch of atomic assignment previously required imported
provenance. It now checks supplied manual/imported flags against the stored
transaction, allowing saved manual rows without inventing originals. Existing
revision/account/allocation checks, generated-row restrictions, transfer paths,
category validation and group journal remain. See the
[contract](manual-transactions.md#right-click-category-assignment--fix-authorized-2026-11-03).
Formats stay 48/47 and existing manual entries work without recreation.

Verification on macOS:

- Rebuilt rules-enabled Release `build/SpentIt`.
- Extended `import.manual_transactions` passed in rules-enabled and rules-disabled
  Release. Checks include reopened manual entries, accepted/new/manual/imported
  mixed selections, category and Uncategorized, exact retry, stale/forged request
  rejection, unchanged balances and acceptance state, persistence and group Undo.
- Recovery checks include cancellation after the first child, sampled full-disk
  and write/sync failures, retries and Undo after reopening, and process exit on
  both sides of durable commit. These are sampled fault positions, not an
  exhaustive new sweep.
- Release `import.transfer_assignment`, `import.transfer_release` and `import.qml`
  passed. The extended offscreen menu case passed 3/3 including setup/cleanup:
  select an ordinary category for a manual row, change a mixed selection to
  Uncategorized, reopen and Undo the group.
- Final full offscreen `import.navigation` passed. The extended manual storage
  suite also passed ASan/UBSan with `ASAN_OPTIONS=detect_leaks=0`; no sanitizer
  diagnostic was reported. `git diff --check` passed.
- Initial new test assertions read the original accepted draft instead of the
  current revision, and opened a submenu without its parent context menu. Those
  harness errors were corrected before the passing reruns.

Native mouse interaction, Linux/Windows and the full suite were not run for this
fix. No new transfer behavior or document format is introduced.

## System light and dark palettes — 2026-11-03

Implemented the owner's [system appearance direction](../features/005-transaction-sheet-navigation.md#system-light-and-dark-palettes--owner-go-2026-11-03).
The desktop window and inherited Qt controls share semantic light/dark colours
with custom ledger rows, sidebar selections/balances, category lists, rule draft
highlights, dialogs and completion scrollbars. Appearance follows Qt's system
colour-scheme notification; unknown uses light. Formats remain 48/47.

Verification on macOS, Qt 6.12.0, rules-enabled Release:

- Rebuilt `build/SpentIt` and the selected UI test executables.
- Offscreen `import.navigation`, `import.category_ui`, `import.rules_ui` and
  `import.qml` passed 4/4 before the final sidebar-selection contrast correction.
- Native `spentit_import_navigation_tests appearanceSwitching` passed 3/3
  including setup/cleanup after that correction. A synthetic account remained
  open through light → dark → light via Qt's native appearance API. Checks cover
  inherited input text, window and ledger colours, sidebar balance/selection,
  and at least 4.5:1 contrast for primary text on the main semantic surfaces,
  highlighted text, and secondary/error/warning text on the control surface.
- Final offscreen `import.navigation` and `import.qml` passed 2/2 after the
  sidebar correction (native appearance case explicitly skipped offscreen).
  `git diff --check` passed.
- Native light/dark ledger captures were inspected. Initial captures exposed a
  light-mode selected-account contrast issue, corrected with an explicit paired
  selection background/text. The native test now checks that rendered pair.
- Offscreen cannot change the platform colour scheme: the first attempted
  switching test failed there, and this native-only case now explicitly skips
  on offscreen. A sandboxed native attempt had no screen access; the native pass
  used permitted desktop access. No private document was opened.

This is not an exhaustive accessibility audit or visual review of every dialog.
Linux, Windows, and rules-disabled builds remain unverified for this change.
The separate older synthetic sheet prototype is unchanged.

## Exact amount expressions — 2026-11-03

Implemented the owner's [amount-expression contract](ledger-editing.md#amount-expressions--owner-go-2026-11-03).
Editable transaction totals, split amounts, opening balances and manual transfers
accept `+`, `-`, `*`, `/`, parentheses, unary signs, decimals and whitespace.
Valid field completion and saving replace the expression with the formatted amount;
only the answer reaches storage/history. Final fractional minor units are rejected,
while intermediate fractions remain exact. No formula history or format change
(48/47). Existing import/storage decimal parsing remains strict.

The independent engine parser uses checked, reduced unsigned-64-bit rational
magnitudes, bounded input length and nesting. Oversized intermediate calculations
are rejected explicitly. Split remainders and existing atomic commands receive
validated Money values; no storage transaction or schema changes were needed.

Verification on macOS:

- Rebuilt the rules-enabled Release application at `build/SpentIt`.
- Release engine tests plus offscreen `import.navigation` and `import.qml` passed
  **13/13**. Engine expression cases include precedence, signed/zero arithmetic,
  exact fraction cancellation, currency scales, final precision rejection,
  division by zero, malformed syntax, signed monetary boundaries, intermediate
  overflow, length/depth bounds and 6,231 exact cancellation combinations.
- Six focused Qt cases passed (**8/8** including setup/cleanup): expressions in
  opening corrections with reopen, split evaluation, automatic-remainder identity,
  manual split editing, remainder removal and manual transfer/acceptance/reopen.
  The navigation test exercises expression-based manual creation, answer-only
  display and reopening. A final focused manual-entry run passed **3/3** including
  setup/cleanup after adding rejection of `10/3`, verifying that its text remains
  editable and no transaction is created.
- The engine expression suite passed ASan/UBSan with leak detection disabled.
- `git diff --check` passed. Native macOS interaction, Linux/Windows and the full
  storage recovery suite were not run for this parser/UI change. No new durability
  behavior is claimed; existing money-valued storage commands are reused.

## Manual-entry Undo presentation — 2026-11-03

Removed the temporary Undo add transaction button at the owner's request. The
existing standard Undo shortcut remains available (Cmd+Z on macOS, Ctrl+Z on
Windows/Linux), including in the clean transaction editor. No Undo behavior or
storage changed. Rebuilt `build/SpentIt`; offscreen `import.navigation` and
`import.qml` passed (2/2). `git diff --check` passed.

## Manual-entry rules and Delete fixes — 2026-11-03

Fixed both owner-reported failures under the follow-up Go. Rules explicitly support
manual records with no imported allocation associations and match their current
payee/memo. Uncategorized manual allocations can receive a rule category, retain
manual text, and pass subsequent rule evaluation and reopening. The editor now
uses a plain-language fallback if rule checking fails instead of exposing internal
association/provenance terminology.

Delete selected now confirms and atomically removes saved new transactions,
including reopened manual entries and mixed accepted/new selections. New deletions
are represented by the existing encrypted selection journal, without accepting the
row or changing formats 48/47. The ledger, balances, rules, acceptance and reimport
recognition exclude deleted new rows. Session Undo restores the whole selection;
accepted-only selections retain their existing bulk preview/history Undo. A new
linked transfer requires Unlink first, with no partial group changes. See the
[updated contract](manual-transactions.md#rules-and-deleting-saved-transactions--fixes-authorized-2026-11-03).

Verification on macOS:

- Rebuilt `build/SpentIt`. Final focused Release tests `import.manual_transactions`,
  `import.navigation`, and `import.qml` passed (3/3, offscreen Qt).
- Extended storage coverage includes manual rule evaluation before/after Apply
  and reopening; reopened manual deletion; deletion persistence, retry and Undo;
  mixed accepted/new removal; one-of-many imported deletion; acceptance of the
  remaining rows; invalid selection rejection and later reimport recognition.
- New deletion recovery cases cover selected full-disk/write/sync failures,
  cancellation after the first child write, and process exits immediately before
  and after durable commit. Recovery and exact retries preserve whole-group
  deletion/Undo and reconcile balances. These are sampled fault points, not an
  exhaustive new deletion sweep.
- The extended `import.manual_transactions` test passed in rules-disabled Release
  and ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`). An initial test-harness failure
  installed the fault VFS twice; installing it once fixed the harness, and both
  configurations then passed.
- The bounded related Release regression passed 19/19: Review, ledger and selection
  editing, transfer assignment/release, rules preview/evaluation/application,
  category gates, balances, reimport recognition, committed history/validation,
  and the accepted-transaction UI. Together with the focused run, 22 selected
  Release tests passed. An earlier overly broad selection of existing exhaustive
  fault sweeps was stopped before completion; no result is claimed for that run.
- `git diff --check` passed. Native manual interaction, Windows and Linux remain
  unverified.

## Manual transactions — 2026-11-03

Implemented the owner's [manual-entry contract](manual-transactions.md). The
ledger context menu offers Add transaction…, and a + button sits immediately
right of search Apply. Existing unsaved work uses Save / Discard / Stay. Creation
opens an unsaved row with the focused selected transaction's date (today with no
selection), blank text, Uncategorized and zero at the account currency's scale.
Enter validates and saves; Escape cancels creation. The new row stays visible
through the current search; saved date changes follow the transaction's identity.

Manual creation is one encrypted, atomic repository command with exact retry and
durable outcome lookup. Entries participate in the inclusive balance, normal
editing/session Undo, acceptance, committed history and account Discard. Creation
Undo is session-only and ends at the next dataset mutation. It is excluded from
Undo last import. The internal creation journal is explicitly manual and supplies
no original imported amount or bank-file bytes. Formats advance to **48 with
rules / 47 without**; 46/45 and older files are rejected unchanged, without
migration. Earlier sections record their dated format checkpoints.

Later imports suggest same-account/currency, exact signed effective amount and
same-date matches without automatically consuming unmatched manual entries.
Review Merge keeps a manual survivor and its categories, splits and notes, attaches
the imported original, and supports atomic Review Undo. Ambiguous selections
containing several manual entries require choosing a manual survivor. Subsequent
reimports recognize the confirmed bank occurrence. The original-import checkbox
is disabled until an imported occurrence is attached; Restore imported values is
still unavailable for manual survivors because their splits have no imported
allocation identities. Transfer accounts become selectable after the first save.

Verification on macOS:

- Rebuilt the rules-enabled Release application at `build/SpentIt` and the
  existing Release test targets; no compiler warnings were reported.
- A 39-test selected Release regression run passed 38 and exposed a new
  cross-account batch-Review lookup error. Fixed the lookup to use account-independent
  batch metadata. Final `ctest --test-dir build/import-ui-rules-release -R
  '^import\.(manual_transactions|review_batch|review_sheet|navigation|qml)$'
  --output-on-failure -j 2` passed all five tests with `QT_QPA_PLATFORM=offscreen`.
  The other selected checks covered engine money, ledger editing and selection,
  import recognition/rollback/Undo, verification cache, incomplete cleanup,
  account lifecycle, transfer storage, category history, committed history and rules.
- The new storage test covers create/retry/conflict, absent imported original,
  reopen, balanced mixed-sign splits, pending and committed edit Undo, account
  Discard, invalid dates/splits, acknowledgement loss, generation-guarded creation
  Undo, explicit Merge/Undo and subsequent reimports. It also checks corrected
  manual date/amount matching, category retention, mismatched date/amount exclusion,
  and ambiguous manual survivors against an already accepted bank entry.
- Selected injected full-disk/write/sync failures and subprocess exits immediately
  before/after commit recover to a complete old/new state and retry without a
  second transaction. These are selected failure points, not an exhaustive fault
  sweep.
- `import.manual_transactions` passed in `build/import-ui-no-rules-release` and
  `build/import-ui-rules-sanitized` (ASan/UBSan, `ASAN_OPTIONS=detect_leaks=0`).
  The rules-disabled `import.schema_gate` also passed, including rejection of
  formats 45/46 without modifying their files.
- The Qt navigation test covers both creation entry points, filtered visibility,
  zero/default fields, disabled original control, cancellation, selection-date
  reuse, Save / Discard / Stay, reopen and following a saved date change.

Limits: automated Qt interactions used the offscreen platform; native interactive
macOS use and Linux/Windows builds were not verified in this change. The complete
158-test Release suite was not run. No performance claim is made for large manual
ledgers, and there is no unsaved-entry crash recovery or migration.

## Undo a completed import — 2026-11-03

Implemented the owner's [import-undo contract](import-undo.md). Account right-click
**Undo last import…** and Review **Undo import…** share source-scoped preview and
atomic undo. Review filters imports by account, newest first; both routes show the
whole file's affected accounts before confirmation. Account-wide Discard remains
separate. Formats stay **46/45**, including existing documents. Per owner choice,
labels use type, transaction dates, record count and order; filenames were not
persisted and no migration or format extension was added.

Source associations protect earlier and shared transactions, including an earlier
source undone before its final remaining owner. Source-local cleanup removes
exclusive generated counterparts and removes a later bank proposal without
removing a committed counterpart. Explicit Review Links into committed work restore
unchanged prior relationship/transaction snapshots through persistent history.
Records needed to replay that surviving history remain in a discarded batch;
conflicting later links/edits or shared linked rows refuse the whole operation.
Account/category creation, aliases and opening choices remain. Imports containing
accepted transactions are not offered for whole-import undo.

Verification uses synthetic data only on macOS arm64:

- Final Release `statement.import_undo` passed after grouped-file coverage (5.98 s):
  wrong-account second import, preserved earlier edit, earlier/later shared source
  removal and final-owner cleanup, duplicate-only import against accepted work,
  whole mapped-file removal, generated counterparts, committed bank proposals,
  direct Review Link with/without a pre-existing generated counterpart (ordinary
  and grouped inputs), failed rows, stale previews, scope-bound retry, reopen and
  restored encrypted backup.
- Release offscreen `undoSelectedImport importUndoOutcomes
  datasetAcceptanceAndRollback unifiedReviewLinkDifference` passed **6/6** QtTest
  cases including setup/cleanup (6.072 s). Coverage includes menu enablement,
  cancellation, latest and older import selection, acceptance disabling undo,
  before-save failure, lost acknowledgement, unknown outcome and failed refresh.
  The final two focused UI tests passed again (**4/4** including setup/cleanup,
  2.701 s). The confirmation dialog was rendered and visually inspected offscreen.
- Final rules-disabled Release `statement.import_undo` passed (5.63 s). ASan/UBSan
  passed that storage suite (15.36 s; leak detection disabled), with no diagnostics.
- Final Release recovery sweep passed (271.97 s), together with the functional
  suite (**2/2**, 271.98 s total): **181 ordinary / 137 committed-link** write/sync
  positions. It exercises both paths with alternating disk-full/write failures, abrupt exits at every observed
  write/sync position, before/after durable-boundary exits, reopen, receipt/retry
  and encrypted marker checks. The earlier sweep also passed (285.56 s).
- Existing Release rollback, reimport and overlap checks passed. Rebuilt Review,
  incomplete-preparation cleanup and QML smoke checks passed **3/3** (48.27 s).

Native Cocoa execution was attempted but the test process reported no screens and
aborted before interaction; native interaction remains unverified. Linux/Windows
and the full unrelated suite were not run. The rules-enabled Release application at `build/SpentIt` was rebuilt; the running
application was not restarted. The user's financial document was not opened or
modified. The existing working-tree split-editing changes were preserved.

## Stable balancing split — 2026-11-03

Owner follow-up Go implemented in the [split-editing contract](ledger-editing.md#split-amount-editing--owner-go-2026-11-03).
The initial automatic remainder is the largest eligible absolute amount, preferring
the total's sign; ties retain row order. Account destinations/aliases and linked
allocations are excluded. The editor marks the chosen row **Automatic remainder**.
Its allocation identity persists through balanced saves, restart, acceptance and
backup, so a newly entered larger split never takes over. Manual editing/removal
of the chosen row disables automatic adjustment. Removing an earlier row retains
the chosen identity. No eligible row means manual balancing; mixed signs remain
supported. Undo restores the amounts, identity and manual mode together.

Current experimental formats are **46 with rules / 45 without**. Saved revisions,
retry equality, rule protections and audit checks include the identity. Earlier
experimental formats are rejected unchanged, without migration. The rules-enabled
Release application at `build/SpentIt` is rebuilt; the running app was not restarted.

Verification on macOS arm64 with synthetic fixtures:

- Focused Release offscreen checks initially passed 6/6 QtTest cases including
  setup/cleanup (4.862 s): imported fee/purchase allocations, a larger new split,
  reopen, acceptance, preceding-row removal, Undo, manual mode, signed selection,
  stable ties, transfer exclusion, invalid amounts and int64 minimum magnitude.
- Native Cocoa final split checks passed 6/6 (3.155 s), including removal of the
  initial balancing row before its first save and Undo of that removal.
- Ten selected Release suites passed (83.19 s): editor, selection editing, rule
  application/schema/reapply/account application, committed/category/rules UI and
  QML smoke.
- Rules-disabled Release editor and schema-gate checks passed 2/2 (9.36 s).
- ASan/UBSan focused UI checks passed 6/6 (6.988 s; Qt leak detection disabled),
  and storage editor checks passed 1/1 (14.37 s), with no sanitizer diagnostics.
  Storage editor coverage checks restart, restored backup and committed history
  Undo of the balancing identity.

- The extended Release edit/Undo fault sweep passed 1/1 (202.59 s), exercising
  137, 129, 125, 125, 153 and 145 injected disk-full/write-failure positions across
  six modes, retries, and process interruptions before/after durable commit.
  Assertions cover the identity together with amounts and manual mode. This sweep
  preceded the small follow-up allowing removal of the initial chosen row before
  its first save; that case passed final native and sanitizer UI checks.
- The first full Release import UI run passed 72 cases and failed two obsolete
  repeated-restore assertions expecting Split 1 to absorb the adjustment. Those
  now assert the unchanged fee and adjusted purchase. The final full offscreen
  rerun passed **74/74** QtTest cases including setup/cleanup (89.029 s), including
  12 repeated edit/restore cycles each for new and committed transactions.
- Compiler logs had no warnings; relative-link target checks and `git diff --check`
  passed. Linux and Windows remain unverified.

## Split Enter crash investigation — 2026-11-03

Owner Go: investigate the intermittent crash after repeated edits/restores,
adding a third split, typing a negative amount and pressing Enter. **Unresolved;
no production fix claimed or applied.** The supplied report terminates on the
main thread with EXC_GUARD / GUARD_TYPE_MACH_PORT / INVALID_OPTIONS during
QQuickAccessibleAttached::setName → AppKit accessibility notification →
_XMIGPostNotification. The worker was rebuilding Review in a post-command
snapshot; this does not prove whether the owner's edit was durably saved.
No private report, transaction text or dataset was copied into the repository.

Added `repeatedSplitRestoreEnter` synthetic UI coverage for new and committed
transactions: real amount-field key events, editor Restore imported values,
selection Restore imported values, session Undo, original-source visibility,
250 background rows, periodic close/reopen and exact saved split/total checks.
The test waits for deferred editor focus restoration before entering keys.
Optional `SPENTIT_SPLIT_REPRO_CYCLES` extends each case (default 12, cap 100).
`SPENTIT_SPLIT_AX_TRACE=1` counts accessibility events in this test only, forwarding
every event through the original handler/native bridge; it logs no field values.

Native Cocoa Release on macOS arm64 / Qt 6.12:
- Traced run: 24 edit/restore cycles, 4/4 QtTest cases including setup/cleanup,
  14.070 s. Accessibility reported active. Status-label name changes: 286/293;
  other name changes: 6432/6890; announcements: 286/293 (new/committed).
- Untraced run: 80 cycles, 4/4 cases, 47.341 s. No reproduced crash.
- ASan/UBSan offscreen run: 24 cycles, 4/4 cases, 60.386 s; no sanitizer
  diagnostics (Qt leak detection disabled). This does not exercise native Mach
  notification delivery. Linux/Windows and live assistive-client churn remain
  unverified. `git diff --check` passed.

The dynamic status label is a demonstrated source of notifications matching
this stack, but the report does not identify its originating QML object.
[Mozilla bug 1838947](https://bugzilla.mozilla.org/show_bug.cgi?id=1838947)
reports the same macOS notification stack and intermittent INVALID_OPTIONS
failures in other applications; that is supporting context, not proof of a shared
root cause. No accessibility suppression or speculative arithmetic change was
introduced. An exact reproduction or another owner report is still needed to
establish a fix. Existing app binary and financial behavior are unchanged.

## Sidebar account drag ordering — 2026-11-03

Owner Go implemented in the [sidebar drag contract](../features/005-transaction-sheet-navigation.md#sidebar-account-drag-ordering--owner-go-2026-11-03).
Drag an active account's name/balance row: a translucent rendered copy follows
the pointer, the original dims, and a blue line marks the insertion point. Drop
saves the order; Escape or an outside drop cancels. Long lists scroll near their
edges. Clicks still open accounts. No sidebar controls were added.

Order is saved as a local display preference per canonical document path, using
only account IDs under the existing hashed Settings group. It survives reopening
on this installation, appends new accounts and leaves archived/fixed sections
separate. It does not travel with copied/moved documents or backups. Reordering
publishes only after Settings synchronization and does not invoke a financial
command or clear session Undo. Existing experimental formats remain 44/43.

Verification on macOS arm64, rules-enabled Release:

- Rebuilt the application and navigation test executable; `build/SpentIt` is
  updated. The owner's running application was not restarted.
- Full offscreen navigation suite: 9 passed, 0 failed, 1 skipped (opt-in diagnostics),
  including setup/cleanup, 11.917 s. New actual-mouse tests cover upward/downward
  drops, insertion-line coordinates, checked preview visibility, Escape/outside
  cancellation, ordinary clicks, unchanged selected account during drag, invalid
  permutations, reopen in a new model, independent documents, appended accounts,
  and edge scrolling through 16 accounts.
- Native Cocoa drag and scrolling cases: 4/4 including setup/cleanup, 5.329 s.
  A synthetic screenshot was inspected; it exposed an initial stale line position,
  corrected by calculating the window-relative insertion point during movement.
  An intermediate concurrent GUI run failed its second drag; the final native
  run was isolated and passed. The owner subsequently confirmed the interaction
  works in the running application. The offscreen position assertion was also corrected
  to compare scene coordinates, accounting for its in-window menu bar.
- `git diff --check` passed. Settings-write failure injection, Linux and Windows
  remain unverified for this change.

## Single suggested Review link — 2026-11-03

Owner Go implemented: a Review link question with exactly one candidate selects
that candidate automatically, enabling Yes when the worker is idle. Multiple
candidates still require a choice. Yes continues through the existing link
preview and split-selection validation; selection alone does not save a link.
See [Review interaction](review-sheet.md#user-interaction).

Verification: rules-enabled Release application and UI tests rebuilt on macOS
arm64. Five focused offscreen QtTest cases passed (7/7 including setup/cleanup,
6.020 s): multiple link candidates, single-candidate amount-difference linking,
unified Review, batch Review workspace and link exclusions. The single-candidate
case confirms the checked radio and enabled Yes without clicking the candidate,
including after dismissing the amount-difference popup. The multiple-candidate
case checks neither candidate is selected initially and Yes enables after a choice.
Updated `build/SpentIt`; the running application was not restarted. Native mouse
interaction, Linux and Windows remain unverified for this change.

## Split amount editing — 2026-11-03

Owner Go implemented in the [ledger editor](ledger-editing.md#split-amount-editing--owner-go-2026-11-03).
Split 1 absorbs the exact remainder, including negative values, until its amount
is manually edited. That choice persists in encrypted transaction revisions.
Balanced edits save immediately on Enter; mismatches retain the in-memory buffer
and last balanced saved transaction. Leaving or quitting offers Change amount /
Keep editing. Change amount saves the total and splits together, then continues
only after success. Undo restores amounts and editing mode. Removing the first
split disables automatic adjustment; other removals recalculate the remainder.
No arithmetic expressions, recovery drafts or migration were added.

Experimental formats are **44 with rules / 43 without**. Metadata, history
comparison, retry checks, rule protections, format gates and audit verification
include the new flag. Earlier formats are rejected unchanged. Updated the
rules-enabled Release application at `build/SpentIt`; the owner's running
application was not restarted.

Verification uses synthetic fixtures on macOS arm64:

- Release focused split/immediate-editor UI checks passed 4/4 QtTest cases
  including setup/cleanup (3.478 s). Coverage includes negative remainders, the
  manual $0.20 amount after reopen and acceptance, mismatched Enter, Keep editing
  during quit, Change amount when closing, Undo, precision rejection, overflow,
  and cancellation across values whose intermediate sum exceeds int64.
- Release editor storage/history checks passed, including pending/committed and
  rule-assigned transactions, restart, restored backup and persistent history
  Undo. The first editor/failure sweep passed 2/2 (206.74 s total), checking the
  flag and values together across disk-full/write failure, retry and process
  interruption on both sides of durable commit. Final rerun recorded below.
- Rules-disabled Debug editor and schema rejection checks passed 2/2 (10.84 s).
- ASan/UBSan editor storage checks passed 1/1 (15.34 s); focused split/immediate
  UI checks passed 4/4 (13.105 s), with leak detection disabled for Qt. Final UI
  follow-up recorded below.
- Native Cocoa split interaction passed 3/3 including setup/cleanup (1.592 s).
  The initial sandboxed native attempt had no window-server access and aborted;
  the authorized native rerun above passed.

The first broader 15-suite Release run passed 11/15. It exposed a split-removal
callback running after its delegate was destroyed (fixed by moving removal and
recalculation into the window), assertions expecting worker errors instead of
inline amount errors (updated), and an obsolete rule-button layout assertion
(updated to its existing Add split row). A keyboard/search timing check failed
under parallel load, then passed alone (3/3, 1.008 s); the serial full UI rerun is
recorded below. Validation failure also clears a pending navigation action, so
later Enter cannot accidentally resume a previously failed leave/quit request.
Final Release failure sweep passed **1/1** (196.90 s) on the final storage code.
The six edit/Undo modes exercised 137, 129, 125, 125, 153 and 145 injected fault
positions respectively, plus before/after-commit process interruptions and retries.

Final serial Release UI rerun passed **7/7** suites (149.20 s): import UI,
navigation, recovery, committed edits, categories, rules and QML. The eight
selected storage/rule suites in the earlier broader run also passed. Final
compiler logs contained no warnings. Relative-link target checks and
`git diff --check` passed.

Final ASan/UBSan UI follow-up passed 5/5 split/immediate/navigation QtTest cases
(10.545 s) and 3/3 add/remove-empty-split cases (2.465 s), including setup/cleanup,
with no sanitizer diagnostics. Linux/Windows interactions and a full all-tests
sanitizer run remain unverified.

## Owner-reported Linux build and launch — 2026-11-03

The owner built the rules-enabled Release application on Linux and launched its
window using the preparation notes in
[Linux development setup](linux-development-setup.md). The recorded hosts are
Ubuntu 24.04.5 LTS, x86_64, with GCC 13.3.0 and Qt 6.8.3 `gcc_64` (2026-11-02),
and an aarch64 QEMU virtual machine using Qt 6.8.3 `gcc_arm64` (2026-11-03).
The guide gives separate aqt, configure, and launch paths for those two
architectures. apt packages, PCRE2, and SQLCipher stay shared. This is a
developer dependency/build guide, not packaged-application runtime requirements.
The native MSVC build of this checkout is recorded in
[Windows development setup](windows-development-setup.md).

The x86_64 host reported warnings; the guide retains the observed Mesa/EGL
messages. Warning investigation is deferred. Mesa output from the aarch64
launch was not recorded. No Linux automated test suite, broader interaction
verification, other distribution, or warning-free build is claimed. The x86_64
commands were not rerun when filing this document. Checkout paths in the
repository copy are placeholders. README linking and download packaging remain
deferred as requested.


## Ledger rule button placement — 2026-11-02

Moved Create rule (or Edit rule/Edit rules when applicable) to the left of Add
split on the same transaction-editor row. Existing action visibility and behavior
are retained; rule attribution remains below. Rebuilt rules-enabled Release
`build/SpentIt`; `import.qml` passed (1/1, 1.50 s) and `git diff --check`
passed. Native layout inspection and other platforms were not rerun.


## Shared rule and ledger category completion — 2026-11-02

Owner Go implemented. Both editors use `src/ui/CategoryCompletion.qml` for
select-all on focus, subsequent mouse text selection, live case-insensitive
substring filtering while the popup remains open, keyboard highlight/acceptance,
and plain-text category names (including literal ampersands). The existing
scrollbar appearance and narrow-window popup positioning are retained.

Rules supply ordinary categories and accept the choice into the unsaved rule;
there is no Uncategorized or transfer destination choice. The ledger supplies
its existing eligible categories/accounts, retains Uncategorized, and calls the
same immediate category/transfer commands with split navigation and session Undo.
Caller-specific eligibility and financial commands remain outside the shared
component. See the [rules contract](../features/004-rules-and-explanations.md#shared-category-completion--owner-go-2026-11-02)
and [ledger contract](ledger-editing.md#shared-category-completion--owner-go-2026-11-02).

Verification: rebuilt the rules-enabled Release application and affected UI
test targets. The seven-suite CTest selection (import UI, navigation, recovery,
committed UI, categories, rules and QML) passed **7/7** (158.89 s).
Rules passed **25 QtTest cases**, categories **7**, including setup/cleanup.
Navigation retained one optional skipped check. New/focused coverage includes
replacement after focus, partial mouse drag selection, filtering with retained
focus, literal ampersand labels, no matches, Escape cancellation, draft-only rule
selection, excluded Uncategorized, ledger scrollbar drag, split navigation,
immediate assignment and Undo. All fixtures were synthetic.

Native Cocoa focused rules and ledger completion checks each passed **3/3**
including setup/cleanup (1.249 s and 4.395 s). A native narrow-window capture was
inspected and showed the ampersand correctly in the field and result row. The
final scrollbar styling was checked by those native tests and the category/rules
suites; the broader import/navigation/recovery/committed binaries were built before
that presentation-only adjustment. `git diff --check` passed. Linux, Windows and
rules-disabled builds remain unverified. Updated `build/SpentIt`; the owner's
running application was not restarted.


## Quit after dismissing the rules dialog — 2026-11-02

Fixed a regression introduced by the separate rules confirmation window. Its
close handler rejected every close event, even while hidden after Discard/Save/
Stay. Qt application shutdown attempts to close hidden windows with native
handles too; the hidden confirmation could therefore veto Quit after the main
window disappeared. The handler now accepts closure when the confirmation is
inactive or the document-close audit has authorized final exit. Active-dialog
close/Escape still mean Stay. Document auditing and failure recovery are unchanged.

Verification: a regression assertion reproduced the old failure after Discard:
`dialog->close()` returned false. Rebuilt rules-enabled Release application
(`build/SpentIt`) and rules UI tests. Full offscreen rules suite passed **24/24**
QtTest cases (28.779 s), including a child-process test that discards a rule and
exits through the document-close workflow. The final version of that test uses
an application Quit request instead of a main-window close; it passed offscreen
**3/3** including setup/cleanup (0.772 s), and native Cocoa together with the
dialog lifecycle case **4/4** (3.595 s). The parent verifies normal child-process
exit; the child verifies the document is closed. Synthetic fixtures only.
This tests the shutdown path rather than a physical Cmd+Q keystroke or the exact
transaction-prefill sequence. Other platforms remain unverified.
`git diff --check` passed; the owner's running application was not restarted.


## Rules dialog layout and native window correction — 2026-11-02

Owner Go implemented after the screenshot showed excessive vertical space and an
attached macOS sheet. The rules confirmation now uses application modality, so
macOS presents a separate framed dialog. Height follows content, with 20-pixel
outer padding and 16-pixel message/button spacing; the content layout no longer
stretches vertically. Save / Discard / Stay and the prior navigation semantics
are unchanged. This corrects the window-modal implementation described below.

Verification: rebuilt the rules-enabled Release application at `build/SpentIt`
and rules UI tests. The full offscreen rules suite passed **23/23 QtTest cases**
(27.239 s). Native Cocoa testing with window access passed the independent-draft
and invalid-save cases, **4/4 including setup/cleanup** (4.336 s). The native run
checks application modality, a nonzero top window frame, programmatic movement,
content-sized height, Escape/close cancellation, save/discard and invalid-save
atomicity. The native content capture was inspected for spacing and clipping.
Physical title-bar dragging was not exercised; Linux/Windows remain unverified.
The final native run includes an added Cocoa-only frame assertion after the full
offscreen run. `git diff --check` passed. The owner's application was not restarted.


## Movable rules leave dialog — 2026-11-02

Owner follow-up Go implemented: leaving Rules with unsaved edits now opens a
separate window-modal dialog with Save / Discard / Stay. A normal dialog title
bar permits repositioning. The underlying workspace is not dimmed; the selected
rule, search and scroll position remain in place. Save and Discard concern all
outstanding rule edits; selected-rule editor buttons retain their existing scope.
Escape and window close mean Stay. Buttons and dismissal are blocked during the
operation. Validation failure cancels navigation, hides the dialog and reveals
the invalid rule. This supersedes the inline bar and selection-on-leave behavior
in the November 1 checkpoint below. See the
[contract](../features/004-rules-and-explanations.md#single-rules-workspace-and-independent-edits--owner-go-2026-11-01).

Verification: rules-enabled Release application and rules/navigation test targets
rebuilt successfully. The full offscreen rules suite passed **23/23 QtTest cases**
(including setup/cleanup, 26.935 s); navigation and QML CTest checks passed **2/2**
(9.91 s). Coverage includes dialog modality/parent, repositioning, exact button
labels, close/Escape cancellation, selected-rule retention, save/discard navigation,
and invalid-save atomicity with return to the editor. No QML warnings appeared in
the final rules run. `git diff --check` passed. Updated `build/SpentIt`; the
owner's application was not restarted.

A native Cocoa test attempt aborted at window creation with “no screens available”
in this execution environment (exit 134), before either requested interaction case
could run. Native title-bar dragging/appearance, other platforms and the full
project suite remain unverified. The movement check above changes the window
position programmatically; it is not a physical drag test.


## Ledger selection Rename payee and Restore — 2026-11-02

Owner Go implemented: right-click Rename payee and Restore imported values work
on single/multiple selected imported transactions, including filtered selections
and mixed new/committed rows. Rename accepts one manual payee value. Restore
always confirms the selection count and replaced fields. Each command validates
all members before one atomic write and offers one visible group session Undo,
with Cmd+Z/Ctrl+Z and viewport/selection retention. Full requests and snapshots
use the encrypted journal; retries match the original request. Committed child
history cannot be undone individually. No schema change. See the
[contract](ledger-editing.md#selected-rename-payee-and-restore--owner-go-2026-11-02)
for restoration, transfer and session Undo restrictions.

Rules-enabled Release rebuild and 10 selected CTest suites passed (48.53 s):
selection edit, ordinary editor, Review sheet, transfer assignment/release,
committed/category/rules UI, navigation and QML. Storage coverage includes mixed
new/committed rename and full restore, exact ownership/split/date/amount restoration,
balance reconciliation, group Undo, retry conflict, stale request rejection,
invalid/missing-original members rejecting the entire selection, cancellation
after a child write, backup/reopen and committed transfer restoration/Undo.

New selection fault sweeps passed (190.09 s), injecting disk-full/I/O failure at
every observed write/sync position for Rename, Restore and each group Undo,
checking whole-selection outcomes, reopening and identical retries. Process exits
immediately before/after durable commit also passed. ASan/UBSan ordinary and
selection editor checks passed 2/2 (15.81 s), without diagnostics. Rules-disabled
selection storage and committed UI passed 2/2 (13.62 s).

Focused offscreen UI checks passed: single and multi selection, rename through
search, Cancel, confirmed Restore, native Undo shortcut, stacked group Undo,
restart, lost acknowledgement and failed view refresh (4 Qt results, 4.604 s).
A later 150-row viewport test passed (3 Qt results, 1.171 s), checking retained
anchor/offset and multiselection through rename and Undo. The Restore confirmation
was rendered at 1440×850 and visually inspected; no QML warnings were observed
in the new interaction regression. Changed-document relative links and
`git diff --check` passed.

Limitations: generated counterpart entries, pending linked transfers, unavailable
original fields/categories, changed currencies and removed original source-split
associations cannot use bulk Restore; the operation rejects the entire selection
and directs individual editing. Generated counterpart entries also reject Rename.
Committed transfer coverage uses the existing planner; no new transfer discovery
or relationship creation is added. The full test suite, native desktop interaction,
Linux and Windows were not run. `build/SpentIt` is updated; the running application
was not restarted.

## Restore imported text and rule application — 2026-11-02

Owner Go implemented: Restore no longer marks restored payee, transaction memo
or retained source-split memo as manually entered. The editor sends explicit
restoration intent and tracks subsequent typing independently. Storage accepts
source ownership only for exact imported text when releasing manual/rule
protection; ordinary edits of source-owned text still become manual. Historical
verification permits those exact restorations while preserving category ownership,
source association and rule-application protections. No schema change or automatic
rewrite of existing transactions. Already affected rows can be restored/saved
again, then categorized with the rule. See the [contract](ledger-editing.md#restore-imported-values--owner-go-2026-11-02).

Rules-enabled Release application rebuilt. Final selected CTest pass: 14/14 in
35.68 s, covering rule UI/application/account Apply/reapply/validation, category
assignment/history/validation, committed basics/history/validation and ledger
navigation/committed UI/QML. New UI cases exercise new and committed Restore →
Apply, payee/memo changes, group Undo, reopen, manual typing after Restore
(including identical text), releasing manual protection on another Restore, and
committed split-memo Restore/Apply/Undo/reopen. Storage tests reject false source
claims, verify exact split/transaction restoration, inject a first-write disk-full
failure, check rollback and retry, and reopen. The original Restore/repair UI
checks also passed (4 Qt results, 3.196 s).

Existing committed I/O and crash sweeps passed (150.11/153.66 s) before the final
adjustment preserving ordinary source-text-save canonicalization; those full
sweeps were not repeated afterward. The final targeted restoration disk-full
check passed. Rules-disabled category assignment/validation passed 2/2 (8.22 s).
An earlier corruption fixture was updated to reject different text falsely marked
as source, rather than rejecting an exact original-value restoration now allowed
by the contract. `git diff --check` and changed-document relative links passed.
Native interaction, sanitizers and other platforms were not run for this fix.
`build/SpentIt` is updated; the running application was not restarted.

## Split-only editor buttons — 2026-11-02

Owner Go implemented: Remove and Create rule from split memo are hidden when
only one allocation remains. Remove also rejects direct/queued activation on
that final allocation. Add split and existing transfer protections remain.
The category width now derives from the scroll viewport, avoiding a recursive
layout calculation when controls disappear. See the
[contract](ledger-editing.md#split-only-editor-actions--owner-go-2026-11-02).

Rules-enabled Release application and committed UI target rebuilt. CTest
`import.committed_ui` and `import.qml` passed 2/2 in 13.05 s after correcting
the layout warning found on the first run. The updated add/remove regression
checks one-to-two-to-one allocation transitions, both buttons' visibility,
direct final-line removal rejection, unchanged saved splits/balance and no QML
warnings during the edit. `git diff --check` passed. Native interaction and
other platforms remain unverified. `build/SpentIt` is updated; the running
application was not restarted.

## Compact original transaction and Restore — 2026-11-02

Owner Go implemented: original inspection now shows only Format, Restore imported
values and the selectable source snippet. The indicator has an explicit click
target, and the editor reserves right-side scrollbar clearance. Restore fills the
whole unsaved editor, including the original split count, using source ordinals
to retain matching split identities. Enter saves through existing validation;
Escape discards. Missing original dates/amounts are blank, not guessed. Individual
copy controls and their helper are removed. See the
[contract](ledger-editing.md#restore-imported-values--owner-go-2026-11-02).

Rules-enabled Release application and affected UI targets rebuilt. Targeted
`inlineSourceRestore` and `recordRepair` passed (4 Qt results including setup and
cleanup, 3.260 s): actual indicator click, no early persistence, Escape, Enter,
restoring two edited splits to one, source retention/reopen, accepted-editor
availability, scrollbar clearance and rejection of an unavailable original date.
CTest `import.navigation`, `import.committed_ui` and `import.qml` passed 3/3 in
21.27 s. Inspected a synthetic 1440×850 render showing the compact panel and
unobscured Restore button. `git diff --check` passed. Full import UI suite was not
repeated for this follow-up; native macOS interaction, other platforms and
restore-specific linked-transfer cases remain unverified. `build/SpentIt` is
updated; the running application was not restarted.

## Ledger duplicate-decision cleanup — 2026-11-02

Owner Go implemented: the import duplicate-decision row is absent from normal
ledger inspection. Review retains the controls, including Keep removed versus
Include as separate transaction for removed-record reimports. Correct and
record-level import-blocker navigation now open inspection inside Review.
Merge reversal remains Review Undo; no storage or financial behavior changed.
See the [contract](ui-import-cleanup.md#duplicate-decisions-belong-in-review--owner-go-2026-11-02).

Rules-enabled Release application and affected UI test targets rebuilt.
`removedReimportRequiresReview` passed both choices (4 Qt results including
setup/cleanup, 2.427 s), checking hidden ledger controls, Review Correct routing,
available decisions and exact projected/accepted balances. CTest `import.ui`,
`import.navigation` and `import.qml` passed 3/3 in 93.87 s. `git diff --check`
passed. Native interaction and other platforms remain unverified. The running
application was not restarted; `build/SpentIt` contains the updated Release build.

## Ledger transfer navigation arrows — 2026-11-02

Owner Go implemented; see the [contract](../features/005-transaction-sheet-navigation.md#ledger-transfer-arrows--owner-go-2026-11-02).
Linked single-split rows show a small incoming → or outgoing ← button before the
category. Saved split viewers show it separately for each linked split. Zero
amounts use ↔. The worker supplies direction and exact saved counterpart
identities, including attached imported endpoints; unresolved, released or removed
links offer no arrow. Tooltips and accessible names describe the destination.
Click uses the existing guarded account navigation, reveals/selects the matching
transaction and records the departing viewport for Back. No financial/storage
or format change.

Rules-enabled Release build passed. Targeted offscreen `transferManagementUi`
and `qifTransferResolution` passed (4 Qt results including setup/cleanup): both
directions between imported postings, selection identity/count, and a split-row
button leading to its generated counterpart. The ordinary fee split has no
navigation entry. Navigation and QML CTest suites passed 2/2 in 9.66 seconds,
including the existing viewport restoration regressions. A synthetic 1440×850
render was inspected. The broader `import.ui` run passed 66 Qt results and failed
one stale starter-category count assertion (41 expected, 46 actual) in 83.32 s.
That expectation was updated for the existing catalog; the rebuilt isolated
`configuredDatesAndCategories` case then passed (3 Qt results, 0.903 s). The full
suite was not repeated after this test-only correction. `git diff --check` passed.
Native interaction, spoken accessibility, mixed-sign/zero
arrow-specific cases and other platforms remain unverified. The running
application was not restarted; the Release build updates `build/SpentIt`.

## Accounts grouped below categories — 2026-11-02

Owner follow-up Go implemented: account destinations form a separate trailing
section in Categories, transaction completion, the category picker and the ledger
category menu. Accounts always sort case-insensitively A–Z, independent of the
ordinary category A–Z/Z–A/Order added choice. Newly created categories remain
below accounts during the current visit, then join the ordinary section on return
or explicit sorting. Account visibility and transfer restrictions remain intact.
See the [contract](../features/010-categories-and-selection.md#category-selector-widths-and-visit-sorting--owner-go-2026-11-02).

Release build and targeted offscreen category/rules UI and QML suites passed.
The category regression uses three accounts with names interleaved alphabetically
with ordinary categories, checks each sort mode and verifies the new-row exception.
No storage or format change. Native interaction and other platforms remain
unverified; the running application was not restarted.

## Category widths and alphabetical selectors — 2026-11-02

Owner Go implemented; see the [contract](../features/010-categories-and-selection.md#category-selector-widths-and-visit-sorting--owner-go-2026-11-02).
Transaction completion, the ledger category menu, the category picker and rule
selector now use case-insensitive alphabetical ordering. Categories opens A–Z on
every visit, including Back/Forward; additions stay at the bottom during that
visit unless the user explicitly sorts. Z–A and Order added are visit-only choices.
Category controls measure current labels with font metrics; dropdowns can exceed
the field width within window bounds. Menu labels preserve literal ampersands.
The owner's revised starter-category list is retained. No format change.

Verification: rules-enabled Release build passed. All four targeted suites
(categories UI, rules UI, navigation and QML) passed in 46.68 s. Coverage included
creation placement, departure/Back/Forward/reopen ordering,
selection, filtering and long-label field width. Rendered 1440×850 and 720×700
transaction views were inspected: “Utils: Electricity, Gas & Heating” fits in both
the field and completion list; narrow ledger horizontal scrolling remains.
Native desktop interaction and other platforms remain unverified. The running
application was not restarted.

## Category-gated rules and imported categories — 2026-11-02

Owner Go implemented; see the [accepted contract](../features/004-rules-and-explanations.md#next-implementation-category-gated-rules-and-imported-categories--2026-11-02).
Every rule requires an explicit category. Split rules run first, then transaction
rules fill remaining Uncategorized allocations. Existing categories and manual
text stay protected; deliberate category clearing permits another assignment.
Transaction fallback on split rows changes categories only. Inheritance and force
replacement are removed. Selected Preview/Apply respects saved surrounding rule
order while writing only the selected rule; raw/categorized/earlier/eligible/change
counts accompany the eligible match list. Independent editor saves remain.

All normal UI imports use atomic account preparation and one missing-category
checklist spanning the import's accounts and declared catalog. Initially selected
names can be deselected, with All/None, Apply Selected and Use Uncategorized.
Selected names are created; declined effective assignments become Uncategorized;
original source fields remain. Existing categories and transfer aliases resolve
separately. Automatic rules run after resolution. A closed dialog remains pending
across restart and blocks rules/acceptance for those rows; the sidebar reopens it.
Repairs honor the saved choices. Creation, assignments, rules and decision finish
atomically, with identical retries supported and changed retries rejected.

Current experimental formats are **42 with rules / 41 without**. Earlier files
are rejected without migration, per the disposable pre-release policy.

Verification uses synthetic data on macOS. Release builds and selected engine,
preview, ordered application, account Apply, committed reapply, restart, multiaccount
category/repair and rules-off format checks passed. Category-decision fault sweeps
passed at all 185 write/sync positions with disk-full/I/O failures, cancellation,
retries and process interruption on both sides of commit. The fixture covers
category creation and automatic rule assignments in the same operation. Account Apply fault sweeps passed
(101.25 s); reapply failure checks passed (1.54 s). Seven selected ASan/UBSan suites
passed without diagnostics (9.03 s). The offscreen rule UI suite passed 20 QtTest cases;
the category dialog was rendered and visually inspected, including restart.

The final 11-suite Release pass (125.89 s) covered engine/preview, category
decisions, selected/account Apply, reapply and offscreen import/category/rules UI
and QML. Navigation and committed UI also passed after their fixtures explicitly
resolved missing categories. A broad regression was stopped during unrelated
backup fault checks; no full-suite pass is claimed. Native desktop interaction,
Linux and Windows remain unverified; the running application has not
been restarted. Low-level legacy ingest APIs remain for internal tests/history;
normal UI imports use the new preparation and category-decision path.
Changed-document relative link targets and `git diff --check` passed.

## Ledger rule action placement and wording — 2026-11-01

Owner Go implemented: Create/Edit rule is left-aligned, with field-specific
assignment text beside it. The text identifies Payee, Memo or Category and the
rule name, including split numbers where needed; internal revisions/identities
are not shown. Existing saved attribution and rule actions are unchanged.
See [the contract](../features/004-rules-and-explanations.md#actionable-ledger-rule-attribution--owner-go-2026-11-01).

Rules-enabled Release rebuild passed. The focused rule creation/attribution
regression passed (3 QtTest cases including setup/cleanup, 1.96 s), checking the
exact useful text and the button's left-edge placement; QML smoke passed (1.27 s).
`git diff --check` passed. Native interaction and other platforms were not rerun;
the running application was not restarted.

## Single rules workspace and independent saves — 2026-11-01

Owner Go implemented; see [the contract](../features/004-rules-and-explanations.md#single-rules-workspace-and-independent-edits--owner-go-2026-11-01).
Rules now use one searchable list and expanded editor. New/changed rows are
highlighted, selection is distinct, and one new draft is allowed. The New rule,
Edit rule and Categorisation subheadings, All rules switch, Preview all enabled
rules button and duplicate editor Enabled control are removed. Save/Discard sit
beside the editor. Row Enabled retains its saved document-wide meaning.

Selected Save/Save and apply merges only that identity into the saved definitions,
retaining other local drafts, pending deletions and reorderings. Preview is
optional; Apply still prepares a fresh validated single-rule operation internally.
Stale preview output is hidden. Save and Apply remain separate durable operations,
with existing account scope, manual protections, transfers and Undo unchanged.
No financial/storage implementation or format changed.

Leaving selects the new draft, otherwise the first changed rule, and reveals a
nonmodal Save all and leave / Discard all and leave / Stay bar above the editor.
Save all preflights pending definitions through the existing worker preview
validation before one atomic full-image save; failure selects the invalid rule
without writing any definitions. Save all does not apply transactions. Discard all
drops only outstanding local changes, retaining earlier successful saves/applies.
Deferred post-layout scrolling reveals the new row and editor in a 26-row list.

Verification: rules-enabled Release build passed without compiler warnings.
The six-suite offscreen regression (import UI, rules, categories, navigation,
committed UI and QML) passed **6/6** (83.02 s). After the final independent-reorder
and post-layout-scroll corrections, rules/navigation/QML passed **3/3** (27.03 s);
the rules suite passed **19 QtTest cases**, including setup/cleanup. The optional
diagnostics privacy case was skipped because diagnostics testing was not enabled.
Coverage includes incomplete-new plus existing-rule Save and apply without a
Preview click, persisted definition and transaction results after reopen,
discard-all retaining those results, invalid save-all leaving the saved revision
unchanged, new/first-changed leave selection, one-new enforcement, search clearing,
independent enable/order changes, and visible new-row/editor/leave-bar geometry.
Synthetic offscreen renders of the 26-row editor and leave bar were inspected.
Existing cancellation, manual-value protection and post-apply refresh-failure
coverage passed. Native interaction, narrow-window visual inspection and other
platforms remain unverified; the running app was not restarted.
Documentation link-target checks and `git diff --check` passed.

## Consistent search clearing — 2026-11-01

Owner Go implemented: one shared ClearableSearchField supplies the inline × in
ledger search, category manager/picker, currency picker and synthetic sheet.
Separate Clear buttons are removed. Clear focuses its field; ledger clearing
retains the existing edit guard, query and selection/viewport restoration. Its
× remains available when applied filtering remains despite manually emptied text.

Rules-enabled application and synthetic-sheet Release builds passed. Initial
checks caught a Qt SearchField type-name collision and then an inherited signal
handler clearing text before the ledger guard. Both were corrected. The full
import UI run passed 66 cases and failed that guard case before its correction;
the corrected search-return/combined-search selection passed all four QtTest
cases (3.03 s). Final category UI and QML checks passed (15.86 s / 1.25 s).
Synthetic-sheet smoke/interaction checks passed 2/2 after the correction;
`git diff --check` passed.
The complete import UI suite was not repeated after the guard correction.
Native visual inspection and other platforms remain unverified; the running
application was not restarted.

## Category view controls — 2026-11-01

Owner Go implemented: search has an inline × clear button shown only for nonempty
text, restoring focus to search. Show archived excludes deleted entries even when
checked. New category precedes the sort buttons with a gap; a thin divider marks
the top of the category list. Internal deleted definition history and receipts
remain unchanged. See [the contract](../features/010-categories-and-selection.md#category-view-controls--owner-go-2026-11-01).

Rules-enabled Release build passed and updated `build/SpentIt`. Offscreen
`import.category_ui` and `import.qml` passed **2/2** (17.07 s), including a
pointer click on search clear, restored focus/list contents, and deleted-category
exclusion with Show archived enabled. `git diff --check` passed. Native visual
inspection and other platforms were not tested; the running app was not restarted.

## Remembered account sorting — 2026-11-01

Owner Go implemented; see [the contract](../features/005-transaction-sheet-navigation.md#remember-account-sorting--owner-go-2026-11-01).
Each account retains its ledger sort column/direction in local QSettings, saved
immediately when a successful query changes the sort. Unchanged sorting performs
no settings write. Default is date ascending. Restoration happens before building
the selected account's ledger view, including fresh application instances.
Settings are scoped by canonical document location hash and account identity;
account renaming retains preferences, different files remain isolated and explicit
Create/Overwrite clears preferences for the replaced file. Moving a file starts
with fresh preferences. No financial storage or dataset-format change.

Rules-enabled Release build passed without compiler warnings. Navigation, QML
and committed UI checks passed (3/3, 9.56 s). New coverage checks immediate settings
persistence, actual row order, independent account choices, direction changes,
fresh model/reopen, account rename, file isolation and overwrite reset. Existing
split-removal regression remains passing. `git diff --check` passed. Native
interaction, other platforms and settings I/O-failure injection were not tested;
the running application was not restarted.

## Removing an empty split — 2026-11-01

Owner Go: the split Remove handler marks the window editor dirty before removing
its own delegate from the split model. Previously removal destroyed that QML
context before the subsequent unqualified `dirty` assignment, causing an invalid
global-property write. Added stable Add/Remove control names for regression checks.

Rules-enabled Release build passed. `import.committed_ui` and `import.qml` passed
(9.79 s); the new regression adds an empty split, immediately removes it, checks
that no QML warnings occur and the editor remains dirty, then saves and verifies
the original splits and balance remain unchanged. `git diff --check` passed.
Native interaction and other platforms were not rerun; the running app was not
restarted. No financial/storage behavior changed.

## Plain ledger payees — 2026-11-01

Owner Go: removed the repeated `[Not yet accepted]` suffix from the ledger payee
column. Existing pale-yellow new rows, light-grey accepted rows and blue selection
highlight remain. This is display-only; stored payees and acceptance state do not
change. Rules-enabled Release build and `import.qml` passed; `git diff --check`
passed. The running application was not restarted.

## Direct ledger category assignment — 2026-11-01

Owner Go implemented; see [the contract](../features/010-categories-and-selection.md#direct-ledger-category-menu--owner-go-2026-11-01).
The right-click Change category submenu applies its choice immediately, with a
bounded scrollable list and no ordinary category confirmation/preview dialog.
The old route called committed-only bulk preview even for new rows, and required
a second Apply for accepted rows. Both new and accepted selections now use the
existing atomic assignment/group Undo operation. Ordinary categorisation permits
unresolved new rows while retaining their duplicate-review decision. A split
selection asks only which split; ambiguous multi-split selections remain refused.

Category assignment and Undo capture the ledger anchor, offset, row fallback and
horizontal position before mutation and restore after layout settles, including
failures. Selection identities remain selected; refreshed editor data no longer
shows the previous category. No format change or automatic transaction acceptance.

Verification: rules-enabled Release build passed without compiler warnings.
The five selected navigation, committed UI, QML, transfer assignment and release
checks passed (14.68 s). The assignment fault sweep passed (138.40 s), exercising
I/O failure, retry, and process termination around the durable boundary. Added
storage coverage checks cancellation rollback, idempotent retry, unresolved review
status preservation, reopen, audit and group Undo for mixed new/accepted rows.
The full import UI suite passed earlier in this change (79.65 s); focused category
transfer and ledger interaction cases also passed. Final navigation/QML checks
passed after the hover correction (6.41 s), including an 80-category hover menu,
scrollbar, immediate pending assignment, near-bottom viewport retention, mixed
selection persistence and reopened Undo. The synthetic submenu render was
inspected. `git diff --check` passed. Native interaction, other platforms and
sanitizers were not rerun; the owner's running application/dataset were untouched.

## Simpler rule patterns and inline key — 2026-11-01

Owner Go implemented; see [the contract](../features/004-rules-and-explanations.md#simple-generated-patterns-and-regex-key--owner-go-2026-11-01).
Generated payee/split patterns omit the non-capturing wrapper and quote source
text only when it contains regex metacharacters. Whole field supports simple
anchors and older grouped patterns, distinguishing literal/escaped/class symbols
from outer alternatives; it retains necessary grouping for alternatives and
preserves captures and the case-sensitive prefix. Existing saved patterns are not
rewritten automatically. A one/two-line regex key sits immediately below Pattern.

Rules-enabled Release build passed without compiler warnings. All six selected
checks passed (14.02 s): rule regex/evaluation/preview, rules UI, QML and navigation.
Tests cover plain/quoted generated literal matching, embedded quote terminators,
anchors, checkbox round trips, alternatives, classes, escaped dollars, existing
captures, case flags and incomplete quotes. The UI key was checked at normal and
900-pixel window widths (at most two lines); its narrow offscreen render was
inspected. Correcting an incomplete pattern clears the obsolete helper message.
`git diff --check` passed. No matching engine, financial write or dataset-format
behavior was changed. Native interaction, other platforms and sanitizers were not
rerun; the running owner session was left untouched.

## Ledger return position — 2026-11-01

Owner Go implemented; [the contract](../features/005-transaction-sheet-navigation.md#ledger-return-position--owner-go-2026-11-01)
covers per-account session viewport restoration when returning with Back/Forward
or an account button. Previously only cross-account departure captured a viewport;
Categories could reset the hidden ledger model, and same-account return did not
restore it. Departures now capture the first visible transaction and offset,
restore after inline-editor layout, retain horizontal position and use a clamped
row-index fallback when the anchor is gone. Hidden pages cannot overwrite the
saved viewport. Document changes clear positions. Explicit counterpart reveals
retain their existing priority. Focused rule navigation now records its account
in navigation history consistently with other pages.

The synthetic navigation regression passes the actual category-create/Back flow
with an open transaction near the bottom of a 300-row ledger, including its pixel
offset and selected/editor identity. It also checks Forward and account-button
return, independent positions for two accounts, cross-account Back/Forward,
filtering away the anchor into a shorter list, filter retention and document reset.
Release build passed without compiler warnings. All 6 selected UI checks passed
(23.88 s): navigation, Categories, Rules, committed editing/viewport retention,
Review and QML. `git diff --check` passed. No financial/storage behavior changed;
the running owner session was left untouched. Native interaction, other platforms
and persistent viewport restoration across app restarts are not claimed.

## Account-scoped rule Apply and Undo — 2026-11-01

Owner Go implemented; [the scope contract](../features/004-rules-and-explanations.md#rule-scope-and-account-apply--owner-go-2026-11-01)
supersedes the separate pending Apply / accepted Reapply UI described below.
The focused editor and Rules screen offer All transactions in this account
(default) or New transactions only. Single-rule and all-enabled previews use that
scope; changing the rule, scope or replacement choice requires a fresh preview.
After preview, Apply to this account now or Save and apply completes the operation.
Save and apply saves definitions durably, previews the saved definitions, then
applies; an Apply failure leaves the definitions saved. It does not Commit rows.

Account Apply streams eligible ledger transactions across imports, prepares only
changed snapshots, and writes one atomic snapshot-journal group for both new and
committed rows. The request checks account, mutation generation, ruleset/category
revisions, rule selection and manual replacement. Manual values remain protected
by default and live transfer categories remain unchanged. Undo rule application
restores the whole mixed group; intervening mutations end its Undo sequence.
Committed corrections and Undo remain in persistent history. No schema change.
The normal Reapply entry button is removed; legacy receipt and history Undo APIs
remain. Existing per-record evaluator and 128/512 MiB payload budgets remain;
this path has no 4,096-row committed-reapply ceiling.

Release verification: the broader regression passed 24/24 tests (205.33 s).
After the final balance-reuse and account-isolation changes, 11/11 focused checks
passed (96.56 s), including engine rules, storage, QML, rules UI, committed UI and
Review. The rules UI executable passed all 14 Qt cases including setup/cleanup;
its new end-to-end case exercises unsaved single-rule Save and apply, both scopes,
committed and new rows, scope-change invalidation, and group Undo. A synthetic
compact Rules render was inspected.

The mixed new/committed account fault test swept all 209 write/sync fault positions
with disk-full/write errors, reopened and checked all-or-none results, retried the
same operation identity and undid the whole group. Forced process exits immediately
before and after durable commit also recovered as expected. Cancellation after
writes began rolled the group back. Stale previews and wrong-account requests
were rejected; another account stayed unchanged. Live-linked blank transfer
categories remained protected even with Replace; exact balances and committed
history verification passed.

The 5,000-row case now commits all rows before the account Apply, updates the 500
matching committed rows and undoes them, proving this path exceeds the legacy
4,096-row limit. Account Apply and its group Undo reuse an unchanged committed
balance instead of resumming the entire account for each changed row, while
checking every change has zero balance delta.

The direct 100,000-row synthetic run also passed: compact full/single previews,
a 100,000-match result, cancellation, the legacy pending Apply, and the new account
Apply/Undo of 10,000 matching new transactions. Unmatched rows and exact balances
stayed unchanged. The new account preview/Apply/Undo took 0.870 / 10.939 / 49.740 s
in that run, concurrently with other verification; these are observations, not
an isolated performance benchmark. Large Undo remains worker-bound and is not
cancellable; native responsiveness and larger committed-account timings are not
established by this run.

The rules-enabled Release application at `build/SpentIt` was rebuilt without
compiler warnings. The running owner diagnostics session was left untouched;
implementation and verification use synthetic files. Native interaction, spoken
accessibility, sanitizers, rules-disabled builds, Linux and Windows were not rerun.
Payload-budget exhaustion boundaries remain untested; the budgets are payload
accounting, not process RSS bounds.

## Compact matching previews and Apply — 2026-11-01

Owner Go implemented; [the contract](../features/004-rules-and-explanations.md#match-list-preview-and-non-reverting-apply--owner-go-2026-11-01)
supersedes the earlier full-recomputation behavior below. Rules previews now show
one Date / Imported payee / Amount row per matching transaction, with a count or
“No matches.” Unmatched transactions, per-field explanations and change prose are
absent. Multiple matching rules/fields/splits still produce one transaction row.
The result list has a bounded viewport and reuses visible delegates. Unsaved full
previews explain how to enable Apply.

Pending Preview/Apply and accepted-account reapply preserve values and attribution
that no enabled matching action writes. Disabling/deleting a rule, or changing its
pattern away from a transaction, no longer resets earlier assignments by itself.
Manual and transfer protection remain. Pending Apply prepares changed rows and
rechecks revisions before one atomic transaction; no evaluation chunk is committed
separately. Unsuccessful previews clear any previously prepared Apply.

Account and pending-import preview stream one source/draft snapshot at a time
through the existing bounded evaluator, compiling rules once. The UI requests
compact matching rows rather than retaining all before/after snapshots. The old
4,096-total-row preview and pending Apply limit is removed. Individual transaction,
allocation, text and regex limits remain, as does cancellation. Preview result
memory grows with matches; atomic Apply preparation grows with changed rows.
Retained payload accounting limits previews to 128 MiB and pending Apply
preparation to 512 MiB, with visible failure before writes rather than truncation.
These are payload budgets, not process RSS bounds; budget-exhaustion boundaries
have not been separately exercised.

Initial verification: six focused engine/storage checks passed, including the new
match-only and 5,000-row cases. The Qt rules executable passed 13 cases including
initialization/cleanup. The enable-toggle test shows three matches with three
rules, one match with two disabled, and no matches with all disabled. The 100,000-
row result-list test rendered and scrolled in 291 ms with 30 delegate items; the
offscreen render was inspected.

The final direct Release `spentit_rule_store_tests large_preview` run passed with
100,000 synthetic imported transactions. It checked 10,000 matches, a single-rule
account preview, a 100,000-match result, cancellation during preview and Apply
preparation, and atomic Apply of the 10,000 matching transactions. Unmatched rows
and the exact account balance stayed unchanged. Timings from that run:

| Operation | Time |
| --- | ---: |
| Import fixture | 42.355 s |
| Full enabled-rule preview, 10,000 matches | 1.445 s |
| Single-rule account preview, 10,000 matches | 0.816 s |
| Single-rule account preview, 100,000 matches | 0.846 s |
| Cancellation checks, Apply and balance verification | 5.077 s |

The initial `/usr/bin/time -l` wrapper could not read the sandboxed kernel clock
rate after its child passed; the direct rerun above exited successfully. No peak
RSS claim is made. The broader Release regression passed 22/22 tests, including
rule Save/Apply disk-failure and crash sweeps with two changed rows, committed
reapply fault coverage and surrounding import/navigation/category/review UI
checks. After the final payload-budget and single-line-payee changes, another
10/10 focused engine/storage/QML/rules-UI checks passed. The UI test verifies
that an imported payee containing a line break remains one line, and both compact
and 100,000-row offscreen renders were inspected. The Release executable at
`build/SpentIt` was rebuilt; `git diff --check` passed and 214 relative document
links resolved.

Limits: the separate accepted-account reapply still has its existing 4,096-row
limit and group Undo contract. The 100,000-row workload is synthetic and does not
establish a bound for all regexes, category catalogs or split shapes. Native
interaction, spoken accessibility, sanitizers, rules-disabled builds, Linux and
Windows have not been rerun. No private files or the owner's running session were
used.

## Focused transaction rules — 2026-11-01

Owner Go implemented; see the [editor contract](../features/004-rules-and-explanations.md#transaction-rule-editor--owner-go-2026-11-01).
The transaction has one Create/Edit action. Stored field assignments identify
responsible rules even after their pattern changes; multiple contributors open a
chooser, and deleted definitions produce an explanation and creation fallback.
Current matching rules are used only when there are no retained assignments.
Creation copies the imported matching text and current ordinary category. Split
selection explicitly matches the imported split memo. Rule name and pattern have
separate labels, category is an editable completion field, and Enabled describes
the checkbox. Transfer suggestion exclusions moved to Review.

Transaction entry opens a focused editor. Save retains it. Preview this rule is a
read-only test of the editor definition over imported current-account ledger rows,
including accepted transactions. It preserves unrelated rule-owned fields, shows
only matching rows, and clears any previously prepared Apply. The full ordered
Preview all enabled rules remains separate, with its existing pending-import
scope; accepted-account reapply retains its existing workflow. No schema or
financial write contract changed.

Verification: rules-enabled macOS Release built; the full synthetic Qt
rules UI executable passed 10 cases, including initialization/cleanup. Coverage
includes category prefill, single-rule preview before/after Save, full-preview
Apply invalidation, multiple-rule chooser, changed-pattern attribution and deleted
rule fallback. The focused screenshot run passed 3 cases and its offscreen render
was inspected. Engine preview and storage application checks passed, including
preserving other rules' assignments and accepted-row preview without writes.
All 21 selected regression checks passed in 263.12 seconds: rules engine/preview,
all 12 rule-storage cases (including disk-full/I/O and process-interruption sweeps,
backup and reapply), Review storage, and five Qt UI checks. Initial QML syntax
and old-control test-helper failures were corrected before the passing run.
After the final category-field and split-chooser adjustments, all five affected
Qt UI checks passed again in 18.29 seconds. The final rules UI case also verifies
explicit split-memo choice and category prefill without mutating either split.
The local `build/SpentIt` application was rebuilt without compiler warnings;
`git diff --check` passed and 179 relative documentation file links resolved.

Follow-up investigation: the synthetic `savedRulesNavigationAndEnabledChanges`
Qt case passed (3 cases including initialization/cleanup, 1.124 seconds). Saving,
clicking the Rules sidebar button, and previewing all enabled rules retained saved
state and enabled Apply. Changing Enabled boxes after Save produced an unsaved
preview and disabled Apply; saving those changes and previewing again enabled it.
This does not establish which actions preceded the owner's reported screenshot.
No application behavior changed during this investigation.

Limits: single-rule preview retains the 4,096-ledger-row evaluator bound and
reports excess rather than partially evaluating. Generated transfer counterparts
have no imported source and are excluded. Native interaction, spoken accessibility,
sanitizers, rules-disabled builds, Linux and Windows have not been rerun. Only
synthetic files were used; the owner's running session and financial file were
not operated on.

## User-facing wording — 2026-11-01

Owner Go implemented across the main desktop UI and its presentation messages.
The [language guide](user-facing-language.md), linked from AGENTS.md, records the
owner's three questions: what does the user do with this, does it help a decision,
and is it the simplest accurate wording?

Opening, import, Review, transfers, category/rule editing, history, setup,
backup, quit and recovery text now uses actions and relevant consequences.
Acceptance buttons say “Accept transactions”; Link and quit buttons name their
actions. Routine internal identifiers, revision counts, rule-limit inventories,
and redundant preservation/history paragraphs were removed. Original imported
values remain inspectable. Destructive scope, currency conversion limitations,
password consequences and saved-versus-unknown outcomes remain explicit.
The wording follow-up changes presentation, not acceptance or storage policy;
the opening behavior change is recorded separately below. Internal API names,
serialized status values and diagnostic identifiers retain technical terminology.

Verification: the rules-enabled macOS Release build passed, including the local
`build/SpentIt` application. All 38 selected checks passed across the initial run
and reruns: 32 engine/parser, transfer, mapping/lifecycle, Review and UI checks,
plus six rule-storage, opening, import-error/repair and verification-cache checks.
The initial run failed four test executables because assertions expected old
phrases; those assertions were updated and all four executables passed on rerun.
The full import UI rerun covers the cases beyond those earlier assertion failures.
Saved/unknown recovery cases and financial state assertions remain in place.
`git diff --check` passed and 178 relative documentation file links resolved.
Native interaction, sanitizers, rules-disabled builds, Linux and Windows were not
rerun for this wording pass. The running owner application and dataset were not
touched.

## Opening balance without confirmation — 2026-11-01

Owner Go implemented; see [the contract](ledger-editing.md#opening-balance-editing--owner-go-2026-11-01).
The ordinary confirmation strip and confirmation-based import/transfer Commit
blockers are removed. Commit uses the stored opening, including untouched zero.
Double-click Opening Balance or use the account menu to edit its amount/date
through the existing atomic historied correction and account Undo, before or
after Commit. Genuine transfer warnings remain without opening-confirmation text.
Owner wording follow-up: the opening dialog now says “Update the opening date
and/or balance.” Internal tracking and history explanations were removed.
The historical stored flag remains revision metadata; it no longer gates these
operations. Formats remain 40/39 and the source-bound verifier identity changes.

Rules-enabled macOS Release rebuilt without compiler warnings. The initial
15-test selection passed 14; the full import UI test dereferenced the removed
`openingSetup` object. Both obsolete assertions now require that object to be
absent. The rebuilt full import UI rerun passed in 79.12 s, completing all 15
selected checks across the runs. The passing selection covered opening Commit, account lifecycle, mapping
openings, Review, transfers, verification cache, navigation, QML and category UI.
The new UI case double-clicks the initial opening, edits it, undoes it, commits
an import and edits the opening afterward. Storage cases cover untouched-zero
Commit, transfer counterparts, correction/Undo, unchanged evidence and verified
reopening. The extended lifecycle boundary suite passed separately in 19.58 s,
including default-zero acceptance and first opening correction: injected
SQLITE_FULL/I/O failures at first/middle/last writes, retry, and process exits on
both sides of durable commit. This is selected failure coverage, not a complete
write-position sweep.

The active owner process and dataset were not restarted or modified. Native UI
interaction, sanitizers, rules-disabled builds, Linux and Windows were not rerun
for this change. All 175 relative file links in the changed guidance documents
resolved, and `git diff --check` passed. The rebuilt local application is
`build/SpentIt`.

## Inline ledger category completion — 2026-11-01

Implemented the owner's follow-up Go in the [ledger editing contract](ledger-editing.md).
Category is now a text field, blank for Uncategorized, with an anchored dropdown,
case-insensitive substring filtering, first-match highlighting, Up/Down selection,
and an interactive scrollbar. Enter/Tab accepts the highlighted choice and moves
to Memo; clicking also selects. Date, Payee, Amount, Category and Memo have explicit
forward/reverse navigation, with allocation memo/amount traversal for multiple
splits. Add split remains a button. No matches cannot assign or create a category.
Escape cancels the search before the editor's existing discard behavior applies.

Assignments reuse complete-edit acceptance and existing transfer commands.
Unresolved imported transfer allocations retain account-only explicit resolution.
No financial/storage logic or schema changes; formats remain 40/39.

Release regression passed **5/5** in **104.84 s**: ledger UI, navigation,
committed UI, category UI and QML smoke. Focused transfer checks passed **5/5**
including init/cleanup in **4.613 s**, covering unresolved QIF resolution,
account destination selection and existing context transfer behavior. The new
completion test exercises case-insensitive substring filtering, first-match and
arrow highlighting, Enter/Tab and Shift+Tab, mouse selection, actual scrollbar
thumb dragging, no-match rejection, clearing/Undo and multiple-split navigation.
ASan/UBSan passed this focused test **3/3** including init/cleanup in **4.381 s**,
with no sanitizer diagnostic (`detect_leaks=0`). The test waits for deferred Undo
focus restoration before starting the next pointer interaction.

The synthetic dropdown render was inspected and the rules-enabled Release
`build/SpentIt` was rebuilt. `git diff --check` passed; all **152** relative file
links in changed guidance/status/contract documents resolved. Verification used
synthetic offscreen Qt interactions. Native Cocoa interaction, spoken accessibility,
other platforms and large-catalog latency remain unverified.

## Stable single-click selection — 2026-11-01

Implemented the follow-up Go for flashing in the open editor and sidebar.
Ordinary unmodified row selection now updates the published ledger selection
locally, using cached transfer flags, without a worker command, SQL, model reset
or global busy state. Repeating the same selection/anchor is a no-op; explicitly
choosing a search result still records that choice. The worker's selection and
range anchor stay synchronized, so later bulk actions and Shift selection use the
same identities. Range/toggle selection retains the bounded worker path. Actual
operations still disable controls while running. No storage/financial changes.

The focused Release selection/editor/transfer-category checks passed **5/5**
including init/cleanup (4.652 s). A new pointer-dispatch regression observes no
editor or sidebar enabled-state transitions and no model reset during ordinary
selection; unchanged clicks emit no presentation update. It also checks editor
identity/content, range anchors, multiselection collapse, search-result selection,
and disabled controls during a real save. ASan/UBSan passed the same **5/5** in
**10.849 s**, with no sanitizer diagnostic. UI tests use synthetic offscreen data;
native interaction and other platforms remain unverified for this follow-up.
The broader Release ledger UI, committed UI, navigation and QML regression passed
**4/4** in **79.65 s**.
The rules-enabled Release `build/SpentIt` was rebuilt.

## Ledger editor and session Undo — 2026-11-01

Implemented the owner's Go for the [ledger editing contract](ledger-editing.md).
Double-click toggles the active transaction editor. Ordinary row selection leaves
that editor attached to the inspected transaction, without the former bottom-panel
jump and empty reserved space. Enter accepts the complete valid edit directly for
both uncommitted and editable committed transactions; category-picker choices also
accept. Save draft / Cancel edit and the committed-edit opt-in/Apply step are gone.
Escape discards only the unaccepted buffer. Unsaved navigation remains guarded.
Acceptance retains transaction selection, visible position and the field caret.

Accepted edits have stacked session Undo, using exact encrypted snapshots in the
existing journal. It restores split/source identities, rule ownership and changed
transfer relationships atomically. Other dataset mutations end that session Undo
sequence; inspection/selection/filtering do not. Closing resets the session
capability while saved edits and committed history survive. Persistent committed
history Undo retains its strict eligibility checks. First repair of a malformed
record remains on the existing Review repair path, outside this ledger scope.
Editing/Undo preserves stale duplicate-review stamps rather than reconfirming
matches. Formats remain 40/39; no migration or private-data access.

Release selected regression passed **11/11** in **79.16 s**: ledger UI, committed
UI, category UI, rules UI, navigation, QML, editor storage, Review, batch Review,
and forward/reverse transfer assignments. Final focused Qt checks passed **7/7**
including init/cleanup (6.740 s) for direct editing, selection/placement,
double-click toggling, two keyboard Undo steps, invalid input/Escape, cursor and
viewport retention (including a date change that moves the row in sort order),
restart, committed editing, recycling and source copying.
After the sort-position follow-up, committed UI, navigation and QML passed
**3/3** in **8.64 s**. The final failure/outcome checks passed **4/4** including init/cleanup (1.941 s):
failed acceptance preserves the buffer, lost acknowledgement resolves durably,
unknown outcome closes the session, and failed post-save refresh reports the
saved state accurately. Final storage functional checks also cover signed splits,
removed source-allocation restoration, rule-derived fields, stale duplicate
stamps, retries/conflicts, mutation barriers, backup restoration and linked Undo.

Release editor fault sweeps passed **814 write/sync positions**: draft acceptance
137, draft Undo 129, committed acceptance/Undo 125 each, and linked committed
acceptance/Undo 153/145. Disk-full and I/O failures alternated. Every trial checked
all-or-none values, exact balances, full audit and retry; all six modes also passed
forced exits immediately before and after durable commit. The CTest sweep passed
in **192.56 s**. ASan/UBSan editor functional checks passed with no diagnostic;
rules-disabled Release editor functional checks also passed. Sanitizer fault
sweeps were not repeated. `git diff --check` passed and all **289** relative file
links in the changed Markdown documents resolved.

The rules-enabled Release application at `build/SpentIt` was rebuilt. The synthetic
mid-ledger render was inspected. Native Cocoa interaction was attempted but Qt
reported “Cannot create window: no screens available”; UI evidence above uses the
offscreen platform. Linux/Windows, spoken accessibility and large-dataset save
latency remain unverified. No performance or portability claim is made.

## Changing transfers back to categories — 2026-10-31

Implemented the owner's Go for the reverse right-click category action. Selecting
Uncategorized or an ordinary category routes selections containing transfer links
through the atomic group command. It ends the links and changes the selected
allocations together, removes only disposable generated uncommitted counterparts,
and preserves committed/independently imported counterparts. Origin money and
dates stay. Mixed linked/ordinary selections are supported. Split ambiguity and
invalid relationships surface errors and reject the whole group. The original
ordinary bulk preview remains for selections without transfer links.

One Undo restores counterpart relationships, original categories, field ownership
and rule provenance. Pending-draft audit checks now recognize exact recorded
Review Undo snapshots, including stacked Undo, without relaxing ordinary-edit
ownership checks. Source facts use per-transaction acceptance state even in an
otherwise pending import batch. No schema change; the build-derived verifier ID
changes with the verification implementation.

Release functional tests passed for single/multiple selection, mixed ordinary
rows, a named category, uncommitted generated removal, committed counterpart
retention, independent imported counterpart retention, rule-derived classifications
before/after acceptance, cancellation, stale facts, retry, stacked Undo, reopen
and restored backups. Final focused Qt checks (`contextCategoryTransfer`,
`transferManagementUi`, `ledgerInteractions`) passed 5 cases including init/cleanup,
0 failures, in 5.416 s. The local rules-enabled Release `build/SpentIt` was rebuilt.

The Release regression selection passed 13/13 in 157.04 s: Review, forward/reverse
transfer assignment, category assignments/history/validation, committed
history/validation and rule application/reapply. Fault sweeps passed 181 write/sync
positions for category release and 165 for its Undo, alternating disk-full/I/O
failures; forward assignment/Undo also passed their 109/97 positions. All-or-none
balances, retry and full audit were checked after each interruption, including
process exits before and after durable commit. The final stacked-Undo audit
extension was then checked by the functional release and UI runs.

Final ASan/UBSan Review, release and assignment functional checks passed 3/3
in 26.08 s with no sanitizer diagnostic. `git diff --check` passed and all 209
relative file link targets in the changed guidance/status/feature documents
resolved. Native interaction, rules-disabled builds, large-batch performance and
other platforms remain unverified; sanitizer fault sweeps were not repeated for
this follow-up. Tests used synthetic data only.

## Batch category transfer assignment — 2026-10-31

Implemented the owner's follow-up Go for all selected transactions in the
[right-click category dialog](../features/010-categories-and-selection.md#right-click-transfer-assignment--owner-go-2026-10-31).
Each transaction gets a separate generated counterpart with opposite amount and
its original date. Existing retarget behavior applies to already-linked origins.
One transaction encloses all writes, one durable receipt binds retries, and one
Undo restores the entire group. Undo is available in the ledger, survives reopen
and backup restoration, and respects intervening-mutation barriers. Purely
generated drafts are retired on Undo; imported postings remain. Ledger account
category labels and filtering now derive live destinations from transfer links.
Ambiguous split rows need individual allocation selection. Existing currency,
eligibility and committed-row restrictions remain. No new matching search or
format change; experimental schemas remain 40/39.

Release storage functional checks passed for three separate postings, exact
amounts/dates, cancellation before and during the batch and at the final commit
gate, stale revisions, changed facts, currency mismatch, duplicate selections,
idempotent and conflicting retries, retargeting existing links, stacked Undo,
reopen, backup restore and later-mutation barriers. Fault sweeps passed 109
write/sync positions for assignment and 97 for Undo (alternating disk-full/I/O
failures), with retry and exact all-or-none balances. Process exits immediately
before and after each durable commit also passed. Release regression selection
`import.review_sheet`, `import.transfer_assignment_faults`, `import.review_batch`
and `import.review_policy` passed 4/4 in 55.23 s.

The rules-enabled Release application was rebuilt at `build/SpentIt`. Final
focused offscreen Qt checks (`contextCategoryTransfer`, `transferManagementUi`,
`ledgerInteractions`) passed 5 cases including init/cleanup, 0 failures, in
4.420 s. They cover multiple selection, Cancel, exact destination balances,
ledger destination labels, the visible group Undo button, explicit split choice,
ambiguous multi-row split refusal, existing editor transfers and ordinary bulk
category changes. The ASan/UBSan storage functional and fault selections passed
2/2 in 114.54 s with no sanitizer diagnostic; the final additional retarget/stack
case was verified in Release. `git diff --check` passed and all 208 relative file
link targets in the changed guidance/status/feature documents resolved. Native
interaction, large-batch responsiveness, rules-disabled builds, Linux and Windows
remain unverified. No private financial data was used in testing.

## Right-click category transfer assignment: initial single-selection checkpoint — 2026-10-31

Implemented the owner's Go for [single-transaction account selection](../features/010-categories-and-selection.md#right-click-transfer-assignment--owner-go-2026-10-31)
in the right-click Change category dialog. Account selection invokes the existing
transfer-retarget command; ordinary categories retain bulk preview/apply.
Split rows require an explicit allocation. Current-account destinations and
multi-row transfer assignment are disabled with visible guidance. No storage,
engine, format or matching-policy change.

Rebuilt `spentit_import_experiment` (local `build/SpentIt`) and
`spentit_import_ui_tests` in `build/import-ui-rules-release`.
With `QT_QPA_PLATFORM=offscreen`, focused Qt cases `contextCategoryTransfer`,
`transferManagementUi` and `ledgerInteractions` passed (5 including init/cleanup,
0 failed, 4.055 s). Synthetic coverage checks cancellation, current-account and
multi-selection guards, ledger selection differing from the inspected record,
the exact generated destination balance, existing editor transfers and ordinary
bulk-category preview/apply with viewport retention. Native interaction, explicit
split-picker interaction and other platforms were not verified. Existing engine
and storage transfer restrictions remain; candidate matching and match-or-create
confirmation are separate future work. `git diff --check` passed.

## Shared operation feedback — 2026-10-31

Implemented the owner's follow-up Go for consistent [feedback placement](ui-import-cleanup.md#feedback-placement--owner-go-2026-10-31).
Import mapping, correction/Merge previews, bulk changes, rules reapplication,
Review link exclusions/differences and close-verification failure dialogs show
operation errors locally while suppressing the duplicate status-line message.
Closing them restores an uncleared error to the status line; Details follows the
same ownership rule. Routine Rules results and transfer completion messages use
the status line. Transfer balance-impact details, candidate guidance, substantive
previews, history and record diagnostics remain in their views.

Rebuilt the local rules-enabled Release application. Category, Rules and committed
UI suites passed 3/3 in 28.73 s. Coverage includes one visible error across all nine
error-owning dialogs (including Details), restoration on dismissal, invalid inline
edits, rejected deletion and one visible Rules Save result. Four focused transfer,
Merge and Review checks passed six Qt cases including init/cleanup (5.830 s).
Final rebuilt Merge-preview and Rules preview/apply checks each passed three Qt
cases including init/cleanup (1.358 / 1.968 s). Relative links/anchors and
`git diff --check` passed. Native interaction and other platforms remain
unverified. No format change.

## Category feedback uses the status line — 2026-10-31

Removed the Categories workspace's duplicate error label and redundant error
state. Rejected deletion and invalid-name feedback now use the permanent bottom
status line; invalid edits retain their text and focus. The error Details dialog
remains available. Record-specific diagnostics and modal validation are unchanged.
Rebuilt the local rules-enabled Release application. `import.category_ui` passed
(12.16 s), including rejected deletion with exactly one visible copy of the error
and invalid inline edits reported through the status line. Native interaction
remains unverified.

## Documentation reconciliation — 2026-10-31

Reconciled current format references to 40/39, removed superseded cleanup Go
placeholders, documented sidebar-only Settings and updated recovery, Review and
build guidance. Historical schemas and measurements remain explicitly dated;
feature lifecycle and deferred reports/licensing/platform scope are unchanged.
Documentation validation passed: 789 repository-relative links and heading
anchors across 47 Markdown files, plus `git diff --check`. Two stale roadmap
anchors were corrected. Runtime evidence for the Settings change is below.

## Settings navigation follow-up — 2026-10-31

Removed the duplicate upper-right Settings button at the owner's request; Settings
remains in the lower-left sidebar. Existing navigation, unsaved-change guard and
keyboard/accessibility tests now use that sidebar entry. Rebuilt the local
rules-enabled Release application. Offscreen navigation and committed UI tests
passed 2/2 (10.71 s); the focused accessibility/keyboard check passed all three
Qt cases including init/cleanup (0.787 s). Native interaction remains unverified.

## Opening, ledger and interrupted imports — 2026-10-31

Implemented under the owner's subsequent Go; see the [contract](ui-import-cleanup.md).
Current experimental schemas are **40 with rules / 39 without**. Earlier schema
references below are historical; older experimental files are rejected unchanged.

Opening uses the bundled light logo, centered tagline and truthful Cancel with
bottom progress. The ledger toolbar contains Search/Apply/Clear. Import lives
above Review and in account context menus; routing distinguishes unnamed,
identified, mismatched and multi-account sources. The title uses a home-relative
or absolute dataset path. Review contains Commit, repair progress and mapped
transfer resolution; failed records remain reachable through correction entries.
Category changes use a separate dialog, Discard remains account-owned in its
context menu, and backup/restore remains in Settings. Post-import status totals
all mapped source accounts. Full source bytes and inline original records remain.

Durably captured preparation is removed after opening verification, including
when startup approval is reused. Capture roots, history and exclusively created
accounts are removed atomically; retained account changes, prior pending work,
accepted history and completed imports are preserved. Preparation already saves
rows, associations, categories and generated counterparts atomically with its
completion marker, so no partially published financial rows survive that boundary.
Cancel removes incomplete capture; uncertain/failed cleanup closes the desktop
connection or prevents activation. Cleanup retries after interruption, and durable
absence resolves a lost acknowledgement. Original external files are untouched.

Verification on macOS: the selected rules-enabled Release run completed 105 tests
in 361.10 s, with 101 passing. Three obsolete expectations for retained interrupted
captures were corrected; rebuilt account-group, mapping-opening and rule-application
reruns passed. A final rebuilt selection of those three tests plus recovery UI
passed 4/4 in 17.27 s. The remaining diagnostics QML test could not create its endpoint
under the sandbox (permissions were not relaxed). Full import UI, recovery UI,
committed/category/rules UI, QML, navigation and benchmark smoke tests passed.
Long existing fault/crash sweeps and diagnostics transport were excluded; this is
selected coverage, not a full-suite claim.

The new cleanup suite passed in Release and ASan/UBSan and covers preserved prior
pending and accepted linked work, capture-only account removal, completed malformed
imports, cached startup, cancellation, retries, lost acknowledgements and process
exit on both sides of preparation/cleanup commit. Its measured write/sync fault
sweep exercised 133 positions with rules, and 125 without, alternating disk-full
and I/O errors. ASan/UBSan cleanup plus cancellation passed 2/2 in 115.69 s;
rules-disabled cleanup plus schema gates passed 2/2 in 41.69 s. Sanitizers used
`detect_leaks=0`; no leak-check claim. Six focused import/routing/cancellation UI
functions passed in both configurations (eight Qt cases including init/cleanup).
Extended confirmed-bank routing and home-relative title checks also passed in
Release, ASan/UBSan and rules-disabled Release. All three configurations built
without compiler warnings/errors. The local rules-enabled Release launcher was
rebuilt.

Native OS interaction and Linux/Windows remain unverified. Synthetic offscreen
ledger and Review renders were inspected; no private dataset was used.

## Shorter closing verification — 2026-10-31

Owner Go implemented. A desktop full opening audit now retains an in-memory
approval only when full hashes before/after the read-locked audit agree. Closing
requires a fresh identical hash and unchanged connection write count before
reusing it; rollback and a verification attempt clear it. Cancellation and failed
publication invalidate the session approval through rollback. Existing persistent
approval, authentication, locking, verifier-version and journal gates remain.
Headless opens and backup validation still perform full audits. Schemas stay 38/37.

The full audit now directly reuses its already-loaded immutable rows and keeps a
bounded cache of reconstructed committed snapshots (65,536 entries / 64 MiB
accounted allocation budget). Each historical reference, ordering, ownership,
allocation, currency and reconciliation check still runs. Both caches are scoped
to one audit; no new persistent financial cache is written. See
[the contract](fast-startup.md).

Verification on macOS: the selected Release regression passed 102/103 tests in
167.76 s. The sole failure was an obsolete benchmark assertion expecting the old
per-account receipt's `included` field. The current dataset-wide UI returns an
operation and summary. The benchmark now checks the exact pending count before
Commit, zero pending transactions afterward and a nonempty operation, retaining
its exact money/source/reopen assertions; its rebuilt rerun passed in 9.86 s.
The selection included full import UI, corruption/history validation, lifecycle,
Review, backup/restore and approval tests; long fault sweeps and IPC tests were
excluded. This is selected coverage, not a complete suite claim.

Expanded approval tests passed: unchanged read-only sessions skip a second audit;
successful edits, rejected/rolled-back commands and injected failures immediately
before/after durable commit require another audit. Cancellation, external encrypted
page damage, wrong passwords, stale receipts, journal presence, process interruption,
backup reopening, failed publication/retry and emergency close are covered.
The ASan/UBSan selection passed 4/4 (approval plus committed validation/history/exact
money) in 20.79 s. Rules-disabled Release approval passed 1/1 in 3.37 s. Focused
startup/close/failure/timing UI checks passed 5 Qt cases each under ASan/UBSan and
rules-disabled Release (5.550 / 2.491 s, including init/cleanup). Sanitizers used
`detect_leaks=0`; no leak-check claim. Builds reported no compiler warnings/errors.

Timing evidence and reproduction are recorded below. The owner's running instance
and private dataset were not accessed or changed. Native Dock/window teardown,
Linux/Windows, physical power loss and a full fault sweep remain unverified.

### Closing measurements

Serial macOS arm64 rules-enabled Release runs used a fresh synthetic account with
10,000 accepted transactions, distinct dates and a zero CAD opening. The edited
case adds one -2.00 pending transaction; a final full reopen asserts -10,002.00 CAD.
The baseline is one pre-change run; the final values are medians (minimum–maximum)
of three runs. Files were warm from fixture creation/opening. No builds or other
tests competed with timed runs; unrelated OS activity was not controlled.

| Close path | Baseline certify seconds | Final certify seconds | Final close-to-process-exit seconds |
|---|---:|---:|---:|
| Full opening audit, unchanged session, no saved approval | 4.851 | 0.0092 (0.0089–0.0094) | 0.0212 (0.0210–0.0215) |
| Already-approved unchanged session | 0.0043 | 0.0044 (0.0044–0.0046) | 0.0078 (0.0078–0.0080) |
| Written session requiring full audit | 4.994 | 4.045 (4.028–4.196) | 4.057 (4.040–4.208) |

The written-session audit dropped from 1,140,218 SQL executions to 920,214;
committed-history verification dropped from 1.041 s to a 0.335 s median. These
observations support an approximately 19% reduction in full-close time for this
fixture, not a guarantee for arbitrary histories or disks. The unchanged paths
recorded no full-audit span. Process-exit measurements use an isolated headless
child, from the parent's receipt of its pre-close signal through `waitpid`, including
store destruction and test output. They do not measure native Qt/Dock teardown.

Reproduce each serial trial with:

```sh
SPENTIT_CLOSE_ROWS=10000 build/import-ui-rules-release/tests/storage/spentit_verification_cache_tests
```

The final 10,000-row Qt offscreen timing case passed all 3 cases including
init/cleanup: full open 5.931 s, first unchanged close 0.051 s, approved open
1.826 s and deferred Review 1.822 s. It asserted 10,001 displayed rows and the
exact -10,000.00 CAD balance. Its 50-ms polling granularity limits interpretation
of the close measurement; the legacy output field is still named `audit-close-ms`
even though the unchanged close now reuses the opening audit.

The measured final executable SHA-256 was
`289189177a9c51762912c77d74811132bddea6d68b42c3afaa4cd5cdbd9833ac`.
Diagnostics are read in process and print only phase/counter information; fixtures
are synthetic temporary files removed after the run. The existing UI timing case
is also available with `SPENTIT_STARTUP_ROWS=10000` as documented below.

## Whole-window geometry correction — 2026-10-31

Owner correction implemented: “Remember window geometry” restores size and
position plus maximized/full-screen state. The existing enabled flag is preserved.
Normal geometry is retained while maximized/full-screen so restoring down recovers
the chosen dimensions; minimized windows reopen visibly. Saved size and position
are fitted to an available display, respecting application minimum dimensions.
Only changed values are persisted before quit hides the window. See
[the feature contract](../features/005-transaction-sheet-navigation.md#remember-window-geometry--owner-correction-2026-10-31).

The rules-enabled Release build and focused offscreen startup/geometry checks
passed (10 Qt cases). Native Cocoa checks passed 5 cases, including tall-window
restoration and maximized-to-normal geometry. Counts include init/cleanup.
The full Release UI executable passed all 63 cases (70.484 s). An additional
offscreen full-screen save/restore-and-restore-down check passed (3 cases including
init/cleanup). Final build/whitespace checks were clean. Native full-screen Spaces, actual monitor removal and
other platforms remain unverified. Closing-audit optimization remains deferred to
the next session as requested.

## Remember window position — 2026-10-31

Owner Go implemented: Settings now has an opt-in “Remember window position”.
It uses the existing native preferences store, independently of dataset state and
verification receipts. Enabled windows restore their saved frame coordinates
before showing, with an available-screen fallback/clamp. Graceful quit saves before
hiding only when enabled and coordinates changed. Disabling retains but ignores
old coordinates. Normal window position is saved; window size, maximized/full-screen
state and other layout are not added. Preference-write errors are surfaced.
See [feature 005](../features/005-transaction-sheet-navigation.md#remember-window-position--implemented-2026-10-31).

The rules-enabled Release build passed. Focused offscreen startup/window/close
checks passed 11 Qt cases, and native Cocoa placement and Settings/quit checks
passed 4 cases (both counts include init/cleanup). The tests use isolated settings
and synthetic datasets. The complete rules-enabled Release UI executable passed
62 Qt cases (63.271 s); final build logs and whitespace checks were clean, and
220 relative documentation file links resolved. Other platforms and physical
monitor removal remain unverified.
The [closing-audit optimization](roadmap.md#shorter-closing-verification--owner-go-2026-10-31)
is preserved as next-session work, not implemented in this change.

## Verified fast startup — 2026-10-31

Committed as `76af3d1`. The owner requested [shortening closing verification as the
next handoff task](roadmap.md#shorter-closing-verification--owner-go-2026-10-31),
starting with reusing a successful opening audit during an unchanged session.
That follow-up is recorded, not implemented or separately authorized yet.

Owner Go implemented: the desktop app reuses a full audit only when the complete
file SHA-256 and verifier fingerprint match an owner-private approval in the
platform cache. Authentication, format checks, document locking, required storage
settings and journal/recovery gates remain. Missing/stale approval falls back to
full verification; corruption is never repaired automatically. Graceful quit hides
the window while the serial worker audits and atomically publishes approval. A
failed audit/cache write restores the window with the error and an explicit option
to quit without approval. Emergency close never audits. Schemas remain 38/37.
See [the contract](fast-startup.md).

Initial account loading now defers the full Review suggestion sheet until Review
or Commit is requested. An unloaded count is not displayed as zero. Existing
mutation refreshes and saved Review decisions are unchanged. First use of this
build (or a changed verifier/file) still requires verification; a successful
close establishes approval for subsequent unchanged opens. The current API for
ordinary headless opens and backup validation continues to audit fully.

Before implementation, one authorized native Release opening of the owner's
default dataset measured 19.791 s worker time: 13.582 s verification, 5.202 s Review
construction and 0.621 s acceptance-review preparation. There was one Open and
one verification, with no dropped/stale diagnostic events. GUI model application
was 0.248 s. This was a single observation, not a controlled comparison or a
financial-content audit. The owner's running instance/data were not changed by
the implementation tests.

Verification: the selected rules-enabled Release regression ran 104 checks.
102 passed in the sandbox; `diagnostics.transport` and `diagnostics.qml` could not
create their observation endpoints there and both passed with native/socket
permissions. The selection included the full import UI executable, storage/domain
validation, backup publication/cancellation/restoration, Review and lifecycle
coverage, while excluding the long fault sweeps. An initial selection accidentally
included two long fault sweeps and was stopped; those interrupted runs are not
claimed as passing.

Focused storage tests cover approved/unchecked/changed inputs, wrong passwords,
exclusive locking, truncated/stale approval, journal presence, encrypted-page
corruption, cancellation, interruption during audit, process exit after approval
publication, failed cache publication/retry, restored backup opening, and graceful
versus emergency gateway close. A warm-connection corruption check confirms the
cipher integrity audit reads actual stored pages. Focused UI cases cover deferred
Review, Commit routing, immediate window hiding, and window restoration on failed
approval. Native Cocoa execution of the latter behavior passed 4 Qt cases including
init/cleanup. Final ASan/UBSan and rules-disabled Release builds each passed the
expanded storage approval test and all 10 focused Qt cases (including init/cleanup).
No sanitizer diagnostic was reported (`detect_leaks=0`; no leak-check claim).
Final build logs were free of compiler warnings/errors; whitespace checks and 215
relative file links in the changed documentation passed. Linux/Windows and physical
power loss remain unverified; no performance guarantee is made for arbitrary disks
or datasets.

Three serial macOS arm64 Release/offscreen runs used fresh synthetic one-account
fixtures with 10,000 accepted transactions, distinct posting dates, a zero opening,
and a 21,577,728-byte dataset. Each checked the exact balance (-10,000.00 CAD),
10,001 displayed rows including the opening, and no outstanding Review questions.
The files were warm from fixture creation; no builds or other tests competed with
the timed runs. Durations include worker/model snapshot delivery and the test's
50-ms polling granularity, not native-frame presentation or QML construction:

| Operation | Median seconds | Minimum–maximum seconds |
|---|---:|---:|
| Open without approval, full audit, Review deferred | 5.982 | 5.834–6.042 |
| Graceful close, audit and approval publication | 5.033 | 4.896–5.206 |
| Open with matching approval, Review deferred | 1.861 | 1.812–1.867 |
| Explicit Review preparation after opening | 1.811 | 1.762–1.858 |

Reproduce with `SPENTIT_STARTUP_ROWS=10000 QT_QPA_PLATFORM=offscreen
build/import-ui-rules-release/src/ui/spentit_import_ui_tests cachedStartupTiming`
(on one shell line). The test binary SHA-256 remained
`9705ae31d093743dd46f7114d636066e7ddefaa7977ff6f265af777c54f6490f`.
A preliminary fixture placed every equal-value transaction on the same date and
was stopped because it stressed duplicate matching rather than a spread-out
transaction history; it is not a reported startup measurement. The earlier small
fixture initially omitted the displayed opening row from its expected row count;
that test expectation was corrected. The owner's private workload has not been
retimed on the new implementation; its existing process was preserved.

## Review responsiveness and batch Merge — 2026-10-31

Owner Go implemented: every right-pane workspace has a permanent bottom status
line for progress, feedback and errors, with Details for long errors. Review
preserves its visible group anchor when resolved groups leave. Group selection
is separate from transaction selection; ambiguous groups require chosen rows and
any required survivor. Leaving Review or a dataset mutation clears pending intent;
read-only inspection retains it. No selections persist across restarts.

Batch Merge revalidates selected duplicate groups and stages them in one atomic
transaction. Stale/invalid selections reject the whole batch; cancellation before
the final durable commit gate rolls back all children. One Undo reverses the entire
batch, including accepted/pending rows and source associations, while preserving
later-mutation barriers. Each group retains independent choices/history beneath
one parent receipt. Undo checks the parent request against the complete ordered
child receipts before restoring them. Link remains individual. Schema 38/37 and
matching eligibility are unchanged. See [the contract](review-sheet.md#batch-merge-and-stable-status--owner-go-2026-10-31).

Profiling a synthetic 2,000-row / 1,000-group fixture found repeated source loads
and saved-choice SQL in the same-account peer loop: the initial single decision
plus Review reads took 11.586 s, including 11.552 s in Review construction and
15.151 ms in durable COMMIT. Reusing comparison facts and running the unchanged
engine predicate before saved-choice queries reduced that observation to 0.780 s
(0.746 s Review construction, 14.245 ms COMMIT). Statement reuse calls fell from
4,033,028 to 51,017. Those are single before/after storage observations, not native
UI timings or a claim about the owner's private workload. The runner retains
exact-balance, row-count, verify and Undo assertions.

Three serial repetitions before the final partial-selection follow-up, on fresh
synthetic fixtures (warm restored copies,
macOS arm64, rules-enabled Release, diagnostics enabled) passed all independent
financial assertions. No build or other test competed with these timed runs.
Times include the storage commands and their Review reads, not GUI presentation:

| Workload | Median seconds | Minimum–maximum seconds |
|---|---:|---:|
| One Merge plus Review reads, 2,000 rows | 0.774 | 0.773–0.778 |
| 34 selected groups applied individually | 26.314 | 26.261–26.362 |
| The same 34 groups in one batch | 0.866 | 0.864–0.871 |

The measured test binary SHA-256 stayed
`c80ff7dfbf065da240de3906735d456b97426736b3d3a7078550d9162f4b3358`
through all three runs. Reproduce with
`build/import-ui-rules-release/tests/storage/spentit_review_sheet_tests decisions 1000`.
Other OS activity and thermal/cache conditions were not controlled; these timings
do not promise constant batch cost or the same latency on the owner's dataset.

A final serial run after the unanswered-remainder correction also passed: single
decision 0.791 s, 34 individual decisions 26.449 s, and the 34-group batch 0.874 s.
No other build/test competed. Its unchanged test-binary SHA-256 was
`402ac155e8096d0a57bb48b6de9e01a601e57f3b731b38c5bb7fdb5a013a96ad`.
This final run is one observation, not an additional three-run distribution.

Verification uses synthetic fixtures only. The broad macOS rules-enabled Release
selection passed 97/97 checks; the complete UI executable passed 55 Qt cases before
the additional batch-outcome case. Focused UI checks also cover lost acknowledgement,
unknown outcome, saved-but-failed refresh, group selection lifetime, fixed status
geometry and viewport identity after removing earlier groups. Native Cocoa Review,
Undo and Link-difference interaction checks passed. The sandbox's initial Cocoa
attempt could not access screens; the same checks passed with normal macOS access.
A rendered synthetic Review workspace was inspected. No owner dataset was modified.

The batch Merge/Undo recovery sweep passed 446 injected write/sync positions
(237 Merge, 209 Undo), alternating disk-full and I/O errors, plus four process
interruptions before/after durable commit. Each trial checked all-or-none exact
balances/row counts, reopened and retried from the receipt, verified history, and
restored an encrypted backup. The same sweep had also passed before those stronger
assertions were added. After the final Undo receipt-consistency safeguard, its
209-position sweep and both interruption boundaries passed again. Pending and accepted rows, cross-account/partial groups,
retained-source reimport, stale/duplicate selections, changed retry, effective-total
mismatch, linked-loser refusal, and cancellation including the final boundary are
covered by the focused storage case. New tests initially exposed fixture mistakes
(stale transfer revision/unresolved transfer setup and a missing committed-entry
forwarder in the outcome test double); those were corrected, not production policy.

Final ASan/UBSan and rules-disabled Release selections each passed all three
Review storage checks and four focused Qt Review cases; after the final Undo
receipt-consistency check, each passed those three storage checks again and both
batch UI cases. No sanitizer diagnostic was reported (`detect_leaks=0`; no leak
check claim). The final Release collector/navigation/Review selection passed 5/5,
and the four focused Review UI cases plus the three existing commit/group/transfer
outcome cases passed. All 278 relative links/anchors in the seven changed guidance
and feature documents resolved, and whitespace checks passed. Final build logs
were free of compiler warnings/errors.

A final real-click ambiguous-group test caught a visual-only checkbox problem:
cancelling the survivor chooser left a tick despite an empty pending selection.
The binding now follows staged group intent, and leaving Review closes an unfinished
batch survivor choice. The final five-case Release Review UI selection passed,
including cancellation, explicit survivor choice, partial-group Merge and Undo.
The subsequent native run exposed a second partial-selection edge: merging only
the uncertain import's rows could hide an unanswered clean-original remainder.
Review now seeds that remainder from the saved unresolved choices, without
changing engine eligibility. Deterministic storage tests verify the remainder,
reopen/backup persistence, a later No, and both Undo steps. Final Release,
ASan/UBSan and rules-disabled Review storage selections each passed 3/3; both
partial-selection/workspace Qt cases passed in all three configurations and native
Cocoa. The additional statement review/reimport/overlap, Review-state, navigation
and collector Release selection passed 6/6.

Other platforms, a repeat of the owner's private 3–5-second observation, physical
power loss and complete accessibility behavior remain unverified. The peer loop
still has pairwise comparison cost in dense ambiguous datasets. Reports remain
deferred. The rules-enabled Release executable is rebuilt at `./build/SpentIt`.

## Next-session Review tasks — 2026-10-31

Historical handoff; the implemented result is recorded above.

The owner reports that the common matching correction is much better. Next,
address the reported 3–5-second wait per Review Yes and layout jumps caused by
transient status text below Hide sidebar. The proposed sequence is stable layout,
diagnostics of one decision, then group-level selection and batch Merge. See
[the handoff](../features/005-transaction-sheet-navigation.md#next-tasks-review-responsiveness-and-batch-decisions--2026-10-31)
for interaction proposals, unresolved financial/selection behavior and validation.
This was the documentation-only handoff. The owner subsequently agreed the behavior
and gave Go; the implementation and evidence above supersede that pending state.
The common matching correction and original handoff were included in the earlier
owner-requested commit.

## Common duplicate matching policy — 2026-10-31

Owner Go implemented: all formats now use same-day ordinary amount-based
possible-duplicate matching. The old unlimited-date OFX-family exception is
removed, including its storage SQL branch. Both import classification and row
candidate lookup now filter through the engine matcher; Review uses the same
policy. Strict automatic recognition tests original evidence rather than format
labels, while preserving account, uniqueness, liveness and occurrence guards.
Retained bank-ID sets remain visible to candidate matching after Merge, avoiding
an accidental ID-less fallback when multiple IDs have been attached.

The shared internal record remains named `qif::Record`; renaming it is not needed
for policy parity. Transfer matching, schema, timezone handling and account
settings are unchanged. The [contract](review-sheet.md#common-duplicate-policy--owner-go-2026-10-31)
supersedes earlier format-specific date guidance. Previously recorded explicit
duplicate choices retain their date allowance during history validation; new
suggestions never receive it. Saved relationships still require compatible
original evidence, target/account validation and stale-target checks.

Verification: final selected Release parser/matching/Review checks passed 7/7;
ASan/UBSan and rules-disabled selections passed 5/5 each. The new synthetic matrix
covers all 25 format pairings, same-day versus adjacent/years-apart amounts,
clean initial imports, exact candidate membership, Review grouping and restored
backups. Additional cases cover retained multiple bank IDs after explicit Merge
and a synthetic historical off-date decision: a recorded human choice survives
reopen/acceptance/backup, while a relationship without that decision is rejected.
Existing strict overlap/re-import, Review-state, transfer management, amount
repair and candidate-limit regressions passed. Selected UI/navigation/diagnostic
workload checks passed 3/3. Both bank-ID and original-field re-import failure
sweeps passed, each covering 165 preparation and 105 association write/sync
positions plus durable-boundary interruptions/retry; the original-field sweep
was repeated successfully after the saved-decision safeguard. Documentation
relative links and whitespace checks passed. The rules-enabled Release application
was rebuilt and launched; diagnostics confirmed successful dataset opening with
no health failures. The owner reports that matching is much better; systematic native interaction
coverage and other platforms remain unverified.

The 1,000-row / 2,000-counterpart synthetic workload completed with retry,
balance, acceptance, reopen and restored-backup assertions passing. Its timed
import was 0.962 s and counterpart generation 0.525 s, consistent with the prior
0.980 s / 0.530 s observation. No build or other synthetic test ran concurrently
with this timed import. This is one synthetic observation, not a new measurement
of the owner's import or a timing distribution.

## Adjacent-day duplicate experiment withdrawn — 2026-10-31

An owner-authorized experiment broadened same-account possible-duplicate review
to equal signed original amounts/currencies on adjacent calendar days, including
already imported rows and same-import peers. Automatic recognition remained
strict. The owner rejected the result after the base-file import produced 155
review issues where the previous same-day policy produced none. The broad
amount/date rule creates too much review noise for legitimate repeated postings.

The implementation and its test changes have been reverted to the prior matching
behavior; the import-performance fix is retained. Do not restore this global
window based on the earlier Go. Any narrower date-tolerance policy requires
further discussion and authorization. Recognizing the same transaction across
exports with differing dates remains an unresolved need; no narrower policy has
been selected. Timezone inference and per-account settings remain unimplemented. No dataset edits or Review decisions were made by the agent.
The experiment's focused checks passed, but they did not establish useful matching
precision; its broader recovery/regression runs were stopped after owner rejection.
The restored rules-enabled Release build completed, and statement parser, Review
sheet and Review policy checks passed 3/3.

## Batched counterpart generation — 2026-10-30

Owner Go implemented: unresolved explicit counterparts now use a local batch
working set instead of calling the general transfer command for every link.
The helper requires the caller's open transaction, reuses prepared statements
and accepted starting balances, keeps individual request/version/history rows,
and validates the complete resulting transfer set before durable completion.
Currency, writable-account, collision and sequence checks remain. It preserves
the accepted generation policy, including ambiguous imported counterparts.
No schema change or migration is introduced. See [the implementation contract](review-sheet.md#batched-counterpart-generation--2026-10-30).

Verification: selected Release storage checks passed 17/17; the
complete import UI, navigation and diagnostic workload selection passed 3/3.
ASan/UBSan generated-batch, Review and grouped-import checks passed 4/4 with no
sanitizer diagnostic; the same rules-disabled selection passed 4/4. The new
synthetic case checks signed splits, exact accepted/inclusive balances, retry,
acceptance, reopen and restored backup. Grouped cancellation after staging a
generated child verifies rollback and retry; the recovery suite now generates
three counterparts, including mixed-sign splits, in its import failure scenario.
The Review-policy recovery sweep and account I/O/crash selection passed 3/3;
grouped-import I/O/crash checks passed 2/2, each sweeping 1,186 write/sync
positions across ten scenarios, with durable-boundary interruption/retry checks.

One serial synthetic before/after comparison (rules-enabled macOS Release,
1,000 rows / 2,000 signed-split counterparts) measured generation at 13.843 s
before and 0.530 s after; whole import was 14.282 s before and 0.980 s after.
No build or other test competed during either timed import. These are single-run
observations, not a timing distribution or a claim about the owner's workload.
Both the pre-change and optimized large workloads passed their independent
balance, retry, acceptance, reopen, history and restored-backup assertions. The smaller 120-row
case passed in Release, ASan/UBSan and rules-disabled builds. The local application
is rebuilt. The owner repeated the same file into a fresh dataset and measured
23 seconds. Diagnostics measured 22.179 s of mapped-import worker execution,
versus 157.366 s before optimization (about 7.1× faster). Counterpart generation
fell from 136.920 s to 1.549 s. Draft population was 10.824 s, reciprocal matching
3.049 s, and final snapshot 3.481 s. The import succeeded with confirmed saved
persistence; the resulting generation reached interaction readiness and native
presentation observation. The trace reported no dropped events or health failures.
These are single native before/after observations, not a distribution or independent
verification of private financial contents. Other platforms remain unverified.

## Import regression instrumentation — 2026-10-30

Owner-authorized profiling reproduced a fresh-dataset mapped import at 149.66 s
of worker execution. Approximately 143.14 s elapsed between captured-source
selection and sealing preparation, with 3.35 s in the final snapshot. The trace
reported no dropped events or collector health failures. Those original spans
cannot distinguish draft materialization, reciprocal matching and generated
counterparts; at that checkpoint, counterpart generation was only a hypothesis.
No private source contents or dataset paths are retained here.

Following explicit Go, five function-scoped import timings were added using the
existing bounded collector. No financial queries, policies or transaction boundaries
changed. The semantic UI publisher now reserves the removed projected-toggle slot
instead of referencing the missing object. See [the timing contract](application-diagnostics.md#import-phase-timings--2026-10-30).

Verification: rules-enabled Release build passed without compiler warnings/errors;
collector, 120-row grouped diagnostic workload and account-group tests passed.
The workload emitted all five completed spans and retained its independent financial
assertions. The diagnostics-enabled navigation suite initially failed because one
test inherited the previous test's deleted temporary dataset in Settings; clearing
test Settings between cases fixed isolation, and the complete navigation suite
passed on rerun (2.12 s). Native isolated smoke collected 79 samples and observed
an exposed window and matching presentation readiness. `git diff --check` passed.
The owner repeated the same fresh-dataset workload with the new spans. Worker
execution was 157.366 s: capture 0.294 s, draft population 10.544 s across six
accounts, reciprocal matching 2.895 s, explicit counterpart generation 136.920 s,
and final snapshot 3.572 s. The grouped preparation parent was 153.489 s and
includes those child phases; do not add it to them. Generation accounts for 87%
of worker execution. Execution succeeded with confirmed saved persistence, and
the resulting generation reached native presentation observation.

The bounded polling window expired before this repeat, but all 109 events were
retrieved afterward from the same process/session's ring, sequences 1–109 with no
stale cursor, truncation, dropped events or health failures. No exact native
readiness elapsed time is claimed from this post-run observation. Source inspection
shows generation calls the general transfer command for each counterpart; that
command reloads the growing transfer set, computes balances and validates the full
set repeatedly. This identified the optimization candidate, without measuring a breakdown
of the 136.920 s within that function. The subsequent Go, implementation and
verification are recorded in the batched-generation entry above.

## Opening setup during import mapping — 2026-10-30

Owner Go implemented in the working tree: concise mapping text, new-account
opening choice defaulting to zero or explicitly using the reported amount literally,
and per-source-account earliest valid dates with an editable 1900-01-01 fallback.
Existing accounts hide these controls and retain their opening settings. Fixed
creation-time choices persist atomically with account creation, affect the inclusive
balance immediately and do not trigger another opening prompt after repairs or
Commit. Invalid selected reported amounts fail visibly. Account opening corrections
remain separate explicit commands.

Current experimental schemas are **38 with rules / 37 without**; this revision
requires a new dataset. Older formats, including 36/35, are rejected without
migration. Four new capture/resume/Commit/Abort recovery scenarios passed 536
injected write/sync positions, eight before/after durable-boundary interruptions,
idempotent retries and verified backup restores. Functional storage checks passed
for both opening choices through repairs, restart and Commit; selected UI checks
passed in Release, ASan/UBSan and rules-disabled builds. The 95 selected Release
regression cases passed after updating the old post-repair opening expectation and
rerunning the complete import UI suite. Final mapping/cancellation checks passed six
Qt cases each in Release, ASan/UBSan and an explicitly configured `SPENTIT_RULES=OFF`
build (`build/import-ui-no-rules-release`). That rules-disabled build also passed
account groups, mapping openings and schema rejection. The pre-existing directory
named `import-ui-release` had rules enabled during this run; it is not the evidence
for the rules-disabled results. The local rules-enabled `./build/SpentIt` is rebuilt.
Screenshot inspection confirmed the new controls and their absence for an existing
account. Native interaction, Linux and Windows remain unverified. The entries below
describe prior checkpoints.

## Category-aware link suggestions and Review placement — 2026-10-30

Owner Go implemented: ordinary categories, including unmapped imported text,
disqualify an allocation from link suggestions on either endpoint. Empty and
Uncategorized/Uncategorised allocations remain eligible; resolved account names
must identify the other endpoint's account. Splits are checked independently,
including when accepting a suggestion. Category edits refresh the candidate cache.
Exact case-insensitive Opening Balance and Starting Balance payee labels are also
excluded. Manual Link, duplicate recognition and known-transfer waiting tasks keep
their existing behavior. Review is now above Categories in the sidebar action group.

Verification: five focused Release storage tests, two ASan/UBSan Review tests,
and six Qt offscreen cases passed. Synthetic cases cover both endpoints, bound and
unmapped Gifts, both Uncategorized spellings, account destinations, opening labels,
split selection, category-edit invalidation and manual linking. Screenshot inspection
confirmed sidebar order. The local rules-enabled `./build/SpentIt` is rebuilt.
The schema remains 36/35; existing datasets on that format still work. Native
interaction and other platforms were not exercised for this follow-up.

## Next: Review workspace and link learning — 2026-10-30

Owner testing rejected the initial Review presentation and link-suggestion quality:
the modal/card UI is too large, and equal amount OR same date produces excessive
false suggestions. The accepted replacement is a compact account-level Review
ledger with full inspection/edit/source capabilities, inline decisions, stronger
initial link heuristics, and explicit Payee/Memo exclusion learning managed through
Rules. See the [next-session handoff](../features/005-transaction-sheet-navigation.md#next-session-handoff-review-workspace-and-link-learning--2026-10-30) for accepted scope, matching policy, incremental updates and verification obligations.

Implemented following owner **Go**, on experimental schemas **36 with rules /
35 without**. The sidebar Review workspace replaces the modal and separate
Transfers entry. It provides compact account-grouped rows, full transaction/split/
source inspection, inline decisions and the shared Commit count. Link suggestions
use the inclusive seven-day window and exact amounts, or an explicitly identified
outgoing destination with an outgoing excess of at most 4%. Explicit Payee/Memo
exclusions default dataset-wide and support endpoint-account scope, preview,
editing and removal in the separate Rules → Link suggestions section.

The owner explicitly chose to generate a counterpart for **every unresolved explicit
transfer**, including when plausible imported postings already exist. Imported and
generated entries remain visible until confidently matched or explicitly linked.
The temporary balance effects are accepted; uncertainty must not select a pairing.
Review Link removes generated duplicates atomically and Undo restores them. A
persistent waiting task can be explicitly accepted without bank evidence. Repaired
imports retain imported provenance even when their original amount was invalid;
group resolution generates the counterpart after the repaired row is included.

Verification on macOS using synthetic data:

- All 94 selected Release regression tests passed, including the complete import
  UI test executable and grouped imports. This selection excludes the separate
  long-running fault/crash and diagnostics suites.
- Eleven focused engine/Review/grouped-import tests and six focused Qt cases passed
  in both ASan/UBSan and rules-disabled Release builds. Earlier focused transfer
  controls/exclusion checks also passed in all three builds.
- Six policy recovery scenarios passed 662 injected write/sync positions; the six
  existing Review scenarios passed another 1,122 positions. They exercise durable
  boundary interruptions, retries, verified backup restores and encrypted markers.
- Grouped import, repaired-transfer generation and Abort passed 387 positions each
  for write-failure and abrupt-crash sweeps, plus before/after durable interruptions
  and retry verification. Statement rollback and overlap recovery sweeps passed.
- A synthetic 20,000-row workload produced 10,000 expected pairs and zero unexpected
  pairs: first Review 974 ms, five cached opens 13 ms, and an amount save plus
  affected Review 1,318 ms. These are storage/offscreen results, not private-data
  matching-quality or native responsiveness claims.

The local rules-enabled `./build/SpentIt` is rebuilt. Native interaction, Linux and
Windows remain unverified. Current formats require a new dataset; older experimental
formats are rejected without migration. See [the Review contract](review-sheet.md).
Earlier documentation-only Merge/Link notes below are historical and superseded
by the implemented Review baseline.

## Merge and Link decision recorded — 2026-10-30

The owner approved documentation of separate **Merge** and **Link** commands: same-account duplicate Merge selects two or more rows and automatically keeps the sole committed transaction without a survivor prompt; with none or more than one committed, the user chooses the survivor; transfer Link keeps both independent postings and supports choosing endpoints across accounts. Preferred shortcuts are Alt+M and Alt+L, subject to platform verification. Context guides suggestions without silently changing operations. Generated-placeholder Link repairs must expose their removal/balance effects. See the [accepted feature decision](../features/005-transaction-sheet-navigation.md#accepted-merge-and-link-interaction--2026-10-30).

Retaining non-survivor source evidence and merge history, future recognition through all attached original representations, and safe Undo are required direction. Current ID-less matching does not yet search every attached representation; eligibility, conflicts, occurrence consumption and Undo details remain design work. The proposed memo-based automatic fallback is not approved. This update changes documentation only; commands, shortcuts, financial behavior and schema are unchanged, and implementation awaits a new Go. Verification is documentation consistency, relative links/anchors and `git diff --check`; no new runtime result is claimed.

## Hooks attachment procedure clarified — 2026-10-30

Documentation now records the owner's standing authorization for native macOS PID-targeted window capture/Accessibility inspection when a hooks investigation cannot reliably attach through the normal UI tool. Agents must preserve and verify the original PID/executable/endpoint/session, avoid app-name calls that can launch a second instance, and establish inspection access before asking the owner to prepare a selection. See the [agent guide](agent-diagnostics-guide.md#standing-owner-authorization-and-attachment-discipline--2026-10-30). This changes documentation only; it does not add financial contents to hooks or authorize financial mutations.

An owner-authorized live investigation established that the standalone process can be absent from the UI tool's app inventory while its endpoint remains connected. Native Accessibility access was denied; PID-filtered window capture and a narrowly scoped read-only source comparison succeeded. Temporary inspection files were removed. This is operational evidence for the attachment fallback, separate from the synthetic matching verification below; no private paths, identifiers or record contents are retained here. Documentation verification: relative links/anchors and `git diff --check`; no runtime change or new runtime test is claimed.

## First priority: duplicate-matching correction — 2026-10-30

The owner gave Go for **same-account re-import recognition**, separately from cross-account transfer matching. The correction now automatically associates a unique incoming original currency/amount/date/payee key with a unique existing transaction in the resolved account. It works across supported parser labels and accepted/uncommitted targets, preserving later ledger corrections. Payee matching is exact and nonempty; there is no fuzzy text or date-window matching. Repeated keys in changed exports remain reviewable. Bank-ID conflicts, malformed originals and removed targets cannot use this automatic path. Existing strict bank-ID and identical-source occurrence rules remain in place.

Associations use the existing atomic import machinery and produce one monetary posting, accurate already-present counts and duplicate-only completion without accepting older work. No schema change is required (still 32/31). The existing pending decision can be changed to Include. The proposed ledger badge, paired evidence inspection and completed-match unlink/history interaction are **not implemented by this bounded correction**; the unified Review sheet and broader interaction design remain follow-ups. See the [accepted scope and rationale](import-commit-discussion.md#accepted-same-account-correction--2026-10-30).

Verification on macOS arm64 with synthetic inputs:

- Rules-enabled Release `ctest --test-dir build/import-ui-rules-release --output-on-failure -E 'io$|crashes$|faults$|failures$|backup_publication|benchmark|diagnostics\.' -j 4` passed **92/92** in **71.25 seconds**, including the full offscreen import UI suite.
- Rules-enabled Release `ctest --test-dir build/import-ui-rules-release --output-on-failure -R '^statement.reimport_faults$'` passed **1/1** in **124.65 seconds**. The new exact-field path was exercised through QIF-to-OFX attachment with alternating disk-full/write failures across materialization and association completion, process interruption immediately before/after durable commit, reopen/retry and encrypted-file leakage checks.
- Rules-enabled ASan/UBSan `ctest --test-dir build/import-ui-rules-sanitized --output-on-failure -R '^statement.(parser|review|reimport|overlap|rollback)$' -j 2` passed **5/5** in **26.77 seconds**, with no sanitizer diagnostic. Regressions include changed/reordered/shorter QIF exports, split preservation, either-order QIF/OFX matching, accepted/uncommitted targets, original-versus-edited values, repeated-key ambiguity, distinct IDs, removed targets, account boundaries, exact balances, pending Include reversal, source-owned rollback, reopen and restored backups.
- Rebuilt the rules-enabled Release `build/SpentIt`. `git diff --check` and relative file-link checks passed. Existing tests that expected exact matches to remain unresolved were updated; the balance test retains an uncertain-payee fixture to verify inclusive unresolved totals.

All fixtures are independently synthetic. No private dataset was accessed. Native-window testing, private-workload timing, Linux and Windows remain unverified. This is selected verification, not a run of every fault or benchmark suite.

## Unified Review sheet — 2026-10-30

The owner gave Go for the complete Review feature. It is implemented in the working tree with account-grouped questions, one outstanding-question count, entry from Review and Commit, saved Yes/No decisions, partial duplicate-group resolution, same-account Merge with retained source representations, cross-account Link and an atomic amount-difference split/category/link dialog. Standard keyboard Undo and one sheet button reverse a saved sequence safely; intervening dataset mutations stop that sequence. New experimental formats are **34 with rules / 33 without**; older datasets are rejected unchanged.

See the [implementation contract and verification](review-sheet.md) for financial boundaries, recovery behavior and test commands. The rules-enabled Release regression selection passed **93/93** in 81.75 s, including the updated UI and current-format tests. The original private-data count-flip sequence was not reproduced; synthetic 53-question tests verify stable counts across inspection, account changes, blocked Commit and reopen. The final import UI executable passed 51 Qt cases; final Review storage and focused UI checks passed in ASan/UBSan and rules-disabled Release builds. Six dedicated recovery scenarios passed 1,126 write/sync fault positions, twelve durable-boundary interruptions, retries, verified backup restores and encrypted-source marker checks. Post-Merge rollback preserved accepted survivors and removed exclusively uncommitted work. The local `./build/SpentIt` rules-enabled Release executable is rebuilt. Native interaction, Linux and Windows remain unverified.

## Dataset acceptance storage refactor — 2026-10-29

The accepted model was implemented on experimental schemas **32 with rules / 31 without** at this checkpoint. The subsequent Review workspace revision above advances the current formats to **36/35**. Previous formats are rejected without migration or modification; testing the current build requires a new dataset.

- Multiple pending imports persist across restarts. Strict unique bank-ID matching uses original account/currency, exact amount, calendar date and type across OFX-family labels. Identical QIF sources preserve occurrence multiplicity. Duplicate-only sources complete without accepting older work.
- Dataset-wide Commit references saved financial revisions, accepts generated counterparts atomically, consumes mutation-prepared review and leaves failed records visible. Later accepted corrections retain persistent history. The ordinary ledger and balance include uncommitted rows.
- Account-wide rollback removes its uncommitted ownership, exclusively generated counterparts and unowned raw/review/source material. Independently imported counterparts and accepted transactions remain. Shared source facts remain only while retained transactions depend on them. Account creation, confirmed aliases, staged categories and reviewed opening proposals survive transaction rollback.
- A confirmed whole-record bank match to an accepted generated transfer adds source ownership immediately. It does not overwrite accepted financial/display fields or require Commit. Subsequent exports can match through that source; rollback preserves the generated transaction and any other retained owner.
- Commit has no routine confirmation. Warnings are navigable and valid overridable concerns support Ignore and commit. Quit offers Yes/No/Cancel; a failed Commit keeps the document open. Import outcome counts and failures remain visible. Changing the selected pending source refreshes its repair panel. Rollback preview generations survive reopening and reject stale requests.

Verification on macOS arm64, Apple clang / Qt 6.12, using synthetic inputs:

- Rules-enabled Release: `ctest --test-dir build/import-ui-rules-release --output-on-failure -E 'io$|crashes$|faults$|failures$|backup_publication|benchmark|diagnostics\.' -j 4` passed **91/91** in **87.24 seconds**. This includes the full import UI suite, ordinary committed editing/history, rules, grouped imports, recovery and restored backups. UI coverage includes multi-account/multi-currency acceptance, partial errors, warning Ignore, all quit choices, failed quit-time acceptance, saved-state reopen and account-wide discard.
- Rules-enabled Release: `ctest --test-dir build/import-ui-rules-release --output-on-failure -R '^statement\.(generated_faults|rollback_faults|acceptance_faults|overlap_faults)$' -j 4` passed **4/4** in **219.55 seconds**. Sweeps cover **1,067** write/sync positions, alternating disk-full and I/O errors: acceptance 165; rollback 181; overlap preparation/association 165/105; generated source match/exclusive rollback/shared rollback 93/269/89. Each scenario also passed process interruption immediately before and after durable commit, verified reopen/retry, and encrypted-file leakage checks.
- Diagnostics transport and QML checks passed **2/2** in **2.19 seconds** when run with local socket access. Their initial sandboxed runs could not create the endpoint; that was not treated as a passing run.
- After the final accepted-row transfer guards, the selected Release overlap/rollback, transfer import/QIF/manual-evidence, account-group and full import UI regression passed **7/7** in **60.00 seconds**. This includes refusing draft transfer actions on an accepted row within a still-pending partial import, and refusing stale rollback previews after reopening.
- Final rules-enabled ASan/UBSan `statement.rollback` and `statement.overlap` passed **2/2** in **15.96 seconds**, with no sanitizer diagnostic. Final rules-disabled Release overlap/rollback and schema gate passed **3/3** in **8.27 seconds**.
- Rebuilt `build/SpentIt` with the rules-enabled Release configuration. `git diff --check` passed. Relative file links in the changed documents were checked.

This is selected verification, not a claim that the entire fault/benchmark suite ran. Qt interactions used the offscreen platform; separate native-window/manual testing, Linux, Windows, physical power loss and private-workload timing remain unverified. All inputs used for implementation and verification are synthetic. No private sample was accessed, copied or recorded. Legacy per-batch storage commands remain for internal/history tests; they are not the UI Commit/Discard path. Reports, new matching policies and unrelated lifecycle scope are not part of this change.

The sections below retain historical checkpoints. Their “next work,” older schemas and incomplete-scope statements describe those checkpoints; the current implementation above supersedes them.

## Earlier strict bank-ID overlap matching slice — 2026-10-29

The owner approved strict automatic recognition across OFX-family export formats. The engine compares original bank ID, currency, exact amount, calendar date and transaction type. Storage scopes matches to the selected account and requires a unique active committed ledger identity; several source associations to that one identity do not create ambiguity. Source associations are scanned once per materialization, rather than once per imported record. Edited ledger amounts/dates/payees remain unchanged. Conflicting source facts, several target identities, removed targets and every occurrence of an incoming repeated ID stay reviewable. Bank-ID matching checks byte-identical exports too. Format labels, export metadata and payee escaping do not affect this rule.

This is a bounded matching slice, **not completion of the handoff**: matching uncommitted/generated targets, repeated imports before Commit, duplicate-only completion without Commit, independent transaction acceptance, source-owned rollback and partial-error acceptance remain unfinished. No schema change or migration is included. The existing source association and history mechanisms persist the automatic decision exactly as a reviewed duplicate. See [the accepted decision table](import-commit-discussion.md#accepted-strict-bank-id-matching--2026-10-29).

Verification: rules-enabled Release regression passed 12/12 (including import UI and current review-state checks), with supplemental committed-history/transfer checks passing 10/10. New overlap coverage includes both range orders, all four format labels, repeated payments with distinct IDs, multiple associations to one target, ambiguous prior identities, conflicting facts, removed/corrected targets, separate accounts and restored encrypted backups. `statement.overlap_faults` passed all 133 preparation and 97 association write/sync positions, plus process interruptions on both sides of each durable boundary and verified reopen/retry. After the final one-to-one guard adjustment, statement parser/review/overlap passed 3/3 in rules-enabled Release, ASan/UBSan and rules-disabled Release. Rebuilt `build/SpentIt`. All 151 relative file targets across the five changed documents exist; `git diff --check` passed. Native interaction, Linux/Windows and physical power loss remain unverified. Verification used synthetic inputs only; no private file paths, contents or identifying details were added to repository files or tests.

## Next priority: repeated imports and format-independent matching — 2026-10-29

The owner rejected duplicate-only reimports requiring Commit, the one-pending-import restriction, and large duplicate-warning lists when exporting the same transactions in another supported format. These are the next work within the already-accepted transaction model. The strict committed-target matching slice above is now implemented; the wider storage/UI work remains. [The updated handoff](import-commit-discussion.md#immediate-next-priority-repeated-imports-and-format-independent-matching--2026-10-29) records settled behavior, verified UI/storage/matching gaps, remaining identity-policy decisions, implementation order and required acceptance evidence. Automatic matching must operate on shared internal fields against accepted and uncommitted transactions, preserve occurrence multiplicity and source ownership, and reserve review for actual uncertainty. A duplicate-only import adds no acceptance obligation, while previously outstanding new transactions remain uncommitted.

This follow-up documents next work only. The date-order slice below is implemented; the pending-import restriction and old candidate/Commit behavior remain. The reported ASO-to-other-export warning count is owner evidence, not a private-file reproduction. No additional runtime change or private-data access occurred for this handoff.

## Automatic import date order — 2026-10-29

Implemented the owner's Go to remove the ledger date-order selector and its Import statement prerequisite. The button retains existing busy/read-only/pending-import guards. QIF preparation examines transaction dates across all account sections; valid dates that allow only one order establish DMY or MDY. ISO dates and dates equivalent under either order need no question. OFX-family imports do not ask. If both orders remain possible and differ, a modal explains that the format cannot be determined, shows an actual date with both interpretations, and requires an explicit choice before Continue; Cancel import discards the pending in-memory source. Confirmation uses the original bytes already read. Conflicting valid orders refuse import explicitly; invalid dates remain parser diagnostics. No storage schema or persistence command semantics changed. See [the QIF contract](../features/009-qif-profile.md#conditional-date-order-prompt--accepted-and-implemented-2026-10-29).

Verification: Release `ctest --test-dir build/import-ui-rules-release -R 'qif|import.ui|import.qml|import.navigation' --output-on-failure` passed 8/8 (including transferstore.qif and the full import UI suite). ASan/UBSan `ctest --test-dir build/import-ui-rules-sanitized -R '^qif\.' --output-on-failure` passed 4/4. After extending assertions, the Release/offscreen `spentit_import_ui_tests automaticImportDates` passed: initial button availability, no preselection, visible ambiguity dialog, cancel, retained bytes after source-file replacement, exact chosen/inferred dates, ISO and OFX without a profile, and contradictory-order refusal. Rebuilt `build/SpentIt`. Native pointer interaction, Linux and Windows remain unverified; no private dataset was accessed.

## Commit reuses transaction review — 2026-10-29

The owner gave Go to keeping warnings current during mutations and consuming them at Commit. The storage command now uses prepared row inclusion/duplicate/transfer-review results and drafts rather than its two previous row parse/validation loops. Review refresh runs before mutation durability across import, editing, rules, categories, accounts and transfer commands. Duplicate-target changes invalidate dependent decisions. Rolled-back work drops speculative review/cache state; verified reopen reconstructs it. Group review reuses validated rows and immutable mapped-source parsing, and unchanged transfer graphs are reused. The bounded row cache refreshes relationship-derived fields separately from unchanged financial drafts.

This does not finish the broader transaction model. Commit retains old publication writes, category/opening behavior, transfer transition checks and history. Dataset-wide status-only acceptance, shared-source rollback, partial-error acceptance, warning overrides and quit prompting remain incomplete. Dependency enumeration/group aggregation still scan collections. No private data was accessed and no end-to-end speedup has been measured.

Verification: Release regression passed 17/17, supplemental rules/category/statement/cancellation/backup checks passed 9/9, and account lifecycle checks passed 2/2. The warning-presentation follow-up passed 10/10, including the UI suite. New `import.review_state` tests cover a 2,000-row import, one-row review refresh after an edit, dependent duplicate-target invalidation, stale-review refusal, rollback followed by reuse of a revision number, verified reopen, idempotent commit and both commits of a grouped reciprocal transfer. They assert zero row financial-validation calls during the tested commits. Selected ASan/UBSan checks passed 4/4 and rules-disabled checks 2/2; the review-state test passed again in both after the warning-presentation follow-up. Import and transfer I/O/process-interruption sweeps passed; import I/O and interruptions were repeated after the cache changes and passed 2/2 (317/321 seconds). The local rules-enabled Release `build/SpentIt` was rebuilt. All 265 relative link file targets across the changed documents exist and `git diff --check` passed. Native interaction, Linux/Windows, physical power loss and private-workload timing remain unverified.

## Single balance and visible uncommitted ledger — 2026-10-29

The owner gave Go for the accepted transaction model and confirmed preserving existing account/category creation and opening-review behavior while account rollback removes transaction work only. That authorization remains in effect.

Implemented the first read/UI slice: one displayed balance and one normal ledger include saved uncommitted transactions. Valid saved rows stay visible even while another record needs repair or duplicate review. Unresolved duplicate candidates count until explicitly excluded; confirmed duplicates and removed transactions do not. Generated transfers and proposed bank matches reconcile without counting both representations. The sidebar shows a balance label instead of a committed/projected toggle, and ledger rows identify uncommitted work.

**The overall model is incomplete.** Commit still uses the old per-account publication path, copied initial committed revisions and whole-import error blocking. Rollback still uses the old discard/Abort behavior and retains source material. Dataset-wide acceptance, source-owned account rollback/cleanup, partial-error acceptance and quit-time commit prompting still require implementation. The [handoff storage proposal](import-commit-discussion.md#storage-implementation-proposal--not-yet-implemented) records the affected invariants and next work. No schema or persistence-command changes are included in this slice; existing experimental documents remain readable. No private data was accessed.

Verification: rules-enabled Release `ledger.balance`, `accounts.basics`, `accounts.groups`, and `import.navigation` passed; the final `import.ui` rerun passed after updating superseded committed-only/hidden-ledger expectations. The new `ledger.balance` test also passed under ASan/UBSan and in the rules-disabled Release build. It covers signed splits, unresolved/confirmed duplicate decisions, valid rows beside an invalid amount, restored encrypted backups, generated/proposed transfer matches, separate CAD/JPY balances, and overflow refusal. The local rules-enabled Release `build/SpentIt` was rebuilt. All 264 relative link file targets across nine changed documents exist, and `git diff --check` passed. Native interaction and Linux/Windows were not verified. These checks verify the read/UI slice, not the unfinished acceptance/rollback model.

## Next work: dataset-wide commit and new transactions — 2026-10-29

The owner accepted normal saved ledger transactions marked new/uncommitted, dataset-wide Commit as acceptance, continued editing after acceptance, and a quit-time commit prompt whose No preserves uncommitted work. Forget removes exclusively owned import material rather than archiving discarded imports; a separate permanent original-import evidence archive is no longer required. Transfer rollback distinguishes creating a counterpart from linking to an existing transaction. This superseded earlier per-account commit and evidence-retention requirements. At the initial handoff none of the new runtime behavior was implemented; the subsequent read/UI slice is recorded above.

The [handoff](import-commit-discussion.md) records accepted decisions, the now-settled transaction policies: account-wide rollback of uncommitted work and its effects elsewhere; shared transactions survive while another retained source supports them; one balance includes uncommitted rows, with the same default for future reports; successful imports can be committed while failed records remain visible until repaired or explicitly discarded. These decisions preceded the Go and implementation slice recorded above. The handoff now includes a storage proposal; changes to account/category creation remain deferred. The reported 18-second commit remained unprofiled at that handoff; the later accepted review-state follow-up is recorded above. That initial handoff update changed documentation only. The application was not launched, private data was not accessed, and runtime tests were not repeated. Earlier verification below belongs to the existing implementation.

Prior handoff documentation verification: all 251 relative links/anchors across eight changed Markdown files and `git diff --check` passed. The earlier transaction-decision update changed documentation only: all 134 relative link file targets across its four updated documents exist, and `git diff --check` passed. Runtime tests were not run.

## Account aliases and transfer categories — 2026-10-29

Implemented the owner-authorized shared account/category-name policy. Account creation names, explicit rename history and confirmed named-QIF mappings form a durable alias list; the user-selected current display name stays unchanged by imports. Bracketed and unbracketed account allocations share grouped transfer resolution and commit blocking, including exact matching of a transfer split beside a fee. Source bytes, occurrence counts, per-account commits and transfer history remain unchanged. Existing schema 30/29 drafts use Resolve mapped transfers without reimport or migration.

Account entries and aliases are visible/searchable and read-only in the category editor. Allocation pickers display account destinations alongside ordinary categories and route account names to transfer commands. Rule/bulk selectors display accounts with explicit unsupported-transfer guidance; they do not save them as ordinary classifications. Rules protect recognized account allocations even when replacing manual fields. Imported category definitions matching accounts generate an import-review warning and no duplicate category. New category/account/alias collisions are refused; old conflicts remain inspectable, and ambiguous aliases are reviewable blockers. Historical committed ordinary classifications are not retroactively rewritten.

Synthetic tests cover caseless aliases, user-created differently named accounts, renames, capture/reopen/resolution on existing drafts, transfer-plus-fee allocation matching, exact evidence, duplicate category suppression, collision refusal, per-account commit, backup restoration and read-only category-editor account rows. Earlier fixtures that deliberately reused an account name or swapped an established source alias now assert the new refusal policy. No private dataset was accessed.

Verification: selected Release regression passed 70/70; supplemental checks passed after updating the workload smoke test’s superseded expectation that balanced repeated transfers remain blocked. The final grouped workload/UI/account-groups selection passed 3/3 and checks exact balances and preserved occurrence counts with mixed bracketed/unbracketed aliases. Selected ASan/UBSan tests passed 12/12; rules-disabled account groups/validation passed 2/2. Alias-based write/sync-failure and process-interruption sweeps both passed preparation (165 positions), explicit resolution (77), and Abort (101), including reopen/retry and the explicit durable boundary. The final UI rerun passed 1/1 after limiting unresolved-import diagnostics to pending drafts. All 268 relative links/anchors across the 10 modified Markdown files and `git diff --check` passed. The local rules-enabled Release application was rebuilt at `build/SpentIt`. Native interaction, Linux/Windows and physical power loss remain unverified.

## Repeated-transfer matching — 2026-10-29

Implemented the owner-authorized extension to confirmed-mapping QIF transfers. Preparation and **Resolve mapped transfers** now associate equivalent reciprocal groups one-to-one when remaining eligible counts match. Eligibility retains reciprocal mapped accounts, exact opposite amounts, identical currency definitions and dates, current included drafts and unavailable/already-linked endpoint exclusions. Stable source/record/allocation order chooses associations without claiming uniquely identified historical pairs. Unequal counts remain wholly reviewable. Blocker explanations distinguish balanced repeated groups from unequal counts.

Every imported occurrence and its evidence remains separate; no new posting is generated. Existing schema 30/29 and transfer history are reused, with no migration or reimport requirement. Each account still commits explicitly. Synthetic verification covers single and repeated split transfers, exact source/record evidence, balances and ledger multiplicity, either commit order, backup restoration/reopen, already-linked endpoints, unresolved repaired rows, unequal counts, different dates/amounts/currencies, cancellation after staging a pair in preparation and explicit resolution, idempotent reruns and whole-import Abort.

Verification: selected Release account/transfer/UI checks passed **12/12**, including transfer I/O and crash sweeps. ASan/UBSan selected account/transfer storage checks passed **6/6**. Rules-disabled Release account groups/validation passed **2/2**; a second rules-enabled Release tree also passed those **2/2**. Focused `accounts.groups` and offscreen `mappedTransferImport` / `mappedTransferBlockers` passed in both Release and ASan/UBSan. Direct Release `spentit_account_tests mapped_io` and `mapped_crashes` both passed: repeated preparation, explicit resolution and Abort exercised respectively **165 / 77 / 101 write/sync positions** with all-or-none reopened state, retries, and explicit before/after durable-commit interruptions. The Release executable was rebuilt at `build/SpentIt`. `git diff --check` passed; all 237 relative links/Markdown anchors in the seven changed Markdown files passed. Native interaction, Linux/Windows and physical power loss are not claimed by these synthetic/offscreen checks. No private dataset was opened or modified.

## Diagnostics-guided startup and account switching — 2026-10-29

Owner acceptance (2026-10-29): the owner measured startup at approximately **7 seconds**, accepted the current performance for now, and considers the application usable. This closes the current performance pass without claiming near-instant startup. A redesigned startup/loading screen is deferred; its design and implementation require a later discussion and explicit Go.

Implemented the owner-authorized second performance pass: automatic startup now opens/verifies once; scoped format checks avoid repeated format reads; QIF category lookup is built only when slash notation needs it; bounded validated-row reuse covers verification and subsequent read-only views. Any write or transactional access invalidates the connection cache, every explicit verification starts fresh, and failed verification publishes no cached state. Financial checks, encryption, format, locking, matching and durability policies remain unchanged. No private dataset copy, financial export or screenshot was created; regression/fault inputs remain synthetic.

On the authorized dataset, a final native launch reached observed matching-frame readiness in **7.715 s**, versus the owner's approximately 30 s baseline. Two large-account switches reached observed native readiness in **0.567–0.617 s**, with **0.282–0.287 s** worker time. Startup is still not near-instant: **6.375 s** was full verification. Native sample count is limited, caches were warm, and the Qt frame/readiness proxy is not physical display latency. [Detailed measurement boundaries and verification](import-ui-performance.md#diagnostics-guided-startup-and-switching-follow-up--2026-10-29) preserve these limitations.

Release regression passed 91 sandbox-compatible checks plus both IPC checks rerun with required socket access. ASan/UBSan selected checks passed **26/26**, rules-disabled selected storage checks **3/3**, and final five-case startup checks passed in Release, sanitizer and rules-disabled builds. Ordinary-import I/O/interruption sweeps passed **2/2**. The rebuilt application is `build/SpentIt`. Three final 18,972-row synthetic grouped runs passed, with verified reopening median **5.958 s** and 36 switches median **0.410 s**. A separate 19,000-row ordinary full workload passed import/edit/evidence/search/scroll/commit/reopen assertions; committed reopening remained **9.389 s**. This does not complete the wider sheet feature or authorize deferred report/matching work.

## Agent diagnostics handoff — 2026-10-29

Added an [agent-facing hooks/profiling guide](agent-diagnostics-guide.md), discoverable directly from `AGENTS.md`. It covers setup, intended-instance endpoint discovery, exact-operation and UI waits, numeric outcome decoding, timing/counter attribution, failure handling and reproducible synthetic comparisons. Recorded the owner’s acceptance of the existing OFF/ON build switch, including inert-code and distribution limitations. This follow-up changes documentation only; it does not start profiling or performance optimization. Verification: all 609 relative links/anchors across 41 repository Markdown documents passed; `git diff --check` passed. Runtime tests were not repeated for this documentation-only follow-up.

## Application diagnostics implementation — 2026-10-29

The owner gave Go for all seven diagnostics stages, excluding screenshots and MCP and stopping before performance optimization or repeated-transfer matching. The current change adds the optional standard-C++ collector, worker/outcome/storage/cache hooks, semantic QML state, owner-only local IPC on an independent thread, a read-only CLI, and correlated test/profiling support. No schema, financial command semantics, validation, durability or matching policy changes are intended. The prior documentation edits are retained.

Verification includes 91/91 broad Release checks, ordinary-import I/O/interruption sweeps, targeted sanitizer checks, native JSON readiness and twelve serial workload comparisons. Grouped medians met the provisional 5% observation budget; ordinary pending reopen measured +15.2% amid substantial variation, so the overall overhead target remains unverified. The 19,000-row sanitizer replay passed import/reopen/edit/search/scroll. Actual verification and limitations are recorded in [application diagnostics](application-diagnostics.md). Feature 012 remains active while its documented semantic/profiling and platform/fault coverage gaps remain. Normal launch is disabled; explicitly use `./build/SpentIt --diagnostics` after building with the diagnostics option.

## Application diagnostics investigation and plan — 2026-10-29

Completed the owner-authorized investigation and documentation only. [Feature 012](../features/012-application-diagnostics.md) records inspected worker/model/QML/storage/test seams, privacy boundaries, operation/durability/presentation distinctions, semantic screen state, bounded local observation and a seven-step implementation plan. Screenshots are excluded. Tests and local profiling will use the same observation contract without requiring MCP; a possible MCP adapter remains separate future scope.

This planning-only checkpoint preceded the remaining performance and repeated-transfer matching work. At that checkpoint no hooks, endpoint, MCP server, financial behavior or runtime changes were implemented; the implementation authorization and subsequent evidence are recorded above. No private dataset was accessed and no runtime tests or benchmarks were run. Documentation verification: all 202 relative links and referenced heading anchors across the five changed/new documents passed; `git diff --check` passed.

## Performance-first application pass — 2026-10-29

The owner reversed the handoff order and gave Go for performance first. Implemented scoped SQL-plan reuse for reads and writes; candidate queries driven by the selected account's committed batches; a write-invalidated group snapshot for read-only switching; one starting committed balance per account during draft-only linking/Abort; bounded page caching; equivalent append-only transfer-history validation; and a single read of each committed batch's evidence associations. Identical progress labels no longer trigger repeated UI refreshes. Full verification, exact money, encryption, durable atomic commands, and current mutual-unique transfer matching remain in place. Existing schema-30/29 datasets need no reimport or migration. See [implementation and measurement boundaries](import-ui-performance.md#application-performance-work--2026-10-29).

For 18,972 synthetic transactions across six accounts with 942 unique transfer links and twelve unresolved repeated allocations, the Release/offscreen app measured preparation **8.212 s** (baseline **75.821 s**), opening **14.504 s** (**34.966 s**), and twelve account switches **0.719–0.756 s** (**13.441–14.573 s**). Startup measured separately at **0.393 s**. A saved ordinary edit refreshed in **2.849 s**; rerunning resolution afterward took **1.627 s**, and Abort **3.025 s**. All workload assertions passed, including exact projections, occurrence counts, evidence retention and unchanged repeated-transfer blockers. Baseline preparation/some switches overlapped short builds/tests; final timing runs did not. These are single-run observations, not guarantees for the private export or native rendering.

A separate 19,000-row ordinary-import workload passed import, signed splits, edit/evidence preservation, query/scroll, commit and reopen. Commit took **8.921 s**. Committed reopening improved from **39.912 s** to **26.409 s** after replacing per-occurrence evidence-association scans; those two values compare intermediate optimized builds. **Opening still misses the few-seconds target**, especially for committed history. That performance goal remains open; do not describe this pass as complete responsiveness or start repeated-transfer matching without its next Go. No private dataset or original export was accessed. Native Cocoa timing, OS disk-cold behavior, Linux/Windows and physical power loss remain unverified.

Verification: broad macOS Release checks passed **89/89** (127.38 s), selected ASan/UBSan checks **26/26** (158.43 s), and rules-disabled Debug checks **3/3** (23.61 s). New regressions cover group-snapshot invalidation after edits, rollback/retry, balance review and commit, plus rejection of missing/repeated/wrong evidence associations and corrupted mapped-transfer history. Selected exhaustive grouped/ordinary-import I/O-failure and process-interruption sweeps passed **4/4** (1292.52 s), including reopen/retry and durable-boundary checks. Rebuilt `build/SpentIt`; final build logs contained no compiler warnings/errors. Final relative Markdown file/anchor checks and `git diff --check` passed. [Reproduction and test commands](import-ui-performance.md#reproduction-and-verification-commands) record the exact selections.

Owner follow-up: opening the actual dataset took approximately **29 s**, and switching to TD Visa (the largest account) approximately **2.5 s**. These are owner-observed native timings, not instrumented measurements. Switching is improved but still short of acceptable; opening remains far off target. The offscreen synthetic results do not establish a native-rendering multiplier. A future performance pass should separate storage verification, account/view loading and first usable native frame. The owner requested committing this checkpoint before switching to an inspection/introspection side quest; no MCP hooks are implemented in this checkpoint.

## Next-session priorities — 2026-10-28

The owner requested a [handoff for repeated transfer matching and application performance](import-performance-handoff.md). Accepted next behavior pairs equivalent repeated transfers one-to-one, consuming each endpoint once rather than blocking legitimate equal-count groups. This extends the current mutual-unique-only implementation; it is not implemented yet. Performance is a show-stopper: owner reports 4m39s preparation, roughly 12s account switches and 56s opening, versus tens of milliseconds for parsing alone. Profile and improve the full application on synthetic equivalent workloads without weakening financial or durability guarantees. This turn documents the work only; coding resumes after explicit Go.

## Transfer blocker diagnosis, scrolling and record reveal — 2026-10-28

Implemented the owner's Go after the blocker dialog repeated a generic reason, hid its scrollbar and restored the old account viewport after selecting a requested record. The read-only group snapshot now explains the specific mismatch and includes bounded candidate details: account/record, date, exact amount/currency and inclusion/link status. The dialog uses a virtualized list with a visible draggable scrollbar and separate navigation to blocked records and possible counterparts. Blocker navigation now uses explicit record reveal rather than ordinary account viewport restoration. Automatic matching rules and financial writes are unchanged; existing schema-30/29 datasets remain usable without reimport. See [diagnostic contract](qif-account-imports.md#actionable-blocker-details-and-navigation--2026-10-28).

Verification: final Release grouped-import/validation/QML checks passed **3/3** (9.50 s), ASan/UBSan grouped-import checks passed **1/1** (22.48 s), and offscreen blocker/counterpart navigation plus mapped-transfer and multi-account UI tests passed **5/5 including init/cleanup** (4.027 s). Rebuilt `build/SpentIt`. Final build logs contained no compiler warnings/errors. Changed/new Markdown links and `git diff --check` passed. Storage write paths were unchanged; fault sweeps were not repeated. A synthetic long-list test puts the first blocked transaction at record 42, exercises actual scrollbar pointer dragging, inspects a candidate, then verifies that record 42's expanded editor is inside the ledger viewport. Synthetic storage assertions cover distinct destination/self/currency/date/amount/missing-counterpart/repeated-candidate reasons and an already-linked candidate's status. Native Cocoa execution was attempted but unavailable: Qt reported “Cannot create window: no screens available.” The offscreen dialog was rendered and visually inspected; no native-platform interaction claim is made. No private dataset or original export was accessed. The broader import/status latency and the actual reasons for the user's remaining rows are not inferred from synthetic tests.

## Transfer resolution through confirmed import mappings — 2026-10-28

Implemented the owner's Go to reuse confirmed source-account mappings in complete named-account QIF exports. Reciprocal included draft allocations link automatically when each uniquely identifies the other by mapped account identity, currency definition, opposite signed amount and exact posting date. Source names may map to differently named existing accounts. Repeated candidates, missing destinations/counterparts, self references, currency mismatches, different dates and unavailable/unequal amounts stay blocked. No fuzzy naming, date tolerance, generated counterpart, extra posting, or source-record rewriting is introduced. See [accepted contract](qif-account-imports.md#reusing-confirmed-mappings-for-transfer-pairs--accepted-2026-10-28).

Materialization and all automatic links share one atomic transaction. An explicit **Resolve mapped transfers** command applies the same operation to an existing draft; reruns do not duplicate links or events. Existing schema-30/29 datasets remain usable; no reimport, format change or migration is needed. Both sides retain independent evidence, ordinary per-account commit and protection. Matching writes normal Merge history. Abort now releases both independent draft sides before discarding members, records both releases, and removes unused created accounts; committed or external counterparts remain protected. A new valid Clear transition retires an already-unlinked independent draft destination without attached/proposed evidence. Previously successful history transitions retain their interpretation.

Unresolved transfer allocations now contribute to readiness before Commit is offered. **What’s blocking commit?** lists up to 200 account/record-specific reasons and navigates to the right account or transaction. The aggregate count still includes all items. The list distinguishes opening decisions and record-level review from transfer blockers. Rebuilt `build/SpentIt`. The broader storage/status latency remains unresolved; this change only avoids a quadratic membership scan in transfer-history replay and uses indexed candidate buckets during matching.

Verification: final Release engine-transfer, grouped-import, validation, storage transfer-management/QIF/import and QML selection passed **14/14** (18.73 s); ASan/UBSan engine-transfer and grouped-import selection passed **9/9** (30.58 s). Rules-disabled Debug grouped-import checks passed **1/1** (9.16 s). Offscreen mapped-transfer, blocker navigation and existing multi-account UI cases passed **5/5 including init/cleanup** (3.362 s). Tests cover renamed target accounts, opposite commit orders, exact split totals/fees, no duplicate postings, reopen/restored backup, repeated candidates, absent/foreign/self destinations, currency/date mismatches, repaired evidence, preserved staged openings, cancellation after the first staged pair with full rollback, retry, and Abort. The new materialize/resolve/Abort fault cases passed write/sync failure and process-interruption sweeps at **165 + 77 + 101 = 343 positions each**, with durable-boundary checks, reopen/retry and encrypted marker scans. Final build logs had no compiler warnings/errors; changed/new Markdown links (35) and `git diff --check` passed. These are selected checks, not a full unrelated regression or performance benchmark. Tests use synthetic records only; no private dataset or export was opened. Initial tests caught a synthetic fixture using disallowed creation names, an account-selection mistake in the new fault assertions, and incomplete independent-counterpart cleanup on Abort. These were corrected before the final verification recorded below. Actual-export acceptance, native UI performance, Linux/Windows and general automatic statement matching remain unverified.

## Either-order QIF split category and memo — 2026-10-28

Implemented the owner's Go after the value-free local report identified `E S $` split groups. Optional `S` category and `E` memo now work in either order before the closing `$`. Each still appears at most once per group; incomplete splits, parent fields after splits, conflicting parent categories and exact-total failures retain their previous checks. Source bytes, memo/category text, split positions, classes and transfer intent remain intact. No private QIF or path was accessed; the local diagnostic report supplied only structural evidence.

The parser is now **bank-v6**, and new datasets use experimental schemas **30 with rules / 29 without**. Reopening reparses retained evidence; accepting formerly blocked records requires rejecting earlier formats unchanged, including 28/27, without migration. Reimport therefore requires a fresh dataset. Rebuilt `build/SpentIt` and the standalone diagnostic. This change does not address the separately measured storage/status latency.

Verification: Release selected parser, storage, repair, validation, grouped-import, rules/schema, statement and QML checks passed **13/13** (8.80 s). ASan/UBSan parser and ordinary import checks passed **5/5** (15.62 s). Rules-disabled Debug import and schema-gate checks passed **2/2** (4.53 s). Offscreen memo-before-category, structural-diagnostic and multi-account UI cases passed **5/5 including init/cleanup** (2.280 s). Synthetic tests cover mixed ordering, signed and zero splits, transfers/classes, stable allocation ordinals, CRLF, duplicate/missing fields, invalid amount repair evidence and unbalanced totals. The persistence regression now uses `E S $` through save, commit, committed correction, reopen and restored backup. Both format gates test unchanged rejection of previous versions. The standalone diagnostic privacy/read-only checks also pass with a newly accepted `E S $` input. The rebuilt ASan/UBSan libFuzzer harness completed **60,019 runs in 16 seconds** without a crash or sanitizer report. Build logs contained no compiler warnings/errors. Changed/new Markdown links (30) and `git diff --check` passed. Full storage fault sweeps, Linux/Windows and the private export have not been retested.

## Private QIF diagnostics and large-import investigation — 2026-10-28

Implemented the owner's Go for a value-free local diagnostic and clearer blocked-import messages. The standalone `spentit_qif_diagnose` executable uses the engine parser, prompts for a path locally, reads the input without modifying or copying it, suppresses paths/values/exception contents, disables process core dumps on macOS/Linux, and prints bounded field markers, line ranges, specific split reasons and counts/timings. The application now reports full-source line ranges and the offending line instead of only account-local record numbers; the selected record also shows the specific reason. No parser acceptance, repairability, financial persistence, schema or version change. `build/SpentIt` was rebuilt. See [local usage and privacy boundaries](qif-diagnostics.md).

Release parser/import/repair/validation/group/QML selection passed **10/10** (6.13 s); ASan/UBSan parser checks passed **4/4** (14.39 s). Offscreen structural-diagnostic and multi-account UI cases passed **4/4 including init/cleanup** (1.881 s), asserting line ranges/reasons, withheld values and blocked commit. The synthetic prompt/read-only/privacy test passed against both Release and sanitizer executables, including unchanged bytes, no new files in the input directory and sanitized input/profile failures. Engine coverage includes six split reasons, CRLF and embedded CR, account-header offsets and unknown-marker suppression. Build logs contained no compiler warnings/errors. Changed/new Markdown links (29) and `git diff --check` passed.

No private export or path was accessed. The synthetic workload uses exactly **116,621 lines / 19,436 transactions**, with four deliberately malformed splits; it reproduces scale, not the unknown private record structure. Release indexing took **3.32 ms**, parsing/reporting **23.27 ms**. A subsequent Release run with builds finished measured capture/initial status **0.104 s**, materialization/returned status **28.186 s**, status reload **9.481 s**, and Abort **8.100 s**. All assertions passed, including four blockers and unchanged committed balance. These single-run timings are not a benchmark distribution. Code inspection finds that group status loads and validates each stored row; the worker requests group/status information repeatedly. This is a measured investigation, not a performance fix or attribution of the user's delay. Native UI responsiveness, actual-export diagnosis, Linux/Windows, full storage fault sweeps and broad performance remediation remain unverified or outstanding. The next input is the locally generated value-free report, not the original file.

## Embedded CR in QIF text — 2026-10-28

Implemented the owner's Go after their hex screenshot identified lone `0x0D` bytes inside a payee. LF/CRLF still delimit QIF field lines. Lone CR is accepted only in ordinary payee/memo/reference/split-memo/address values and account/category description or category tax-schedule text. Both interpreted text and immutable source retain the exact CR bytes. Text beginning with `T`, `!` or `^` after an embedded CR cannot become a new amount, header or record. Amounts, dates, names, category/class references, status, headers and terminators remain strict; CR-only files remain unsupported. A final lone CR is no longer stripped as if it were CRLF. Domain review validation permits CR in payees and memos without loosening category validation. This does not implement the deferred multiline editor.

Rebuilt `build/SpentIt`. The parser is now **bank-v5**, with experimental schemas **28 with rules / 27 without**. Previously rejected retained sources could reparse differently, so earlier datasets, including 26/25, are rejected unchanged without migration. Reimport requires a fresh dataset. No private export file was opened or copied; all regression inputs are authored synthetic equivalents. See the [profile contract](../features/009-qif-profile.md#embedded-carriage-returns--accepted-2026-10-28).

Verification: Release selected parser/import/repair/validation/category/rules/schema/statement/QML checks passed **15/15** (10.81 s). ASan/UBSan parser, ordinary persistence and grouped-import checks passed **6/6** (20.54 s). New assertions cover LF and CRLF, literal CR preservation, field-like continuation text, strict non-text/control rejection, a final CR without LF, exact money, draft save, committed correction, reopen and restored backup. The offscreen multi-account UI test passed **3/3 including init/cleanup** (2.112 s), including an exact CR-containing payee assertion on the actual editor. A rebuilt sanitizer libFuzzer harness completed **48,523 runs in 21 seconds** without a crash or sanitizer report. The initial storage run caught an accidentally changed table-count check during the format bump; that check was restored and the passing runs above followed. Rules-disabled Debug import and schema-gate checks passed **2/2** (3.27 s). Changed Markdown relative links and `git diff --check` passed. Full fault sweeps, native UI interaction, Linux/Windows and the actual private export were not retested for this follow-up.

## Complete QIF category catalogs and account exports — 2026-10-28

Implemented the owner's Go for [complete QIF export imports](../features/009-qif-profile.md#category-catalog-and-literal-slash-names--accepted-2026-10-28). Existing named account definitions and repeated Bank/Cash/CCard switches now coexist with `!Type:Cat` catalogs. Bank-v4 accepts ISO dates and strictly validated comma thousands grouping for transaction totals, splits and reported balances. Starting-balance payee/memo text remains an ordinary transaction; closing-balance review includes it exactly once.

The complete catalog, including unused names and source metadata, is staged in the encrypted immutable source and shown in a review dialog. Capture, repairs and review do not create committed category definitions. Whole-import Abort removes every staged category, even when draft transactions use it. The first account's explicit financial publication (including transfer-first publication) atomically publishes the complete catalog, category history and financial changes. Later accounts stay drafts. Matching uses existing case-insensitive identity rules, never fuzzy similarity. Matching archived names requires explicit reactivation. Source-owned allocations bind at account commit using the first publication's immutable catalog epoch; rename/name reuse cannot redirect another account's pending source references. Used pending identities are protected from deletion, while genuinely unused definitions remain deletable. Manual/rule assignments and live transfer categories are preserved.

Literal slashes are allowed in category names. QIF uses exact catalog declarations to distinguish literal names from class suffixes; ambiguous declarations remain explicit blockers, without silent rewriting. Income/expense, descriptions, tax and budget fields remain source metadata, not report behavior. The new parser/source-binding semantics use experimental schemas **26 with rules / 25 without**. Earlier schemas, including 24/23, are rejected unchanged without migration under the pre-release policy. No private file was opened or copied.

Verification on macOS:

- Rebuilt the rules-enabled Release application at `build/SpentIt`. Final broad regression selection passed **87/87** (114.61 s), excluding the unrelated exhaustive I/O/crash/benchmark suites. The command was `ctest --test-dir build/import-ui-rules-release -E '(\.io$|\.crashes$|_io$|_crashes$|faults$|backup_failures|backup_publication|combined|benchmark|group_crashes|group_io)' --output-on-failure -j 2`. Targeted regression passed **15/15** (55.07 s), including the entire import UI suite, category/rules UI, bulk, transfer management, parser and category checks. Additional archive-conflict rollback and authenticated unused-catalog corruption assertions passed `accounts.groups` and `accounts.validation`, **2/2** (7.83 s).
- The first broad run exposed an eager source-name comparison on empty labels; fixed it and reran the affected checks. It also exposed an obsolete rules-test category-save helper and slash-invalidity expectations. Tests now use the actual inline editor and a still-invalid pipe character. The initial category UI test crashed after its failed invalid-name expectation; the corrected test and final UI suites pass.
- Extended `accounts.group_io` and `accounts.group_crashes` to include used and unused categories in capture, materialization, repair, Abort, staged balance, ordinary publication and transfer-first publication. An initial full sweep passed **2/2** (265.75 s), including disk-full/write/sync failures, durable-boundary interruptions, reopen, retries and encrypted marker scans. The final sweep also passed **2/2** (258.96 s).
- ASan/UBSan parser and grouped-import checks passed **5/5** (16.05 s); the sanitizer UI multi-account import case passed **3/3 including init/cleanup** (3.994 s). The UI case exercises the category review dialog and verifies that categories appear only after commit. A synthetic offscreen render was inspected. Rules-disabled Debug `accounts.groups` and `import.schema_gate` passed **2/2** (7.43 s).
- A rebuilt ASan/UBSan libFuzzer harness, seeded with synthetic catalog/account/ISO/grouped-amount input, completed **65,240 runs in 31 seconds** without a crash or sanitizer report. Compiler logs contained no warnings. Changed Markdown relative links and `git diff --check` passed.

These are bounded synthetic checks. Native platform interaction, Linux/Windows, private-export acceptance, universal QIF compatibility and a complete sanitizer/fault regression remain unverified. Reports remain deferred.

## Explicit category sorting — 2026-10-28

Implemented the owner's [sorting follow-up](../features/010-categories-and-selection.md#user-controlled-category-sorting--accepted-and-implemented-2026-10-28): A–Z, Z–A, and Order added controls, with no automatic replacement of the existing order. Only a user action selects a sort. The application remembers that choice across navigation, dataset switches and restarts. Successful saves and sort changes retain the category identity as selection and scroll it into view; filtered-out saved names are temporarily revealed until search is edited. New editing rows still begin at the bottom. Order added uses the immutable creation catalog revision from existing version history, so renames keep their original age. The catalog read exposes this metadata without changing its existing ID order, stored data, schema or financial commands.

Verification: rebuilt the rules-enabled Release application and category/storage/shared UI targets. Selected storage checks `category.catalog`, `category.setup`, `category.backup` and `category.validation` passed **4/4**, including creation-order preservation after rename, deletion/name reuse and reopening. The first UI sorting run caught a delayed Settings write; explicit immediate persistence fixed it. Final `ctest --test-dir build/import-ui-rules-release -R '^import.(category_ui|qml|ui|navigation)$' --output-on-failure -j 2` passed **4/4**. Native Cocoa category tests passed **5/5 including init/cleanup** (11.722 s); the rendered controls were inspected. Tests cover unchanged initial order, both alphabetical directions, oldest-first creation order, persistence in a new model/QML engine after reopening, saved selection and viewport visibility, rename age, filtered creation and existing editing/menu/scroll interactions. No QML warnings appeared. Changed-document relative links and `git diff --check` passed. No private dataset was opened. Full storage fault suites, sanitizers, Linux and Windows were not rerun for this follow-up.

## Compact Categories editor — 2026-10-28

Implemented the owner-approved [category editor cleanup](../features/010-categories-and-selection.md#compact-category-editor--accepted-and-implemented-2026-10-28): exact replacement guidance, compact 30-pixel left-aligned rows, and a table filling the remaining page height. The table owns its scrollbar without the former outer scrolling container; row hit areas reserve scrollbar space. A native pointer-drag check verifies the thumb moves the list.

New category appends a temporary bottom row, scrolls it into view even with a search filter, and focuses the name. Enter/focus loss saves directly; Escape cancels and an empty creation row is discarded. Rename uses the same inline editor. Rename, Archive/Unarchive and Delete unused category are right-click actions. The separate form, save/cancel buttons, review dialog and definition-history presentation are removed. Existing worker validation, atomic storage commands, history and deletion restrictions remain. Invalid names/duplicates preserve text and show an error; navigation proceeds only after successful saving. Category edit/menu state resets across datasets.

Verification on macOS: rebuilt the rules-enabled Release application and affected UI tests. `ctest --test-dir build/import-ui-rules-release -R '^import.(category_ui|qml|ui|navigation)$' --output-on-failure -j 2` passed **4/4** (51.64 s). Native Cocoa `spentit_category_ui_tests` passed **4/4 including init/cleanup** (7.427 s). Coverage includes inline Enter acceptance, Escape cancellation of create/rename, actual pointer focus-loss saving, empty-row discard, invalid/duplicate names, filtered creation, valid/invalid navigation, right-click targeting, archive/unarchive, referenced-delete refusal, unused deletion, retained assignments/source evidence after rename/reopen, and actual scrollbar dragging. Native and offscreen synthetic table renders were inspected. No QML warnings appeared in the selected runs. Changed-document relative links and `git diff --check` passed. No private dataset was opened. Storage behavior/schema did not change; storage fault suites, sanitizers, full accessibility, Linux and Windows were not rerun for this UI change.

## Transparent icon corners — 2026-10-28

The owner approved deterministic transparency editing after an image-generation attempt changed artwork and was rejected without applying it. Only the alpha channel of the original light/dark 1024 px masters changed; their RGB bytes are identical to the owner-supplied originals. The external corner backgrounds were removed with a smoothed boundary while preserving opaque tile interiors. All smaller PNGs, both ICNS containers and the Windows ICO were regenerated. See [asset maintenance](../assets/images/README.md). Native `iconutil` rejected the generated iconset in this environment; the new standard-library ICNS packer uses the original files' existing PNG-backed representation layout and requires no image resampling.

Verification: all 14 PNGs have the expected dimensions and fully transparent extreme corners; master RGB equality and opaque central interiors passed. Both 11-representation ICNS files and all five ICO sizes decoded with transparency. Light/dark composites on white/dark backgrounds were visually inspected. Release application, UI-test and bundle targets rebuilt. Offscreen `welcomeFlow` passed 3/3 including init/cleanup (2.844 s), and `import.qml` passed 1/1 (1.00 s).

The refreshed local package is `build/packages/macos-transparent/SpentIt.app` with a matching `.tar.gz`. It reuses the previously verified deployed dependencies, replaces the rebuilt executable and ICNS, restores the deployed dependency paths, removes the build rpath, and re-signs the bundle. Strict/deep signature verification passed. Native Cocoa `welcomeFlow` passed 3/3 including init/cleanup (3.035 s), exercising light/dark switching; its dark welcome render was inspected with no white corner background. The packaged native `--smoke` exited 0 using isolated settings. The packaged ICNS matches the corrected source, and the refreshed executable has no non-system absolute load dependencies. Linux and Windows execution remain unverified; no financial logic or data format changed.

## Application artwork and local packaging — 2026-10-28

The owner approved the supplied light/dark PNG/ICNS artwork and local packaging only. The welcome screen embeds a 128-pixel logo; it and the runtime application icon follow Qt system color-scheme notifications, using light when unknown. Installed icons use light. The normal `build/SpentIt` executable and existing settings identity are preserved. A separate macOS bundle target includes ICNS metadata; Windows has a generated five-size ICO/resource and packaging preparation; Linux has install rules for the executable, desktop entry and hicolor icons. See [local package commands and limitations](../packaging/README.md).

Windows remains blocked by the existing POSIX storage implementation; neither its guard nor durability behavior was changed. Linux packaging requires compatible installed runtime dependencies and is not a self-contained AppImage. No installers, public distribution, distribution signing/notarization or license decision were added.

Verification on macOS: rules-enabled Release application, bundle and UI-test targets built successfully. `ctest --test-dir build/import-ui-rules-release -R '^import.(ui|qml)$' --output-on-failure` passed **2/2** in 49.04 s. The finalized native Cocoa `welcomeFlow` test passed **3/3 including init/cleanup** in 2.830 s, covering live application-local dark/light overrides and image readiness; light/dark synthetic renders were inspected. The offscreen welcome test passed **3/3** in 2.512 s. An initial attempt to override appearance on the offscreen platform failed because that platform does not honor the override; that part of the test now runs only on Cocoa. A native attempt inside the sandbox failed for lack of screen access; native verification passed with window access.

Python syntax, exact ICO PNG payloads at all five sizes, package overwrite refusal, changed documentation link targets and `git diff --check` passed. The initial macOS deployment reported unresolved Homebrew QtSvg paths while returning success; that output was marked incomplete. The script now searches the deployment tool's adjacent library directory, accepts extra `--libpath` values, saves deployment diagnostics, treats reported errors as failures, and verifies bundle signing. A search path alone did not eliminate the QtSvg diagnostics, so the macOS bundle now explicitly links QtSvg before deployment scans it. The final deployment log was empty. The verified local package is `build/packages/macos-local/SpentIt.app`, with `build/packages/macos-local.tar.gz` (about 52 MiB). `codesign --verify --deep --strict` passed, and the packaged executable’s native `--smoke` exited 0 using isolated settings. An `otool -l` audit of all 130 Mach-O binaries found no non-system absolute load dependencies; library identity metadata is not a load dependency. This is local verification, not a clean-machine portability claim.

These are selected UI/packaging checks, not financial/storage, sanitizer, clean-machine or cross-platform verification. No private dataset was opened. Linux/Windows execution, desktop icon behavior outside macOS, and Windows storage portability remain unverified.

## Dataset setup dates and independent categories — 2026-10-28

Implemented the owner-approved [setup preferences](../features/001-documents-storage-security.md#creation-date-format-and-starter-categories--accepted-2026-10-28) and [independent category model](../features/010-categories-and-selection.md#independent-categories--accepted-2026-10-28). New Dataset has currency-suggested, overrideable date presets plus Custom and a checked-by-default Use default categories option. The approved 41-category list includes Salaries & Wages, Pension and Investment Income under Income. Initial account, preferences, and starter definitions/history are one atomic initialization transaction. Preferences survive reopening and validated backup/restore. Raw import evidence and parser date-order decisions remain independent of display/manual-entry formatting.

Every category is independently assignable. One colon is allowed, separator spacing is canonicalized, and case-insensitive matching preserves saved capitalization. No parent object is created; rename/archive/delete affect only that category. The picker accepts an existing full name with different capitalization on Enter. Ledger search adds `category:Auto` (direct plus colon stem) and `category-exact:Auto` (direct only); existing whole-row totals remain whole-row totals, not allocation reports. Full date searches use the dataset format. QIF ampersands and XML `&amp;` decoding have explicit regression cases. Reports and exports remain deferred.

New experimental schemas are **24 with rules / 23 without rules**. Schemas 22/21 and earlier are rejected unchanged without migration under the pre-release policy. The rules-enabled Release application was rebuilt at `build/SpentIt`. No private financial dataset was opened.

Verification on macOS:

- A 35-check rules-enabled Release selection covered date formats, QIF/statement parsers and storage, search/ledger, category lifecycle/history/backup/corruption/I/O/interruption, rule integration, account lifecycle, creation replacement and UI. It passed 34/35 in 253.34 s; the rules UI test still dereferenced removed group/parent controls. That helper was updated to create the full category name directly. The final affected CTest selection (`import.ui`, `import.qml`, `import.navigation`, `import.recovery_ui`, `import.committed_ui`, `import.category_ui`, `import.rules_ui`, `import.replacement`, `import.search`, `rulestore.schema`, `category.setup`, `category.validation`) passed **12/12** in 53.26 s.
- After widening the date selector, `QT_QPA_PLATFORM=offscreen .../spentit_import_ui_tests welcomeFlow configuredDatesAndCategories` passed **4/4 including init/cleanup** in 3.950 s. The synthetic setup render was inspected. Checks include currency suggestions, preserving a manual format override, default categories checked, an ambiguous MDY import displayed as DMY, invalid manual date rejection, saved correction/search, and reopening with the preference retained.
- ASan/UBSan initially passed **14/14** selected engine/parser/search/category/setup/replacement/UI checks in 90.34 s. Subsequent UI/search checks (`import.ui`, `import.category_ui`, `import.rules_ui`, `import.search`) passed after the final model changes. The extended replacement interruption test exposed a macOS post-fork Foundation/libplatform lock failure (`SIGKILL` before the intended injected exit), not a sanitizer memory-error report. Its launcher now uses `posix_spawn` for fresh test processes, retaining fault points and complete-state verification without disabling diagnostics. Final `ctest --test-dir build/import-ui-rules-sanitized -R '^import.replacement$' --output-on-failure` passed **1/1** in 10.54 s; the matching Release check passed **1/1** in 3.82 s. It covers selected disk-full/write/sync positions, interrupted creation, invalid-format preservation before overwrite, full seeded state or unusable incomplete creation, and explicit retry.
- The rules-off Release build passed **5/5** (`import.schema_gate`, `category.setup`, `category.catalog`, `category.names`, `category.backup`) in 2.82 s.

This is selected verification, not a new full-suite or portability claim. Native interaction/accessibility, Linux, Windows, large-catalog performance and full report behavior remain unverified. The starter names were checked against the approved document; Markdown relative-link file targets and `git diff --check` were also checked.

## Startup opening and Close Dataset — 2026-10-28

The last-used dataset now opens automatically at startup if the no-password probe verifies it. The welcome screen is skipped during that attempt; existing worker progress/cancellation remains available. Protected, missing, invalid or locked datasets stay on the welcome screen with the applicable password controls/errors. File → Close Dataset uses the existing unsaved-edit guard and returns to the welcome screen without reopening automatically. Browsing/opening another dataset remains explicit. This supersedes the explicit-startup-Open behavior recorded below; storage validation and locking are unchanged.

Verification on macOS: rebuilt the rules-enabled Release application and UI/navigation/recovery targets. `ctest --test-dir build/import-ui-rules-release -R '^import.(ui|qml|navigation|recovery_ui)$' --output-on-failure -j 2` passed **4/4** (51.07 s). New startup cases exercise public-key opening, password-protected, missing, invalid and locked files, Close Dataset with unsaved-edit Stay/Discard, remaining closed after probing, and explicit reopening. `git diff --check` passed. Tests used temporary synthetic datasets; native menu interaction, other platforms and sanitizers were not checked for this change.

## Account sidebar usability — 2026-10-28

Account actions now use a right-click menu on the account name, with guarded selection of the clicked account before actions become available. **+ New Account** sits beside the Accounts heading. Setup clearly labels the initial account name; creation and minimum-one-account deletion rules are unchanged. See [account behavior](accounts.md).

Verification on macOS: rebuilt the rules-enabled Release application, UI and navigation tests. `ctest --test-dir build/import-ui-rules-release -R '^import.(ui|qml|navigation)$' --output-on-failure -j 2` passed **3/3** (44.22 s). After the final menu/header adjustments, `QT_QPA_PLATFORM=offscreen .../spentit_import_ui_tests accountContextMenu` passed (test plus init/cleanup: **3/3**, 1.034 s). It covers right-click targeting, unsaved-edit Stay, rename, eligible empty-account deletion, last-account deletion refusal, and archived-account selection/reactivation. Synthetic menu/header renders were inspected; `git diff --check` passed. No private dataset was opened. Native keyboard interaction, sanitizers, Linux and Windows were not checked for this change.

Account-dialog keyboard follow-up: Return/keypad Enter in Rename, opening-correction fields, and the currency-code field invokes the existing acceptance action, respecting busy/disabled controls. Rename focuses its field on opening. Rebuilt the Release application and UI tests; `accountDialogKeyboard accountContextMenu` passed **5/5** including init/cleanup (2.362 s, offscreen macOS). Checks include both Enter keys, Escape cancellation, invalid-name/currency handling, both opening fields, and existing menu targeting. The first opening test setup was rejected by the existing account-command guard; using a separate new account corrected the fixture. No storage or financial policy changed.

## Welcome, creation and usable scrollbars — 2026-10-28

Implemented the owner-approved [welcome and creation flow](../features/001-documents-storage-security.md#welcome-and-creation--accepted-2026-10-28): centered SpentIt/tagline, one path field, Browse/New, conditional Open/password row, and red errors below the controls. A future icon has reserved space without placeholder artwork. Startup probes the remembered path (or Documents/`spentit.spentit`) on the worker; no-password verification does not activate the ledger. Protected/unrecognized encrypted input shows a password field without an initial authentication error. Enter and Open use the same unlock command. File dialogs start in Documents; new names receive `.spentit` when needed. Creation defaults to SpentIt/CAD/password-on and rejects an empty password explicitly.

Existing destinations require Cancel/Overwrite. Confirmed overwrite truncates/recreates under the existing exclusive lock and discards old journals, with no mandatory backup or staged replacement. This owner-approved creation operation is intentionally destructive; failure can leave an incomplete replacement. Inputs and regular-file/lock checks precede replacement, and success still follows verified durable creation. Tests cover refusal without confirmation, byte preservation on Cancel/locked/invalid-input cases, symlink refusal, empty replacement/reopen, selected disk-full/write/sync failures, retry, and abrupt interruption before and after creation. Backups retain their separate no-overwrite behavior.

Menus and scrollable panes use persistent draggable 16 px bars when content overflows. The ledger bar stays at the visible viewport edge even when its columns scroll horizontally. Its columns reserve bar space. Interaction tests drag actual currency-menu and ledger thumbs. The viewport regression now accounts for Qt's movable content origin after a long drag.

ASan initially exposed a document-worker stack overflow during SQLCipher verification. The single worker now reserves a bounded 4 MiB stack in all builds; the affected sanitizer checks subsequently passed. Startup smoke checks explicitly use a missing path inside temporary settings, preventing a probe of the user's Documents file.

Verification on macOS: rebuilt `spentit_import_experiment`, `spentit_import_tests` and the six affected UI test targets in `build/import-ui-rules-release`. `ctest --test-dir build/import-ui-rules-release -R '^import.(ui|qml|navigation|recovery_ui|committed_ui|category_ui|rules_ui|replacement)$' --output-on-failure -j 2` passed **8/8** in **43.49 s**, with no QML warnings in the final CTest log. In `build/import-ui-rules-sanitized`, `import.replacement` passed **1/1** (5.84 s); `QT_QPA_PLATFORM=offscreen .../src/ui/spentit_import_ui_tests welcomeFlow ledgerInteractions` passed both selected cases plus init/cleanup (4/4, 6.296 s) without ASan/UBSan diagnostics after the worker-stack fix. The initial sanitizer stack-overflow failure is recorded above, not counted as a pass. Native `./build/SpentIt --smoke` exited 0. Synthetic missing-file and creation/currency-menu renders were inspected and corrected; the interaction check exercises a narrow ledger viewport. No private dataset was opened. These are selected checks, not a complete regression/fault sweep or Linux/Windows verification.

## Prototype wording cleanup — 2026-10-28

The application no longer shows the experimental/synthetic-only banner, synthetic account default, experimental filename/dialog labels, obsolete schema-specific advice, or the development-plan Reports placeholder. At that intermediate checkpoint, new-document suggestions used `My Finances.spentit`; the subsequent welcome flow above supersedes that default with `spentit.spentit`. Existing saved paths are preserved. Forgotten-password consequences appear beside password setup, and the no-password confidentiality waiver remains. Unsupported-format and unavailable-feature errors describe the actual limitation without instructing users to create another experimental dataset. The current unsupported-format error is `This dataset format is not supported by this version of SpentIt; the file has not been changed`; older quoted messages below are historical.

Only presentation/default text and matching test expectations changed. Financial commands, supported formats, encryption and rejection behavior remain unchanged. Internal build target/settings identifiers are retained; the settings identifier preserves the remembered document. Separate synthetic harnesses and historical verification retain their accurate labels. Reports, migrations and distribution remain deferred.

Verification: rebuilt the application and affected test targets in rules-enabled Release. `ctest --test-dir build/import-ui-rules-release -R '^(import.(qml|ui|rules_ui)|rulestore.schema)$' --output-on-failure -j 2` passed 4/4 (41.91 s). Rebuilt the rules-off Debug schema-gate target; `ctest --test-dir build/import-ui-debug -R '^import.schema_gate$' --output-on-failure` passed 1/1 (1.98 s). These schema tests retain checks that rejected files are unchanged. Native `./build/SpentIt --smoke` with window access exited 0 using isolated settings. `git diff --check` passed. No private dataset was opened; sanitizer and other-platform checks were not run for this presentation cleanup.

## Application launch and current direction — 2026-10-28

The owner has deferred reports, expanded document lifecycle/recovery, and distribution in favor of piecemeal usability improvements. The earlier next-area statements below are historical. The first change provides `./build/SpentIt` as the regular local application launch path. The existing rules-enabled Release tree is configured with `SPENTIT_APPLICATION_OUTPUT_DIRECTORY` pointing to `build/`; rebuilding its `spentit_import_experiment` target updates that executable. Other trees default to their own binary directories. The application display name and window title are SpentIt; the QSettings identity remains unchanged to preserve the remembered document. No app bundle was added. See [launch commands](../README.md#run-the-application).

Verification: reconfigured `build/import-ui-rules-release` and built `spentit_import_experiment` successfully on macOS. `ctest --test-dir build/import-ui-rules-release -R '^import.qml$' --output-on-failure` passed 1/1 (2.56 s). `./build/SpentIt --smoke` initially failed inside the sandbox because no screens were available; the same native smoke launch with window access exited successfully (0). Smoke mode uses isolated temporary settings and no private documents. These are startup checks, not a full regression suite or cross-platform verification.

Point 7 is committed as `b6b9346` on the [sequential roadmap](roadmap.md#current-authorization-and-delivery-checkpoints--2026-10-26). Point 6 is committed as `cfad307`. Point 5 is committed as `786bc96`. Point 1 is committed as `933ede1` and `f77b4ac`. Point 2 is committed as `ec17bb2`. Point 3 is `f1004ea`. Point 4 is `47010b5`. New documents use bank-v3 together with OFX SGML parser `ofx-sgml-1` or OFX XML parser `ofx-xml-2`, experimental schema 22 with rules and 21 without. Schema 20, schemas 18 and 19, and every other unexpected `user_version`, are rejected with `Unsupported experimental format; no migration was attempted`. Application id remains `1397770577`. Saved rules can be reapplied to the selected account's active committed rows in one preview, one Apply, and one group Undo. Manual fields stay unless that preview replaces them. Amounts, dates, evidence, and a live transfer category stay. A rules-off document has no reapply command. The locked committed link edits remain: retiring a purely generated committed duplicate, ending a link when one committed side's amount, category, or removal changes, and keeping a date-only link. Moving a committed row to another account and rewriting source-occurrence links are not done. Point 8 is committed as `b1ff74b`. Feature 005 stays active. Linux and Windows remain unverified. The eight-point sequence is complete. On 2026-10-28 the owner said that sequence was the work to finish before reports. Feature 006 is unimplemented, and its open questions remain. Report implementation waits for an explicit Go and those answers. See [feature 005](../features/005-transaction-sheet-navigation.md#point-8-sheet-slice--2026-10-27), [feature 006](../features/006-reporting-drill-down.md), and the [handoff](roadmap.md#point-8-completion-handoff--2026-10-27).

Point 2 evidence remains in [hardening](hardening.md). Spoken screen-reader output, a paired large native cache comparison, and 200k native rendering stay unverified from that checkpoint.

The schema-6/rules-sheet verification gap is closed: the `3f94fe6` baseline passed all 72 tests in Release and ASan/UBSan on macOS. Combined search, identity-based multiselection, and schema-7 atomic committed bulk category/removal/Undo are implemented in the bounded QIF experiment; their contract and limits are in [search and ledger](search-ledger.md). Detailed verification is recorded at the end of this file. The owner confirmed that experimental formats are disposable until an explicitly announced release; migrations, upgrades, and backward compatibility are not pre-release obligations. The [account-scoped workflow](accounts.md) now provides multiple durable workspaces, and the [bounded transfer integration](transfer-engine.md) adds per-account commits, protected drafts, provisional evidence matching, allocation-level Merge, and unlink. [QIF transfer admission and explicit resolution](qif-transfers.md) are now implemented. The locked committed link edits are implemented. Selected-account committed rule reapplication is implemented. The point 8 sheet slice adds session sidebar balances, fixed ledger column ratios, horizontal scroll below 1080 pixels, and viewport retention for bulk, reapply, and history undo. Moving a committed row, rewriting source-occurrence links, multiline notes, and the rest of the sheet remain unfinished. On 2026-10-28 the owner said the finished sequence was the work before reports. Feature 006 holds the open report questions.

SpentIt has a buildable C++20 financial engine, an optional isolated encrypted-storage/recovery prototype, an optional synthetic Qt Quick transaction-sheet prototype, and a partial durable QIF workflow with an optional setup/review UI. The durable UI is now the SpentIt application being improved toward release; the separate prototypes remain development harnesses. Release readiness and the outstanding verification below remain separate obligations.

A [pure transfer lifecycle prerequisite](transfer-engine.md) now implements allocation-linked generation, manual origins, unmatched acceptance, provisional match transitions and per-account commit/clear proposals, including protection of an entire split origin. The subsequent durable transfer and QIF destination packages connect these transitions to storage and review; remaining repair/reversal policies are recorded there.

## Implemented engine

Exact signed integer money with explicit currency/scale, strict decimal input, checked arithmetic, validated mixed-sign allocations, original imported amounts retained separately from effective-total corrections, and designated-remainder editing with explicit mixed-sign choice. Manual allocations and prior values survive rejected edits unchanged.

The current account-selection catalog contains all 165 numeric-scale ISO codes in the pinned SIX List One publication of 2026-09-17. Retained amount definitions are separate: those 165 definitions plus verified BGN and HRK definitions preserve exact units, code, scale, and identity independently of current eligibility. Unknown definitions fail explicitly; no historical scale or conversion is guessed. Broader historical coverage and date-based resolution remain unimplemented. See [feature 002](../features/002-accounts-money-splits.md), [currency provenance](../third_party/iso4217/README.md), and the [split contract](split-editing.md).

The engine has no UI, concrete storage, or third-party software dependencies. Nine engine test groups passed in Debug, Release, and ASan/UBSan on macOS 27.0.1 arm64 with AppleClang 21.0.0, CMake 4.4.4, and Ninja 1.13.2. The offline catalog check and a compiled simulated CAD-withdrawal regression passed: its original amount remained readable after CAD was removed from a temporary current selection catalog.

## Encrypted-storage prototype

The owner approved SQLCipher Community Edition for this bounded experiment. The optional `spentit_storage_prototype` target uses pinned SQLCipher 4.19.0, with the tested OpenSSL 3.5.9 provider. It saves immutable synthetic source evidence and validated draft snapshots, restores historical money, excludes drafts from committed-amount queries, and atomically publishes a batch with operation history and draft transitions. Stable operation identities make identical retries idempotent; conflicting reuse is rejected.

Encrypted backups use the supported database backup API, validation and reopen before exclusive publication. Restore is tested into a new document. Existing backups are not overwritten. Unknown schemas and invalid definitions/amount relationships fail explicitly. See [prototype decisions, build instructions, verification and recovery limits](storage-prototype.md).

Debug, Release and ASan/UBSan verification passed all 17 engine/storage test groups, including injected disk-full and write/sync failures, abrupt exits around commit and backup I/O, both commit boundaries, retry checks, wrong passwords, malformed authenticated documents, writer contention, backup restoration and scans for synthetic private-data markers. No compiler warnings or sanitizer diagnostics were emitted. The source-preparation helper also reproduced the pinned hashes from a fresh extraction. These are bounded test results, not proof of safety under physical power loss or all filesystems.

## UI requirements recorded — 2026-10-25

The owner-approved main layout is documented in [feature 005](../features/005-transaction-sheet-navigation.md): resizable/hideable navigation, Accounts/Reports sections, persistent toolbar, ledger sorting and filtered totals, committed/projected balance toggling, and Save / Discard / Stay navigation. Search will combine whole-record text matching, period filters, and a QIF-source mode; grammar remains unresolved. [Feature 002](../features/002-accounts-money-splits.md) records opening-balance defaults, account archiving, and deliberate currency reassignment without conversion. Scale/transfer handling for reassignment still needs design. Settings, category/rules editors, and report-generator contents remain future discussions. These are requirements only; no UI or account behavior was implemented by this documentation update.

## Transfer requirements recorded — 2026-10-25

[Feature 008](../features/008-transfer-matching-and-lifecycle.md) records accepted transfer matching, provenance, Fix match, and removal behavior. Commit is explicit per account: committing checking protects its generated Visa draft without committing Visa. That draft survives destination workspace clearing, affects projected balance only, and requires editing/reversing its origin to remove the purely generated movement. Independent bank evidence is preserved. Per-operation relationship/history updates remain atomic. Matching in either import order, attention for unmatched generated entries, and match-icon navigation are specified. [Feature 005](../features/005-transaction-sheet-navigation.md) records search selection/view restoration. Later committed reversal, independent-evidence reconciliation, and detailed match policies still need design. No transfer, search, or UI implementation or runtime verification is claimed by this documentation update.

## Synthetic transaction-sheet prototype — 2026-10-25

The owner authorized a bounded sheet experiment. The optional `spentit_sheet_prototype` executable now implements the shell, resizable/hideable sidebar, two synthetic account views, exact totals and balance toggles, virtualized ledger, sorting/literal filtering, selection, inline split/total editing and Save / Discard / Stay navigation. Temporary edits and selections use stable identities outside recycled delegates. All data is synthetic; saves last only for the running session. It does not use the storage prototype or implement production account/history behavior.

Plain C++ session use cases remain independent of Qt. The Qt adapter performs generation, queries and saves in background workers; QML does no money arithmetic. All 13 engine/session/model/QML/interaction CTest entries passed in Debug, Release and ASan/UBSan on macOS arm64. Native scrolling was measured separately. The owner selected 100,000 transactions/account as representative and 200,000 as the stress workload, not record limits. See [prototype scope, build, measurements and limitations](sheet-prototype.md). Qt Quick remains a product candidate, not a final architecture decision.

## Remaining product work

All [feature documents](../features/README.md) remain **active**. Features 001, 002 and 005 have bounded implemented subsets. Production accounts, transfers, and reports do not exist. A bounded rules evaluator, pure preview adapter, schema-6 Save/Apply store, and rules sheet are recorded below. The sheet is `147a5be`. A bounded experimental durable UI now connects setup, review, saved edits and explicit commit. The new experimental headless QIF workflow implements a bounded parser, duplicate review commands, one account and deterministic ledger ordering; it does not complete these product features. The synthetic sheet conventions do not settle those designs. The original storage prototype does not implement full document lifecycle or draft editing/discard. The new headless import experiment adds durable draft edits/discard, and the optional UI connects those commands, but full document lifecycle, transfer-aware undo, password changes, migrations and their recovery workflows remain incomplete. It is not intended for personal financial data.

The [project layout](project-layout.md) is realized for engine, optional storage, synthetic app/UI, their tests, build support and reference/dependency metadata. Qt Quick/QML is used in the synthetic sheet and durable import UI experiments; final adoption remains a proposal. PCRE2 10.49 is used by the optional bounded rules evaluator. SQLCipher is selected for the prototype only. Linux is unverified; the storage prototype is POSIX-only and explicitly does not build on Windows. No portability or general responsiveness claim is made; bounded macOS sheet measurements are recorded separately.

The owner's licensing intent remains permanent free access, prohibition on selling SpentIt or derivatives, and free internal business use. Exact terms remain unresolved; no project license has been selected or added. Third-party notices are retained separately. See [licensing and distribution](../features/007-licensing-and-distribution.md).

The next bounded milestone has been selected; its remaining material questions must be settled before dependent implementation. The engine and storage experiments do not settle the final application schema, UI or distribution design.

## Accepted next milestone scope — 2026-10-25

The owner authorized a design for the [first durable import workflow](../features/009-durable-import-milestone.md): one-account QIF evidence, duplicate review, durable draft edits, explicit commit and exact ledger reconciliation. The owner accepted all five policy groups: bounded QIF profile/resource limits, duplicate review, whole-batch scope, durable saves/evidence retention, opening-balance freeze and chronology/balance modes, with Qt Quick/SQLCipher continued for the experiment. Rules, transfers and category reports are deferred; transfer-bearing imports block commit. QIF will be completed first. Future QFX, ASO, QBO and OFX support is an accepted requirement, with detailed dialects and implementation deferred. The owner has matching private exports of the same real-world dataset in those four formats for possible future comparison. The owner clarified that each running application has one active dataset, selected by a Settings path, and can close it and open another. No application-wide instance limit is required; exclusive document locking remains in place. Private samples are available from the owner on request; no paths or contents are recorded and none were inspected for this update. The owner subsequently authorized implementation. The [headless subset](durable-import.md) now implements the bounded parser, encrypted source/draft revisions, duplicate review, commit receipts/retries, opening confirmation/freeze and ledger reconciliation. The optional [durable UI](import-ui.md) now adds setup/unlocking, Settings path selection, serialized close/reopen, review/edit, commit and backup/copy. Bounded date/split-total repair is now implemented as described below; broader repair and UI scope remain incomplete.

Design verification: all 34 relative link targets across the new milestone, feature index and this status document exist; `git diff --check` passed. The proposal was compared with features 001–006 and 008 and the inspected prototype interfaces, storage constraints and existing test assertions. No runtime tests were run for this documentation-only change, and no milestone acceptance criterion is claimed implemented or verified.

Headless implementation verification: Debug, Release and ASan/UBSan each passed 31/31 engine/parser/storage/import/ledger tests on macOS arm64. The unchanged sheet Debug configuration passed 17/17 checks. Synthetic 100,000/200,000-row durable measurements and exact fault-injection counts are recorded in the [implementation report](durable-import.md). The local toolchain lacks libFuzzer's runtime, so coverage-guided fuzzing remains unverified; deterministic mutation tests ran under sanitizers. No private samples were accessed. This is partial milestone progress, not a completed writable application.

## Version control baseline — 2026-10-25

The project is tracked in a local Git repository on `main`. The initial snapshot includes source, tests, requirements, verification records, dependency notices and pinned currency references. Ignore rules exclude generated builds, local settings and common sensitive/runtime files; attributes define text line endings, binary assets and byte-preserved upstream XML. See [repository conventions](../README.md). No remote publication or project-license selection is part of this setup.

## Optional-password setup clarification — 2026-10-25

The owner accepted optional password protection, checked by default, and a browsable Documents/SpentIt location during setup. No-password mode knowingly waives confidentiality and must use a documented fixed public unlock mechanism recoverable without the original installation; it must not depend on a machine-local secret or collide with user passwords. Unlock selection and file/corruption errors have separate handling. See [accepted requirements and recovery obligations](../features/001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25). These were requirements only at that handoff; the new [UI implementation](import-ui.md) implements a separate gateway with password-domain separation and public-key opening. The direct headless API remains unchanged.

## Setup/unlocking and durable UI — 2026-10-25

The optional `SPENTIT_IMPORT_UI` target builds `spentit_import_experiment`. It provides new-dataset setup, default-on optional password protection, a portable public no-password key, verified unlock/error routing, saved dataset-path selection, close/reopen, bounded QIF file reading, duplicate review, exact split/total draft editing, Save / Discard / Stay, opening confirmation, explicit batch commit, receipts, ledger filtering/sorting and backup/restore-copy publication. All document operations run serially on a worker; switching is unavailable during an in-flight command. The separate synthetic sheet remains unchanged.

The [UI report](import-ui.md) documents the credential convention, installation-independent SQLCipher recovery, build/run commands, an owner walkthrough and verification. The [review fixture](../tests/fixtures/qif/durable-review-synthetic.qif) is wholly synthetic. No private financial samples were accessed. Older direct-headless credentials are not silently converted; their API remains available.

Verification: Debug, Release and ASan/UBSan passed 33/33 CTest entries, with affected UI/QML checks rerun after final refinements. The four existing sheet Debug checks also passed. Direct SQLCipher public-key backup recovery and password-mode separation passed; the populated QML review screen was rendered and inspected. See the [detailed verification record](import-ui.md#verification-and-remaining-work).

Full feature 009 remains incomplete. Repair beyond dates/split-total mismatches, full multi-account/report navigation, broader selection/search interactions, accessibility and further durable-UI performance work remain. Later entries below record implemented split previews, bounded account/Settings navigation and cooperative opening/import cancellation. The review screen is usable for bounded synthetic owner review, not personal financial data. Linux/Windows, physical power-loss guarantees, password changes, migrations and forgotten-password recovery are not claimed.

## Next-session starting point — 2026-10-25

Read the [UI implementation report and walkthrough](import-ui.md), [durable command report](durable-import.md), and features 001, 005 and 009. Continue from the writable experimental UI rather than rebuilding setup. The automated walkthrough has been extended to use the documented fixture, verify receipt counts and ledger reconciliation, and restore a separate working copy. Owner native-interaction review remains available. Preserve the recorded limitations and inspect Git status before making changes. No publication or license selection was authorized.

The follow-up baseline passed 33/33 Debug tests. Backup verification now covers the import wrapper itself: Debug, Release and ASan/UBSan each passed five affected entries, including 120 injected SQLite backup I/O failures, 120 backup process-exit positions and 20 publication-boundary cases. The [recovery guidance](durable-import.md#backup-interruption-recovery) explains retained hard-linked candidates, separate-copy verification and retry to a new destination. Filesystem failures are simulated at boundaries; physical power-loss guarantees remain unverified. No production backup behavior or financial policy changed.

The owner selected bounded date and split-total repair. It is now implemented across parser evidence metadata, atomic saved interpretations, verification and the review UI in new schema-2 datasets. Schema 1 remains readable without migration. Original amounts/bytes/diagnostics remain retained; a first repair requires explicit duplicate review before commit. Other malformed records remain blocked. Before extending repair to missing/invalid original amounts, settle unknown-value and duplicate-evidence policy. Inline editing/source-field copying is implemented as described below; bounded large durable-UI measurements are now recorded in the [performance report](import-ui-performance.md). See [repair details](durable-import.md#bounded-record-repair).

Repair verification: Debug, Release and ASan/UBSan passed 36/36 CTest entries; the three affected repair/UI/QML entries passed again after final refinements. Checks cover atomic repair-save failures/interruption, restart, duplicate review, original evidence, backup/restore and legacy-format refusal. The QML repair screen was rendered and inspected. See [the detailed record](durable-import.md#bounded-record-repair). All feature documents remain active; no broader UI, portability or production readiness claim follows.

## Inline durable editor follow-up — 2026-10-25

The durable UI now expands one selected ledger record inline and exposes parsed source-field copying into its unsaved edit buffer. Hidden/blocked/excluded records retain a separate review panel. Copies use existing validation and atomic saves; original evidence remains unchanged. Sorting/filtering preserve the selected occurrence identity, and Save / Discard / Stay remains in effect. See [scope and verification](import-ui.md#inline-editing-and-source-copying).

The affected `import.ui` and `import.qml` entries passed after rebuilding Debug, Release and ASan/UBSan; the full 36-entry suite was not rerun for this UI change. The rendered inline source view was inspected. Simultaneous read-only split expansions were subsequently implemented (see below); broader navigation/search, native accessibility review and further performance work remain outstanding. Bounded large durable-UI measurements are now recorded in the [performance report](import-ui-performance.md). All features remain active; no production or portability claim follows.

## Durable UI performance follow-up — 2026-10-25

Native Release workloads at 100,000 and 200,000 synthetic transactions now have [recorded measurements](import-ui-performance.md). Read-only queries reuse verified ledger data, display maps are built only for requested rows, large retired views are released on the worker, and read-only inspection skips unused duplicate-action labels. Writes and document switches invalidate cached data; storage/schema/financial policies are unchanged.

In one serial before/after run per size on an Apple M2 with 16 GiB RAM, sorts fell from 4.1–4.2 seconds to 38–87 ms at 100,000 rows and from 8.3–8.8 seconds to 68–114 ms at 200,000. Peak RSS through saved-row reopening fell from 813 to 453 MiB and 1,473 to 742 MiB. The baseline processes were stopped in their redundant post-commit inspection; no completed baseline commit timing is claimed. Both optimized workflows completed all correctness assertions, including commit and reopening. Saves still take about 4.2/8.5 seconds including authoritative ledger refresh, and opening takes about 9.4/19.4 seconds including full verification. These are bounded single-run results, not general responsiveness guarantees.

The affected UI, QML and new benchmark smoke CTest entries passed in Debug, Release and ASan/UBSan; the full 37-entry suite was not rerun. Native logs and test logs showed no Qt or sanitizer diagnostics. Broader workloads, cancellation, native accessibility and Linux/Windows validation remain outstanding. All feature documents remain active.


## Read-only split viewing — 2026-10-25

The durable QIF ledger now supports several independent read-only split expansions alongside its single active editor. Saved categories, memos and signed allocation amounts are displayed with currency; view state follows occurrence identities through sorting/filtering/recycling and clears on dataset changes. Long split views are height-bounded and virtualized. See [behavior and verification](import-ui.md#read-only-split-viewers--2026-10-25). This completes that bounded review interaction, not the remaining navigation, cancellation or recovery UX work.

The affected `import.ui` and `import.qml` tests passed in Debug, Release and ASan/UBSan after rebuilding; final logs had no compiler, Qt or sanitizer diagnostics. Offscreen rendering was inspected. The full 37-entry suite, native accessibility and other platforms were not checked for this UI-only change.


## Filter return navigation — 2026-10-25

The durable QIF UI now restores pre-filter selection, sorting, mode and viewport when a filter is cleared without choosing a result. Explicitly inspecting a result instead preserves it and brings it into view. Clear/Escape use the existing Save / Discard / Stay guard. Occurrence identities anchor the view through recycled rows; session state clears on dataset changes. See [behavior and verification](import-ui.md#filter-return-navigation--2026-10-25). Broader search grammar, multiselection, navigation shell, cancellation and recovery UX remain incomplete.

The affected UI/QML tests passed after rebuilding Debug, Release and ASan/UBSan, with no compiler, Qt or sanitizer diagnostics in final logs. Tests include result visibility, pre-search viewport identity and sort restoration, guarded clear outcomes and dataset isolation. The full suite and other platforms were not rerun for this change.


## Source-evidence direction — 2026-10-25

The owner accepted retaining each imported file in its original format, alongside common parsed source facts and an independently editable transaction. Stable relational associations will allow one transaction to retain evidence from several overlapping exports without counting it twice. See [the accepted direction and remaining design](../features/003-qif-import-review.md#accepted-source-evidence-direction--2026-10-25). This is documentation of future cross-format behavior; the implementation remains QIF-only, and committed deletion/reimport policy and the final relational schema remain to be designed.

Documentation reconciliation before commit: updated the feature index and current UI summaries for split previews/filter return, and recorded the source-evidence agreement separately from future schema/deletion decisions. All 108 relative links and heading anchors across the eight changed documents validated; `git diff --check` passed. The current `import.benchmark_smoke` test also passed in Debug, Release and ASan/UBSan via `ctest --test-dir build/import-ui-<configuration> -R '^import.benchmark_smoke$' --output-on-failure`; this complements the affected UI/QML results above without claiming a new full-suite or large native workload run.


## Category direction recorded — 2026-10-25

[Feature 010](../features/010-categories-and-selection.md) captures the ahead-of-roadmap category discussion: one grouping level with assignable leaves only, meaningful parent rollups, intentional Amazon classification for practical quarterly review, familiar Auto/Utilities naming, special Uncategorized handling, QIF-compatible names, and no mandatory expense/income designation. Account destinations remain explicit transfer references under feature 008. The supplied category list is retained as a proposal; heading/prefix reconciliation, final safe spellings, category lifecycle, and income/refund reporting details remain open. Related feature documents and the index link this design for future work. No implementation scope or runtime behavior changed.


## Coordinated cancellation, recovery and navigation — 2026-10-25

The [coordinated roadmap](roadmap.md) now governs delegated work without per-step owner approval. The durable experiment adds a resizable/hideable account sidebar, guarded account/Settings Back/Forward navigation, operation-phase feedback and cooperative cancellation. Commit acknowledgement failures resolve through the same durable operation identity; an unreadable outcome closes the session for explicit reopen.

New experimental schema-3 datasets retain source evidence separately from atomic draft preparation. Cancelled/interrupted preparation contributes no financial rows or authoritative projection and offers explicit Resume/Discard. Original bytes, actual parsed counts, revisions and outcomes survive restart and backup restoration; selecting the same file does not silently resume it. Existing schema-1/2 documents remain readable without migration. See the [storage contract and failure coverage](durable-import.md#retained-evidence-and-cooperative-cancellation--2026-10-25) and [UI behavior](import-ui.md#coordinated-navigation-and-operation-recovery--2026-10-25).

Release and ASan/UBSan each passed all 40 registered CTest entries. After final QML sizing/contrast and one durability-guard test refinement, the six affected entries passed again in both configurations, with clean final build, Qt and sanitizer logs. Debug also passed all 40 cases across two invocations, with the final cancellation assertion included in the later run. Native macOS recovery UI checks passed 4/4 and the rendered incomplete-import screen was inspected. Earlier shell checks also passed natively. These are bounded synthetic interactions, not a full accessibility audit or Linux/Windows validation. No private financial inputs were accessed.

The owner accepted the [ordinary committed correction/removal/strict-undo policy](../features/011-committed-editing-and-reversal.md). The subsequent bounded schema-4 implementation is described below. Transfer changes, rule semantics, reporting policies and distribution decisions remain separate work.


## Ordinary committed editing, removal and Undo — 2026-10-25

The accepted one-account policy is implemented in new experimental schema-4 datasets. Corrections update current interpretations; removal removes the balance effect while preserving source evidence, original publications and history. Strict Undo appends a new revision only when the target and its recorded dependencies remain unchanged. Original import receipts retain their original values. Reimports matching removed entries require explicit review; later target changes invalidate pending exclusion reviews and withdraw authoritative projection until reconfirmed.

The UI provides explicit editing, exact before/after and balance previews, retained editor buffers, guarded navigation and a virtualized change-history page including removed entries. Durable operation identities resolve lost acknowledgements; unknown outcomes or failed post-save refresh close the session with an accurate explanation. Schemas 1–3 remain readable without migration and reject these new commands. Authenticated version/shape mismatches fail before storage-setting mutations. See [implementation, recovery and limitations](committed-changes.md).

Release and ASan/UBSan each passed all 51 registered cases, then passed the nine affected checks after the final integrity guard and replay-cancellation refinements. Debug passed the existing 40 regressions and all ten new committed groups across separate invocations, then passed 17 affected checks after rebuilding the final source. Final build, Qt and sanitizer logs were clean. Independent committed failure coverage includes 49 injected write/sync failures and 49 process exits for each of correction, removal and Undo, plus six explicit before/after-durability exits. The final validation additions cover three authenticated version downgrades as well as eleven other corruption cases. Native and offscreen committed-UI checks passed 7/7 each. These are synthetic bounded checks; cross-platform builds, native accessibility and physical power loss remain unverified. No personal financial files were used.

The owner accepted current category organization for all periods, category lifecycle/standalone leaves, and the bounded rules policy on 2026-10-25. Categories and rules were then the active next package; their implementation and verification were not yet claimed. Transfers, multi-account documents, reports, further import formats, migrations and distribution remain unfinished. All feature documents remain active.


## Categories and bounded rules checkpoint — 2026-10-25

New schema-5 datasets add stable category identities, one-level groups and standalone leaves, explicit mapping from imported text, archive/unarchive and unused-empty deletion, immutable definition/assignment history and guarded editor/picker interactions. Current names/hierarchy drive current ledger display and filtering while retained source and earlier snapshots keep original labels. Group archive hides children from new assignments without changing their flags. Historical references protect both leaves and their earlier parents from deletion. Source-allocation identities and manual field ownership persist without inferring relationships from current row positions. Schemas 1–4 remain readable without migration. See [category contract](categories.md).

Debug passed all 61 category/import/engine/storage/UI cases across separate invocations, then passed four affected checks after the final category-verification cancellation refinement. Release passed all 64 combined cases, including the three standalone rules checks. Native and offscreen category UI runs passed 4/4 each, and manager/picker rendering was inspected. Independent category coverage includes 17 authenticated corruption cases, 361 injected write/sync positions and 361 abrupt-exit positions across catalog mutation and draft/committed assignment, plus six explicit category durable-boundary exits. Release also passed five affected checks after the final verification refinement and seven affected UI checks after the final restored UI build. ASan/UBSan passed the remaining 62 cases in its full run; its two timed-out backup cases passed direct reruns as described below. All 64 combined cases are therefore covered by passing results. Ten affected cases passed again after the final source rebuild. Final compiler and sanitizer logs were clean.

An intermediate category fault-test harness registered its VFS twice; that test bug was corrected and both fault modes passed on rerun. Two sanitizer backup sweeps hit the former 600-second test limit after schema-5 metadata increased backup coverage to 330 positions; both passed on direct rerun with a 1,200-second allowance, retaining every position and durability check.

The optional standalone rules evaluator and pure preview adapter passed all 16 registered cases in Debug, Release and ASan/UBSan with clean final builds and sanitizer logs. They implement bounded UTF-8 regex/capture templates, immutable-source transaction/split phases, field-first precedence, manual protection, retained explanations and exact financial preservation. That checkpoint did not persist rules. Schema 6 Save and Apply are recorded in the following section. See [rules contract and limits](rules-engine.md).

An intermittent Qt `DelegateModel::cancel` warning appeared during a targeted filter-reset test (1 of 8 repetitions); occurrence/viewport assertions passed. A workaround was rejected because broader viewport checks regressed, and the original behavior was restored. Final restored Release UI checks passed 7/7 with clean logs. The warning remains a known interaction issue for the later search/ledger work.

At the owner's request, that run stopped after the category checkpoint, documentation and commit. The storage half of the rules package has since landed and is recorded below. The rules sheet's Debug check is recorded after that. Search and ledger completion is the next package. Other-platform validation, full native accessibility, transfers, reports, migrations and distribution remain unfinished.

## Schema 6 rule persistence — 2026-10-25

Rules-enabled import documents are schema 6. They keep the schema-5 category catalog and committed-change history, and add versioned rule definitions, whole-image Save, and one atomic Apply over the current pending batch. Schemas 1–5 stay readable and are not migrated. They reject rule commands. A rules-disabled import build still creates schema 5 and refuses a version-6 file before journal-mode writes. Save does not touch drafts. Apply calls the existing preview once before the write transaction, refuses committed rows, and does not change money, dates, or split structure. Field marks record the rule identity and version. Discarding an import leaves the saved rules in place. The rules sheet that calls these commands is recorded in the next section. The contract is [durable rules](durable-rules.md); the evaluator limits stay in [rules engine](rules-engine.md).

`rules.preview` passed on `main` after the field-mark commits. Schema 6 did not change that preview source. Before `9b7cf40` was cherry-picked, its Debug worktree binaries were re-run here: `rulestore.definitions` 0.34s, `rulestore.application` 1.12s, `rulestore.schema` 1.85s, `rulestore.validation` 36.72s, `rulestore.backup` 0.51s, `rulestore.io` 61.67s, and `rulestore.crashes` 63.01s, all passed. Those binaries were not rebuilt on `main` after the cherry-pick. Release and ASan/UBSan were not run for schema 6. Linux, Windows, physical power loss, and PCRE2 under SpentIt's sanitizer flags remain unverified.

The same Debug build's `LastTest.log` recorded Passed for `import.io`, `import.crashes`, `committed.io`, `committed.crashes`, `category.io`, `category.crashes`, `import.backup_failures` (8 min 32 s), and `import.backup_crashes` (8 min 27 s). Those eight were not re-run while reading the log. The rules-off log recorded `import.schema_gate` Passed. The rules-off fault sweeps were not re-run.

## Rules sheet — 2026-10-25

The rules-enabled import UI has a sidebar rules page. It lists, reorders, enables, and edits rules. Save and Discard apply to the whole image. Preview is separate from Apply. An unsaved preview cannot be applied. Apply runs only when the latest preview used the saved ruleset and the batch, ruleset, and catalog revisions still match. Create and edit start from the open row. Evaluation and writes stay on the existing one-thread import worker. A schema-5 document does not show the page. The UI does not construct a ruleset or execute SQL. The page prints the published rule limits. The sheet is `147a5be`. Schema 6 storage remains `9b7cf40`.

Debug on macOS 27.0.1 arm64, Qt 6.12.0, offscreen. Directory `build/import-ui-rules-debug` (`SPENTIT_RULES=ON` with the import-UI flags) passed `import.category_ui` in 3.76 s, `import.rules_ui` in 4.66 s, and `import.qml` in 0.69 s. That invocation ran those three entries. It did not run the rest of the 72-test list. `import.rules_ui` passed seven slots: schema 6 shows the page and schema 5 hides it; Save, Discard, and Stay, including a dirty draft then dirty rules on close; unsaved reorder preview that does not write, then Apply after Save on a synthetic Wise-shaped pending draft, including a blank manual payee kept when replacement is off, a committed row that does not offer Apply, and create-from-source quoting; obsolete derived-payee removal; a busy worker ignoring a second command; cancel during preview; and a known-saved Apply whose refresh fails. The rules-off Debug directory `build/import-ui-debug` then passed `import.ui` in 15.52 s, `import.navigation` in 1.20 s, `import.recovery_ui` in 1.83 s, `import.committed_ui` in 5.04 s, `import.category_ui` in 3.60 s, and `import.qml` in 0.76 s. That invocation ran those six entries. Release and ASan/UBSan were not run for the sheet or for schema 6. Linux, Windows, physical power loss, committed reapplication, and PCRE2 under SpentIt's sanitizer flags remain unverified. No personal financial files were used.

## Handoff — 2026-10-26

The verification gap and authorized bounded search/ledger package are complete. The [current handoff](roadmap.md#handoff--2026-10-26) records resumption steps, accepted decisions, remaining verification limits, and the transfer-policy gates for the next substantive package. Search is no longer pending, and schema-6 Release/sanitizer coverage is no longer missing. The detailed historical checkpoints above retain their original evidence; the results below supersede their then-current gaps. Experimental-format migration work is not required before an owner-announced release.

## Schema-6 verification gap closed — 2026-10-26

Before search implementation, clean rules-enabled Release and Debug+ASan/UBSan trees were configured and built from `3f94fe6f4e96d745533eb107786861fd93365b72`, with the same pinned dependencies and macOS arm64 toolchain. `ctest --test-dir build/import-ui-rules-release --output-on-failure -j 2` passed 72/72 in 1,127.92 seconds. The corresponding `build/import-ui-rules-sanitized` invocation passed 72/72 in 2,327.82 seconds. This includes schema-6 Save/Apply, corruption rejection, write/sync failures, process exits, backup restoration, and the rules-sheet UI. Baseline build/test logs contained no compiler, Qt, ASan, or UBSan diagnostics. The historical Debug-only statements above describe their original checkpoints and are superseded by this verification.

## Search, selection, and atomic bulk ledger changes — 2026-10-26

The owner accepted literal AND terms, quoted phrases, explicit regex, a query-wide original-QIF mode, inclusive year-first/named-month periods, stable multiselection and search-return behavior. The durable UI now implements those queries on its worker, keeps a previous valid view after failed/cancelled queries, preserves chronological balances, and calculates exact whole-transaction filtered totals. Pointer/keyboard selection, context actions, and guarded bulk previews are connected. New rules-enabled schema-7 documents retain atomic bulk category/removal receipts and group Undo; only committed rows are eligible, Opening Balance is excluded, and every allocation is shown in the preview. Original evidence, non-category fields, exact amounts, and retained history survive. See [implementation contract](search-ledger.md).

The owner explicitly stated that no datasets exist in the wild and will announce the release that starts compatibility obligations. No migration/upgrade infrastructure was added. Existing experimental-format support was left where already present. This is still a one-account synthetic-data experiment, not a production application or full implementation of feature 005. Transfers, reports, general multi-account behavior, separate note fields, an ambiguous-date preference, native accessibility auditing, and other-platform validation remain open.

Current-format verification on macOS 27.0.1 arm64 / AppleClang 21 / Qt 6.12.0:

- Debug passed the 15 targeted application/storage/UI checks, both complete bulk fault sweeps, and the final four search/bulk/UI checks. The final eight affected UI/QML/benchmark-smoke checks passed 8/8 in 27.86 seconds.
- Release passed all 60 selected cases in 294.56 seconds. This includes rules and bulk write/sync/crash sweeps. Final four targeted checks passed again; after restoring the original delegate-reset behavior, eight affected UI checks passed 8/8 in 24.60 seconds.
- ASan/UBSan passed 59/60 in the initial selected run; the failing new bulk test iterated allocations from a temporary snapshot and triggered stack-use-after-scope in test code. The test was corrected to retain the snapshot. All four final targeted checks, including that case and provenance restoration, then passed in 45.72 seconds. Thus every selected case has a passing result, not a claim of a clean first run. The final eight affected UI checks passed 8/8 in 34.94 seconds. No ASan/UBSan diagnostics appeared in the corrected targeted runs.
- Bulk fault sweeps covered every observed write/sync position for category changes, removal, and group Undo, plus exits immediately before/after durable commit. The I/O/crash entries passed in Debug (149.50/153.41 seconds), Release (145.21/148.49), and ASan/UBSan (302.95/317.52). Recovery checks assert all-or-none member revisions, complete receipts, retry identity, reopened verification, and synthetic-marker confidentiality.
- The rules-off Debug build passed `import.search`, `import.schema_gate`, `import.ui`, and `import.qml`; bulk-specific UI tests deliberately skip in that configuration. Import search now requires the pinned PCRE2 adapter even without rule persistence.
- The bulk preview was rendered and inspected. UI tests cover mixed-sign allocation preservation, explicit preview/cancel/apply, reopening and group history/Undo, search-error/cancellation preservation, multiselection restoration, keyboard ranges, lost acknowledgements, unknown outcomes, and post-save refresh failure. A temporary test-helper name collision was fixed before the final builds.

The pre-existing Qt `DelegateModel::cancel: index out range` warning remains observable during some filter resets. Selection, viewport, inline-editor and split-view assertions pass. Disabling delegate reuse did not remove it; detaching the model removed the warning but broke viewport tests, so that workaround was reverted. This is a recorded Qt interaction limitation, not a clean-Qt-log claim. Final compiler logs were clean. The current-format run selected 60 of 76 registered cases; older storage/import/category/committed fault sweeps were not rerun for schema 7 after their baseline passes. See [exact build/test scope](building.md#search-and-bulk-follow-up--2026-10-26).

Final Release offscreen rendering passed both `bulkInteractions` and `combinedSearchAndSelection`; the captured bulk preview was inspected with readable allocation categories, exact balance impact, and Apply/Cancel controls. A native-window attempt failed because this execution session reported “Cannot create window: no screens available,” so native interaction remains unverified. Documentation checks found no missing targets among 133 relative links, and `git diff --check` passed.

Single-run Release in-memory application search measurements, after the long test sweeps completed:

| Synthetic rows | Literal + May–June | Regex + May–June | QIF text + May–June | Exact matches |
|---|---:|---:|---:|---:|
| 100,000 | 179 ms | 103 ms | 75 ms | 1,666 |
| 200,000 | 224 ms | 215 ms | 165 ms | 3,333 |

Every query asserted exact counts, withdrawals/deposits and unfiltered ending balance. These include application copy/sort/filter/totals; the QIF benchmark uses synthetic in-memory source slices. They exclude database/source-file loading and Qt rendering, and do not establish native scrolling responsiveness. The existing small UI benchmark also passed in the final UI runs. No personal financial inputs were used. Other operating systems, full accessibility, physical power loss and sanitizer instrumentation inside the installed PCRE2 library remain unverified.

## Documentation handoff refresh — 2026-10-26

Reconciled the README, current handoff/roadmap, storage/UI/category guides, feature index and affected requirements with the completed search/bulk work and owner-announced release gate. Checked all 30 root/project/feature Markdown documents: 286 local links and heading anchors resolved. Historical measurements remain labeled by checkpoint. This final pass changed documentation only; runtime checks were not repeated. `git diff --check` passed before committing.

## Transfer scope decisions — 2026-10-26

The owner accepted destination-first commit with protection of the linked origin draft, explicit confirmation of suggested matches, provisional generated dates copied from the origin, allocation-level transfer destinations and counterpart links, and explicit rejection of cross-currency transfers for the first package. See [accepted transfer scope](../features/008-transfer-matching-and-lifecycle.md#accepted-first-implementation-scope--2026-10-26). Neither commit order implicitly commits the other account. Differing amounts require review without silent correction.

At the decision checkpoint, implementation inspection found a singleton account store and QIF parser tests rejecting transfer destinations. At that decision checkpoint the next implementation prerequisite was account-scoped persistence and commands. That documentation-only checkpoint changed no runtime code; the subsequent implementation and verification are recorded below. Point 5 later implemented allocation-level Merge, unlink, and replacement. Point 6 later implemented the locked committed link edits. Automatic matching and cross-currency conversion remain deferred. Moving a committed row and rewriting source-occurrence links remain open.

## Account-scoped durable workflow — 2026-10-26

Implemented the multi-account prerequisite for transfers in the existing durable QIF store and worker/UI. Accounts have stable identities, immutable creation receipts and independent import workspaces, duplicate recognition, exact committed/projected balances and ordinary history. Explicit account switching rejects stale commands targeting another account. Whole-document verification reconstructs separate account balances; rules remain document-wide and Apply is account-scoped. The sidebar adds scrolling accounts, inline creation with the setup currency, guarded account switching and Back/Forward between account screens. See [contract and limitations](accounts.md).

Rules-enabled builds now create/open experimental schema 8; rules-off builds use schema 9. Earlier formats are rejected without migration, as permitted by the owner's pre-release policy. This does not implement transfer links, generated/protected drafts, matching, transfer-aware correction/reversal, account archival, currency reassignment or reports. Account selection starts at account 1 on reopen. Selection/search/viewport restoration across account switches remains incomplete.

Verification on the available macOS arm64 toolchain:

- Debug, Release and ASan/UBSan each ran 58 selected functional/storage/UI cases. Each initially passed 56/58. Both failures were new test-harness issues: the corruption setup needed to remove the existing batch-transition trigger before injecting invalid ownership; the rules UI test needed to clear its startup setting before constructing a second session. Both were fixed and passed in all three configurations. The initial results are not described as clean runs.
- Final account checks passed basics, authenticated corruption rejection, all observed write/sync positions for creation (17) and second-account commit (125), process exits at those positions, and exits immediately before/after durable commit. Debug's five-case affected run (the four account checks plus rules UI) passed in 67.64 seconds. Release and ASan/UBSan each passed all 12 account/UI affected checks in 70.67 and 125.23 seconds respectively. Account fault cases took 54.20/54.32 seconds in Release and 98.54/99.45 seconds under ASan/UBSan.
- New synthetic cases cover simultaneous pending accounts, same-file imports into separate accounts, CAD/JPY scale isolation, scoped rules and bulk operations, correction/Undo, stale cross-account requests, idempotent creation and conflict rejection, reopening, backup restoration, and marker scans. UI cases cover creation acknowledgement loss, unknown outcomes, known durable creation with failed refresh, guarded switching and independent commits.
- The rules-off Debug build passed account basics/validation, schema rejection, import UI and QML (5/5, 21.09 seconds) before the final sidebar navigation refinement.
- An offscreen account view was rendered and inspected. No private records were used. Native-window interaction, other operating systems, full accessibility, physical power loss and sanitizer instrumentation inside installed PCRE2 remain unverified. This is selected coverage, not a full 80-case suite result. Older storage/import/category/committed fault sweeps were not all repeated for these formats.

After the inline sidebar creation and account-aware Back/Forward refinement, all eight affected UI/QML/benchmark-smoke checks passed in Debug (23.59 seconds), Release (23.28 seconds) and ASan/UBSan (31.98 seconds). The rules-off Debug UI/navigation/QML checks also passed 3/3 in 20.39 seconds. The final creation-selection-failure refinement passed a further `import.ui` rerun in Debug (22.64 seconds), Release (21.64 seconds), ASan/UBSan (30.31 seconds), and rules-off Debug (20.39 seconds). The pre-existing intermittent Qt delegate-reset warning remains; no blanket clean-Qt-log claim is made. Final compiler and sanitizer diagnostics were checked separately. All 310 relative links and heading anchors across 31 root/project/feature documents resolved, and `git diff --check` passed.

## Account-workflow handoff refresh — 2026-10-26

Reconciled current schema and account-scope descriptions across the feature index, rules/import/UI guides and committed-change requirements. The roadmap handoff now identifies the implemented account prerequisite, exact verification limits, accepted transfer decisions, next implementation steps and remaining financial decision gates. Historical checkpoints remain labeled separately. This final refresh changed documentation only; runtime tests were not repeated. All 311 relative links and heading anchors across 31 root/project/feature Markdown documents resolved, and `git diff --check` passed.

## Transfer engine prerequisite — 2026-10-26

Added pure, dependency-free transfer lifecycle proposals and relationship-set validation. Four new groups cover generation and both commit orders, exact match compatibility, invalid/overflow inputs, and multi-allocation protection. The whole-origin protection case verifies that committing one destination prevents clearing sibling transfer allocations from the protected origin. This is an engine prerequisite only: no database format, parser admission, durable financial command, evidence attachment or UI behavior changed. See [contract and accepted follow-up decisions](transfer-engine.md).

`cmake --build --preset debug -j 3` followed by `ctest --preset debug` passed 17/17 (3.25 s). Equivalent Release and ASan/UBSan preset build/test invocations passed 17/17 (0.47 s and 9.20 s). These are the nine existing engine groups, four QIF groups and four transfer groups, on the available macOS toolchain. A signedness warning in the initial build was corrected; final builds and test output contained no compiler warnings or sanitizer diagnostics. The first three transfer groups also passed in all configurations before the complete-set tests were added.

The application/storage/UI suites were not run for this isolated engine addition. Transfer durability, interruption/retry, backup restoration, cross-account evidence attachment and UI navigation remain unimplemented and unverified. Linux and Windows were not tested. No private inputs were used. The owner subsequently accepted provisional evidence matching until the importing account commits, and dropping those proposals on import discard while preserving evidence and existing transfer state. Manual/unmatched entries must support explicit acceptance and separate commit, even without a future bank match. Mismatched amounts may be independently committed as unmatched entries, but cannot be linked until the difference is explicitly accounted for. These policy updates changed documentation only; acceptance history, manual-origin generation and durable attachment remain unimplemented. Existing implementation authorization remains in effect.

## Transfer acceptance and match proposals — 2026-10-26

Extended the pure engine prerequisite with manual-origin generation, explicit unmatched acceptance, separate proposal/published evidence, proposal discard, and per-account attachment publication. The generated creation identity/date and manual origin provenance survive later matching. Unmatched generated/manual sides require acceptance before ordinary commit. Acceptance cannot bypass currency/amount consistency. Whole-entry acceptance preserves sibling consistency; whole-import helpers reject reused evidence allocations and leave unrelated accounts/imports unchanged. No database schema, import-store command, financial ledger query or UI changed.

The engine/parser suites passed 20/20 in Debug (3.33 s), Release (0.64 s) and ASan/UBSan (9.45 s), using the corresponding preset build and test commands. The first targeted run exposed an outdated test expectation: a newly published destination match protects its origin, so clearing must preserve that origin rather than reject it. The test was corrected, with a separate independently evidenced draft case retaining its explicit policy rejection. Final provenance-validation checks were added afterward and the seven affected transfer groups were rebuilt and rerun in all three configurations. No compiler warnings or sanitizer diagnostics appeared in these runs.

This is an additional tested engine step, not a completed durable transfer workflow. Persisted acceptance/history, provisional attachment storage, explicit per-account publication, recovery/fault sweeps, restored backups and UI integration remain outstanding. The accepted user decisions are recorded in feature 008; no further approval was requested for this engine implementation. No private inputs were used.

## Durable transfer integration — 2026-10-26

Implemented the bounded [same-currency transfer workflow](transfer-engine.md) in experimental schema 10 (rules-enabled) / 11 (rules-off): encrypted immutable command/version history, exact retry identities, manual and imported-allocation creation, unmatched acceptance, explicit per-account commit, symmetric draft protection, retained protected import rows, and provisional bank evidence attachment/discard. Bank publication replaces local movement accounting atomically and preserves immutable source evidence. Open replays relationships and financial effects; ordinary linked-entry corrections/removal/Undo fail explicitly.

The worker-backed Transfers dialog adds creation, allocation selection, acceptance, candidate review, explicit proposal confirmation, per-account commits and counterpart navigation. Account view state is retained in-session. A synthetic offscreen render was inspected at 1440×850; creation fields, state labels and counterpart controls were readable without clipping. This is not native-window or cross-platform interaction verification. Dedicated browsable transfer history, bracketed QIF destination admission, independent committed-entry linking, repair/unlink/reversal, cross-currency and automatic matching remain outside this bounded implementation. Feature 008 remains active.

Initial Debug transfer storage groups passed 5/5 (195.62 s), including failures at every observed write/sync position and abrupt termination across ten operation types, both durable boundary observers, identical retries, authenticated corruption rejection, plaintext marker scans and restored encrypted backups. After additional relationship/receipt validation, Debug basic/import/corruption plus the complete `import.ui` test passed 4/4 (25.97 s); two added mixed-sign/multiple-destination/manual-origin evidence groups passed 2/2 (1.28 s). UI tests cover lost acknowledgement, unknown outcome, acceptance, separate commits, navigation/filter restoration and discard/reimport/attachment with zero duplicate movement. Broader configuration results are recorded below.

Final selected functional checks passed 70/70 in Debug (84.93 s). The rules-off Debug selection passed 9/9 (282.97 s), including all seven transfer storage groups plus `import.ui` and `import.qml`. After the final selected-draft clearing fix and compiler-warning cleanup, the five affected split/evidence/UI/navigation/QML cases passed in Debug (32.62 s), Release (31.23 s), ASan/UBSan (51.79 s) and rules-off Debug (30.20 s). A further draft-bank publication assertion in `transferstore.manual_evidence` passed in all four builds. Final build logs contained no compiler warnings. The existing occasional Qt delegate-reset warning is not claimed resolved.

The initial broader ASan/UBSan run reached the 1,200-second limit in both existing import backup fault sweeps, without an assertion or sanitizer diagnostic before timeout. Both corresponding Release sweeps passed (724.36 and 743.17 s). The current-format backup test budget was increased to 2,400 seconds, preserving every fault position and durability check; the affected sanitizer tests were rerun separately. Do not describe the initial broad sanitizer run as a clean pass.

The broader Release invocation, `ctest --test-dir build/import-ui-rules-release --output-on-failure -j 3`, passed 92/92 in 1,458.35 s. Its test list was captured before the two additional split/manual-evidence groups were registered; those passed separately as recorded above. This includes the new transfer I/O/crash sweeps (233.42/236.10 s) and all pre-existing import, account, committed-change, category, rules/bulk, prototype-storage and UI checks in that captured list.

The broader ASan/UBSan invocation passed 90/92 in 2,615.23 s; its only non-passing results were the two 1,200-second backup timeouts described above. The unchanged backup sweeps then passed 2/2 with the larger timeout (1,324.28 and 1,348.06 s; 1,348.07 s elapsed), using `ctest --test-dir build/import-ui-rules-sanitized -R '^import.backup_(failures|crashes)$' --output-on-failure -j 2`. The new transfer I/O/crash sweeps passed in 466.87/483.07 s. With the two separately registered split/manual-evidence groups, all 94 current cases have passing Release and ASan/UBSan results across these invocations. No ASan/UBSan diagnostic was reported. The timeouts remain part of the record; this was not a clean first sanitizer run.

All 326 relative links and heading anchors across 32 root/project/feature documents resolved, and `git diff --check` passed. No private financial inputs were used. Linux/Windows, native-window interaction, physical power loss, full accessibility and representative transfer responsiveness remain unverified. The optional experiment still requires new-format datasets; no compatibility or migration work was added.


## Transfer handoff and commit preparation — 2026-10-26

Refreshed the roadmap handoff to the completed schema-10/11 transfer package, its current 94-case verification record, the two sanitizer backup timeout/rerun results, and the accepted provisional-match/unmatched-commit decisions. The next package is QIF transfer-destination admission with explicit account resolution; a browsable transfer journal and unified allocation/account picker follow. Committed repair/unlink/reversal and other unresolved financial policies remain decision gates. New implementation waits for the owner's explicit Go. This handoff refresh changed documentation only; runtime tests were not repeated.

## QIF transfer destinations — 2026-10-26

Implemented [bounded QIF transfer admission and explicit resolution](qif-transfers.md): whole-record `L[Account]` and split `S[Account]`, stable allocation selection, account labels that distinguish duplicate names, and an explicit destination placeholder. Imported destinations never auto-resolve by name. Included transfer allocations must have live transfer/evidence links before commit; rules and ordinary category edits cannot erase transfer intent. The existing commands preserve per-account commits, protection, provisional matching, source evidence and exact retry receipts. Parser `bank-v2` uses experimental schema 12 with rules and 13 without; older formats are rejected without migration. Reports were explicitly excluded and were not implemented.

Verification uses synthetic fixtures only. New cases cover both import orders, repeated equal allocations, mixed signs, missing/export-alias and duplicate names, invalid/self/cross-currency destinations, unresolved and partially resolved commits, rule replacement protection, duplicate reimports, restart and restored backups. The existing transfer write/sync and interruption sweeps now exercise bracketed origin and bank evidence. The protected-import regression also checks that a discarded unresolved transfer row is not required when the retained subset commits or reopens.

The first functional runs found a historical-version validation path that omitted allocation metadata and a Qt ComboBox that auto-selected the first destination. Historical validation now loads its persisted metadata before checking source identities; the selector uses a non-account placeholder and disables creation until a real account is selected. Debug's broad selected invocation passed 70/71 in 57.64 s; its only failure was the selector assertion. After that correction, all three affected QIF/UI/QML cases passed, then passed again after the diagnostic refinement. Release's selected invocation passed 19/20 in 195.49 s, with the same initial UI assertion; the corrected four-case QIF/UI/QML/rules-preview selection passed 4/4 in 26.51 s. The Release transfer I/O and crash sweeps passed in 195.49 s and 193.60 s. These are passing results across initial runs and targeted corrections, not clean first runs or a full 95-case suite.

The rules-off Debug selection passed all nine selected transfer/schema/UI/QML checks in 27.55 s. A synthetic offscreen 1440×850 rendering of the updated Transfers dialog was inspected: destination prompt, explanatory text and controls were readable without clipping. Native-window interaction, Linux/Windows, full accessibility, physical power loss and representative transfer performance remain unverified. The existing intermittent Qt delegate-reset warning remains; no claim is made that this work fixes it.

ASan/UBSan passed all 20 selected parser/rules/transfer/import/UI checks in 461.27 s, including transfer I/O (445.00 s) and crash (461.27 s) sweeps, with no sanitizer diagnostics. After the retained-row check and additional duplicate/repair cases, the eight affected parser/QIF-import/UI checks passed 8/8 in Debug (27.74 s) and Release (26.66 s); the rules-off nine-case selection passed 9/9 (24.37 s). The sanitizer follow-up passed its eight non-UI cases; its UI failure came from a test using QObject lookup for a visual Repeater delegate. That test now traverses the visual tree and verifies that clicking the second transfer allocation selects its stable identity. The direct Debug regression passed. Final UI/QML reruns are recorded below.

Commands used the existing `build/import-ui-rules-debug`, `build/import-ui-rules-release`, `build/import-ui-rules-sanitized` and rules-off `build/import-ui-debug` trees, with `cmake --build <tree> -j 3` and `ctest --test-dir <tree> --output-on-failure -j 3`. The broad Debug run excluded `(\.io|\.crashes|_io|_crashes|backup_failures|backup_crashes|storage\.)`; the Release/sanitizer selection was `^(qif\.|rules\.|transferstore\.|import.(errors|repair|ui|qml)|rulestore.(basic|schema))`. Targeted final selections covered changed parser/transfer/import tests and the UI/QML pair. No full new-format legacy backup fault sweep or coverage-guided fuzzing is claimed; restored encrypted backups and deterministic parser mutation checks ran within the selected suites.

Final UI/QML checks used `cmake --build <tree> -j 2` and `ctest --test-dir <tree> -R '^import.(ui|qml)$' --output-on-failure -j 2`. They passed 2/2 in Debug (26.62 s), Release (25.58 s), ASan/UBSan (35.96 s), and rules-off Debug (24.08 s), after rebuilding all four trees. These reruns include second-allocation navigation and the corrected visual-delegate lookup. Build logs contained no compiler warnings; sanitizer runs reported no ASan/UBSan diagnostics. All 341 relative links and heading anchors across 33 root/project/feature documents resolved, and `git diff --check` passed. No private data was used. The handoff stops at this QIF package and explicitly excludes starting reports.


## QIF handoff and check-in preparation — 2026-10-26

Reviewed current scope, schema versions, parser profile, feature index and handoff against the completed package. Added the QIF resolution guide to the README and concrete synthetic resumption/verification steps to the handoff. Recorded the exact final UI/QML commands. This final documentation pass did not change implementation or rerun runtime tests; the measured results above remain the verification evidence. Reports remain outside the authorized scope.

The final check-in pass validated all 343 relative links and heading anchors across 33 root/project/feature documents. `git diff --check` passed.


## QIF point 1 independent backup progress — 2026-10-26

The owner authorized the [eight-point sequential roadmap](roadmap.md#current-authorization-and-delivery-checkpoints--2026-10-26), with one implementation agent per point and a documentation/handoff/commit checkpoint followed by explicit continue-or-stop approval. Point 1 is in progress, not completed. At this historical backup-only checkpoint, broader QIF policies were still pending; the accepted follow-up below supersedes that decision status.

Independent backup work adds cancellable bounded page copying, verification/reopen checkpoints, phase feedback and a publication cancellation gate. Existing source/candidate/backup preservation, encryption and exclusive durable publication remain intact. Tests cover cancellation before copying, after a completed 128-page chunk, during verification/reopen, before publication, rejection after publication starts, source/prior-backup preservation, fresh retry/restore, plaintext-marker scans and the model's cancellation/error/healthy-session flow. See [storage behavior](durable-import.md#backup-cancellation-and-publication--2026-10-26) and [UI behavior](import-ui.md#backup-cancellation-and-progress--2026-10-26).

This backup-only checkpoint used parser `bank-v2` / schema 12, before the later parser/nullable-amount changes. Debug initially passed five affected cases (backup, cancellation, publication, UI and QML) in 31.78 seconds. After adding active-copy and model cancellation coverage, the two affected cases passed in 29.47 seconds. An initial test-only compilation error used a nonexistent source accessor; it was corrected to the existing evidence accessor before these passing runs. Release passed all seven affected cases in 575.01 seconds, including 560 backup failure positions (575.01 seconds), 560 crash positions (572.61 seconds), and 20 publication boundary cases; the final cancellation/UI refinements passed 2/2 in 27.00 seconds. ASan/UBSan passed 7/7 in 1230.77 seconds, including the same 560 failure (1204.75 seconds) and crash (1230.77 seconds) positions. The existing intermittent Qt delegate-reset warning appeared in both UI runs; no sanitizer diagnostic was reported. These results do not verify the later schema 14/15 changes. No full-suite or portability claim is made.


## Ordinary QIF and unknown-amount repair checkpoint — 2026-10-26

Point 1 remains in progress. Parser `bank-v3` now admits initial Bank/Cash/CCard headers and separates class metadata from category/transfer meaning. Experimental schema 14 (rules-enabled) / 15 (rules-off) stores unavailable original amounts as SQL NULL, while effective interpretations retain exact currency/scale validation. New-format datasets are required; no compatibility or migration work was added. Original source bytes, unavailable values, complete split positions and transfer destinations survive repair, commit, reopening and backup. Rules and history validation use the same retained source facts. A repaired original amount is never fabricated as zero.

When either original amount is unavailable, duplicate candidates use compatible source currency and compatible/unknown original dates and are labeled uncertain; they never auto-exclude the repaired record. Candidate enumeration is capped at 4096 with an explicit error on overflow, never silent truncation. Every first repair requires an explicit disposition. Structural/encoding/unknown-field errors remain blocked. Multi-account source mapping/creation, upfront whole-source repair counts, progress and full-import Abort are still unfinished; accepted opening-balance and abort-account decisions are recorded in the milestone.

Verification of this intermediate checkpoint is complete. Engine Debug passed 20/20 in 4.61 seconds. A broader selected Debug run initially passed 21/23; two failures were schema-12 expectations corrected to schema 14, then passed. The new amount-repair regression initially exposed two historical verification paths dereferencing unavailable original money; both were fixed. A subsequent seven-case selection covering old/new repairs, rules preview/schema/definitions, UI and QML passed 7/7 in 27.01 seconds. The current-format Debug I/O/crash/amount/UI selection passed 4/4 in 375.30 seconds. Each fault sweep covered 881 observed positions across eight operations (unknown-original save: 105; unknown-original commit: 125), plus eight explicit before/after durable-boundary exits. Final nullable-original correction/Undo and candidate-overflow tests passed 2/2 in 31.36 seconds. Rules-off schema-15 repair/UI/QML checks passed 4/4 in 30.26 seconds. Release passed all 31 selected cases in 692.92 seconds, including the 881-position import I/O/crash sweeps (298.95/331.90 seconds) and 560-position backup failure/crash sweeps (683.52/674.43 seconds). Final correction/Undo and candidate-cap refinements passed 2/2 in Release (17.15 seconds); rules-off schema gate plus those two cases passed 3/3 (18.45 seconds).

The initial sanitizer selection passed 30/31 in 1346.80 seconds: import I/O passed all 881 positions in 506.92 seconds, and backup failure/crash sweeps passed all 560 positions in 1323.80/1346.79 seconds. The import-crash sweep hit its existing 600-second timeout during concurrent backup sweeps, without an assertion or sanitizer diagnostic before timeout. Its expanded eight-operation budget is now 1200 seconds, retaining every fault position; the isolated single-job rerun passed all three selected cases in 597.62 seconds, including all 881 crash positions and durable boundaries (508.73 seconds), amount repairs (2.83 seconds) and the candidate cap (86.05 seconds), without sanitizer diagnostics. The known Qt delegate-reset warning appeared in Release and sanitizer UI logs. This is selected verification, not a full suite or native accessibility/portability claim. Tests are synthetic only.

Commands: builds used `cmake --build <tree> -j 2` (Debug also used `-j 3`) in `build/import-ui-rules-debug`, `build/import-ui-rules-release`, `build/import-ui-rules-sanitized` and rules-off `build/import-ui-debug`. The Release/sanitizer 31-case selection used `ctest --test-dir <tree> --output-on-failure -j 3 -R '^(qif\.|rules\.|import\.(basics|review|repairs|amount_repairs|errors|validation|opening|cancellation|backup|backup_cancellation|backup_publication|io|crashes|ui|qml)|transferstore\.(basics|qif)|rulestore\.(definitions|application|schema)|committed\.(basics|history))'`. The `backup` prefix includes both full backup sweeps. The final sanitizer rerun uses `-R '^import.(crashes|amount_repairs|amount_candidate_limit)$' -j 1`.


Final checkpoint refinements: source-category ownership now remains source-owned when a user fills only an unavailable amount; an added UI assertion verifies this. The initial assertion used the wrong capitalization for the existing ownership label, then was corrected. A schema audit also found the bulk-support capability gate still checking schema 12, causing the new-format UI test to skip its bulk scenario. The gate now checks schema 14. Debug's explicit bulk/model rerun passed 2/2 in 27.92 seconds. Release and ASan/UBSan subsequently passed all six selected bulk, bulk-I/O, bulk-crash, amount-repair, model and QML cases in 150.01 and 332.56 seconds respectively. Each bulk sweep exercised 327 positions across three operations; sanitizer I/O/crash times were 294.81/310.72 seconds. Earlier 31-case results must not be interpreted as verification of the then-skipped bulk scenario. The final parser ambiguity guard rejects a parent category with structurally present splits even when every split amount is invalid; its four parser cases passed in Debug (3.95 seconds), Release (0.45 seconds) and ASan/UBSan (12.14 seconds). No sanitizer diagnostics were reported.

## Whole-source QIF account review — 2026-10-26

Experimental schema 16 (rules-enabled) / 17 (rules-off) adds shared immutable whole-file sources, account-section associations, captured/reviewing/started/aborted import groups, staged reported-balance opening decisions and retained retired-account identities. Read the [grouped import contract](qif-account-imports.md). Point 1 is implemented and verified within the accepted profile; final results and limitations follow. Schema 14/15 results above remain historical evidence.

Focused Debug storage and UI/QML runs passed the initial group flow, then generated destination balances and both commit orders, explicit nonzero opening/closing interpretation, invalid precision/overflow, cancellation-safe mixed signs, existing-account opening preservation and post-start discard. Actual QML mapping selection exposed an implicit signal-index collision; explicit signal parameters fixed it. Offscreen rendering then exposed a missing editor reload after account mapping; the action now refreshes the editor and its date assertion passed. Unknown-amount split display now retains source category/ordinal/ownership and raw-rule actions without fabricating money. Earlier focused UI outcome tests passed capture/resume/balance/Abort before/after/unknown acknowledgement scenarios and saved-operation selection/refresh failures.

An initial Debug group failure/crash pair passed 2/2 in 212.93 seconds, with 607 observed write/sync positions per sweep across seven operations (97/113/17/61/133/81/105), plus 14 durable-boundary exits per run. This preceded the final first-publication foreign-key association and stronger pre-retry boundary assertions; the final Release/sanitizer runs below supersede that preliminary result. No native-platform or portability claim is made.

Final focused schema-16 checks passed: account groups, transfer imports and rule schema (3/3, 7.00 seconds); restored model/QML mapping and outcome cases (4 QtTest checks including setup/cleanup, 8.237 seconds); mapping cancellation/delegate persistence (3 checks, 0.967 seconds); and the final source-provenance display, unknown-amount repair and mapping cases (5 checks, 3.264 seconds). The broader Debug functional selection passed 74/74 in 110.58 seconds. Rules-off schema-17 grouped/import/repair/model/QML selection passed 6/6 in 36.19 seconds. Offscreen mapping and review renders were inspected: mapping choices, populated date/splits, source category/rule actions, repair counts, Abort and staged-balance controls were readable. This does not establish native accessibility or platform behavior.

Initial integration failures were corrected before these passes: numbered SQL parameters in the balance/publication updates, a mapping delegate's implicit signal index, and missing editor reload after mapping. A test-file editing error was restored from the committed baseline and reconstructed additions before rebuilding; the final tests include all baseline cases and the new regressions. The Debug functional run still emitted the known Qt delegate-reset warnings in existing search-return and split-view cases; they remain point-2 work. No compiler warnings appeared in the final Debug/Release/sanitizer or rules-off build logs. Final Release and ASan/UBSan current-format fault results are recorded below; earlier counts are not substituted for them.

The final Release fault selection passed 11/11 in 1330.64 seconds. Backup failure/crash sweeps each covered 615 positions (638.22/633.85 seconds), import I/O/crash each covered 881 across eight operations (275.09/278.53 seconds), transfer I/O/crash each covered 878 across ten operations (184.87/186.22 seconds), and group I/O/crash each covered 607 across seven operations (203.02/201.54 seconds), with 14 explicit group durable-boundary exits and pre-retry assertions. Backup publication covered 20 boundary cases (30.91 seconds); cancellation passed in 8.54 seconds and full grouped scenarios in 4.87 seconds. This verifies the final publication association and strengthened assertions, replacing the preliminary group-sweep limitation above.

Eight authenticated synthetic tamper cases now supplement account validation: mapping/account mismatch, altered reviewed monetary basis, altered historical opening, current-proposal/history mismatch, missing group transition, missing publication receipt, premature retirement and corruption of an account hidden by retirement. Reopen rejected each case in direct Debug, Release, ASan/UBSan and rules-off validation runs. Rules-off schema-gate and transfer imports/QIF passed 3/3 in 1.84 seconds in addition to its six workflow cases. Release functional verification passed 74/74 in 59.72 seconds, including expanded corruption validation. Final sanitizer functional verification passed 74/74 in 161.86 seconds.

The final focused account validation also backs up and restores a grouped source, verifies `GROUP-FAULT-MARKER` in decrypted evidence, and scans the encrypted source/restored files for that exact marker and the password. It passed in Debug, Release, ASan/UBSan and rules-off builds. The earlier Release group sweep checked passwords and the old account fixture marker, not this newly added group marker; its passing result is not evidence for that later assertion. No implementation changed for these test additions. The final sanitizer group sweeps used the updated assertion.

The final ASan/UBSan fault selection passed 11/11 in 2914.62 seconds. Position counts matched Release: 615 backup, 881 import, 878 transfer and 607 group positions per respective sweep, plus the explicit durable/publication boundaries. Failure/crash times were 1444.55/1475.90 seconds for backups, 517.84/554.60 seconds for import, 438.52/462.74 seconds for transfers and 399.45/413.30 seconds for groups. Backup publication/cancellation and grouped scenarios passed in 68.06/26.59/8.11 seconds. No sanitizer diagnostics or timeouts occurred in this final selection; earlier timeout/failure records above remain historical.

Final commands used `cmake --build <tree> -j 2` in the existing rules-enabled Debug, Release and sanitizer trees and rules-off Debug tree. Functional selection: `ctest --test-dir <tree> --output-on-failure -j 3 -E '(^storage\.|\.(io|crashes|group_io|group_crashes|backup_failures|backup_crashes|bulk_io|bulk_crashes)$|import.benchmark_smoke)'`. Release/sanitizer fault selection: `ctest --test-dir <tree> --output-on-failure -j 2 -R '^(accounts\.(groups|group_io|group_crashes)|import\.(io|crashes|backup_failures|backup_crashes|backup_publication|backup_cancellation)|transferstore\.(io|crashes))$'`. Final test-only corruption/marker additions rebuilt `spentit_account_tests` and ran its `validation` mode directly in all four trees. The rules-off CTest selections covered `accounts.groups`, import basics/repairs/amount repairs/UI/QML/schema gate and transfer imports/QIF. This is selected current-format verification, not a full 101-case suite or a repeat of unrelated prototype/category/committed/rules fault sweeps.

Point 1 closeout: final functional selections passed 74/74 in Debug (110.58 seconds), Release (59.72 seconds), and ASan/UBSan (161.86 seconds), with the later test-only corruption/marker refinement separately passing in all four builds. Final Release and sanitizer fault selections each passed 11/11 as detailed above. Rules-off passed its nine selected cases across two invocations. Final builds had no compiler warnings and sanitizer runs reported no ASan/UBSan diagnostics. The known Qt delegate-reset warning appeared in the Debug, Release and sanitizer functional UI logs; it is not fixed by this work.

The accepted ordinary-money QIF workflow, multi-account review/Abort, staged balance interpretation and controlled backup work are complete for point 1. Features remain active because their wider product scope extends beyond this checkpoint. Coverage-guided fuzzing, representative new-format performance, accessibility/native keyboard audits, Linux/Windows and physical power-loss behavior remain unverified. Transfer repair/reversal, other import formats, general account management, broader rules and ledger refinements remain the later roadmap points. No private inputs, license choice or publication were involved. The [completion handoff](roadmap.md#point-1-completion-handoff--2026-10-26) requires a commit and explicit continue-or-stop answer before point 2.

Final coordinator check: all 384 local links and heading anchors across 34 repository documents resolved; `git diff --check` passed.


## Point-2 current-format hardening — 2026-10-26

The accepted hardening checkpoint adds coverage-guided QIF fuzzing, combined failure/recovery workflows, accessible ledger semantics and keyboard/focus checks, a bounded fix for the reproduced Qt delegate-reset warning, and measured current-format large-workload read improvements. Financial policies and bank-v3/schema 16/17 remain unchanged. Linux/Windows builds and native interactions were explicitly excluded by the owner. Actual spoken VoiceOver and full manual assistive traversal remain unverified; this is a delivered checkpoint with recorded verification limits, not exhaustive accessibility or product readiness.

The pinned optional libFuzzer runtime enables two actual ASan/UBSan target campaigns (750,204 executions total) covering ordinary and indexed account parsing, both date orders and three currency scales. No parser crash or target sanitizer diagnostic occurred. Synthetic combined workflows cover 30 before/after durable boundaries and 45 injected write/sync cases with rules, or 26/39 without rules, followed by retries, exact money/evidence/history checks and restored backups. The harness corrections and runtime instrumentation boundaries are documented rather than misreported as product defects.

The UI exposes transaction list/row semantics, selection, currency/status, named controls and operation/error announcements. Twenty repeated warning-fatal reset/split-view pairs passed with asynchronous offscreen caching disabled; visible delegates remain virtualized/reusable. Existing selection, viewport, unsaved editor and split-view assertions remain enabled. A final scripted Cocoa keyboard/accessibility case passed 3 Qt checks in 960 ms; closed modal controls report invisible. Native AX inspection observed named rows/fields but did not verify speech.

Scoped read-plan reuse clears bindings/results on every use, remains bounded, finalizes before operations return and changes no storage durability setting. Inspection now retrieves exact retained occurrence bytes; search avoids unnecessary empty-filter/string copies; category replay reuses an already validated head without skipping historical validation. Broad Release and sanitizer selections passed 75/75 after these read changes except the final category-head reuse, which was followed by focused Debug/Release/sanitizer 6/6 and rules-off 5/5 checks. Earlier Debug broad 75/75 and final combined-I/O sanitizer checks are separately recorded. This is not a claimed full 103-case suite. See [exact commands, all evidence and limitations](hardening.md) and [workload measurements](import-ui-performance.md).

The final 200k native interaction benchmark completed exact correctness assertions but had no frame samples. The final 100k attempt produced valid cache-zero frames, then failed on unavailable default-cache frames before save/final completion. A prior full 100k diagnostic did complete all correctness checks. Non-monotonic timings are individual observations, not scalability or causal speedup claims; see the workload report. The coordinator must commit the checkpoint and ask whether to proceed to point 3 or stop.

Final coordinator check: all 404 local links and heading anchors across 40 repository documents resolved; `git diff --check` passed.

## Point-3 statement imports — 2026-10-27

OFX 1.x SGML and OFX 2.x XML bank and credit-card statement lists are implemented. QFX, QBO, and ASO are labels of that grammar. ASO was identified by in-place structural inspection of the owner's private export: the export is OFX 1.0.2 SGML credit-card statement syntax. No path, filename, account identifier, amount, name, FITID, or other content was recorded. Format `aso` is legal in `source_headers` and `source_occurrences` while grammar stays `ofx-sgml` or `ofx-xml`. This checkpoint used schema 18 with rules and 19 without. Point 4 replaced that pair with schemas 20 and 21. The contract is [features/003-ofx-profile.md](../features/003-ofx-profile.md). Opening a schema 16 or 17 document fails with `Unsupported experimental format; no migration was attempted` before journal-mode or shape checks. Application id remains 1397770577. QIF bank-v3 behavior for new documents uses the same `source_headers`, `source_occurrences`, and `source_associations` tables, so a later OFX-family confirmation can attach to a QIF-originated ledger row without posting the money again.

Original files are left unchanged. Encrypted `sources.bytes` retain those bytes. Occurrence slices stay original bytes. Rules and manual edits do not alter source facts. A currency that does not match the target account refuses the prepare and saves nothing. There is no conversion. Investment and other unsupported message sets, malformed structure, non-UTF-8 bodies, and pending transaction lists refuse before capture. Statement transfers, corrections, foreign `ORIGCURRENCY`, and unknown financial children block the affected record and keep the evidence. No destination account is created by name. Account rename, archive, deletion, post-commit opening correction, and currency reassignment were not implemented.

Synthetic fixtures only: `tests/fixtures/statements/bank-sgml.ofx`, `bank-xml.ofx`, `empty-and-refused.ofx`, and `card-sgml.aso`. The empty fixture is a successful empty statement list. The ASO fixture is a synthetic credit-card SGML file with `SECURITY:TYPE1`, charset 1252, no `USERKEY`, one debit, one credit, timezone-bracket dates, opaque FITIDs, and ledger and available balances. Private export paths and contents were not recorded.

Verification is selected macOS coverage on Apple clang, arm64. It is not a full-suite claim and not a Linux or Windows claim.

After the conflicting-`CURDEF` fix, `ctest --test-dir <tree> --output-on-failure -R '^statement\.parser$'` passed 1/1 in each of:

- `build/import-ui-rules-sanitized` (0.02 seconds)
- `build/import-ui-rules-debug` (0.00 seconds)
- `build/import-ui-rules-release` (0.00 seconds)
- rules-off `build/import-ui-debug` (0.00 seconds)

The previous crash input was replayed with `./build/qif-fuzz/tests/engine/spentit_statement_fuzzer <artifact> -runs=0` and exited 0. That replay is not a campaign.

The clean campaign, after both parser fixes, was:

`./build/qif-fuzz/tests/engine/spentit_statement_fuzzer build/qif-fuzz/statement-corpus -seed=20261027 -max_total_time=180 -max_len=65536 -len_control=0 -timeout=10 -rss_limit_mb=2048 -artifact_prefix=build/qif-fuzz/statement-artifacts/ -print_final_stats=1`

Exit 0. `Done 197324 runs in 181 second(s)`. `number_of_executed_units` 197324, average 1090 executions/second, 1628 new units, peak RSS 602 MiB, slowest unit 0 seconds. The artifact directory was empty. The corpus directory already held inputs from the aborted campaign below plus the three synthetic fixtures (592 files at start). Two earlier campaigns are failures, not successes: the first exited 134 after 2 executions on an ASan stack-use-after-scope in the account-label temporary; the second exited 134 after 80816 executions on a null `CURDEF` assignment. Both were fixed before the clean run. The point-2 QIF campaigns (750,204 executions) are unchanged historical evidence and are not this parser.

These selections passed after the account-label and QIF-encoding UI fixes and before the one-line `CURDEF` return. They do not include that return. `statement.parser` above is the post-fix parser proof. Storage and UI binaries were not rebuilt again for that parser-only line.

- Rules-on Release, `ctest --test-dir build/import-ui-rules-release --output-on-failure -j 2 -R '^(statement\.(parser|review|faults)|import\.(ui|basics|review|repairs|errors|validation)|qif\.(basics|malformed|limits|mutations)|rulestore\.schema|accounts\.groups)$'`: 15/15 in 63.93 seconds.
- Rules-on ASan/UBSan, the same regex in `build/import-ui-rules-sanitized`: 15/15 in 142.44 seconds. `statement.review` in that run covers the owned account label under ASan.
- Rules-off Debug, `ctest --test-dir build/import-ui-debug --output-on-failure -j 2 -R '^(statement\.(parser|review|faults)|import\.(schema_gate|basics|review)|qif\.(basics|malformed)|accounts\.groups)$'`: 9/9 in 56.37 seconds. `import.schema_gate` rejects versions 6, 16, 17, and 18 when the binary expects 19.
- Rules-on Debug after the UI refusal narrowing: `statement.parser`, `statement.review`, and `import.ui` passed 3/3 in 39.88 seconds.

The `statement.review` test source covers a QIF include, an overlapping OFX SGML duplicate association with the balance unchanged, an idempotent commit retry, exact evidence after backup restore, an uncertain `FITID` that posts only when explicitly included, a different `FITID` kept as a distinct posting, an uncertain cross-format date excluded only by explicit disposition, legitimate same-day repeats, a repeated in-file `FITID` that must be included because it cannot target its uncommitted sibling, missing-amount repair, multi-account map and create, an `aso` credit-card import that posts once and associates a later same-FITID import without a second posting, and visible refusal of malformed, investment, currency-mismatch, non-OFX `aso`, and mislabeled input with the external file unchanged. `statement.faults` sweeps the new include commit's observed write positions, one abrupt VFS exit, observer exits on both sides of the SQLite commit, and cancellation during group prepare with a later successful retry. An earlier rules-on Debug faults run, before the account-label fix, printed `statement commit fault positions: 141` and passed. The later Release (51.29 seconds), ASan/UBSan (101.41 seconds), and rules-off Debug (51.86 seconds) faults runs passed inside the selections above. Their printed position counts were not copied here and are not restated as 141.

An earlier rules-on Debug selection of 75 tests excluding `io`, `crashes`, `backup`, and `benchmark` finished 74 passed and `import.ui` failed, in 142.24 seconds. The failure was a QIF encoding diagnostic thrown before capture. That throw was narrowed so plain QIF evidence is ingested again while OFX file-level refusals still throw. The other 72 passing tests from that selection were not rerun after the UI, account-label, or `CURDEF` fixes.

The selections above predate ASO admission. They do not contain the `aso` format check. The 197,324-execution campaign also predates it. No new multi-hour campaign was started.

ASO admission was identified by in-place structural inspection: the owner's private export is OFX 1.0.2 SGML credit-card statement syntax. No path, filename, or contents were recorded. The agent did not open or search for that file. After the parser, format-check, and synthetic-fixture edits, each tree was rebuilt for `spentit_statement_tests` and `spentit_statement_store_tests`, then:

`ctest --test-dir <tree> --output-on-failure -R '^statement\.(parser|review)$'`

- Rules-on Debug `build/import-ui-rules-debug`: 2/2 in 2.91 seconds (`statement.parser` 0.01, `statement.review` 2.90).
- Rules-on Release `build/import-ui-rules-release`: 2/2 in 2.35 seconds (`statement.parser` 0.00, `statement.review` 2.34).
- Rules-on ASan/UBSan `build/import-ui-rules-sanitized`: 2/2 in 4.64 seconds (`statement.parser` 0.02, `statement.review` 4.61).
- Rules-off Debug `build/import-ui-debug`: 2/2 in 2.73 seconds (`statement.parser` 0.01, `statement.review` 2.71).

The post-ASO `statement.review` is the run that covers the credit-card `aso` import, shared source tables, and the second same-FITID import. `import.ui` was not rerun: the only UI edit is the QML name filter, and no UI test asserts that filter. `import_model.cpp` was not changed. `statement.faults`, the full `import.io`, `import.crashes`, and backup failure/crash sweeps were not rerun for this admission. Linux and Windows remain unverified. The pre-ASO 15/15 binaries do not contain this change.

The rebuilt fuzzer replayed the synthetic fixture once:

`./build/qif-fuzz/tests/engine/spentit_statement_fuzzer tests/fixtures/statements/card-sgml.aso -runs=0`

Exit 0. LibFuzzer reported one input executed in 6 ms and noted that fuzzing was not performed. That replay is not a campaign.

Cases still refused: a body labeled `aso` that is neither OFX SGML nor OFX XML; `SECURITY` other than empty, `NONE`, or `TYPE1`; `COMPRESSION` other than empty or `NONE`; a body that is not valid UTF-8; unsupported message sets; a currency that does not match the target account; and malformed input before capture. A repeated `FITID` that exists only inside the current uncommitted batch still has no duplicate target.

Coordinator review on 2026-10-27 accepted this checkpoint with the recorded limits. A fresh rules-on Debug `ctest --test-dir build/import-ui-rules-debug --output-on-failure -R '^statement\.(parser|review)$'` passed 2/2 in 2.55 seconds. `git diff --check` passed. Feature 003 stays active. The question at delivery was whether to proceed to point 4 or stop. Point 4 is the next section.

## Point-4 account management — 2026-10-27

Account management from [feature 002](../features/002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27) is implemented on the serial import worker. Storage owns each transaction. The engine stays independent of Qt and SQL. The UI and QML call the worker commands and do not execute SQL or compute authoritative money. New documents use schema 20 with rules and schema 21 without rules. Both bodies share one shape. Opening a schema 18 or 19 document, or any other unexpected `user_version`, fails with `Unsupported experimental format; no migration was attempted`. Application id remains `1397770577`. The point-2 delegate-cache fix and the point-3 source tables remain in place. Feature 002 stays active. Points 5–8 were not started.

The commands are `rename_account`, `archive_account`, `reactivate_account`, `delete_account`, `correct_opening`, `reassign_currency`, and `undo_account_operation`. Each is atomic and historied. An identical operation-id retry returns the stored receipt. Conflicting reuse is rejected. Success is reported after durable completion.

Rename changes the current display name and leaves `account_creations.name` unchanged. Archive and reactivation keep history, evidence, balances, and transfer links. An archived account is readable and is refused as a target for a new import, commit, transaction, or transfer match. Archive refuses while a pending import exists and leaves that import in place. The unsaved-editor guard remains Save / Discard / Stay. Empty-account deletion retires the integer identity. It requires an untouched zero opening, the creation opening date, no imports, no transfer participation, and another account remaining. Opening correction changes a confirmed or frozen opening amount, date, or both, and leaves transaction amounts and source facts unchanged. Balances are that opening plus the existing committed effective totals in the existing order. Currency reassignment preserves each numeric decimal and rescales minor units to the new scale. A non-zero digit that would disappear, or a signed 64-bit overflow, refuses the whole command. Source bytes, `source_occurrences.currency`, and `source_occurrences.original_units` stay in the old currency. `account_creations.definition` stays the original definition.

A linked transfer keeps both sides. Only the reassigned account's amounts change. `preserved_amounts` records two opposite decimals that may now use different currencies. New generation, proposal, and commit still require a same-currency match. No rate, fee, or equality is invented, and no unlink or repair command was added. That case did not need a separate financial policy, so no decision gate was left open. Undo of an opening correction or a reassignment restores the prior image when no later financial operation on that account or a linked transfer has happened since. Ordinary committed Undo still excludes transfer reversal. After a currency reassignment, ordinary Undo of the rescaled rows stays unavailable because their entry heads advanced with the command.

The currency dialog warns, before OK, when the account has an opening, a draft, or a transaction: `This reassigns the account currency without conversion. Numeric amounts stay the same decimal. It does not apply an exchange rate.` The sidebar exposes rename, archive, the archived section, reactivate, and delete. `spentit_import_model` linked in the rules-debug, rules-release, and rules-sanitized builds. No `import.ui` runtime test was run, so the QML behavior is unverified.

One validation failure was fixed before the passing runs below. A direct `./build/import-ui-rules-debug/tests/storage/spentit_account_tests validation` exited 1 with `validation sql failed: UPDATE account SET name='' WHERE singleton=2 :: no such function: spentit_account_command`. The raw fixture connection does not register `spentit_account_command()`, and `frozen_opening` calls it on every account update. The same `accounts.validation` failure had already stopped the rules-debug selection at 17/18 (that case 1.67 seconds, selection real time 34.72 seconds, exit 8) and the rules-release selection at 17/18 (that case 1.47 seconds, selection real time 32.51 seconds, exit 8). The fixture now drops `frozen_opening` and `account_definition_command` only long enough to plant the corruption. `ImportStore::open` still rejects the document. Product triggers are unchanged.

Verification is selected macOS coverage on Apple clang, arm64. It is a selected run, not a full suite, and not a Linux or Windows claim. Synthetic fixtures only. `accounts.lifecycle_boundaries` arms representative write/sync positions 1, the middle position, and the last position, plus observer exits before and after commit, for rename, archive, reactivate, delete, opening correction, undo of that correction, currency reassignment, and undo of that reassignment. A direct rules-debug run printed these write/sync counts and exited 0:

- rename, archive, reactivate, delete, opening, undo-opening: 57
- currency, undo-currency: 113

`accounts.lifecycle_behavior` covers creation-name retention, archive refusal of a pending import, reactivation with unchanged money, empty deletion and its refusals, permanent retirement, opening correction and eligible Undo, equal-scale reassignment, scale 2 to scale 3 padding, scale 2 to scale 0 acceptance of a zero fraction and refusal of a non-zero fraction, overflow refusal, a linked transfer that keeps both sides, and a restored backup whose account currency changed while `source_occurrences.currency` and `original_units` stayed on the old currency.

Passing selections after the validation fix, each with `ctest --test-dir <tree> --output-on-failure` and exit 0:

- Rules-on Debug, `build/import-ui-rules-debug`, regex `^(accounts\.(validation|basics|groups|lifecycle_behavior|lifecycle_boundaries)|transferstore\.(validation|qif|splits|imports|basics|manual_evidence)|import\.(repairs|validation|review|opening|basics)|statement\.review|rulestore\.schema)$`: 18/18, real time 45.14 seconds. `accounts.validation` 6.24 seconds. `accounts.lifecycle_behavior` 3.06 seconds. `accounts.lifecycle_boundaries` 15.36 seconds. `rulestore.schema` 1.93 seconds.
- Rules-on Release, `build/import-ui-rules-release`, the same regex: 18/18, real time 42.92 seconds. `accounts.validation` 5.86 seconds. `accounts.lifecycle_behavior` 3.17 seconds. `accounts.lifecycle_boundaries` 14.82 seconds. `rulestore.schema` 1.77 seconds.
- Rules-on ASan/UBSan, `build/import-ui-rules-sanitized`, the same regex: 18/18, real time 84.47 seconds. `accounts.validation` 10.72 seconds. `accounts.lifecycle_behavior` 5.94 seconds. `accounts.lifecycle_boundaries` 26.10 seconds. `rulestore.schema` 2.81 seconds. No sanitizer diagnostic was reported by these passing tests.
- Rules-off Debug, `build/import-ui-debug`, regex `^(accounts\.(validation|basics|groups|lifecycle_behavior|lifecycle_boundaries)|transferstore\.(validation|imports|basics)|statement\.review|import\.(schema_gate|basics|review|opening))$`: 13/13, real time 38.58 seconds. `import.schema_gate` 1.55 seconds. That gate creates a schema-21 document and rejects user versions 6, 16, 17, 18, 19, and 20 with the unsupported-format sentence, leaving the file bytes unchanged. `accounts.lifecycle_behavior` 3.20 seconds and `accounts.lifecycle_boundaries` 14.62 seconds are the rules-off new-format path.

`statement.faults`, the full `import.io` and `import.crashes` sweeps, and the broader committed, category, and UI suites were not rerun. `accounts.basics` in these selections still covers ordinary committed correction and Undo. No transfer-reversal command was added. Linux and Windows remain unverified.

Coordinator review on 2026-10-27 accepted this checkpoint with the recorded limits. A fresh rules-on Debug `ctest --test-dir build/import-ui-rules-debug --output-on-failure -R '^accounts\.lifecycle'` passed 2/2 in 17.13 seconds. `git diff --check` passed. Feature 002 stays active. The question at delivery was whether to proceed to point 5 or stop. Point 5 is the next section.

## Point-5 transfer management — 2026-10-27

Transfer management from [feature 008](../features/008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27) is implemented on the serial import worker. Storage owns each transaction. The engine stays independent of Qt and SQL. QML calls the worker and does not compute authoritative money. New documents stay schema 20 with rules and schema 21 without rules. Merge and retarget fit the existing transfer event and version blobs, so this checkpoint did not bump `user_version`. Application id remains `1397770577`. Feature 008 stays active. This checkpoint is committed as `786bc96`. Committed reversal remains point 6. Points 6–8 were not started.

The owner locked the behavior on 2026-10-27. A link is an allocation, not the transaction total. Merge is the visible command for a missed or incorrect match. Ctrl+M is the shortcut; Qt maps it to Command+M on macOS. A purely generated uncommitted duplicate, including a protected generated draft, leaves the active ledger when Merge links the independent allocation. Two independent opposite allocations, including two already committed rows, gain a link and both remain. Destination-only deletion of a generated draft is refused. Discarding an uncommitted origin removes the purely generated counterpart with it. Retarget to a category or another account removes a purely generated uncommitted counterpart, and unlinks an independent or committed counterpart without changing that counterpart's amount. The category selector lists real accounts by identity. The Transfers dialog shows a read-only journal.

Commands that refuse: unequal allocation amounts, a new cross-currency pair, an ambiguous allocation, a committed generated duplicate, rewriting a committed allocation's category, and Merge or retarget of a `preserved_amounts` link. A pending import proposal on a committed generated side must be committed or discarded before that side is unlinked. Two manually entered rows are refused because the destination side of a new link is an imported allocation. Automatic matching remains deferred.

Verification is selected macOS coverage on Apple clang, arm64. It is a selected run, not a full suite, and not a Linux or Windows claim. Synthetic fixtures only. Passing selections, each with `ctest --test-dir <tree> --output-on-failure` and exit 0:

- Rules-on Release, `build/import-ui-rules-release`, regex `^(transfers\.|transferstore\.(basics|imports|management|splits|manual_evidence|qif|validation)|accounts\.lifecycle|import\.(ui|navigation|recovery_ui|committed_ui|category_ui|rules_ui|qml)$)`: 24/24, real time 84.63 seconds. `transferstore.management` 3.96 seconds. `import.ui` 36.67 seconds.
- Rules-on ASan/UBSan, `build/import-ui-rules-sanitized`, the same regex: 24/24, real time 146.56 seconds. `transferstore.management` 9.62 seconds. `import.ui` 57.73 seconds. No sanitizer diagnostic was reported by these passing tests.
- Rules-off Debug, `build/import-ui-debug`, the same regex without `import.rules_ui`: 23/23, real time 76.60 seconds. `transferstore.management` 4.18 seconds. `import.ui` 34.93 seconds.

Fault sweeps of the thirteen transfer operations, including Merge of a generated duplicate, category retarget, and Merge of two committed independent allocations:

- Rules-on Debug, `build/import-ui-rules-debug`, regex `^transferstore\.(io|crashes)$`: 2/2, real time 593.06 seconds. `transferstore.io` 296.98 seconds. `transferstore.crashes` 296.07 seconds.
- Rules-on Release, `build/import-ui-rules-release`, the same regex: 2/2, real time 537.13 seconds. `transferstore.io` 265.15 seconds. `transferstore.crashes` 271.97 seconds.
- Rules-off Debug, `build/import-ui-debug`, the same regex: 2/2, real time 526.87 seconds. `transferstore.io` 260.69 seconds. `transferstore.crashes` 266.17 seconds.
- Rules-on ASan/UBSan, final test binary, `ctest --test-dir build/import-ui-rules-sanitized --output-on-failure -R '^transferstore\.io$'` passed in 644.55 seconds. `ctest --test-dir build/import-ui-rules-sanitized --output-on-failure -R '^transferstore\.crashes$'` passed in 638.92 seconds. CTest set `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0` because the default ASan quarantine made `fork` receive SIGKILL on this macOS. The crash log lists operations 0–12 and ends with `Transfer store crashes passed`. An earlier crash attempt on this checkpoint failed in about 33 seconds with signal 9 before that setting.

The rules-on Debug fault binaries were built before the final test-file cleanup. The final ASan crash binary contains that cleanup. `import.ui` in the functional selections includes `transferManagementUi`: two-step Merge across accounts, the journal row, and choosing Visa in the category selector. Linux and Windows remain unverified. The full suite was not run.

Coordinator review on 2026-10-27 accepted this checkpoint with the recorded limits. Feature 008 stays active. The question at delivery was whether to proceed to point 6 or stop. Point 6 is the next section.

## Point-6 committed transfer editing — 2026-10-27

The owner said Go and locked the four answers in [feature 008](../features/008-transfer-matching-and-lifecycle.md#accepted-committed-transfer-editing--2026-10-27) and [feature 011](../features/011-committed-editing-and-reversal.md#accepted-committed-transfer-editing--2026-10-27). Storage owns each transaction. The engine stays independent of Qt and SQL. QML calls the worker and does not compute authoritative money. New documents stay schema 20 with rules and schema 21 without rules. The commands use the existing transfer event and version blobs and ledger kinds 0–5, so this checkpoint did not bump `user_version`. Application id remains `1397770577`. This checkpoint is committed as `cfad307`. Features 008 and 011 stay active because account moves of a committed row, source-link rewrite, reports, and the rest of those features remain open.

A confirmed Merge retires a purely generated committed duplicate and links the independent row of the same amount. The generated account's committed balance drops by that amount. The other account is unchanged. A generated row with its own import evidence is not removed. A zero-change Merge still applies immediately. Category rewrite or removal of a committed origin ends the link, leaves a committed counterpart's amount, and removes a purely generated uncommitted counterpart in the same operation. An amount correction ends the link and leaves the other amount. A date-only correction keeps the link. Undo restores that prior link only when no later edit, transfer relationship, or source association blocks it. Bulk linked edits are refused one at a time. Committed rule reapplication remains point 7.

Verification is selected macOS coverage on Apple clang, arm64. It is a selected run, not a full suite, and not a Linux or Windows claim. Synthetic fixtures only. Rules-off Debug was not rerun. Qt checks used `QT_QPA_PLATFORM=offscreen`. They loaded `ImportMain.qml`, opened the merge review dialog, and read its note. There was no separate native-window visual inspection.

Direct commands, each exit 0, in `build/import-ui-rules-release` unless noted:

- `tests/engine/spentit_transfer_tests` for `lifecycle`, `matching`, `boundaries`, `groups`, `acceptance`, `proposals`, `manual`, and `management`. Each printed its passed line.
- `tests/storage/spentit_transfer_store_tests` for `basics`, `imports`, `splits`, `manual_evidence`, `qif`, and `validation`. Each printed its passed line. `management` passed in 10.57 seconds and printed `Transfer store management passed`.
- `tests/storage/spentit_transfer_store_tests io` passed in 1068.78 seconds. It printed operations 0–14, including operation 13 (committed generated Merge, 73 write/sync positions) and operation 14 (committed category retarget, 113 write/sync positions), and ended with `Transfer store io passed`.
- `tests/storage/spentit_transfer_store_tests crashes` passed in 1079.84 seconds with the same operation list and ended with `Transfer store crashes passed`.
- `tests/storage/spentit_committed_tests` for `basics`, `history`, `review`, `dependencies`, `exact`, `backup`, `legacy`, and `validation`. Each printed its passed line.
- `tests/storage/spentit_committed_tests io` then `crashes` passed in one command of 956.41 seconds. Each kind 0, 1, and 2 printed 93 fault positions, and each sweep printed its passed line. Split times for the two sweeps were not recorded.
- `src/ui/spentit_import_ui_tests` with `QT_QPA_PLATFORM=offscreen` passed 31, failed 0, real time 160.52 seconds. That includes `transferManagementUi` and `committedMergePreview`: a zero-change Merge leaves `mergePreview` closed; a committed linked import origin keeps `canCorrect` and `editable` false; a date-only preview contains `remains` and leaves the balance unchanged; a money-changing Merge names Visa and Checking, leaves the committed balance at `100.00` until confirm, then drops it to `0.00`, and leaves Checking at `0.00`. Cancel between the two merges also leaves `100.00`.
- `src/ui/spentit_committed_ui_tests` with `QT_QPA_PLATFORM=offscreen` passed 8, failed 0, in 25.445 seconds.

Rules-on ASan/UBSan, `build/import-ui-rules-sanitized`, Debug with sanitizers. The functional command passed in 340.56 seconds and printed the passed line for every engine transfer case, every transfer-store case except `io` and `crashes`, and every committed case except `io` and `crashes`. No sanitizer diagnostic was present in that output. `ASAN_OPTIONS` was `halt_on_error=1:abort_on_error=1` for that command.

Rules-on ASan/UBSan fault sweeps, direct binary `build/import-ui-rules-sanitized/tests/storage/spentit_transfer_store_tests`, with `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1:abort_on_error=1`:

- `io` passed in 1129.74 seconds, exit 0. It printed operations 0–14, including operation 13 (73 write/sync positions) and operation 14 (113 write/sync positions), and ended with `Transfer store io passed`.
- `crashes` passed in 1132.23 seconds, exit 0, with the same operation list, and ended with `Transfer store crashes passed`.

No sanitizer diagnostic was present in either output. Linux and Windows remain unverified. The full suite was not run.

## Point-7 committed rule reapplication — 2026-10-27

The owner said Go for point 7 and declined a scope question. The settled command is in [feature 004](../features/004-rules-and-explanations.md#accepted-committed-reapplication--2026-10-27). Storage owns each transaction. The engine stays independent of Qt and SQL. The rule evaluator was not changed: a QIF transfer destination in the source category is still skipped there. Storage then puts back a live transfer allocation's category, binding, owner, and rule mark when the text or binding would change, and the preview names that skip. QML calls the worker and does not compute authoritative money. New rules documents use schema 22. Rules-off documents stay schema 21. Opening a schema 20 document, or any other unexpected `user_version`, fails with `Unsupported experimental format; no migration was attempted`. Application id remains `1397770577`. This checkpoint is committed as `b6b9346`. Features 004, 008, 010, and 011 stay active. Point 8 was not started.

Reapply reads the selected account's active committed entry heads. Drafts, removed rows, generated transfer rows, and other accounts stay. One preview, one Apply, and one group Undo. Manual fields stay unless Replace manual was part of that preview. Amounts, dates, split structure, evidence, and transfer links stay. A zero-change preview writes nothing and does not consume the operation id. A later different apply may reuse that unused id. An operation id that already belongs to another command fails even when the fresh preview changes nothing. Pending Apply of a committed batch still fails with `Committed records are not changed by rules`. Save does not change rows.

Verification is selected macOS coverage on Apple clang, arm64. It is a selected run, not a full suite, and not a Linux or Windows claim. Synthetic fixtures only. Qt checks used `QT_QPA_PLATFORM=offscreen`. There was no separate native-window visual inspection. A 4097-row preview and a reapply that would change committed money are implemented and were not executed. The fault probe did not sweep every write position.

Direct commands, each exit 0:

- `build/import-ui-rules-release/tests/storage/spentit_rule_store_tests reapply` passed in 1.05 seconds and printed `Rule store reapply passed`. The case covers manual protection, Replace manual, a stolen operation id, a 97-character id, identical retry, conflicting reuse, an untouched other account, draft, and removed row, unchanged evidence and balance, refusal of a member id and of ordinary Undo, pending Apply still refusing the committed batch, group Undo, Undo of that Undo, a later memo blocking Undo, a zero-change apply that leaves the id free, and a both-committed Shop category skip. The payee-and-category rule writes only the payee. The link, both balances, and evidence bytes stay. Ruleset provenance for the two applies is 1 and 2.
- `build/import-ui-rules-release/tests/storage/spentit_rule_store_tests reapply_faults` passed in 1.45 seconds. It printed `reapply faults: 121 fault positions observer and one disk-full` and `Rule store reapply_faults passed`. The before-commit and after-commit observers exited 72. The outcome is present only after commit. One disk-full injection at the first counted write recovered and the same id then applied. Positions 2 through 121 were not swept.
- `build/import-ui-rules-release/tests/storage/spentit_rule_store_tests schema` passed in 1.80 seconds and printed `Rule store schema passed`. Schema 20 on a legacy file is `Unsupported experimental format; no migration was attempted`. `definitions` passed in 0.29 seconds and printed `Rule store definitions passed`.
- `build/import-ui-rules-release/tests/storage/spentit_account_lifecycle_tests behavior` passed in 2.71 seconds and printed `Account lifecycle behavior passed`. It expects schema 22 when rules are supported.
- `build/import-ui-rules-release/tests/storage/spentit_statement_store_tests review` passed in 2.36 seconds and printed `Statement store review passed`. It expects schema 22 when `rule_state` is present.
- `QT_QPA_PLATFORM=offscreen build/import-ui-rules-release/src/ui/spentit_rule_ui_tests` passed 9, failed 0, Qt totals 5188 milliseconds, real time 5.32 seconds, Qt 6.12.0 arm64. Reapply opened a zero-change preview, the impact text contained `Account balance is unchanged.`, Apply said `No committed rows were changed`, and the rules notice stayed `Committed records are not changed by Apply.`

Rules-off Debug, `build/import-ui-debug/tests/storage/spentit_schema_gate_tests`, passed and printed `Schema gate passed`. A new document is schema 21. Preview and Apply fail with `Rules require a new experimental dataset`. `rule_reapply_group` is empty. Versions 6, 16, 17, 18, 19, 20, and 22 are rejected with `Unsupported experimental format; no migration was attempted`. This is not a rules-off rerun of the whole suite.

Rules-on ASan/UBSan, `build/import-ui-rules-sanitized/tests/storage/spentit_rule_store_tests`, with `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1:abort_on_error=1`:

- `reapply` passed in 3.42 seconds and printed `Rule store reapply passed`.
- `reapply_faults` passed in 3.25 seconds, printed the same 121-position observer and one disk-full line, and printed `Rule store reapply_faults passed`.
- `schema` passed in 3.12 seconds. `definitions` passed in 0.63 seconds.

No sanitizer diagnostic was present in that output. Linux and Windows remain unverified. The full suite was not run. Statement-file reapply was not run again.

## Point-8 sheet and sidebar interactions — 2026-10-27

The owner said Go for point 8. This is the last point in the eight-point sequence. One agent implemented it from `b594e15`. It is committed as `b1ff74b`. No schema change, migration, report generator, multiline note, date-format preference, same-date reordering, or starter catalog was added.

Each account shows its own committed or projected balance beside its name, including an archived account. The label is the word Committed or Projected, the currency code, and the amount. The amount uses the same formulas as the selected account's committed and projected balances. The choice is per account and lasts for the session. It does not call ledger search and does not change the Projected ledger checkbox. Creating, opening, unlocking, or leaving the document clears the choice. It is not written into the document. If a projection does not exist, the control shows the committed balance and a click does nothing. The selected account's packed balance matches that account's existing committed and projected totals.

Ledger columns use the ratios 0.14 date, 0.24 payee, 0.17 category, 0.15 withdrawal, 0.14 deposit, and 0.16 balance. Money text is right-aligned. The user cannot drag a column, and the widths are not saved. The window minimum is 720 by 560. The default size stays 1440 by 850. The ledger's content width is at least 1080, so a narrow window scrolls sideways. The sidebar toggle, Back, and Forward stay in the top toolbar.

Bulk removal, bulk category, rule reapply, and history undo capture the visible row inside the unsaved-edit guard, immediately before the command. Stay does not capture. Preview and Apply put that row back. Cancel puts it back for bulk and reapply. History undo puts it back on Apply. A missing row opens the ledger at the top. An ordinary single-row save does not capture. Reports are a note under the account list: `Reports stay outside this work. Nothing is created here.` There is no reports button and no report dialog.

Feature 005 stays active. Multiline notes, a persisted sidebar or column layout, the exact sidebar shortcut, a date-format preference, same-date ordering, spoken VoiceOver, Linux, and Windows remain open. The sidebar shortcut stays Ctrl+Shift+B. On 2026-10-28 the owner said this sequence was the work before reports. The report generator stays unbuilt. Feature 006 holds the open questions. Implementation waits for an explicit Go and those answers.

Verification is selected macOS coverage on Apple clang, arm64, Qt 6.12.0. It is a selected run, not a full suite, and not a Linux or Windows claim. Synthetic fixtures only. Qt checks used `QT_QPA_PLATFORM=offscreen`. There was no separate native-window visual inspection and no spoken VoiceOver check. Rules-off Debug was not rerun.

Direct commands, each exit 0, in `build/import-ui-rules-release` unless noted:

- `QT_QPA_PLATFORM=offscreen src/ui/spentit_import_ui_tests` passed 32, failed 0, Qt totals 41624 milliseconds. `ledgerInteractions` is included. It checks the sidebar label against the selected account's committed `100.00` and projected `90.00`, that the balance click does not change the ledger checkbox, that the other account keeps its choice, that a dirty payee blocks bulk removal and the rules page, that Stay stays on the ledger, that the payee column is wider than the date, category, and money columns, that the reports note is present and has no button, that bulk category preview and Apply keep the same visible row, and that a 720 by 560 window keeps the toolbar buttons on screen with ledger content width at least 1080.
- `QT_QPA_PLATFORM=offscreen src/ui/spentit_import_navigation_tests` passed 3, failed 0, Qt totals 1284 milliseconds.
- `QT_QPA_PLATFORM=offscreen src/ui/spentit_committed_ui_tests` passed 8, failed 0, Qt totals 8611 milliseconds.
- `QT_QPA_PLATFORM=offscreen src/ui/spentit_rule_ui_tests` passed 9, failed 0, Qt totals 5096 milliseconds.
- `QT_QPA_PLATFORM=offscreen src/ui/spentit_category_ui_tests` passed 4, failed 0, Qt totals 3794 milliseconds.
- `QT_QPA_PLATFORM=offscreen src/ui/spentit_import_recovery_tests` passed 4, failed 0, Qt totals 1880 milliseconds.
- `tests/storage/spentit_account_tests basics` passed and printed `Accounts basics passed`. The duration was not recorded.
- `tests/storage/spentit_account_lifecycle_tests behavior` passed and printed `Account lifecycle behavior passed`. The duration was not recorded.

Rules-on ASan/UBSan, `build/import-ui-rules-sanitized`, with `ASAN_OPTIONS=quarantine_size_mb=8:malloc_context_size=0:halt_on_error=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`:

- `tests/storage/spentit_account_tests basics` passed in 3.14 seconds and printed `Accounts basics passed`.
- `tests/storage/spentit_account_lifecycle_tests behavior` passed in 5.48 seconds and printed `Account lifecycle behavior passed`.

No sanitizer diagnostic was present in those two outputs. The same sanitizer build did not finish the UI checks. `src/ui/spentit_import_navigation_tests` aborted during `guardedNavigationAndIsolation`, and `src/ui/spentit_import_ui_tests ledgerInteractions` aborted during document create. Both were AddressSanitizer stack-overflow reports in `sqlite3ExprCodeTarget` while preparing the existing verify statement at `import_store.cpp:1326`, on the Qt worker thread. Exit 134. That UNION was not changed in this point. Release ran those UI checks successfully. Linux and Windows remain unverified. The full suite was not run.

# Project status

Last updated: 2026-11-18

SpentIt is the local rules-enabled application. The release rules database is
version 1, and the application version is 1.0.1. A file whose number is not 1
is refused, and its bytes stay the same. There is no migrator. The rules-off
build still writes version 69 and is not the release format. The SpentIt
exchange file is JSON version 1.

Build and launch are in the [README](../README.md) and [building](building.md).
macOS arm64 with Qt 6.12.0 is the host used for product work. Linux and Windows
have development notes and are not release-ready. Do not describe a check as
passed unless you ran it.

## Redo acceptance after removing transient transfer history — 2026-11-18

Acceptance reversal verification permits a sequence gap left by removed
transfer operations. It checks that the recorded preview follows or matches
the last retained sequence and precedes the reversal. The verifier also
checks the publication, membership, and exact balances. See
[reversible acceptance](action-undo-design.md#6-reversible-acceptance).

The new synthetic test accepts transactions, adds a manual transfer, then
undoes the assignment, addition, and latest acceptance. Acceptance Redo failed
before the verifier fix and passed afterward. The test also checks a failure
before commit, retry, exact membership and balances, and reopen. Nine
rules-enabled Release CTest targets passed on macOS arm64 with Qt 6.12.0,
including the Redo I/O and crash sweep and offscreen smoke. The new regression
also passed under AddressSanitizer and UndefinedBehaviorSanitizer with leak
detection disabled.

## Undo a manual addition after undoing its transfer assignment — 2026-11-18

Change history uses the account that owns the original posting when undoing
**Add transaction** after its transfer assignment has been undone. The account
being viewed stays selected. See [Change history](change-history-handoff.md)
and [MAN-089](manual-update-backlog.md#open-items).

The synthetic regression reproduced the account error before the fix and
passed afterward. It checks both account balances, the selected account, a
failure before commit and retry, the durable Undo receipt, and Redo followed
by reopening the file. Six rules-enabled Release CTest targets passed on macOS
arm64 with Qt 6.12.0, including the manual transaction and committed UI suites,
action Redo, and offscreen smoke. The new regression also passed under
AddressSanitizer and UndefinedBehaviorSanitizer with leak detection disabled.

## Verify automatic rules after overlapping imports — 2026-11-18

Rule-history verification now uses the duplicate decision at the automatic
application's starting revision. Later Review decisions leave that scope
intact. Explicit pending-batch Apply follows its own scope. See
[rules storage](durable-rules.md). The release schema remains version 1.

The synthetic overlapping QIF regression failed at opening-balance Redo
before the fix and passed afterward. It checks the duplicate's disposition,
the new transaction's rule assignment, exact balances, the durable Redo
receipt, recovery after reopen, explicit Apply, and a later change to the
duplicate decision. On macOS arm64 with Qt 6.12.0, it passed in the
rules-enabled Release build and under AddressSanitizer and
UndefinedBehaviorSanitizer with leak detection disabled. The focused offscreen
Redo failure-message test also passed with diagnostics enabled. Fourteen
Release CTest targets passed, including import Undo, account rollback, the
Redo I/O and crash sweep, and offscreen smoke.

## Identify rule-history failures during Redo — 2026-11-18

When rule-history verification blocks Redo before commit, Change history
explains that Undo is still in effect. Opt-in diagnostics report Redo as its
own operation and include a fixed code for the failed check. See
[diagnostics](application-diagnostics.md#redo-rule-history-checks) and
[MAN-093](manual-update-backlog.md#open-items).

A synthetic overlapping QIF import reproduced `rule_history_application_scope`.
Automatic rules skipped a recognized duplicate and categorized a new transaction
in the same batch. Verification included the duplicate in its count of evaluated
transactions and failed even before the opening edit. Repeating the sequence
with only the new transaction passed verification and Redo. The original
session predates the detailed code, so the cause of that incident is unconfirmed.

On macOS arm64 with Qt 6.12.0, the focused rules-enabled Release UI regression
passed with diagnostics enabled and disabled. It covers successful Redo and
checks that a rejected Redo preserves the opening and both history stacks
across repeated attempts. Six CTest targets passed, including the diagnostic
collector, transport, offscreen smoke, action Redo, action Redo I/O and crash
sweep, and catalog deletion.

## Review after a transfer Link is broken — 2026-11-18

Review can offer the original postings again when an edit breaks their saved
Link and they still meet the matching criteria. A current Link keeps the
previously answered choices settled. **Not this transfer** continues to hide
its pair until the decision is undone. See
[Review](review-sheet.md#one-question-per-transfer--owner-go-2026-11-12-and-2026-11-18)
and [MAN-092](manual-update-backlog.md#open-items).

On macOS arm64 with Qt 6.12.0, the new manual-transfer regression failed before
the fix and passed afterward. It covers breaking a Link, assigning both
originals again, reopening, explicit rejection, Undo, and linking the originals
with exact balances. Ten Release CTest targets passed, including the Review
decision fault sweep. Four focused offscreen Review and ledger UI cases and
`./build/SpentIt --smoke` also passed.

## Undo and close after grouped acceptance — 2026-11-18

After Undo of acceptance, whole-import Undo clears transfer links between its
own batches even when the destination batch comes first. Closing the file
after Undo of acceptance prunes the reversed grouped publication and its
reversal together. See [Import Undo](import-undo.md) and
[session history](session-history-baseline.md#close-and-recovery).

On macOS arm64 with Qt 6.12.0, both regressions failed before the fixes and
passed afterward. Ten rules-enabled Release CTest targets passed, including
session-marker fault tests. The new cases check a three-account import,
failures before and after commit, retry, recovery, and exact balances after
reopening. `./build/SpentIt --smoke` passed offscreen.

## Editing New manual transfers — 2026-11-18

A New manually entered transfer opens the full editor. Its eligible New
generated counterpart follows the edited split amount, date, payee and memo.
Review refreshes after the save. The editor asks before breaking a link and
describes what happens to the other posting. One Undo reverses the save
across accounts. The rules-enabled Release build at `./build/SpentIt` was
rebuilt with database schema version 1. See
[ledger editing](ledger-editing.md#editing-a-new-manual-transfer) and
[MAN-090](manual-update-backlog.md#open-items).

On macOS arm64 with Qt 6.12.0, four focused offscreen navigation cases and six
Release CTest targets passed. Synthetic checks cover reversing a transfer
amount and then linking from Review, confirmation and cancellation, split
edits, changing the destination account, retaining an independent posting
and an accepted bank posting, Undo, Redo, clean reopen, and injected disk-full,
I/O and commit-boundary failures. The storage suite also passed under
AddressSanitizer and UndefinedBehaviorSanitizer with leak detection disabled.
Native interactive, Linux, Windows, and full repository checks were not run.

## Undo and Redo a new transaction across accounts — 2026-11-18

Change history can undo and redo **Add transaction** while another account is
selected. SpentIt reads the saved transaction from the account it belongs to
when preparing Redo. The selected account stays the same. See
[Change history](change-history-handoff.md) and [MAN-089](manual-update-backlog.md#open-items).

On macOS arm64 with Qt 6.12.0, the new offscreen Change history regression
reproduced the account error before the fix and passed afterward. Five
rules-enabled Release CTest targets passed, including the committed UI suite,
manual transactions, and the Redo write-failure and crash sweep.
`./build/SpentIt --smoke` passed offscreen.

## Correcting New transfer dates — 2026-11-18

The original ledger transaction can save a date correction while it is New
and linked. An eligible generated New counterpart follows that date. An
independent or accepted counterpart keeps its own date. Attached or proposed
bank matches protect the dates. The correction and its Undo each save as one
operation. See [ledger editing](ledger-editing.md#correcting-the-date-of-a-new-transfer)
and [MAN-088](manual-update-backlog.md#open-items). The release schema remains
version 1.

On macOS arm64 with Qt 6.12.0, five focused offscreen navigation cases and five
Release CTest targets passed. The new UI case covers the generated entry's
shortcut to the original, Enter to save, and Undo with Date focused. Synthetic
storage checks cover independent and accepted counterparts, proposed and
attached bank matches, invalid dates, Undo, Redo, reopen, and injected disk-full,
write, and commit-boundary failures. The manual transaction storage suite
also passed under AddressSanitizer and UndefinedBehaviorSanitizer with leak
detection disabled. Native interactive, Linux, Windows, and full repository
checks were not run.

## Switch accounts after Undo — 2026-11-18

Returning to an account after Undo skips a remembered transaction that
Undo removed. The account opens in the same session. See
[ledger selection](search-ledger.md#selection-and-interactions).

On macOS arm64 with Qt 6.12.0, the cross-account import Undo regression
reproduced the stale-selection error before the fix and passed afterward.
Five other import UI cases, three navigation cases, and
`./build/SpentIt --smoke` passed offscreen in the rules-enabled Release build.

## Remove account-menu Undo last import — 2026-11-18

The account right-click menu drops **Undo last import…**. File-wide **Undo**
still reverses the latest saved action. Review **Undo import…** still removes
eligible older imports when the stack is empty. See [import Undo](import-undo.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release passed four focused
offscreen UI cases and the import Help walkthrough. The cases cover file-wide
Undo across accounts, older-import removal from Review, and a multi-account
import. `./build/SpentIt --smoke` passed offscreen.

## Redo automatic follow-on links — 2026-11-18

Redo of a manual Link group restores the selected pair and the recorded
automatic follow-ons in one save. The internal replay uses the saved pairs
and allocation identities, with the existing automatic-link validation.
The unmatched-transfer UI test waits for **Apply decisions** to be inside
the window before simulating a click. The bundled manual already describes
the intended Redo behavior.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release follow-on Link,
Review sheet, selected link batch, automatic Review, and action Redo tests
passed. The follow-on case checks exact endpoints and balances, refusal of
automatic links submitted as staged decisions, rollback and retry before
commit, and recovery of a lost acknowledgement after commit. The focused
offscreen Review UI cases and ten isolated offscreen unmatched-transfer runs
passed, as did one native run and `./build/SpentIt --smoke` offscreen.
The existing follow-on Link and decision batch sweeps passed 157 injected
write and sync positions each. The action Redo sweep passed 145 I/O and 145
crash positions.

## Show reciprocal transfer choices once — 2026-11-18

Review shows a pair offered in both directions on the outgoing card when
possible. Choices offered in only one direction stay on their original cards.
Choosing one of two indistinguishable same-day pairs checks the remaining
exact pair. See [Review](review-sheet.md#one-question-per-transfer--owner-go-2026-11-12-and-2026-11-18).

The rules-enabled Release build on macOS arm64 with Qt 6.12.0 passed the new
offscreen UI case for the same-day and seven-day groups, partial reciprocity,
Apply decisions, exact balances, Undo, and Redo. The named-account QIF group,
Review policy, link batch, automatic Review, generated batch, transfer
assignment, and Review sheet tests passed. The transfer-decision batch fault
sweep passed 157 write and sync positions. The Release executable passed
`./build/SpentIt --smoke` offscreen. The existing `import.follow_link` test
failed at Redo both before and after this change. The offscreen unmatched
transfer UI test passed once and later failed when its simulated Apply
decisions click fell outside the window.

## Accounts that don’t need bank matches — 2026-11-18

Owner Go adds a Settings list of accounts where waiting-only generated
transfers can be accepted without a bank match. Listed active accounts no
longer show those cards in Review. Candidate matches still appear. Accept
transactions publishes the entries and the unmatched decision together.
The setting is local to the file at its current path on this computer.
See [Review](review-sheet.md#accounts-that-dont-need-bank-matches--owner-go-2026-11-18).

The rules-enabled Release build on macOS arm64 with Qt 6.12.0 passed the
focused offscreen Settings, Review, acceptance and password cases and the
committed UI suite. The new cases verify add and remove, candidate visibility
before and after acceptance, Undo, reopening, rename, password change, and
file replacement or restoration. The acceptance fault sweep passed 237
injected write and sync positions for the listed-account case and process
exits on both sides of commit. It checked exact balances, durable retry,
and the unmatched decision on the generated entry. The Review policy,
Review batch, account lifecycle and statement Review regressions passed.
`./build/SpentIt --smoke`, changed-document links and `git diff --check` passed.

## Apply unmatched transfer decisions in bulk — 2026-11-18

Owner Go adds an unchecked **Accept without bank match** checkbox to
waiting-only Review cards. Users can select all for the account that holds
the generated entries. **Apply decisions** saves checked entries with any
staged links and rejections, in one operation and one Undo. **Hide
suggestions…** is offered on cards with candidate matches. See
[Review](review-sheet.md#batch-accept-without-a-bank-match--owner-go-2026-11-18).

On macOS arm64 with Qt 6.12.0, ten focused rules-enabled Release UI cases
passed offscreen. The storage test applied 105 synthetic unmatched decisions
in 174 ms, excluding UI refresh, and verified exact balances, unchanged
transaction acceptance status, stale-request refusal, cancellation, Undo and
Redo. The Review policy, duplicate batch and transfer assignment tests also
passed. The rebuilt `./build/SpentIt --smoke` passed offscreen.
The mixed transfer decision sweep passed 157 injected write and sync
fault positions and process exits on both sides of commit. It checked
atomicity, exact retry and Undo. Changed documentation links and
`git diff --check` passed.

## Tab selects a category — 2026-11-18

Tab chooses the highlighted entry in the shared ledger and report category
picker, just like Enter. With no eligible entry, the picker stays open. See
[category selection](../features/010-categories-and-selection.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release navigation tests
`categoryPopupFiltersAndScrolls` and `categoryMenuSavesMixedRowsAndReopens`
passed offscreen, along with `./build/SpentIt --smoke`.

## Change category menu label — 2026-11-18

The shared ledger and report context menu says Change category… to indicate
that it opens the category picker. See
[categories and selection](../features/010-categories-and-selection.md).

The rules-enabled Release navigation test `categoryPopupFiltersAndScrolls`
and `./build/SpentIt --smoke` passed offscreen on macOS arm64 with Qt 6.12.0.

## Home: Household starter category — 2026-11-18

New files created with Use default categories include Home: Household as an
expense. Existing files keep their catalogs. See
[starter categories](../features/010-categories-and-selection.md#starter-categories).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release build passed
`category.setup`, `import.replacement`, and `configuredDatesAndCategories`
with access to the application-data locks. `./build/SpentIt --smoke` passed
offscreen. The 60 starter names match the feature list.

## Keyboard edits on imported splits — 2026-11-18

The inline category picker now offers ordinary categories and Uncategorized
for a split with an unresolved imported transfer destination. Choosing an
account still uses the transfer-resolution command. See
[ledger editing](ledger-editing.md#category-completion--owner-go-2026-11-01).

On macOS arm64 with Qt 6.12.0, both unresolved-destination keyboard cases
failed before the fix and passed afterward. Six rules-enabled Release cases
used Tab and keystrokes to edit and save the category, split memo, and split
amount in the account ledger and report transaction list. The cases covered
ordinary New and Accepted QIF transactions and New transactions with an
unresolved transfer split. Reopening confirmed the edits, exact total, and
retained original fields. The report and category UI suites, transfer
assignment, and transfer cleanup passed. The rebuilt `./build/SpentIt --smoke`
passed offscreen. Changed documentation links and `git diff --check` passed.

## Delete-account confirmation layout — 2026-11-18

Owner Go balances the empty-account confirmation. The dialog is at most 520
wide. Cancel and Delete account sit together on the right, each sized to its
label. The wording stays the same. Cancel leaves the account and history in
place. Delete account still retires the account, and still clears file-wide
Undo and Redo when the confirmation says so.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release
`deletionConfirmations` function passed offscreen (3 QtTest results, including
setup and cleanup). It checks the button widths, the right-hand pair, Cancel,
and Delete account. The same function passed again on the native macOS
platform, and a screenshot of the history-clearing confirmation was inspected.
`./build/SpentIt --smoke` passed offscreen. `git diff --check` passed. The
category confirmation is unchanged. Manual work remains MAN-075; manual text
and pictures have not been edited. No full category suite, sanitizer, Linux or
Windows run is claimed.

## Differing transfer amounts require a choice — 2026-11-18

Owner Go replaces automatic checking of differing-amount candidates. They start
unchecked; single-amount candidates show “Amounts differ — review before
linking”. Selecting one enables the existing Link difference workflow. Exact
one-to-one suggestions still start checked. The display flag compares exact
magnitudes in the same currency in the worker; QML does no monetary arithmetic.
Storage, matching eligibility and saving behavior are unchanged. The Review
contract and MAN-073 reflect this refinement; manual files were not edited.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release application and UI test
executables were rebuilt. The five focused Review functions passed (10 QtTest
results): differing-amount initial state/message, pending batch omission,
explicit selection, difference cancellation/save, pending decisions, partner
switching, multiple candidates, card buttons and batch selection. The Help
`reviewChapterWalkthrough:difference` passed (3 QtTest results). Tests used
synthetic files with access to their required application-data locks. A synthetic
screenshot was inspected. `./build/SpentIt --smoke` passed offscreen, as did
changed-document file links and `git diff --check`. No full suite, sanitizer,
native interaction, Linux or Windows verification is claimed.

## Disabled Review transfer choices — 2026-11-18

Owner Go covers dimming and disabling partner checkboxes that the existing
pending-link conflict check refuses. Rows and Inspect remain available, and
releasing a conflict immediately re-enables the choice. Checked partners can still
be unchecked, and automatic mirrors still permit partner replacement.
Selection and availability share the conflict predicate; storage and matching
policy are unchanged. See [Review](review-sheet.md#unavailable-transfer-choices--owner-go-2026-11-18).
Manual work is MAN-076; manual text and pictures have not been edited.

The rules-enabled Release application and UI test executable were rebuilt on
macOS arm64 with Qt 6.12.0. The four existing focused functions
`reviewMultipleLinkCandidates` (four cases), `reviewLinkCardButtons`,
`unifiedReviewLinkDifference`, and `reviewLinkSelected` passed. The expanded
`reviewPendingDecisions` passed after adjusting the test's delegate lookup and
window size; it verifies disabled opacity, re-enabling after release, checked
partner availability and automatic partner switching. A synthetic screenshot was
inspected. The initial sandboxed run could not access application-data locks;
the successful runs had that access. `./build/SpentIt --smoke` passed offscreen.
Changed-document file links and `git diff --check` passed. No full suite,
native pointer interaction, sanitizer, Linux or Windows run is claimed.

## Delete empty accounts and unused categories — 2026-11-17

Owner Go permits deleting a formerly used account once its current ledger is
empty and opening is zero, and deleting a category with no current transaction,
rule or saved-report references. Pending import errors, live transfers, saved
account report selections and retained import inverses that would refill an
account must be resolved first. The last account remains protected. Historical
identities, original imports and financial proof remain internal. The database
stays version 1. See [accounts](../features/002-accounts-money-splits.md),
[categories](../features/010-categories-and-selection.md) and
[Undo](action-undo-design.md#definitions-can-coexist-with-transaction-undo).

Deletion preserves unrelated Undo and Redo. If an inverse or replay needs the
identity, its confirmation explains that both file-wide stacks will be cleared.
Expiry and deletion commit together. Cancel, stale confirmation and failed saves
preserve the item and history; a committed retry uses its receipt. Eligible
source-owned import Undo can remove retained evidence from a deleted account.
Financial verification continues to reconcile retired account history and refuses
money or visible transactions in a retired account. Manual work is MAN-075;
manual text and pictures have not been edited.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release `catalog.deletion`
regression and the ASan/UBSan version passed (leak detection disabled). They
cover new and accepted rows, formerly used categories, opening corrections,
unrelated Undo/Redo preservation and replay, manual/repair Redo dependencies,
confirmation and stale tokens, saved reports and disabled rules, import Undo,
removed transfers, reopen, and lost acknowledgements on the same connection.
All 11 selected Release CTests passed: deletion behavior and faults, account
lifecycle behavior and durable boundaries, category assignment/history/validation
and I/O/crash sweeps, and Undo/Redo behavior and faults. The deletion fault sweep
injects disk-full and write failures at every observed write/sync position,
reopens and retries, and interrupts before and after durable commit to check
atomic history expiry. The initial sandboxed checks could not access the required
application-data locks; the passing runs had that access.
The full category UI suite passed (10 QtTest results); the final confirmation
function passed again after resetting previews on file changes. A synthetic
confirmation screenshot was inspected. `./build/SpentIt` was rebuilt and its
offscreen `--smoke` launch exited 0. No full repository suite, physical disk
exhaustion, Linux or Windows verification is claimed.

## Transfer cleanup follow-up — 2026-11-17

Owner Go covers correcting imported transfer destinations to categories or
Uncategorized, deleting emptied accounts after release/Unmerge, and reopening
an editor through Review Correct. Source allocation identities and originals
remain intact. Validation accepts explicit manual category replacements, and
accepted-transfer verification reads the current entry snapshot. Account
retirement ignores released relationship history and checks actual eligible
Import Undo inverses rather than every retained relationship snapshot. Correct
explicitly opens the editor after selecting its transaction. No schema change.

Rules-enabled Release checks passed for the four cleanup scenarios (generated,
linked new, Unmerge, and accepted), including Undo/Redo, original source retention,
retirement and reopen. Signed-split correction preserves the sibling allocation
and exact total; invalid source associations and invented destinations are refused.
The synthetic category UI suite passed, including Correct
with a closed editor followed by Uncategorized. Related transfer release,
assignment, import, QIF, validation, management and split tests passed, as did
account lifecycle, category assignment/history/validation, ordinary deletion,
Undo/Redo and rule preview. The old transfer-import test expected a category
correction to fail; it now checks refusal to invent a bracketed destination and
passed on rerun. The new imported-release fault sweep passed all 205 observed
write/sync positions with alternating disk-full and I/O errors, reopen/retry and
Undo, plus interruption before and after durable commit. ASan/UBSan cleanup
passed with leak detection disabled. The Release executable was rebuilt without
restarting the owner's running application or changing its open data. Full
repository, Linux and Windows suites were not run. Manual work remains MAN-075.

## What the application does

- Encrypted local files, optional explicit no-password mode, and private
  document locks. A backup is a copy of the file made after a successful close,
  while SpentIt is not running. Normal open and close use an encrypted clean
  marker. A crash or unclean file gets a full check, which does not repair
  data. There is no forgotten-password recovery. Settings can change, add, or
  remove a password. A copy keeps the password it was made with.
- QIF bank/cash/credit-card files under the [bank-v6 profile](../features/009-qif-profile.md),
  and OFX, QFX, QBO, and ASO bank and credit-card statements under the
  [statement profile](../features/003-ofx-profile.md). New-account mappings must
  choose zero or the reported opening literally.
- One balance that includes new and accepted transactions. Accept applies to
  new transactions across the file. File-wide Undo can reverse the latest saved
  import. Review can remove eligible older imports when the Undo stack is empty.
- Review for duplicates, transfers, and import problems, plus ledger duplicate
  resolution. A payment shown twice in one account is marked there, and Merge
  joins it. Merged and linked rows stay marked until they are accepted.
  Same-calendar-date duplicate suggestions. Strict bank IDs when both sides
  have them. Automatic one-to-one Merge and Link after import and rules,
  with separate local settings that start on. A saved off choice stays off.
  Same-day pairs take priority; remaining mutually unique adjacent-day
  duplicate suggestions can also merge. Transfer suggestions keep their seven-day
  window. Ambiguous and choice-requiring pairs stay manual. Automatic pairs are saved with the import, not as their
  own Undo steps. Undo of the import removes them. Unmerge still breaks one
  pair. Cancel during automatic matching saves none of those pairs; the import
  stays. Automatic results on accepted rows
  stay marked until the next Accept. Apply decisions saves staged transfer links
  and rejections as one operation and one Undo. One-to-one suggestions start
  checked and sort first. Individual Review Link and Not this transfer preserve
  unrelated pending decisions; Review Link applies only its chosen pair.
  See [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
- Manual transactions, ledger editing, splits, amount expressions, amount
  formatting, Set memo, Rename payee, and Restore imported values.
- Categories with an Income flag, rules that assign an explicit category to
  Uncategorized items only, and reports with exact allocation totals and
  drill-down.
- File menu QIF export and SpentIt export/import of rules, reports, and
  categories.
- One file-wide linear Undo/Redo stack with Edit menu commands and platform
  shortcuts. Redo is in memory while the file stays open; import Undo clears it
  and cannot be redone. Change history is a stack viewer with only newest-action
  Undo. Successful close expires both stacks; crash recovery preserves the
  durable Undo chain but loses Redo. The database remains version 1. See the
  [design](action-undo-design.md#version-1-linear-redo--2026-11-14).
- Bundled Help for the current manual, with contextual links.

The feature documents and the contracts they name are the behavior. This page
does not repeat them. The [release plan](first-release-plan.md) says what
release 1 does not add.

## Pending transfer decisions — 2026-11-17

Review stages Link, Not this transfer, or Decide later separately. Automatically
checked one-to-one cards sort first and keep their place when unchecked.
Differing-amount suggestions now start unchecked (owner Go, 2026-11-18).
Single-amount candidates show “Amounts differ — review before linking”.
Choose the candidate and use Link to resolve the difference; Apply decisions
does not save that card.
Multiple-choice cards start undecided; selecting a pair can still check a
remaining one-to-one partner. Related cards stay adjacent, and inferred mirrors
do not jump to the initial one-to-one section. Changing or unchecking a choice
clears its group’s automatic suggestions while preserving explicit and
unrelated decisions. Only positively choosing a partner infers new checks;
rejection and Decide later never do. Cleared checks stay undecided through
refreshes. The header places the counts, Apply decisions, then Undo import
from left to right. Apply decisions saves the pending links and
rejections atomically, with one Undo. Individual Review Link applies only its
pair, even with Auto Link enabled. Individual Link and Not this transfer retain
unrelated pending decisions. Changed choices on remaining cards are flagged
and omitted until chosen again. Inspect preserves pending choices; leaving
Review clears them. See [Review](review-sheet.md#pending-transfer-decisions--owner-go-2026-11-17).
Manual work is MAN-073; the manual has not been edited.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release UI functions
`reviewMultipleLinkCandidates` (four cases), `reviewLinkCardButtons`,
`unifiedReviewLinkDifference`, `reviewLinkSelected`, and
`reviewPendingDecisions` passed offscreen (10 QtTest results including setup
and cleanup). They cover sorting, staged rejection, Decide later, preservation
of pending links and rejections through individual saves, failed-save retention,
split selection, and amount differences. The older candidate test now locates
the current card after sorting instead of retaining a recreated delegate.
A synthetic Review screenshot was inspected.

The mirror-switching follow-up reran those five UI functions successfully
(10 QtTest results). The expanded `reviewPendingDecisions` also passed with
assertions for direct partner replacement, uncheck then replacement, mirror
recalculation, unchanged card order, explicit mirror rejection and deferral,
preservation of unrelated pending decisions, and no save during selection.
The rebuilt `./build/SpentIt --smoke` passed offscreen. This follow-up changes
QML selection and ordering only; it does not change the storage commands.

The rejection/header refinement reran the same five UI functions successfully
(10 QtTest results) and passed the Release smoke launch. The regression now
checks that rejecting a contender never checks the remaining possible partner,
and that cleared mirrors remain undecided after a Review refresh. A synthetic
screenshot verified counts, Apply decisions, and Undo import in that order on
the header. No storage commands changed in this refinement.

Release CTest `import.link_batch`, `import.decision_batch_faults`,
`import.review_batch`, and `import.link_selected_ui` passed. The fault sweep
injects disk-full and write failures, reopens and retries, and interrupts both
sides of durable commit to verify that rejections, links, and Undo publish
together. The initial sandboxed run could not access the required document-lock
directory; these passing runs had that access.

The rules-enabled ASan/UBSan `link_batch` run passed with leak detection disabled.
It covers mixed and rejection-only saves, cancellation, stale requests, exact
and changed retries, Undo, and Redo. `./build/SpentIt` was rebuilt and its
offscreen `--smoke` launch exited 0. No full suite, physical disk-exhaustion,
spoken screen-reader, Linux, or Windows verification is claimed.

## Statement drop and double-click — 2026-11-17

A QIF, OFX, QFX, QBO, or ASO file can start Import. Double-click, Open With,
and a Dock drop use sidebar Import, with no account chosen yet. A drop on a
sidebar account or the open account ledger uses that account's Import. A drop
on a report or another page uses sidebar Import. One statement at a time. A
statement waits until a SpentIt file is open. An archived account asks to be
reactivated. macOS lists the statement types in Open With and still owns only
`.spentit`. See [import](../features/003-qif-import-review.md). Manual work is
MAN-072.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release functions
`statementFileOpen`, `launchDocumentArgument`, `launchDocument`,
`externalDocument`, and `externalDocumentChoice` passed offscreen. They cover
statement arguments, keeping the open file, one statement at a time, a drop
onto another account, an archived account, a report page, the destination
question, a statement delivered while the file is open, and a statement that
waits until a file is created. `./build/SpentIt --smoke` exited 0. The built
local app’s Info.plist registers `.spentit` as Owner and the five statement
extensions as Alternate. No live Finder drag, full UI suite, sanitizer run, or
other-platform run is claimed.

## Searchable Change category — 2026-11-17

Change category opens a compact popup at the context-menu location. The filter
at the top has keyboard focus; typing filters the scrollable list by any part
of the displayed name, ignoring case. Up/Down moves the highlight, Enter or a
click chooses, and Escape or an outside click dismisses. Reopening clears the
filter. Disabled current-account destinations remain unavailable, and no
matches assigns nothing. The cascading-menu attempt lost text focus on item
updates, so this uses the owner-approved standalone popup. See
[categories](../features/010-categories-and-selection.md). Manual work is MAN-071.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release native functions
`categoryPopupFiltersAndScrolls`, `categoryMenuSavesMixedRowsAndReopens`, and
`acceptedManualTransferAssignment` passed (5 QtTest results including setup and
cleanup). They cover typed filtering, Backspace, arrows, focus after hover,
empty results, Escape/outside dismissal, reset on reopening, disabled accounts,
keyboard and mouse assignment, mixed new/accepted/manual rows, Undo and reopen.
The same three functions passed offscreen. A synthetic native filtered-popup
screenshot was inspected. `./build/SpentIt` was rebuilt and its offscreen
`--smoke` launch passed.

The additional offscreen `ledgerViewportSurvivesCategoryNavigation` check initially
failed its `saved.index > 200` assertion. Investigation reproduced the failure
with the pre-picker UI: the test assumed ascending order, while the newest-first
default placed transaction 260 at index 40. This was a test setup issue, not a
picker regression. The owner-authorized follow-up explicitly selects ascending
date order in that fixture, without changing application behavior or assertions.
After rebuilding `spentit_import_navigation_tests`, the complete function passed
offscreen on macOS arm64 with Qt 6.12.0 (3 QtTest results including setup and
cleanup, 0 failures). No full suite, sanitizer, Windows/Linux or spoken
screen-reader verification is claimed. Changed-document relative links and diff
whitespace checks passed.

## Inspect opens the ledger — 2026-11-17

Inspect on a Review row opens that transaction in its account ledger, among
the surrounding transactions, and leaves the transaction editor closed. Back
returns to Review with the question that was on screen. If an edit removes
that question, Review shows the next question that remains. Include match and
checked rows stay for that return. Leaving Review another way clears them. A
search that would hide the row is cleared. Correct still opens the transaction
panel. See [Review](review-sheet.md). Manual work is MAN-070.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release functions
`unifiedReviewSheet`, `mappedTransferBlockers`, and
`removedReimportRequiresReview` passed offscreen. The first keeps Include
match, clears a search that hides the row, shows that row in the ledger with
the editor closed, returns with Back, and clears the match when Rules is
opened. The second returns to the same Review scroll position. Correct still
shows Include as separate transaction and Keep removed. `./build/SpentIt` was
rebuilt. No full UI suite, live pointer check, sanitizer run, or
other-platform run is claimed.

## Amount and category Tab — 2026-11-17

Tab or Shift+Tab into the transaction amount or a split amount selects the
whole amount. A click that enters the field does the same. Tab through a
category field that was not edited moves to Memo and does not save. Typing,
or Up or Down, makes the next Tab choose the highlighted suggestion. See
[ledger editing](ledger-editing.md). Manual work is MAN-069.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`import.category_ui` and `import.rules_ui` passed offscreen. The category
check tabs into the amount and selects it, tabs through an unchanged category
to Memo without saving, restores a category that matches nothing, and selects
a split amount. The rules check tabs through an unchanged rule category
without changing it. `./build/SpentIt` was rebuilt. No full UI suite, live
pointer check, sanitizer run, or other-platform run is claimed.

## Created transfer entry category message — 2026-11-16

Choosing an ordinary category on the entry created for a transfer changes
nothing. The message says to change the category on the original transaction
in the other account, and that this entry will be removed. The same choice on
the original transaction still ends the link and removes the created entry.
See [categories](../features/010-categories-and-selection.md). Manual work is
MAN-067.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release
`import.transfer_assignment` check passed. It refuses the created entry with
that message and leaves the transfer and both balances in place.
`./build/SpentIt` was rebuilt. No UI suite, live pointer check, sanitizer
run, or other-platform run is claimed.

## Report list shows every account's transfer — 2026-11-16

After a report list assigns rows from several accounts to one other account,
every assigned row shows that account's name and the transfer arrow. The
account that was open is not the only one repainted. Rows that were not
assigned keep their category and have no new arrow. See
[reporting](../features/006-reporting-drill-down.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release check
`reportTransferAcrossAccounts` passed offscreen with Checking left open.
Checking and Savings both read `Visa [Account]` and an arrow to Visa, and one
Undo clears both names and both arrows. `./build/SpentIt` was rebuilt. No
full UI suite, live pointer check, sanitizer run, or other-platform run is
claimed.

## Transfer several accounts to one account — 2026-11-16

In a report's transaction list, Change category can send a selection from
several accounts to one other account. Each transaction gets its own opposite
posting there. One Undo reverses the whole selection. A row already in the
chosen account, or a source account in another currency, stops the save and
changes nothing. A transaction with more than one split is still changed one
at a time. Delete and Restore stay inside one account. See
[reporting](../features/006-reporting-drill-down.md) and
[ledger editing](ledger-editing.md). Manual work is an update to MAN-064.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`reportTransferAcrossAccounts` and `import.cross_account_selection` passed.
The report check assigns Checking and Savings to Visa, reads `Visa [Account]`
on both rows, restores both with one Undo, and refuses a Visa row sent to
Visa and a CAD selection sent to a USD account. The storage check covers the
same save, a cancellation on the second account, retry, and one Undo.
`import.transfer_assignment` passed. `./build/SpentIt` was rebuilt. No full
UI suite, live pointer check, sanitizer run, or other-platform run is claimed.

## Transfer name in the Category column — 2026-11-16

Choosing an account from Change category, or from the category field in an
open transaction, shows that account's name in the Category column on the
transaction's top line. The name matches the category field, including while
the transaction stays open, and the transfer arrow points at that account.
Undo restores the previous category text. The account ledger and a report's
transaction list use the same column. See
[reporting](../features/006-reporting-drill-down.md) and
[ledger editing](ledger-editing.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`reportTransferCategoryLabel`, `editableReportLedger`, `contextCategoryTransfer`,
and `ledgerColumnLayout` passed offscreen. The new check assigns Savings from
the report list's category menu and from the open transaction, reads
`Savings [Account]` on the top line and in the category field, restores
Uncategorized with Undo, and reads the same name on the account ledger.
`./build/SpentIt` was rebuilt. No full UI suite, live pointer check, sanitizer
run, or other-platform run is claimed.

## Open a transaction from a split line — 2026-11-16

While a transaction’s saved splits are visible, a double-click on the
“Saved splits” line or on a split line opens that transaction. One click
selects it, including Shift and Command or Ctrl. A right-click opens the
ledger menu. The transfer arrow still opens the other account. The editor
replaces those lines, so closing stays a double-click on the top line. The
account ledger and the report list use the same row. See
[the ledger](../features/005-transaction-sheet-navigation.md). Manual work is
MAN-066.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`splitLineOpensEditor`, `qifTransferResolution`, and `ledgerColumnLayout`
passed offscreen. The first opens the editor from a split line and from the
split heading, selects with one click, Shift-click, and Command-click, and
opens the ledger menu on a right-click. The transfer check clicks the split
arrow and lands in the other account. The column check still shows a cut-off
split memo. `./build/SpentIt` was rebuilt. No full UI suite, live pointer
check, sanitizer run, or other-platform run is claimed.

## Starter categories — 2026-11-16

A new file that uses default categories includes ATM & Cash Advance as an
expense. The device category is Electronics & Devices. The five income
categories stay the only ones checked. An existing file keeps the categories
it already has. See [categories](../features/010-categories-and-selection.md).
The manual does not name individual starter expenses, so no manual entry was
added.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`category.setup` and `import.replacement` passed. `category.setup` requires
both names, leaves them unchecked, requires that Electronics & Gadgets is
absent, and requires 59 categories. `./build/SpentIt` was rebuilt. No full
suite, sanitizer run, or other-platform run is claimed.

## Ledger Page Up and Page Down — 2026-11-16

With the account ledger or the report transaction list focused, Page Down
selects the row one screen further down and brings it into view. Page Up does
the same upward. The first and last rows stop the move. Shift extends the
selection. A text field keeps the key. See
[the ledger](../features/005-transaction-sheet-navigation.md). Manual work is
MAN-065.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`ledgerPageKeys` and `editableReportLedger` passed offscreen. The account
check pages a 60-row list, stops at the ends, extends with Shift, and leaves
a focused payee field unmoved. The report check pages its transaction list.
`./build/SpentIt` was rebuilt. No full UI suite, live key check, sanitizer
run, or other-platform run is claimed.

## Scrollbars that are not needed — 2026-11-16

A scrollbar stays out of the way until there is something to scroll. Clicks
then reach the bottom of a page, including the lower half of Preview and
Apply rule, and the bottom of the ledger down to the status line. When the
ledger row is wider than the window, its horizontal scrollbar appears and can
be dragged. The same rule applies to a vertical scrollbar. See
[rules](../features/004-rules-and-explanations.md) and
[the ledger](../features/005-transaction-sheet-navigation.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release checks
`import.controls_style` (`idleScrollBarIsNotThere`) and
`unusedScrollBarsLeaveTheBottomClick` passed offscreen. The ledger check
presses the bottom of a fitting sheet, then drags the scrollbar after the
window is narrowed. The Rules check presses the lower half of Preview.
`previewMatchScrollbarDrags` passed again. `./build/SpentIt` was rebuilt.
No full UI suite, live window drag, sanitizer run, or other-platform run
is claimed.

## Rule preview scrollbar — 2026-11-16

When a rule preview has more matches than fit in the list, that list's
scrollbar can be pressed and dragged. Dragging it moves the matches. The
wheel still scrolls the list. The Rules page scrollbar still moves the page.
A preview that fits does not shift its columns. See
[rules](../features/004-rules-and-explanations.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release rule UI tests
`previewMatchScrollbarDrags`, `largeMatchListIsVirtualized`, and
`enabledPreviewOnlyShowsMatchingRows` passed offscreen. The drag check presses
the match scrollbar, moves the list, and leaves the page scroll where it was.
`./build/SpentIt` was rebuilt. No full rule UI suite, live window drag,
sanitizer run, or other-platform run is claimed.

## Report account sort and cross-account edits — 2026-11-16

The Account heading in a report's transaction list sorts that list. Rows from
one account stay together, newest date first inside the account. Change
category, Rename payee, and Set memo apply to the whole selection in one save,
including rows from several accounts. One Undo reverses that selection, and
Redo replays it. Delete, Restore, and choosing an account destination stay
inside one account. A transaction with more than one split is still changed
one at a time. An ordinary category change in one account uses the same save
it used before. Manual work is MAN-064. See
[reporting](../features/006-reporting-drill-down.md).

On macOS arm64 with Qt 6.12.0, the rules-enabled Release report UI suite passed
9 checks, including account order, a category, payee, and memo change across
Checking and Savings, Undo of each, and refusal of a mixed Delete. The storage
checks passed for category, memo, and payee across Checking, Visa, and a USD
cash account, including retry, cancellation before the second row, a missing
row, a split row, Undo, and Redo. Same-account payee, memo, transfer
assignment, and linked deletion checks passed. No full repository suite,
sanitizer run, private-file reproduction, or other-platform run is claimed.

## Report PDF export — 2026-11-15

Reports now offer Save as PDF… beside Refresh report and in the File menu.
The modal preview defaults to Readable pages, Letter, Automatic orientation,
and a report-named PDF beside the open file. It supports A4, explicit
orientation, Fit on one page, Browse, and overwrite confirmation. Tables
paginate with repeated headings and group wide comparisons with the main
period repeated. The PDF retains exact report strings and separate currencies;
text and charts use vector output. Exported PDFs are explicitly unprotected.
See the [contract](report-pdf.md). Manual work is queued as MAN-059.

On macOS arm64 with Qt 6.12.0, the rules-enabled Release `build/SpentIt` was
rebuilt and its offscreen `--smoke` launch passed. The report UI suite passed
9 checks, including PDF defaults, saved output, open-file overwrite refusal,
existing-file confirmation enforcement, stale-report refusal, and menu gating.
The PDF suite passed 7 checks, including pagination, complete wide tables,
Unicode labels, exact negative strings, currency separation, negative pie
refusal, cancellation, failed output, and process interruption on both sides
of replacement. A POSIX file-size limit forces a real QSaveFile write failure
and verifies preservation of the previous file. A successful retry is covered.

The PDF layout/writer suite also passed under address and undefined-behavior
sanitizers in an isolated build with leak detection disabled. Synthetic bar,
grouped-bar, pie, table, wide and fit layouts were generated; representative
pages and the dialog were visually inspected with Poppler-rendered output.
Text extraction verified the final category in all three wide-table column
groups and the final comparison. Changed-document links and diff whitespace
checks passed. No native Browse-picker interaction, Windows/Linux run, full
repository suite, leak check, or physical disk-full test is claimed.

## Follow-on links after a manual Link — 2026-11-16

With Auto Link enabled, manually linking a Review pair also links remaining
mutually unique pairs in the original connected suggestion group. The manual
Link and follow-ons are one atomic save and one ordinary Undo step. Redo replays
exactly those pairs. Unrelated groups and remaining ambiguity stay manual;
acceptance remains explicit. Unused splits on the selected rows still count as
competitors, and other splits are unchanged. Cancel restores both data and the
Review choices. The UI reports additional pairs and grouped Undo. No schema
change. Manual work is MAN-063; see [follow-on links](review-sheet.md#automatic-follow-on-links-after-a-manual-link--owner-go-2026-11-16).

The macOS arm64 rules-enabled Release app was rebuilt and its offscreen smoke
launch passed. The offscreen `manualLinkFollowsAutoSetting`,
`automaticReviewSettings`, and `automaticReviewCancelStopsBeforeNextPair` cases
passed with Qt 6.12.0. They cover the setting off/on, cancelling before the second
link, restored Review choices, retry, grouped Undo and the additional-pair summary.
`import.follow_link`, `import.follow_link_faults`, `import.automatic_review`,
`import.review_sheet`, `import.adjacent_day`, `storage.action_redo`, and
`storage.action_redo_faults` passed.
The new fault sweep covers simulated disk-full/I/O failures, exact retries,
reopen recovery, and process exits before/after commit, checking both links,
split totals and the Undo head together. Functional cases cover 2:2 resolution,
3:3 and 2:3 ambiguity, unrelated groups, competing unused splits, one-step
Undo/Redo and individual Unmerge. The follow-on and automatic-review suites also
passed address and undefined-behavior sanitizers with leak detection disabled.
Changed-document relative links and diff whitespace checks passed. No full
repository suite, private-file reproduction or other-platform run is claimed.

## Automatic links for unique split pairs — 2026-11-16

Automatic Link now selects exactly one eligible allocation pair, including when
one or both transactions have other splits. The plan carries stable allocation
identities and the save rechecks their uniqueness, categories, currency, exact
opposite amounts, dates and occupied endpoints. Other splits remain unchanged.
Multiple eligible pairs stay manual. Same-day priority, the seven-day transfer
window, bulk save, import Undo and individual Unmerge remain. No schema change.
Manual work is MAN-062; see [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).

The rules-enabled macOS arm64 Release app was rebuilt and its offscreen smoke
launch passed. `import.automatic_review` and `import.review_sheet` passed.
Automatic matching also passed address and undefined-behavior sanitizers with
leak detection disabled. The offscreen `automaticReviewSettings` and
`automaticReviewCancelStopsBeforeNextPair` cases passed with Qt 6.12.0.
Synthetic split cases cover generated-counterpart replacement, splits on both
sides, mixed signs with an adjacent-day match, ambiguous allocations, category
exclusions, wrong allocation identity, cancellation, unchanged sibling fields,
Unmerge, Undo of Unmerge and import Undo. `import.automatic_review_faults`
also passed with a bulk split-link fixture, verifying unchanged sibling splits
through simulated disk-full/I/O write failures, retries, and process exits on
both sides of durable commit. `import.adjacent_day`, changed-document relative
links and diff whitespace checks passed. No private-file reproduction, full
repository suite or other-platform run is claimed.

## Automatic matching date priority — 2026-11-15

Automatic Review plans mutually unique same-day pairs first, reserving both
endpoints, then examines the remaining candidates once. Adjacent-day duplicate
suggestions can merge when the original/current date and existing survivor
checks allow it. Transfer candidates retain the seven-day window. Competing
remaining candidates stay manual. Both passes still use one bulk save, import
Undo, and individual Unmerge. No schema change. Manual work is MAN-061.

The rules-enabled macOS arm64 Release app was rebuilt and its offscreen smoke
launch passed. The matching, Review, and adjacent-day storage checks passed;
the automatic matching and adjacent-day suites also passed with address and
undefined-behavior sanitizers (leak detection disabled). The offscreen
`automaticReviewSettings` and `automaticReviewCancelStopsBeforeNextPair` cases
passed with Qt 6.12.0. Synthetic cases cover date priority for Merge and Link,
ambiguity in both directions, seven/eight-day transfer boundaries, cancellation,
exact balances, Unmerge availability, and adjacent-day import Undo.
`import.automatic_review_faults` passed, including the new two-pass bulk-save
case: simulated disk-full/I/O write failures, successful retries, and process
exits before and after durable commit preserve all-or-nothing pairs and totals.
Changed-document relative links and diff whitespace checks passed.
No private-file reproduction, full repository suite, or other-platform run is
claimed.

## Automatic matching with the import — 2026-11-15

Automatic Merge and Link from an import are one save. Review is built once.
Each pair is rechecked, then written in that save. The file is synced once.
Those pairs are not Undo steps. Undo of the import removes them. Unmerge still
breaks one pair. Cancel or a failure saves none of the automatic pairs. The
import stays. Manual work is queued as MAN-060.

On macOS arm64 with Qt 6.12.0, `import.automatic_review` passed. It covers two
pairs saved with the import, Unmerge of one pair, Undo of that Unmerge, and
Undo of the import removing the pairs. The same checks passed for one automatic
link. The offscreen cases `automaticReviewSettings` and
`automaticReviewCancelStopsBeforeNextPair` passed. The rules-enabled Release
`build/SpentIt` was relinked. The per-pair fault sweep was not rerun. Sanitizers,
other platforms, and the full repository suite were not run.

## Still to do

- Linux `python3 tools/package.py` produced
  `dist/SpentIt-0.1.0-linux-x64-setup.run` (60566028 bytes) on 2026-11-11 in WSL Ubuntu 24.04.5
  x86_64, with the Qt 6.8.3 `gcc_64` prefix. The staged app and the installed
  copy each created a 942080-byte file with development libraries hidden. The
  installer was then uninstalled from an empty home and left a saved file in
  place. It was not launched from an application menu, and aarch64 was not
  packaged. See [packaging](../packaging/README.md).
- Windows `python tools/package.py` produced
  `dist/SpentIt-0.1.0-windows-x64-setup.exe` (44881059 bytes) on 2026-11-11
  with MSVC 19.44.35229.0. The staged app created a 942080-byte file with
  development tools hidden from `PATH`. The installer was not run, so the
  Start menu entry, desktop icon, and `.spentit` association were not
  exercised. `--sign` was not used. `launchDocumentArgument` and
  `launchDocument` passed: 6 tests, 0 failed, offscreen Qt 6.8.3. A
  password-protected launch stayed on the welcome screen and asked for the
  password. The remembered file was left unchanged. SQLCipher logged an HMAC
  failure while that open was refused.
- macOS uses the same command. `--sign` with a notary keychain profile
  produces a Developer ID DMG aimed at macOS 14.4. A Mac running 14.4 was not
  used. Publication is not approved. `./build/SpentIt` stays the development
  build.
- The release schema is version 1 and the application version is 1.0.1. The
  installers recorded above were built as 0.1.0 before the 1.0.0 stamp.
  Rebuild them before publication. See [release schema](release-schema.md).

## Undo/Redo verification — 2026-11-14

The rules-enabled Release app at `build/SpentIt` was rebuilt. In
`build/import-ui-rules-release`, these passed: `storage.action_repository`,
`storage.action_redo`, `storage.action_redo_faults`,
`import.manual_transactions`, `import.action_undo_ui`, `import.committed_ui`,
`import.linear_redo_ui`, `category.assignments`, `category.history`, and
`category.validation`. The Redo fault sweep covers disk-full/I/O failures and
process exits at write boundaries. Historical category replay, corruption
refusal, close/reopen, identity remapping, retries, and version-1 retention are
covered by the Redo suite.

`storage.action_redo` also passed in `build/category-sanitized` with address and
undefined-behavior sanitizers. These are targeted checks, not a full repository
or cross-platform run. Manual updates were queued as MAN-054 and are now included; see [Manual](#manual).

## Automatic Review verification — 2026-11-14

On macOS arm64 with Qt 6.12.0, the rules-enabled Release application at
`build/SpentIt` was rebuilt. `import.automatic_review`, `import.review_sheet`,
`import.ledger_duplicates`, `import.review_policy`, `import.adjacent_day`,
`import.review_batch`, `storage.action_repository`, and `storage.action_redo`
passed. The new case checks mutually unique pairs, reverse competitors,
ambiguous survivors, adjacent dates, splits and amount differences, accepted
survivor Unmerge, ordinary Undo/Redo, and result marks across successful close
and full history pruning. Older manual requests keep their previous marks.

The offscreen Qt cases `automaticReviewSettings`, `ledgerSamePayment`,
`selectedReviewMerge`, and `commonUndoDirtyTargetAndTextUndo` passed. The new UI
case checks initially off and independently saved settings, placement above
Hidden transfer matches, import-driven Merge and Link, and the visible result
mark on an accepted survivor. `automaticReviewSettings` was rerun after the
completion-message update and passed.

`import.automatic_review_faults` passed 580 injected write/sync failures,
alternating disk-full and I/O errors: 181 for automatic Merge, 157 for automatic
Link, 77 for accepting result marks without new postings, and 165 for Unmerge
with an accepted survivor. All eight process interruptions immediately before
and after durable commit passed. Reopen, retry, exact balances, result marks,
and durable receipts were checked on both sides of each boundary.

`import.automatic_review` also passed in `build/category-sanitized` with address
and undefined-behavior sanitizers (`ASAN_OPTIONS=detect_leaks=0`). No leak-check,
full-suite, native interactive, Linux, or Windows verification is claimed.
Changed-document relative file links and `git diff --check` passed. Manual work was queued as MAN-055 during that implementation and is now
included; see [Manual](#manual).

## Limits that still bind a claim

- The owner-reported Rules crash is a native accessibility notification during
  a model update (`EXC_GUARD` / `GUARD_TYPE_MACH_PORT` through
  `QQuickAccessibleAttached::setName`). A bounded accessible-name mitigation is
  in place. The crash was not reproduced, and it is not proven fixed.
- Spoken screen-reader output, Linux, Windows, and a person clicking through
  Help were not established as release verification.
- Interactive native file-picker overwrite for export was not established.
  Export uses QSaveFile and the Qt file dialog.
- Do not describe the Rules accessibility crash, another platform, or a full
  repository run as passed without a new run.

## Manual

### Focused editing pass on 2026-11-18

Ten bundled Help topics were edited to reduce repetition around acceptance,
Undo, and recovery while keeping the household example. The guide now explains
both ways to correct an unresolved transfer destination, directs split links
to Review, and describes what Accept transactions clears.

On macOS arm64 with Qt 6.12.0, the rebuilt rules-enabled Release passed all
16 native Help reading cases, both continuous walkthroughs, and the offscreen
`./build/SpentIt --smoke` check. `help.prepare`, `help.capture_manual_images`,
`help.catalog`, relative links, and `git diff --check` passed. All 17 topic
IDs, 153 section IDs, and 46 pictures were preserved.

### MAN-077 through MAN-087

The second owner-authorized 2026-11-18 pass completed MAN-077 through MAN-087
in bundled Help. It covers unresolved transfer splits, the category picker
and Household starter category, waiting transfers and bank-match settings,
reciprocal transfer choices, opening amount correction, report navigation,
and file-wide and older-import Undo. Five pictures were refreshed and one
was added, bringing the bundle to 46 pictures. All 17 topic IDs and 153
section IDs were preserved. The [backlog](manual-update-backlog.md) is empty
and its next entry is MAN-088.

On macOS arm64 with Qt 6.12.0, all eleven native screenshot walkthroughs
passed and staged 46 synthetic pictures. The six installed pictures were
visually inspected. The import walkthrough also reopened an affected account
after Undo. Native Help passed all 16 chapter and reference reading cases
and both continuous walkthroughs. The rules-enabled Release app was rebuilt
and `./build/SpentIt --smoke` passed offscreen. `help.prepare`,
`help.capture_manual_images`, `help.catalog`, changed-document links,
and `git diff --check` passed.

### Earlier pass on 2026-11-18

The first owner-authorized 2026-11-18 pass completed MAN-057 through MAN-076 in
bundled Help. It covers password and file opening, statement import, ledger
and category editing, Review and automatic matching, account and category
deletion, report transaction edits, and PDF export. That pass brought the
manual to 45 pictures. Four were refreshed and five were added, including a PDF preview
and both deletion confirmations. All 17 topic IDs and existing section IDs
were preserved.

On macOS arm64 with Qt 6.12.0, all eleven native screenshot walkthroughs
passed and produced the full set of 45 synthetic pictures in staging. The
nine pictures used in this pass were visually inspected before installation.
Native Help passed all 16 chapter and reference reading cases and both
continuous walkthroughs. Review and Recovery passed again after the final
wording edits. The rules-enabled Release app was rebuilt and
`./build/SpentIt --smoke` passed offscreen. `help.prepare`,
`help.capture_manual_images`, `help.catalog`, relative links in the changed
documents, and `git diff --check` passed.

### Pass on 2026-11-14

The owner-authorized 2026-11-14 pass brings MAN-045 through MAN-056 into the
bundled manual, including both notes previously labelled MAN-045. The earlier
selective History guidance was replaced by the current file-wide Undo/Redo
instructions. The chapters and quick references now cover automatic Review,
Unmerge and result marks, linked-row deletion, repeated imports, column sizing,
Status explanations, all-account rule application, zero report rows, and the
report-ledger rule workflow.

`python3 tools/capture_manual_images.py --install` passed all eleven native
chapter walkthroughs and installed 40 synthetic pictures. All staged pictures
were visually inspected, including the new automatic-Merge and report-menu
examples. The rules-enabled Release application at `./build/SpentIt` and the
Help UI test were rebuilt on macOS arm64 with Qt 6.12.0.

`help.prepare`, `help.capture_manual_images`, and `help.catalog` passed in
`build/import-ui-rules-release`. Native `createOpenChapterReading` passed all
16 chapter/reference cases, checking rendered widths, section navigation and
enlarged pictures. Native `continuousManualWalkthrough` also passed both date/amount
formats (20 QtTest results including setup/cleanup across the two test functions).
All 17 topic IDs and existing section IDs were preserved. Changed-document
relative file links and `git diff --check` passed.
Completed items were removed from [manual-update-backlog.md](manual-update-backlog.md);
the next entry is MAN-057. Future feature work still logs manual changes there
unless the owner authorizes a manual pass.

This is Help and synthetic walkthrough verification, not a full repository,
Linux, Windows, or spoken screen-reader run.

## Report ledger menu verification — 2026-11-14

The rules-enabled Release app at `build/SpentIt` was rebuilt on macOS arm64
with Qt 6.12.0. Report ledger commands remain visible when unavailable, using
disabled text. Create rule… inspects one eligible selection without opening
the transaction editor. Save, apply and back retains the report transaction
session. A category emptied by edits or rules now displays a zero drill-down
total. Capturing Redo reads each recorded transaction in its owning account
and restores the prior account on success or failure.

The targeted offscreen Qt cases `reportContextCreatesRule` (light/dark
requests), `ledgerContextCreatesRuleWithoutOpeningEditor` (light/dark requests),
and `ledgerCreatedRuleReturnsAfterSaveAndApply` passed: 7 QtTest results including
setup/cleanup. The report case covers new and accepted transactions across two
accounts, visible disabled menu labels, source prefill, return navigation,
category removal from the result, and exact totals through Undo and Redo.
The offscreen menu screenshot was inspected; native dark appearance was not
established by that platform.

`import.reports_ui`, `storage.action_repository`, `storage.action_redo`, and
`storage.action_redo_faults` passed (4/4). Changed-document relative file links
and `git diff --check` passed. No full-suite, sanitizer, native interactive,
Linux, or Windows verification is claimed. Manual work was queued as MAN-056 during that implementation and is now
included; see [Manual](#manual).


## Large-ledger edit performance — 2026-11-16

The owner authorized performance changes with the release schema unchanged.
The implementation removes repeated category/account lookups, preserves exact
validated rows across unrelated writes, and avoids financial refresh work for a
rule-definition-only save or a transaction-memo-only edit. Other edits and rule
application still refresh import readiness. Review transfer candidates only
recompute when their actual eligibility inputs change; text exclusions remain
current. See [ledger editing](ledger-editing.md) and
[durable rules](durable-rules.md).

The rules-enabled Release application at `./build/SpentIt` was rebuilt on
macOS arm64 with Qt 6.12.0 and passed its offscreen startup smoke test.
The synthetic `spentit_import_benchmark 19000 --edits` workload uses two accounts,
15,200 rows in the selected account, default categories, encrypted durable storage,
and transaction dates spread over multiple years. It checks New and accepted
edits, rule saves, Save and apply, exact balance/membership, and reopen results.
Diagnostics are enabled for both before/after measurements; the final timing run
has no concurrent build or test workload. This is an offscreen synthetic workload,
not a measurement of an owner's private file or native window interaction.

Measured times in seconds (three memo edits per state, one of each other action):

| Operation | Before command | After command | After interaction-ready |
| --- | ---: | ---: | ---: |
| New memo | 1.18–1.21 | 0.29–0.32 | 0.30–0.32 |
| Accepted memo | 2.24–2.25 | 0.27–0.28 | 0.27–0.28 |
| New rule definition | 1.18 | 0.14 | 0.14 |
| Accepted rule definition | 2.25 | 0.10 | 0.10 |
| New Save and apply | 2.49 | 0.82 | 0.82 |
| Accepted Save and apply | 4.13 | 0.53 | 0.53 |
| New payee | Not measured | 0.56 | 0.57 |
| Accepted payee | Not measured | 0.32 | 0.32 |

The half-second target is met here for memo edits, accepted payee edits and
rule-definition saves. New payee edits and Save and apply are below one second
but remain above half a second. No universal latency guarantee is established.

Validation passed for editor/Review state and ledger caches; rule definitions,
application, schema, validation, account/all-account application and previews;
Undo/Redo; composed-action, rule and editor I/O/crash fault tests; and the new
memo-cache and rule-cache failure/retry tests. These include failures before and
after durable commit and same-connection reads after failure. The Rules UI suite
passed after correcting six old assertions that still expected confirmation for
ordinary Undo (the existing UI confirms import Undo only). No UI behavior was
changed by those assertion corrections. The new edit benchmark smoke passed.

ASan/UBSan checks passed for the editor/Review caches and rule application paths,
and for the reusable rule catalog's equivalence, cancellation and resource bounds
(leak detection disabled). The final catalog change was checked again with eight
Release tests and four sanitizer tests. No full repository suite, physical disk
exhaustion, native interactive timing, Linux or Windows verification is claimed.
Database version 1, encryption and durability settings are unchanged.

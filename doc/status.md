# Project status

Last updated: 2026-11-14

SpentIt is the local rules-enabled application. The release rules database is
version 1, and the application version is 1.0.1. A file whose number is not 1
is refused, and its bytes stay the same. There is no migrator. The rules-off
build still writes version 69 and is not the release format. The SpentIt
exchange file is JSON version 1.

Build and launch are in the [README](../README.md) and [building](building.md).
macOS arm64 with Qt 6.12.0 is the host used for product work. Linux and Windows
have development notes and are not release-ready. Do not describe a check as
passed unless you ran it.

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
  new transactions across the file. Undo last import rolls back one source and
  leaves accepted transactions in place.
- Review for duplicates, transfers, and import problems, plus ledger duplicate
  resolution. A payment shown twice in one account is marked there, and Merge
  joins it. Merged and linked rows stay marked until they are accepted.
  Same-calendar-date duplicate suggestions. Strict bank IDs when both sides
  have them. Optional automatic one-to-one Merge and Link after import and rules,
  with separate, initially off local settings. Ambiguous and choice-requiring
  pairs stay manual. Automatic results on accepted rows stay marked until the
  next Accept. See [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
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

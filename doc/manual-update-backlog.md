# Manual update backlog

Last updated: 2026-11-09

Owner direction: accumulate manual changes here while application work continues,
then update and verify the manual together in a later owner-authorized pass.
Logging an item does not authorize editing the manual or recapturing its pictures.

## How to maintain this log

- After a user-visible change, add or update an entry with its date, source
  contract or commit, affected chapters/pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples and cross-references. Merge overlapping work so pictures need only be
  recaptured once against the final UI.
- Keep stable entry IDs. Mark an entry complete only after the affected content
  and pictures have been checked in bundled Help; record verification and the
  completing commit. Retain completed entries as a record.
- During the eventual batch, follow the [Help contract](../features/013-help-system.md)
  and [handoff](help-prototype-handoff.md). Use synthetic examples consistent with
  the manual, preserve topic/section identifiers, and never use private records.

## Pending

### MAN-001 — Income categories and report group selection

Recorded: 2026-11-06. Status: **pending**.
Source: commit `90d4a73`; [Income grouping contract](../features/010-categories-and-selection.md#income-grouping--owner-go-2026-11-06)
and [refresh correction](status.md#income-checkbox-refresh-correction--2026-11-06).

Already updated in that commit:

- [Categories](../manual/categories.md) explains the Income checkbox and grouped
  sorting, including the visit-addition exception and unchanged refund treatment.
- [Reports](../manual/reports.md) explains Expenses/Income group selection,
  partial checks, search-hidden members, separate Uncategorized selection and
  preservation of saved report selections when a category's flag changes.

Remaining work:

- Recapture `manual/images/categories-workspace.png` with the Income column,
  checked income categories after expenses, and accounts last. Update its caption
  and alt text to describe the current layout.
- Recapture `manual/images/reports-editor.png` with the Expenses and Income group
  checkboxes. Include a useful example of a partial group selection if it fits
  the chapter's walkthrough; update caption and alt text accordingly.
- Audit category-picker pictures and examples in Categories, transaction editing
  and Rules for the new Expenses A–Z → Income A–Z → Accounts A–Z order. Recapture
  affected pictures, including `manual/images/categories-menu.png` if its example
  needs an income category to illustrate that order. Do not assume every picker
  screenshot needs replacing when its visible choices are unchanged.
- Add a brief Categories explanation of the pending partial indicator and
  **Saving…** label: the saved check and sorted position appear together after
  completion. Keep it distinct from Reports' partial check, which means only
  some categories in a group are selected. Do not promise a fixed save duration.
- Recheck the Reports walkthrough's Income example and the sample category flags
  so names, selections, screenshots and resulting totals agree. The Income flag
  affects organization/selection, not amount signs or report arithmetic.
- Verify the revised chapters, pictures, links and context targets in bundled
  Help after the batch. Record the actual checks here and in project status.

### MAN-002 — Import progress and date calendars

Recorded: 2026-11-06. Status: **pending**.
Source: [owner-approved import progress follow-up](../features/014-busy-feedback-and-progress.md#import-progress-dialogs-and-remaining-date-pickers--owner-go-2026-11-06).

No manual text or pictures were changed during implementation.

- Update [Importing](../manual/import-transactions.md) to describe mapping/category choices
  being replaced by progress, Cancel availability, Cancelling/Finishing, and
  restoration of still-pending choices after failure or cancellation. Distinguish
  category cancellation from undoing already prepared imported transactions.
- Recapture `manual/images/import-mapping.png` for the Opening date calendar;
  review `import-categories.png` and `import-results.png` captions/sequence.
  Add a synthetic progress picture only if useful to the eventual walkthrough.
- Check account opening-correction and manual-transfer chapters and their pictures
  for the added calendar buttons. Explain that date selection changes the field
  without saving. Audit other date-entry examples for consistent picker guidance.
- Verify revised chapters, pictures and context targets in bundled Help during
  the later owner-authorized manual pass.

## Completed

None yet.

### MAN-003 — Visible shutdown progress

Recorded: 2026-11-06. Status: **pending**.
Source: [shutdown contract](fast-startup.md), owner correction and Go.

No manual text or pictures changed during implementation.

- Update [Look after your work](../manual/backup-recovery.md) and [Accept transactions](../manual/accept-transactions.md):
  windows stay visible with the busy indicator and current checking/closing messages
  until shutdown completes. Describe the **Closing SpentIt…** modal, shared
  spinner/current phase, and absence of buttons or Escape/outside dismissal. Failed checking leaves the window visible for Return
  or Quit anyway; remove descriptions of hiding and restoring the window.
- Review `manual/images/accept-quit.png` and its caption for consistency; the
  decision prompt is unchanged. No shutdown-progress picture currently exists;
  add one with synthetic data only if useful to the walkthrough.
- Verify the eventual text and pictures in bundled Help.

### MAN-004 — Larger transaction selections

Recorded: 2026-11-06. Status: **pending**.
Source: [selection-size contract](search-ledger.md#selection-size--owner-go-2026-11-06).

No manual text or pictures changed during implementation.

- Review [Find transactions](../manual/find-transactions.md),
  [Add/correct transactions](../manual/add-correct-transactions.md),
  [Categories](../manual/categories.md), and Review's batch Merge guidance for
  selections above 4,096 rows. Describe one operation and group Undo; do not
  promise unlimited resources or a fixed processing time.
- Audit selection/context-menu and Review pictures/captions; no new control was
  added, so recapture only pictures whose accompanying example needs changing.
- Verify eventual chapter links and bundled Help in the owner-authorized pass.

### MAN-005 — Selection keyboard shortcuts

Recorded: 2026-11-06. Status: **pending**.
Source: [selection shortcut contract](search-ledger.md#selection-shortcuts--owner-go-2026-11-06).

No manual text or pictures changed during implementation.

- Update [Find transactions](../manual/find-transactions.md) and
  [Keyboard and mouse](../manual/keyboard-mouse.md) with Command+A / Command+Shift+A
  on macOS and Ctrl+A / Ctrl+Shift+A elsewhere. Explain that Select All includes
  all filtered transaction rows, including offscreen rows, excluding Opening
  Balance; Deselect clears the selection.
- State the focus distinction: transaction-list shortcuts select rows, while
  text-field shortcuts select text. Selection leaves unsaved editor text intact.
- Review selection illustrations/captions; no visible control was added, so no
  new screenshot is required unless the eventual example changes. Verify links
  and bundled Help during the later owner-authorized manual pass.

### MAN-006 — Delete linked selections

Recorded: 2026-11-06. Status: **pending**.
Source: [linked-deletion contract](ledger-editing.md#linked-selection-deletion-and-large-operation-performance--owner-go-2026-11-06).

No manual text or pictures changed during implementation.

- Update [Add/correct transactions](../manual/add-correct-transactions.md),
  [Review duplicates/transfers](../manual/review-duplicates-transfers.md), and relevant selection/Undo guidance:
  Delete selected only is the default, kept linked splits become Uncategorized,
  Delete linked entries too preserves unrelated counterpart splits, and the
  preview shows each account's exact balance change. Describe generated-draft
  disposal, cancellation, group Undo and unresolved import-match restrictions.
- Replace or add the deletion-confirmation picture using synthetic linked and
  split transactions. Review transfer and selection captions for obsolete manual
  unlink instructions. Capture both choice summaries if useful.
- Check eventual bundled Help targets and links during the owner-authorized pass.

### MAN-007 — Navigation availability and Settings cleanup

Recorded: 2026-11-06. Status: **pending**.
Source: owner Go for disabled Back/Forward appearance and removal of Settings'
Return to account button; [implementation record](status.md#navigation-and-settings-cleanup--2026-11-06).

No manual text or pictures changed during implementation.

- Review [Accounts](../manual/accounts.md) and
  [Keyboard and mouse](../manual/keyboard-mouse.md) navigation guidance: Back and
  Forward are dimmed and unavailable when there is no destination.
- Recapture Settings pictures `manual/images/recovery-backup.png` and
  `manual/images/recovery-existing.png` without Return to account. Review their
  captions in [Look after your work](../manual/backup-recovery.md); use an account
  in the sidebar or Back to leave Settings. Include the follow-up layout: file
  path directly below Current file, a small gap before Date format, matching
  section gaps, and no Close file button or close-before-opening note. Update
  the chapter's Settings → Close file instructions to File → Close file.
- Audit toolbar pictures for disabled navigation appearance and recapture only
  affected examples during the later owner-authorized manual pass.

### MAN-008 — Session Change History and close recovery

Recorded: 2026-11-07. Status: **pending owner-authorized manual pass**.
Source: [session-history contract](session-history-baseline.md) and
[implementation progress](status.md#session-history-correction--2026-11-07).

No manual text, captions or pictures have been changed.

- Update [Add/correct transactions](../manual/add-correct-transactions.md),
  [Accounts](../manual/accounts.md), [Categories](../manual/categories.md), and
  [Review duplicates/transfers](../manual/review-duplicates-transfers.md): ordinary
  history/Undo ends after successful File → Close file, switching files, or quit.
  Failed/cancelled close and crash recovery retain the recoverable session.
- Update [Create/open a file](../manual/create-open-file.md),
  [Look after your work](../manual/backup-recovery.md), and
  [Messages/problems](../manual/messages-problems.md) for close failure/retry and
  the final recovery workflow. Keep bundled Help topic/section IDs stable.
- Check [Accept transactions](../manual/accept-transactions.md) and
  [Import transactions](../manual/import-transactions.md): close does not accept
  pending transactions; originals and eligible Undo last import survive sessions.
- Review editing/history, acceptance, import-Undo and recovery pictures/captions,
  including `manual/images/import-undo.png`, `manual/images/accept-review.png`,
  and `manual/images/recovery-existing.png`. Capture the revised close-failure
  dialog if the final walkthrough uses it. Include the Settings **Check file**
  button and its result. Recapture only during
  the owner-authorized manual pass; avoid promising a fixed storage-saving percentage.
- Explain the already-saved retry message: refresh current transactions; an old
  request cannot execute twice or display expired values as its historical result.
- Explain normal open/close without a routine full audit, including edits and
  imports left unaccepted; missing cache approvals and ordinary rebuilds do not
  require a check. Explain recovery checks after an abnormal shutdown, full backup
  checks, and Settings **Check file** without ending Undo. Normal opening does not
  independently detect every change made while the application was closed. Check links
  and bundled
  Help during the owner-authorized manual pass.


### MAN-009 — Ledger duplicate review

Recorded: 2026-11-07. Status: **pending owner-authorized manual pass**.
Source: [ledger duplicate contract](review-sheet.md#ledger-duplicate-resolution--owner-go-2026-11-07).

No manual text, captions or pictures have been changed.

- Update [Review duplicates/transfers](../manual/review-duplicates-transfers.md)
  and [Add/correct transactions](../manual/add-correct-transactions.md) for rose
  markers, New/Accepted labels, direct candidate selection, Merge, pair-only
  Separate, None of these match, Show match/reveal hidden matches, Next possible
  duplicate, and Undo review. Explain automatic established-survivor selection
  and exceptional conflicting relationships.
- Update [Import transactions](../manual/import-transactions.md) and
  [Accept transactions](../manual/accept-transactions.md) to mention ledger review
  without changing the existing acceptance or matching policy. Include
  automatic duplicate discovery after opening/importing and Selected: n in the
  permanent ledger status area.
- Recapture affected ledger and Review pictures during the authorized manual pass:
  `manual/images/transactions-ledger.png`, `manual/images/transactions-review.png`,
  `manual/images/review-keep.png`, and `manual/images/accept-review.png`. Add a
  comparison with neighbouring transactions and a multiple-candidate example.

### MAN-010 — OFX statement validation

Date: 2026-11-07. Pending owner-authorized manual pass.
Contract: [statement header validation](../features/003-ofx-profile.md#statement-header-validation--owner-go-2026-11-07).

- Update `manual/import-transactions.md` to explain refusal of conflicting statement
  fields and unreadable ledger balances before account mapping, and obtaining a
  corrected export. Distinguish missing balances from unreadable balances.
- Review `manual/images/import-mapping.png` and `manual/images/import-destination.png`
  and their captions; valid-import screens are unchanged. Decide during the manual
  pass whether a synthetic error illustration is useful.
- No manual prose, screenshots or captions were changed in this implementation.

### MAN-011 — Explicit import openings and retired staging

Date: 2026-11-08. Pending owner-authorized manual pass.
Contract: [legacy staged-balance retirement](qif-account-imports.md#legacy-staged-balance-retirement--owner-go-2026-11-08).

- Audit `manual/import-transactions.md` and `manual/accounts.md` for stale
  instructions about choosing opening/closing meaning after import or publishing
  an opening on acceptance. Describe zero/reported-at-creation and later Correct
  opening; current desktop mapping behavior is unchanged.
- Review `manual/images/import-mapping.png` and `manual/images/accounts-opening.png`
  captions during the batch; no screenshots were changed or newly captured.
- Review experimental-format/recovery wording in `manual/backup-recovery.md` if
  applicable. Formats 66/65 reject earlier files unchanged, without migration.
- No manual prose or screenshot changes completed in this implementation.


### MAN-012 — Rule save history expires at successful close

Recorded: 2026-11-08. Status: **pending**.
Source: [ruleset-retention contract](../features/004-rules-and-explanations.md#ruleset-save-retention--owner-go-2026-11-08).

During the later manual pass, check Rules and file/recovery chapters for any promise
of complete past ruleset inspection or full historical save receipts after close.
Current rules and the definitions needed to explain retained transactions remain.
Retries of expired saves require refreshing current data. No workspace controls or
pictures changed; no new screenshots are required by this change alone. No manual
text has been updated.


### MAN-013 — About, version information and offline notices

Recorded: 2026-11-08. Status: **pending owner-authorized manual pass**.
Source: [About implementation Go](../features/007-licensing-and-distribution.md#about-design-and-implementation-go--2026-11-08).

No manual text or pictures changed during implementation.

- Update Where to find things, Keyboard and mouse, and Messages/problems with
  the About entry and Copy version information for reporting problems. macOS uses
  the application menu; Windows/Linux use Help. Do not claim platform testing
  beyond the recorded verification.
- Use the compact component-picker rows (2026-11-08 owner follow-up) in any About capture.
- Explain License and terms and offline Third-party software, Back/Close/Escape,
  and the absence of an extra description below the tagline. Copyright is
  © 2026 Stefan Wessels. Recheck wording after final licensing selection.
- Review existing menu pictures and decide whether an About screenshot improves
  the reference. Capture only in the later batch with synthetic data.
- Keep risk/backup guidance consistent with the approved notice without presenting
  a disclaimer as guaranteed legal immunity. Verify bundled Help links afterward.

### MAN-014 — Review workspace cleanup

Recorded: 2026-11-08. Status: **pending owner-authorized manual pass**.
Source: [Review cleanup](review-sheet.md#review-workspace-cleanup--owner-go-2026-11-08).

- Update Review, Accept transactions, Bring in transactions, Settings, and
  navigation/keyboard guidance for the one all-account acceptance footer, hidden
  unavailable controls, typed question sections and removal of Review Close.
- Replace Review empty/populated, acceptance warning, Undo import, transfer
  details and suggestion settings pictures. Undo import uses row-specific Undo…
  buttons and still previews the whole import's affected accounts.
- Explain Select all merges and Clear merge selection, ambiguous groups requiring
  choices, account selection in Transfers, and Add transfer revealing its form.
- Remove the read-only imported-category review instructions; retain import-time
  missing-category choices. Replace the old blocking-commit button with inline
  problems and distinguish overridable warnings from required fixes.
- Hidden transfer suggestions is in Settings and has Close. Document the contextual
  Hide suggestions action and explained Set up imported transfers confirmation.
- No manual text or screenshots were changed in this implementation. Synthetic
  verification captures are temporary and are not replacements for the manual.

MAN-014 follow-up (2026-11-08): replace the competing group/selected Merge
instructions with the single selection-aware action. Capture an ambiguous
four-row group with two checked rows and the two-row survivor dialog showing
full categories/splits and explicit unchanged-row count. No manual prose or
pictures updated during this correction.

### MAN-015 — Rules workspace usability

Recorded: 2026-11-08. Status: **pending owner-authorized manual pass**.
Source: [Rules usability](../features/004-rules-and-explanations.md#rules-workspace-usability--owner-go-2026-11-08).

- Update Use rules (chapter 9), Where to find things, keyboard/navigation and
  messages guidance for the empty state, conditional controls, selected-definition
  Save rule / Discard changes (Discard draft for new rules), and removed copies.
- Explain Delete rule… and its modal named Delete rule / Keep rule confirmation.
  No category/action summary or Deleted badge appears before confirmation. After
  deletion, open an existing new draft first, otherwise the nearest remaining rule
  or the empty state. This replaces staged Save removal guidance.
- Explain the account picker inside Rules, All transactions / New transactions
  only, Uncategorized eligibility, optional Preview and selected Apply rule /
  Save and apply rule. Keep definition saves distinct from transaction changes.
  Use “Preview or Apply Rule” plus the name, with a divider and larger gap after
  definition actions. Include inset row numbers and red category validation errors.
- Explain account-specific Undo application independent of the selected rule and
  Save all changes / Discard all changes / Cancel in the movable leave dialog.
- Document ledger Create rule… from one eligible selected transaction without
  opening its editor, and the transaction/split chooser with Cancel. Retain
  multiple-selection and original-field eligibility limits.
- Replace Rules empty, selected/new/edited, deletion confirmation, removed, preview
  and leave-dialog pictures in the later batch. Include an explicit target account
  and narrow layout. Update ledger context-menu and Rules entry-point pictures;
  show disabled text/arrows in both themes.
- No manual prose or screenshots changed in this implementation or documentation
  reconciliation; temporary synthetic verification captures are not replacements.


### MAN-016 — Reports workspace usability

Recorded: 2026-11-08. Status: **pending owner-authorized manual pass**.
Source: [Reports usability](../features/006-reporting-drill-down.md#reports-workspace-usability--owner-go-2026-11-08).

- Update Answer with reports, navigation and messages guidance for empty/search
  states, Continue new report, independent drafts and selected-only Save report.
- Explain Show report versus Save and show, conditional Discard changes/Discard
  draft, and Duplicate report copying current edits without saving its source.
- Replace staged deletion with the named Delete report / Cancel confirmation;
  explain remaining-new-draft selection and the absence of definition Undo.
- Capture empty, selected/edited/new, no-results, deletion and leave-dialog states,
  complete chart/preset labels, explicit checklist scope and result account names.
- Update leaving choices to Save all changes / Discard all changes / Cancel and
  show stable contextual Help and permanent status feedback.
- No manual prose or screenshots changed; synthetic verification images remain
  temporary.


MAN-016 follow-up (2026-11-08): replace Continue new report instructions with
stable, disabled New report above search. Capture Help at the bottom, headings
above action rows, separate Clear expenses/Clear income, stable draggable
checklist scrollbars and overwrite date entry. Search X clears each local search;
saving/selecting reports releases the old text-field focus. No manual text or
screenshots updated.


MAN-016 placement refinement (2026-11-08): capture Duplicate report / Delete
report below the name, and category actions ordered Clear expenses, Clear income,
Select all categories, keeping the Clear buttons stationary.

### MAN-017 — Unified transaction Undo and whole-import rollback

Recorded: 2026-11-08. Status: **implementation integrated; pending the later
owner-authorized manual pass**.
Source: [replacement Undo design](action-undo-design.md).
The generic row-capture prototype was rejected. Its historical measurements are
not evidence of replacement behavior. See the current
[implementation record](action-undo-implementation.md) for actual verification.

- Reconcile Add/correct transactions, Accept transactions,
  Import transactions, Review duplicates/transfers, Accounts, Categories, Use
  rules, Answer with reports, keyboard/navigation, recovery and messages guidance
  with the actual file-wide session chain. Supplement MAN-008; supersede older
  action-specific instructions only when the replacement is implemented.
- Explain whole-operation scope, newer-action blockers across accounts, reversible
  acceptance and one whole-import rollback. Accounts/categories and their permitted
  setup remain; a retained new account can still have its chosen opening balance.
- Document the final accepted scope for definitions/settings, Save and apply,
  dependency-protected deletion/archive, older-import removal and account Discard.
  The replacement uses empty-chain removal without Undo of the removal itself.
- Cover recovery of the chain, successful-close expiry, failed/cancelled close,
  import preparation/initial decisions, unsaved-buffer guards and no Redo. Describe
  undone-action details according to the later History UI design.
- Replace affected Undo/History, acceptance, import removal, Rules and account
  pictures during the later batch pass, after native verification. All listed
  manual text, captions and screenshots remain unchanged by this design session.

MAN-017 implementation follow-up (2026-11-09): common durable Undo, whole-action
confirmation, affected accounts/count, native text Undo, dirty Save revalidation,
reversible acceptance and atomic Save and apply are implemented. Whole-import Undo
retains accounts/categories/openings; older-import removal and Discard require an
empty chain. Update the transaction, review, import, account, rule, recovery and
history instructions together in the later manual pass. No manual prose or
screenshots changed in this implementation; verification uses synthetic fixtures.

### MAN-018 — File export/import and Backup menu

Recorded: 2026-11-09. Status: **pending the later owner-authorized manual pass**.
Source: [File exchange contract](file-exchange.md).

- Update File menu/navigation, import/export, Rules, Reports, Categories and
  backup/recovery chapters for the four commands and their unsaved-edit guards.
- Explain account/date selection, inclusive ranges, optional opening balances,
  optional ordinary categories, retained transfers, plaintext QIF and per-account
  currency choice on reimport. Partial-period openings precede the first selected date.
- Explain independent Rules/Reports/Categories JSON selection, stacked import
  sections, All toggles, namesake rules/reports, category merge/Income choices,
  missing category/account mapping, and importing definitions without applying rules.
- Distinguish portable reconstruction files from complete encrypted backups; File
  Backup and Settings Save backup are the same operation.
- Capture menu, QIF export, definition export/import and dependency/conflict dialogs
  during the later batch pass. No manual prose, captions or screenshots were changed
  in this implementation; synthetic verification screenshots are not manual assets.

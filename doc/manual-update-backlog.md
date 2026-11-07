# Manual update backlog

Last updated: 2026-11-07

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

# Manual update backlog

Last updated: 2026-11-06

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

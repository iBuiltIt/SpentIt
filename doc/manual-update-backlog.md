# Manual update backlog

Last updated: 2026-11-10

The bundled manual includes the user-visible behavior through the 2026-11-09
pass (MAN-001 through MAN-020). Later changes are the open items below.

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs, continuing after MAN-020. Remove an entry from this
  log once the affected content and pictures are in bundled Help.
- Follow [feature 013](../features/013-help-system.md). Use synthetic examples.
  Preserve topic and section identifiers. Never use private records.

## Recapturing pictures

An owner-authorized pass that changes pictures uses
[tools/capture_manual_images.py](../tools/capture_manual_images.py). Do not
write captures straight into `manual/images`. Do not point
`SPENTIT_GUIDE_CAPTURE` at `manual/images`.

1. Rebuild the help UI test so the pictures show the current interface:
   `cmake --build build/import-ui-rules-release --target spentit_help_ui_tests -j 2`
2. Run `python3 tools/capture_manual_images.py`. It runs the eleven chapter
   walkthroughs in `tests/ui/help_ui_tests.cpp`, writes pictures to a staging
   directory, and fails if a picture linked from `manual/` is missing or
   unexpected. It also refuses a test binary older than `src/ui` QML or the
   walkthrough source.
3. If a walkthrough fails, update that walkthrough so it still reaches the
   pictured screen, rebuild, and rerun. Leave the other chapters alone.
4. Look at the staged pictures. Update the chapter text, captions, and alt
   text where the screen or the example changed. The script does not edit
   those words.
5. After the staging run has passed and the pictures are the ones to publish,
   run `python3 tools/capture_manual_images.py --install`. That copies the
   linked pictures into `manual/images`.
6. Check the changed topics in bundled Help before removing the backlog entry.

## Open items

### MAN-021 — Review pair merge

Date: 2026-11-10. Contract: [review sheet](review-sheet.md), Merge pairs.

Chapter: [Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
sections "Is it the same purchase?" and "Merge several matches". Check
[outline](../manual/outline.md) where it describes merging several groups.

Text updated 2026-11-10 in Review duplicates and link transfers. Pictures
still outstanding:

- A question with more than two rows does not show **Include match**.
- Its Merge button reads **Merge N selected rows** when one selected row is
  kept automatically. It reads **Merge N selected rows…** only when the keep
  dialog follows. Until two rows are checked it still reads **Select rows to
  merge**.
- **Merge pairs** saves every two-row question marked **Include match** and
  every larger question with exactly two checked rows when the single
  accepted, manual, or linked row is the one to keep. Three or more checked
  rows are left as they are. One row left afterwards leaves Review and stays
  in the ledger. Two rows left become a two-row question with **Include
  match**. **Select all merges** still applies to ready two-row questions.

Pictures: `review-keep.png` is unchanged. Check `transactions-review.png` and
any other Review picture that shows **Include match** on a group of more than
two, or the old **Merge N groups** button. Recapture those in the manual pass.
Do not edit `manual/` for this entry until that pass.

### MAN-022 — One-day duplicate questions

Date: 2026-11-10. Contract: [review sheet](review-sheet.md), one-day review pass.

Chapter: [Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
section "Is it the same purchase?".

Text updated 2026-11-10 in Review duplicates and link transfers, Import
transactions, and Words. Pictures still outstanding:

- A new transaction older than the newest accepted transaction in that account
  can be asked about when the same amount is one day away and it did not match
  on the same day.
- The question can include the neighboring same-day pair and the transaction
  one day beyond that pair. It does not keep walking through every consecutive
  day.
- **Select all merges** does not include a question whose dates differ. Read
  the dates, then use **Merge 2 rows** or **Merge pairs**.
- A unique same-day match on the payee is still recognized without a question.

Pictures: none until a Review picture shows dates that differ. Do not edit
`manual/` for this entry until the manual pass.

### MAN-023 — Unavailable buttons

Date: 2026-11-10. A Button or ToolButton that cannot be used fades to 40%
opacity. It stays in place. No chapter text describes that appearance.

Pictures: recapture a picture that shows a button which cannot be used.
`import-mapping.png` shows Import after both accounts are chosen, so that
Import button stays available. Back and Forward in the window behind a dialog
may be unavailable. Check those during the manual pass. Do not edit `manual/`
for this entry until that pass.

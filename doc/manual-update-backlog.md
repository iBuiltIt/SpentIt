# Manual update backlog

Last updated: 2026-11-13

The bundled manual includes the user-visible behavior through the 2026-11-12
pass (MAN-001 through MAN-044). There are no open items.

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs, continuing after MAN-044. Remove an entry from this
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

### MAN-045 — Undo hidden while another import holds the transactions

- Date: 2026-11-12
- Contracts: [change history](change-history-handoff.md), [import undo](import-undo.md)
- `manual/add-correct-transactions.md`, section “Undo a saved change”: the
  sentence “Undo is on every open action” is outstanding. Say that a row with
  no Undo names the later action to undo first, and that a change no later
  action can make undoable is left off the page. Also say that undoing an
  earlier Accept leaves a later Accept in place.
- `manual/import-transactions.md`, “Undo a mistaken import”, and the Undo
  import row in `manual/accept-transactions.md`: outstanding text that an
  import another import still holds is left off **Undo import…** until that
  other import is undone. Accepted transactions already make an import
  ineligible; this is a separate case.
- Pictures: `change-history.png` and `undo-import.png` do not show this case.
  No recapture unless a later pass adds an example of the missing button.

### MAN-046 — Status message explanations

- Date: 2026-11-13
- The bottom **Details** button is now **Status**. It still shows the full
  status sentence. **Help with this problem** appears only for a sentence in
  [doc/status-message-help-draft.md](status-message-help-draft.md) and opens a
  dialog with that sentence and its explanation. It no longer opens this
  chapter.
- `manual/messages-problems.md`: the opening still says **Details** and that
  **Help with this problem** opens this chapter. Both are outstanding.
- `manual/where-to-find.md` and `manual/backup-recovery.md`: **Details** is
  outstanding. The name is **Status**.
- Pictures: no recapture unless a later pass shows the status line.

### MAN-047 — Ledger column width and cut-off text

- Date: 2026-11-13
- Contract: [transaction sheet](../features/005-transaction-sheet-navigation.md), Ledger
- `manual/find-transactions.md`, “Sort, scroll and come back”: outstanding
  text that dragging the right edge of a column heading changes that column’s
  width, the other columns stay as they are, and the horizontal scrollbar
  appears when the row no longer fits. Double-clicking that edge sizes the
  column to the longest text in the current list, wider or narrower. Also say that resting the pointer on
  cut-off text shows the full text, including a category or memo in an open
  split. The widths are remembered on this computer.
- `manual/reports.md`: one outstanding sentence that the transaction list
  behind a report amount uses the same column dragging, with its own widths.
- Pictures: recapture `transactions-ledger.png`. Each column edge in the
  heading now has a small shaded notch. The blue line appears only while the
  pointer is on that edge.

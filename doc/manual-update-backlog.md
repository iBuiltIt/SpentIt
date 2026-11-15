# Manual update backlog

Last updated: 2026-11-14

The bundled manual includes the user-visible behavior through the 2026-11-12
pass (MAN-001 through MAN-044). Open items run through MAN-054.

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

## Open items

### MAN-054 — Linear Undo/Redo and History

2026-11-14: See the [Undo/Redo contract](action-undo-design.md#version-1-linear-redo--2026-11-14).
Update the saved-change Undo chapter and relevant screenshots for
Edit → Undo/Redo, platform shortcuts, direct ordinary Undo, the file-wide History
stack viewer, and in-memory Redo lifetime. Explain that new saved changes clear
Redo, import Undo is not redoable and clears pending Redo, successful close clears
both stacks, and crash recovery retains Undo but loses Redo. Manual edits await
the owner's explicit manual-update request.

### MAN-053 — Zero category rows omitted from reports

- Date: 2026-11-14
- Contract: [feature 006](../features/006-reporting-drill-down.md), section "Money"
- `manual/reports.md`, "Read the answer": a category that is 0.00 in the main period and every comparison is left out of the table and the charts. A category that is 0.00 in one period and has an amount in another stays, with 0.00 in the empty period. A negative amount stays. The Total still includes allocations that cancel to 0.00. Double-click the Total to open them.
- Pictures: none. The comparison and pie pictures show Groceries and Household, which have amounts.

### MAN-052 — Rules Apply opens on all accounts

- Date: 2026-11-14
- Contract: [feature 004](../features/004-rules-and-explanations.md), sections "Matching and application" and "Workspace"
- `manual/rules.md`, "Preview, save and apply": the Account box opens on **All accounts**, every active account. Archived accounts stay unchanged. One account can be chosen for this pass. Choosing All accounts leaves the ledger account as it is. Choosing one account selects it and stays in Rules. Each visit to Rules opens on All accounts. **All transactions** and **New transactions only** still apply inside the chosen accounts. Each preview row names its account. Apply, Save and apply, and Save, apply and back use that choice. A saved rule already matches later imports in any account. Command+Z, and Ctrl+Z on Windows and Linux, undoes the whole pass and keeps the saved rule.
- The current text says to choose the Account, and that Transactions chooses entries in that account. The sentence that saved rules can match in any account stays true and should sit next to the new default.
- Pictures: recapture `manual/images/rules-preview.png` and its caption. The Account box and an Account column on the preview rows are not in that picture.

### MAN-051 — Unmerge on the ledger menu

- Date: 2026-11-14
- Contract: [review sheet](review-sheet.md), section "Ledger same payment and result marks"
- `manual/add-correct-transactions.md` or `manual/review-duplicates-transfers.md`: the ledger menu has **Unmerge**. It is unavailable on an ordinary transaction. On the one kept row of a single merge, it puts the transaction that left back on the ledger. A later edit of the kept row stays. On one linked row, it asks first. Unmerging a payment puts the removed rows back in the named accounts. The blue edges come back while the rows are still new. Merge joins that payment again. Review shows both lines, with Link and Not this transfer. After Accept, Unmerge stays available. The accepted payments stay accepted, the extra entries come back and are not accepted, and an accepted row has no blue edge. Unmerging a link that left both transactions separates them and leaves both in place. It remains available after the file is closed.
- Pictures: none.

- 2026-11-14 safety follow-up: restoring removed transfer entries is unavailable
  after a later relationship correction, including a date change. Undo that
  correction first. Acceptance alone still permits Unmerge. See the
  [Unmerge safety contract](review-sheet.md#unmerge-safety-and-retries--2026-11-14).

### MAN-050 — Merge and Link on Change history

- Date: 2026-11-14
- Contract: [change history](change-history-handoff.md), section "Rows", and [review sheet](review-sheet.md), section "Ledger same payment and result marks"
- `manual/add-correct-transactions.md`, "Change history": a keep-one merge and a same-payment merge are each a **Merge** row. The open row names the transaction that stayed and the one that left. Undo puts the one that left back. A same-payment merge also brings the extra entry back and drops the link. **Link to other account…**, and any link that leaves both transactions in place, is a **Link** row on both accounts. Undo removes the link and leaves both transactions. These rows stay until the file is closed.
- Pictures: none. The existing Change history picture does not show a Merge or Link row.

### MAN-049 — Delete leaves linked transactions

- Date: 2026-11-13
- Contract: [ledger editing](ledger-editing.md), section "Delete a selection"
- `manual/add-correct-transactions.md`, "Change a selection or delete entries": Delete removes the selected transactions that are not linked. The confirmation says how many will be deleted and how many linked transactions stay in this account. A selection that is only linked transactions deletes nothing and says they stay. Delete does not break the link or change the other account.
- The same chapter's "Take care with transfers" still says deleting a linked posting ends the link. That sentence is outstanding. Delete leaves the link as it is.
- Pictures: none. The bundled text still offers **Delete selected only** and **Delete linked entries too**.

### MAN-048 — Ledger same payment, Merge, and result marks

- Date: 2026-11-13
- Contract: [review sheet](review-sheet.md), section "Ledger same payment and result marks"
- `manual/find-transactions.md`, "See what needs attention": a blue edge means this payment is also shown by another row in the account. An indigo edge means the transaction was kept when another was merged into it. A green edge means it is linked to a transaction in another account, and the tooltip names that account. Those two edges remain until **Accept transactions**.
- `manual/review-duplicates-transfers.md`, "If there is no suitable question": the ledger command is **Merge** for two rows in the current account, including a payment shown twice. **Link to other account…** is for one selected transaction. Choose it, switch accounts, select the other transaction, and choose it again.
- The right-click menu no longer has **Resolve duplicate**. **Keep this pair separate**, **Not this transfer**, and **Show match** stay on the bar above the ledger.
- Pictures: `transactions-ledger.png` does not show the new edges. Recapture only if a later pass shows a marked row.

### MAN-045 — Repeated import of a file already accepted

- Date: 2026-11-13
- Contract: [committed changes](committed-changes.md), section "Dependencies and duplicate review"
- Chapters: [Review duplicates and link transfers](../manual/review-duplicates-transfers.md), [Import transactions](../manual/import-transactions.md), [Accept transactions](../manual/accept-transactions.md)
- A repeated import that still matches the original date, amount, payee, and bank id stays already imported. It does not appear in Review and is not a new transaction. Category, rule, memo, and amount edits of the matched transaction do not change that.
- When the matched transaction was removed, or those original facts no longer match, Review shows the transaction and asks whether it is already imported or a separate transaction. Accept anyway does not add it again.
- Text and pictures in the bundled manual do not say this yet.

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

### MAN-055 — Automatic one-to-one Merge and Link

- Date: 2026-11-14
- Contract: [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
- Explain the two initially off checkboxes above Hidden transfer matches in
  Settings, their per-file local scope, and the conservative eligibility rules.
- Explain that automatic Merge/Link leaves Accept transactions explicit. Include
  the import summary, marks on accepted survivors, Unmerge, and ordinary Undo.
- Marks survive reopen and clear on the next Accept, even when no new rows remain.
- Recapture Settings and an accepted survivor with a result mark during the next
  owner-authorized manual pass. No manual files changed in this implementation.

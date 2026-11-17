# Manual update backlog

Last updated: 2026-11-16

The bundled manual includes MAN-001 through MAN-056 after the owner-authorized
2026-11-14 pass. Both entries previously labelled MAN-045 are covered; the
older History instructions were reconciled with the later linear Undo/Redo
contract. Verification is recorded in [status](status.md#manual).

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs. The next entry is MAN-069. Remove an entry from this
  log once the affected content and pictures are in bundled Help.
- Follow [feature 013](../features/013-help-system.md). Use synthetic examples.
  Preserve topic and section identifiers. Never use private records.

## Open items

- MAN-068 (2026-11-16) — Line between accounts in the import mapping dialog.
  Contract: [QIF account imports](qif-account-imports.md). Chapter:
  [Import transactions](../manual/import-transactions.md), section "Match the
  accounts". The words still match. Recapture
  [import-mapping.png](../manual/images/import-mapping.png) so the line between
  the two accounts is visible. No caption change.

- MAN-067 (2026-11-16) — Category change on a created transfer entry. Contract:
  [categories](../features/010-categories-and-selection.md). Chapter:
  [Categories](../manual/categories.md), section "An account destination makes
  a transfer". On the entry created in the other account, choosing an ordinary
  category changes nothing. The message says to change the category on the
  original transaction in the other account, and that this entry will be
  removed. Text only.

- MAN-066 (2026-11-16) — Open a transaction from a visible split line.
  Contract: [transaction sheet](../features/005-transaction-sheet-navigation.md)
  and [ledger editing](ledger-editing.md). Chapters:
  [Find transactions](../manual/find-transactions.md) (“Look inside a row” and
  “Select without editing”) and the splits line in
  [Where to find things](../manual/where-to-find.md). While saved splits are
  visible, a double-click on the “Saved splits” line or on a split line opens
  the transaction. One click selects it. A right-click opens the ledger menu.
  The transfer arrow still opens the other account. Closing stays a
  double-click on the top line. Text only; no picture change.

- MAN-065 (2026-11-16) — Page Up and Page Down in the ledgers. Contract:
  [transaction sheet](../features/005-transaction-sheet-navigation.md). Chapters:
  [Keyboard and mouse](../manual/keyboard-mouse.md) and
  [Find transactions](../manual/find-transactions.md). With the account ledger
  or the report transaction list focused, Page Up and Page Down move one
  screen. Shift extends the selection. A text field keeps those keys. Text
  only.

- MAN-064 (2026-11-16) — Report list account sort and cross-account edits.
  Contract: [reporting](../features/006-reporting-drill-down.md). Chapter:
  [Reports](../manual/reports.md), section "Open the transactions behind an
  amount". The Account heading sorts that list. Change category, Rename payee,
  Set memo, and choosing an account can include several accounts in one save
  and one Undo. Each account choice gets its own opposite posting. A row
  already in the chosen account, or a source account in another currency,
  stops the save and changes nothing. A transaction with more than one split
  is still changed one at a time. Delete and Restore stay inside one account.
  Replace the sentence that group edits must concern one account at a time.
  Text only.

- MAN-063 (2026-11-16) — Follow-on links after a manual Link. Contract:
  [follow-on links](review-sheet.md#automatic-follow-on-links-after-a-manual-link--owner-go-2026-11-16).
  Chapters: [Duplicates and transfers](../manual/review-duplicates-transfers.md)
  and Undo guidance. With Auto Link enabled, manually linking one pair also
  resolves remaining unique pairs in the same group. Explain one save/Undo,
  exact Redo, unchanged acceptance, remaining ambiguity and cancellation.
  Describe the additional-pair summary. Text only; no new pictures required.

- MAN-062 (2026-11-16) — Automatic links for unique split pairs. Contract:
  [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
  Chapter: [Duplicates and transfers](../manual/review-duplicates-transfers.md).
  Explain that one uniquely eligible split pair can link automatically while
  other splits remain unchanged. Multiple matching splits stay manual. Date
  priority, bulk save and import Undo stay the same. Text only; no picture change.

- MAN-061 (2026-11-15) — Automatic matching date priority. Contract:
  [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
  Chapter: [Duplicates and transfers](../manual/review-duplicates-transfers.md).
  Explain same-day priority, then mutually unique adjacent-day duplicates;
  transfers retain seven days. Ambiguity stays manual and both passes share
  the import save and Undo. Text only; no picture change.

- MAN-057 (2026-11-15) — Password length. Contract:
  [001](../features/001-documents-storage-security.md). Chapters:
  [Create or open a file](../manual/create-open-file.md) (Choose password
  protection), [Backup and recovery](../manual/backup-recovery.md) (Change or
  remove the password), and [Messages and problems](../manual/messages-problems.md).
  A password is 1 to 64 characters. An empty password says “Enter a password.”
  A longer password says “Use a password of 1 to 64 characters.” Help for that
  sentence says the password is stored as UTF-8 text. No picture change.
- MAN-058 (2026-11-15) — Open a `.spentit` file from Finder. Contract:
  [007](../features/007-licensing-and-distribution.md). Chapter:
  [Create or open a file](../manual/create-open-file.md) (Open your file).
  Include macOS with the Windows and Linux sentence. Double-clicking a
  `.spentit` file starts SpentIt and opens it. A password-protected file stays
  on the welcome screen with that file selected and asks for the password.
  The file opened last time stays closed until this one opens. If SpentIt
  already has a file open, the window comes forward and that file stays open.
  No picture change. `create-file-unlock.png` already shows the password field.

- MAN-060 (2026-11-15) — Automatic matching with the import. Contract:
  [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).
  Chapter: [Duplicates and transfers](../manual/review-duplicates-transfers.md)
  (Let SpentIt handle straightforward matches). After an import, progress says
  it is checking duplicate and transfer suggestions, then merging or linking
  with a count. Those pairs are part of the import. Undo of the import removes
  them. Unmerge still breaks one link or one merge. Cancel, or a failure,
  saves none of the automatic pairs. The import stays, and the summary says to
  review the remaining matches. No picture change.

- MAN-059 (2026-11-15) — Save report as PDF. Contract:
  [Report PDF export](report-pdf.md). Describe the toolbar and File menu entry,
  refreshing a stale result, preview navigation, Readable pages, Letter/A4,
  orientation, Fit on one page and its small-text warning. Explain the default
  report filename beside the open file, Browse, replacement confirmation, and
  that exported PDFs are not password-protected. Include the separate-currency
  and repeated-main-period layout. Consider a preview-dialog picture in the
  next owner-authorized manual pass.

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

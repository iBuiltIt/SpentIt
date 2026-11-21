# Manual update backlog

Last updated: 2026-11-21

The bundled manual includes MAN-001 through MAN-056 after the owner-authorized
2026-11-14 pass. Both entries previously labelled MAN-045 are covered; the
older History instructions were reconciled with the later linear Undo/Redo
contract. Verification is recorded in [status](status.md#manual).

Two owner-authorized passes on 2026-11-18 completed MAN-057 through MAN-076
and MAN-077 through MAN-087. The 2026-11-19 pass completed MAN-088 through
MAN-099. The 2026-11-20 pass completed MAN-100 through MAN-103.
See [Manual](status.md#manual) for verification.

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs. The next entry is MAN-115. Remove an entry from this
  log once the affected content and pictures are in bundled Help.
- Follow [feature 013](../features/013-help-system.md). Use synthetic examples.
  Preserve topic and section identifiers. Never use private records.

## Open items

**MAN-114 — Scroll the category breakdown (2026-11-21).** In
[Answer questions with reports](../manual/reports.md), show the scrollbar
below the period headings for a report with more periods than fit in the
window. Explain that it moves the headings and amounts together and keeps
the Category column visible. Use a synthetic comparison report for the
picture. See [Charts and drill-down](../features/006-reporting-drill-down.md#charts-and-drill-down).

**MAN-113 — Reopened transfer matches (2026-11-21).** In
[Review duplicates and transfers](../manual/review-duplicates-transfers.md),
show how a later import can return an automatic link to Review, and how
Undoing that import restores the earlier link. Use a synthetic example with
one outgoing posting and two equal incoming postings. See
[reopened automatic links](review-sheet.md#reopen-an-automatic-link-after-a-later-import).

**MAN-112 — Open from Browse (2026-11-21).** Update
[Create or open your file](../manual/create-open-file.md#open-your-file)
to show that confirming a file in **Browse…** starts opening it. If asked for
a password, type it and press Return or choose **Open file**. Keep the separate
instructions for a path already shown on the welcome screen. The password
field and button shown in the existing picture still apply. See
[the welcome contract](../features/001-documents-storage-security.md#creation-and-welcome).

**MAN-111 — Apply Review decisions together (2026-11-21).** Update
[Review duplicates and transfers](../manual/review-duplicates-transfers.md)
to show **Apply decisions** and **Clear selection** in the header, the combined
counts, and one Undo for the selected merges and transfer decisions. Update the
unmatched-transfer section to call its account-specific action **Deselect all**.
Refresh the affected Review pictures and captions with synthetic transactions.
See [pending Review decisions](review-sheet.md#pending-transfer-decisions--owner-go-2026-11-17).

**MAN-110 — Auto merge into an accepted transfer (2026-11-21).** In
[Review duplicates and transfers](../manual/review-duplicates-transfers.md),
explain when an imported bank posting can merge into an accepted transfer and
how Unmerge restores the posting while preserving the link. Use synthetic
payee text in any example. See [automatic Review](review-sheet.md#automatic-one-to-one-review--owner-go-2026-11-14).

**MAN-109 — Wider bank-match account picker (2026-11-21).** Refresh the
[Current file settings picture](../manual/backup-recovery.md#choose-how-dates-look)
referenced by [Review duplicates and transfers](../manual/review-duplicates-transfers.md).
Show a longer synthetic name in the wider account picker beside **Add account**.
See [the account setting](review-sheet.md#accounts-that-dont-need-bank-matches--owner-go-2026-11-18).

**MAN-108 — Import problems and repeated payees (2026-11-21).** In
[Import transactions](../manual/import-transactions.md) and
[Review duplicates and transfers](../manual/review-duplicates-transfers.md),
document the repeated-payee warning, ledger inspection and editing, how saving
a payee correction clears the Review warning and Undo brings it back, and the
actions for repairable and excluded records. Update Review pictures and
captions that show the generic Correct button. See [Review](review-sheet.md#user-interaction).

**MAN-104 — Date sort and statement balances (2026-11-21).** In
[Find transactions](../manual/find-transactions.md), explain that Date reverses
the full posting sequence and each balance stays with its transaction. In
[Import transactions](../manual/import-transactions.md), explain how to compare
running balances by sorting Date in the statement's direction, and that
existing imports use the retained statement order.
If updating the ledger or search pictures, use synthetic same-day postings.
See [the ledger contract](search-ledger.md#search-contract).

**MAN-105 — Ledger menu and Create rule keyboard flow (2026-11-20).**
Update the menu order and Set payee label in
[Where to find things](../manual/where-to-find.md),
[Add and correct transactions](../manual/add-correct-transactions.md), and
[Reports](../manual/reports.md). Update the report ledger menu picture. In
[Rules](../manual/rules.md) and [Keyboard and mouse](../manual/keyboard-mouse.md),
describe the initial Category focus, the Tab shortcut to Save, apply and
back after a changed category, pressing Enter to activate it, and using Up
and Down immediately on return to the ledger. Document Option+R and Option+C
on macOS and Alt+R and Alt+C on Windows and Linux in the keyboard reference,
Rules, and [Categories](../manual/categories.md). See
[ledger editing](ledger-editing.md#ledger-rule-creation-and-menu-availability--owner-go-2026-11-08)
and [Rules behavior](../features/004-rules-and-explanations.md#workspace).

**MAN-106 — Ledger Search shortcuts (2026-11-20).** In
[Keyboard and mouse](../manual/keyboard-mouse.md) and
[Find transactions](../manual/find-transactions.md), document Option+F on
macOS and Alt+F on Windows and Linux for moving between Search and the
transaction list. Document Option+Shift+F or Alt+Shift+F for clearing Search
from either place. Both shortcuts work in the account ledger and report
transaction list. See [the search contract](search-ledger.md#search-contract).

**MAN-107 — Rule capacity (2026-11-20).** In [Rules](../manual/rules.md),
explain the 4,096-rule limit, the message at the limit, and that disabled rules
still occupy slots. In [Copy transactions or saved definitions](../manual/backup-recovery.md#copy-transactions-or-saved-definitions),
describe the destination limit when importing saved rules. See
[Rules](../features/004-rules-and-explanations.md#workspace).

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

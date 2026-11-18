# Manual update backlog

Last updated: 2026-11-18

The bundled manual includes MAN-001 through MAN-056 after the owner-authorized
2026-11-14 pass. Both entries previously labelled MAN-045 are covered; the
older History instructions were reconciled with the later linear Undo/Redo
contract. Verification is recorded in [status](status.md#manual).

The owner-authorized 2026-11-18 pass completed MAN-057 through MAN-076.
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
- Keep stable entry IDs. The next entry is MAN-087. Remove an entry from this
  log once the affected content and pictures are in bundled Help.
- Follow [feature 013](../features/013-help-system.md). Use synthetic examples.
  Preserve topic and section identifiers. Never use private records.

## Open items

### MAN-086 — Show reciprocal transfer choices once · 2026-11-18

In [Review duplicates and transfers](../manual/review-duplicates-transfers.md),
explain that a pair offered from both accounts appears on the outgoing card,
and that choosing one pair can check the only remaining exact pair. Keep the
example for a choice offered only from a deposit. Update pictures that show
both directions of the same choice. See [One question per transfer](review-sheet.md#one-question-per-transfer--owner-go-2026-11-12-and-2026-11-18).

### MAN-085 — Forward reopens report transactions · 2026-11-18

In [Answer questions with reports](../manual/reports.md#open-the-transactions-behind-an-amount),
explain that Forward reopens the same report amount with its current transactions.
See [Reporting and drill-down](../features/006-reporting-drill-down.md).

### MAN-084 — Use top-bar Back from report transactions · 2026-11-18

In [Answer questions with reports](../manual/reports.md#open-the-transactions-behind-an-amount),
direct readers to the top-bar **Back** to return to the results. Update the
report transactions and report transaction menu pictures to show the new header.
See [Reporting and drill-down](../features/006-reporting-drill-down.md).

### MAN-083 — Replace the opening amount directly · 2026-11-18

In [Set up your accounts](../manual/accounts.md#set-the-starting-balance),
explain that the entire opening amount is selected when the correction dialog
opens, so typing or pasting replaces it. The existing picture shows the dialog
after entering the new amount. See [opening balance editing](ledger-editing.md#opening-balance-editing--owner-go-2026-11-01).

### MAN-082 — Accounts that don’t need bank matches · 2026-11-18

In [Review duplicates and transfers](../manual/review-duplicates-transfers.md)
and [Accept transactions](../manual/accept-transactions.md), show how to
add or remove a generated-entry account in Settings. Explain that waiting-only
transfers in listed accounts are handled by Accept transactions and possible
matches still appear in Review. Update Settings pictures that show these
preferences. See [account preference](review-sheet.md#accounts-that-dont-need-bank-matches--owner-go-2026-11-18).

### MAN-081 — Select waiting transfers by account · 2026-11-18

In [Review duplicates and transfers](../manual/review-duplicates-transfers.md),
explain the unchecked **Accept without bank match** checkbox, selecting all
waiting entries by the account holding the generated transfers, and
**Apply decisions** with one Undo. Update any Review pictures that show the
old waiting-only controls. See [batch acceptance](review-sheet.md#batch-accept-without-a-bank-match--owner-go-2026-11-18).

### MAN-080 — Tab selects a category · 2026-11-18

In [Categories](../manual/categories.md) and
[Keyboard and mouse](../manual/keyboard-mouse.md), explain that Tab selects
the highlighted entry just like Enter in the right-click picker. See
[category selection](../features/010-categories-and-selection.md).

### MAN-079 — Change category ellipsis · 2026-11-18

Use **Change category…** when quoting the right-click label in
[Categories](../manual/categories.md), [Reports](../manual/reports.md),
[Where do I find…?](../manual/where-to-find.md), and
[Keyboard and mouse](../manual/keyboard-mouse.md). The category picture shows
the picker after it opens. See [categories and selection](../features/010-categories-and-selection.md).

### MAN-078 — Add Home: Household to default categories · 2026-11-18

In [Organize spending with categories](../manual/categories.md), consider
mentioning Home: Household as an example for alarm monitoring or household
supplies. The existing setup picture shows the default-categories checkbox and
needs no recapture for this change. See [starter categories](../features/010-categories-and-selection.md#starter-categories).

### MAN-077 — Correct an unresolved transfer split · 2026-11-18

In [Add or correct transactions](../manual/add-correct-transactions.md) and
[Review duplicates and transfers](../manual/review-duplicates-transfers.md),
explain that the inline category field can replace an unresolved imported
transfer destination with an ordinary category or Uncategorized. Cover the
account ledger and report transaction list. Existing pictures show ordinary
split editing and linked transfers. See [ledger editing](ledger-editing.md#category-completion--owner-go-2026-11-01).

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

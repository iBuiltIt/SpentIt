# Manual update backlog

Last updated: 2026-11-12

The bundled manual includes the user-visible behavior through the 2026-11-11
pass (MAN-001 through MAN-029). Later changes are the open items below.

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs, continuing after MAN-029. Remove an entry from this
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

### MAN-030 — Open a SpentIt file from the desktop

Logged: 2026-11-11. Contract:
[feature 007](../features/007-licensing-and-distribution.md) and
[packaging](../packaging/README.md).

The Windows installer and the Linux installer register `.spentit`. Opening
that file starts SpentIt and opens it. A password-protected file still asks
for the password. The previously remembered file is left closed. Install,
upgrade, and uninstall do not change a file saved in Documents.

Chapter: [create or open a file](../manual/create-open-file.md). Add that
opening the file itself opens it in SpentIt. Keep the Browse steps. No new
picture. The installers are outside the app and do not need a Help picture.

### MAN-031 — Missing transfer destination

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md).

A transfer that names an account which is not in the file is listed beside
**Accept transactions** as **Warnings [n]**, with one line for the account,
date, payee, and destination. Choosing the line opens that ledger row. The
sidebar and the Review heading read **Review (n)** and add **[w]** when
warnings remain. Accepting still waits until that account exists and the
transfer is linked. There is no Acknowledge action.

Chapters: [import transactions](../manual/import-transactions.md) (the
unresolved-transfer steps that say to choose **Correct**), and
[accept transactions](../manual/accept-transactions.md) (the Review heading
and the warning that must be fixed). Recapture
[accept-ready](../manual/images/accept-ready.png): the heading now reads
**Review (0)** when nothing is open. No new picture for the warning list.

### MAN-032 — One question per transfer

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md).

A payment that appears in two imported files is one Review question. The
question starts from the withdrawal, and the deposit is the other line.
**Review (n)** counts that question once. A different payment stays its own
question.

Chapter: [Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
section "Is it a payment between accounts?". Add that one payment is one
question, shown from the withdrawal. No new picture. The existing link
picture already shows one question.

### MAN-033 — Unmatched transfer count

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md).

Review no longer lists “This transfer has not been matched to an imported
transaction. — Can accept anyway” above the questions. **Accept transactions?**
instead shows one line per account, “Account. There is 1 unmatched transfer
transaction.” or “Account. There are N unmatched transfer transactions.”
**Accept anyway** and **Return to review** stay. A problem that must be fixed
stays on its own line.

Chapter: [Accept transactions](../manual/accept-transactions.md), section
"When Review still has questions". Add the per-account count. No new picture.
The current Accept dialog picture shows a record that must be repaired.

### MAN-034 — Link card buttons

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md).

On a Possible transfers card, the button beside **Link** reads **Not this
transfer**. It rejects that pairing. Both imported lines stay, and neither
becomes ordinary spending. **Accept without bank match** is absent when the
other imported line is already on the card. It remains on a card that shows
only the transfer.

Chapter: [Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
section "Is it a payment between accounts?". Replace **Not a transfer** with
**Not this transfer**. In "If SpentIt created the other side", say that
**Accept without bank match** is not offered when the other imported line is
already on the card. Recapture
[review-link](../manual/images/review-link.png). That picture shows **Not a
transfer**.

### MAN-035 — Transfers panel removed

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md).

Review no longer has **Transfers…** or the panel it opened. That panel was
where a person added a transfer, read transfer history, and worked on the
transfer boxes. Ledger **Link** still joins two existing transactions. Cmd+Z
undoes a mistaken Link while it is the newest saved action. **Set up imported
transfers…** stays. An Import problems line that does not name a transaction
opens that account’s ledger.

Chapter: [Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
section "If there is no suitable question". Remove the sentence that says
**Transfers…** is where you add a transfer yourself. Recapture
[review-keep](../manual/images/review-keep.png) and
[review-link](../manual/images/review-link.png). Both headers show
**Transfers…**. Recapture review-link once; MAN-034 also needs that picture.

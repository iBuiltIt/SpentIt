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

### MAN-036 — Starter income categories

Logged: 2026-11-12. Contract:
[categories and selection](../features/010-categories-and-selection.md).

A new file with **Use default categories** checked starts these with Income
checked: Income: Business / Self-Employment, Income: Investment & Interest,
Income: Other, Income: Pension & Retirement, and Income: Salary & Wages. They
appear in the Income group. Taxes: Income is an expense. Every other starter
category, and a category the person adds, starts unchecked. An existing file
keeps the checks it already has.

Chapter: [Categories](../manual/categories.md), section "Keep your category
list useful". The sentence that a new category starts unchecked still describes
a category the person adds. Add the five starter names. No new picture.

### MAN-037 — Rules list and editor layout

Logged: 2026-11-12. Contract:
[rules and explanations](../features/004-rules-and-explanations.md).

The Rules list keeps stable columns: **Enabled**, **Name**, **Type**,
**Category**, and **Order**. The pattern is not on the row. **Up** and
**Down** keep their places when a button is absent. In the editor, each
label is on the same line as its field. The pattern label is **Pattern**.
Payee, memo, and category share one field edge.

Chapters: [Use rules](../manual/rules.md), the Market rule steps and
"Common pattern pieces", which name **Pattern (regular expression)**. Also
[Search examples](../manual/search-examples.md), section "Common pattern
pieces". Recapture [rules-editor](../manual/images/rules-editor.png) and
[rules-preview](../manual/images/rules-preview.png). Both show the old
label and the stacked fields. The editor picture also shows the old list row.

### MAN-038 — Newest-first date order

Logged: 2026-11-12. Contract: [search and ledger](search-ledger.md).

When **Date** is newest first, the ledger is the exact reverse of oldest
first, including two postings on the same day. Each row keeps the balance
after that posting.

Chapters: [Find transactions](../manual/find-transactions.md), section
"Sort, scroll and come back", and [Keyboard and mouse](../manual/keyboard-mouse.md),
section "Find and move around". Those sentences already say a second click
reverses the order and that balance values stay with their dates. No new
picture. The ledger pictures use different dates, or oldest first.

### MAN-039 — Newest-action Undo buttons removed

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md),
[ledger editing](ledger-editing.md), and [change history](change-history-handoff.md).

The ledger no longer shows an Undo button beside search, **Undo category
change**, or **Undo review**. Review no longer shows that same newest-action
button. Cmd+Z, and Ctrl+Z on Windows and Linux, still undoes the newest saved
action. Undo on an open Change history row stays. The duplicate line above
the ledger no longer appears only because a category change can be undone.

Chapters: [Keyboard and mouse](../manual/keyboard-mouse.md), section "Find and
move around"; [Where to find it](../manual/where-to-find.md), section "Undo
and problems"; [Categories](../manual/categories.md), the paragraph under the
split picture; [Find transactions](../manual/find-transactions.md), section
"See what needs attention"; [Add and correct transactions](../manual/add-correct-transactions.md),
section "Undo a saved change"; [Import transactions](../manual/import-transactions.md),
section "Undo a mistaken import"; [Messages and problems](../manual/messages-problems.md),
the "Undo the current actions" message; and [Review duplicates and link
transfers](../manual/review-duplicates-transfers.md), section "Undo a mistaken
answer". Say that Cmd+Z or Ctrl+Z undoes the newest saved action. Keep **Undo
import…**, **Undo last import…**, and **Undo opening or currency…**.
MAN-042 removes **Undo application**.

Recapture [review-linked](../manual/images/review-linked.png). It shows
**Undo Review transactions…**, **Undo review**, and "Review possible
duplicates in this account." after a link, while Review is at zero.
Recapture [transactions-review](../manual/images/transactions-review.png).
Its header shows **Undo Import file…**. That picture also still shows
**Transfers…**, which MAN-035 already asks to remove. Recapture it once.

### MAN-040 — Current file settings

Logged: 2026-11-12. Contract: [ledger editing](ledger-editing.md) and
[review sheet](review-sheet.md).

Settings shows one heading, **Current file settings**, then the file path,
the date format, amount formatting, and **Hidden transfer matches…**.
**Password** stays its own section. The list dialog is titled **Hidden
transfer matches**, and the button inside it is **Hide matches…**. The two
sentences in that dialog still say "suggestions." Review still offers **Hide
suggestions…** on a transfer row, and **Don't suggest links** is unchanged.

Chapters: [Where to find it](../manual/where-to-find.md), "read **Current
file**", and [Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
"**Settings → Hidden transfer suggestions…**". The **Hide suggestions…**
sentence on a transfer row stays. Recapture
[settings](../manual/images/settings.png). It shows **File settings**,
**Hidden transfer suggestions…**, and **Current file**. The password fields
in that picture stay.

### MAN-041 — Save, apply and back

Logged: 2026-11-12. Contract: [rules](../features/004-rules-and-explanations.md)
and [ledger editing](ledger-editing.md).

A new rule started from the ledger **Create rule…** command shows **Save,
apply and back**. Choosing it saves the rule, applies it, and returns to the
ledger. **New rule** and **Copy into new rule** keep **Save and apply rule**
and stay in Rules. An existing rule keeps **Apply rule**, or **Save and apply
rule** when it has unsaved edits, and stays in Rules.

Chapter: [Use rules](../manual/rules.md), "Make a Market rule" and "Preview,
save and apply". The Market pictures are the **New rule** path and still show
**Save and apply rule**. No new picture.

### MAN-042 — Undo application removed from Rules

Logged: 2026-11-12. Contract: [review sheet](review-sheet.md) and
[rules](../features/004-rules-and-explanations.md).

Rules no longer shows **Undo application** or **Last rule application**.
Cmd+Z, and Ctrl+Z on Windows and Linux, still undoes a rule application
when that application is the newest saved action. The saved rule remains.

Chapters: [Use rules](../manual/rules.md), "Preview, save and apply", the
**Undo application** paragraph; and [Where to find it](../manual/where-to-find.md),
"Undo and problems", the clause that names **Undo application** in Rules.
[rules-preview](../manual/images/rules-preview.png) is taken before Apply and
does not show the button. No recapture.

### MAN-043 — Back to Reports removed

Logged: 2026-11-12. Contract:
[reporting](../features/006-reporting-drill-down.md).

The report result no longer shows **Back to Reports**. The top-bar **Back**
returns to the report list. **Refresh report** stays on the result. The
transaction list still shows **Back to report**.

Chapter: [Answer with reports](../manual/reports.md), "Choose a chart". Replace
**Back to Reports** with the top-bar **Back**. **Back to report** in "Open the
transactions behind an amount" stays. Recapture
[reports-comparison](../manual/images/reports-comparison.png) and
[reports-pie](../manual/images/reports-pie.png). Both show **Back to Reports**
beside **Refresh report**.
[reports-transactions](../manual/images/reports-transactions.png) keeps
**Back to report** and does not need a new capture for this change.

### MAN-044 — Help with reports removed

Logged: 2026-11-12. Contract:
[reporting](../features/006-reporting-drill-down.md).

Reports no longer shows **Help with reports**. The top-bar **Help** opens the
reports chapter. From the transaction list it opens the section for the
transactions behind an amount. A stale result still offers **Help with this
result**. The question before leaving Reports still has **Help**.

Chapter: [Answer with reports](../manual/reports.md) does not name the button.
Recapture [reports-editor](../manual/images/reports-editor.png). It shows
**Help with reports** under **Save report**. Recapture
[reports-comparison](../manual/images/reports-comparison.png),
[reports-pie](../manual/images/reports-pie.png), and
[reports-transactions](../manual/images/reports-transactions.png) once, together
with MAN-043. Each shows **Help with reports** at the bottom of the page.

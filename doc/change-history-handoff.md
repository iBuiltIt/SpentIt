# Change history workspace — owner decisions 2026-11-09

Status: **implemented 2026-11-09.** The owner gave Go for this layout. The
page reads the live action chain described below. Working experimental formats
stay **70 with rules / 69 without**. “What is wrong with the page today”
describes the page before this implementation. Manual prose and screenshots
were not changed; [MAN-020](manual-update-backlog.md#man-020--change-history-workspace)
is the later manual pass. Checks are in [status](status.md).

Read this before changing the Change history page. The Undo model stays the
[replacement design](action-undo-design.md). This page is how that stack is
shown. Do not resume the rejected generic row-capture experiment.

## Owner decisions

1. A selection rename, a bulk category change, and a rule application each
   appear as **one row for the whole operation**. The same rule applies to
   every other saved action on the stack: Set memo, restore, delete, one
   editor save, transfer assignment, a Review decision, Add transaction,
   Correct opening, Change currency, Accept transactions, and Import file.
2. The page is **not limited to accepted transactions**. It lists saved
   transaction changes whether those transactions are New or Accepted, and it
   marks each transaction with the state it has now. It also lists the import
   that brought them in and the acceptance that accepted them.
3. **Work done while importing does not get its own rows.** The imported
   transactions, categories and openings chosen for that import, transfer
   matching, and rules that run during the import are one Import file action
   from the user's point of view. Undo of that row is the existing whole-import
   rollback. A rule application the user starts later, after the import is
   finished, is its own Apply rules row.

## What the page is for

The user opens Change history to see what they saved in this account during
this visit, newest first, and to undo the latest action when they are looking
at it. If a newer action is on top, the page names that action so they know
what to undo first. They undo one whole action at a time. There is no Redo,
no skip, and no undo-to-here.

Closing the file successfully clears this list. The transactions stay as they
are. A failed close or a crash keeps the list. That session rule is unchanged.

## What is wrong with the page today

This section describes the page before the 2026-11-09 implementation.

The sidebar still reads the old accepted-transaction journal. A rename of four
accepted transactions appears as four Correction rows. Each row refuses its
own Undo because the rename was one action. The Review Undo button is shown
anyway, and a disabled button does nothing when clicked.

A rename made while the transactions were still New never appears. Accepting
them afterwards shows the acceptance, already using the new payee, and the
rename stays missing. A mixed selection shows only the accepted members. That
cutoff is the old query, not a rule that New edits matter less. The action
chain already records both.

The detail box is 150 pixels tall and prints the full transaction twice, so a
payee-only change looks unchanged once the payee line scrolls away. Refresh
reloads the newest page of that old journal. It does not reload an open card,
and it cannot bring back changes from before the file was closed. The page
does not name the account. An open card can belong to a different moment than
the list above it.

The ledger Undo button beside search, and Ctrl+Z, are the live stack. They
undo only the newest action in the file after a confirmation. Change history
shows that same stack. Undo on this page can restore an older action. It does
not add a second journal.

## Layout

Heading: **Change history**, then an account dropdown. It opens on the account
you came from and lists the same accounts as the sidebar. Choosing an account
stays on this page, reloads that account’s changes, and makes it the current
account. One line under the dropdown:
“Changes in this account before you close this file.” No instructional
paragraph. No Refresh.

Empty account: “No changes in &lt;Account&gt; before you close this file.” One
button: **Back to &lt;Account&gt;**, which returns to that ledger.

The list is newest first. Choosing a row opens it under the list. The open
row stays highlighted. There is no separate Inspect button. Switching accounts
reloads the list and closes the open row. **Older changes** appears only when
another page exists.

The ledger History button, and removal history, open this page for that
transaction’s account and select the newest action that includes it. If this
visit has none, show the account list and say this transaction has no changes
before you close the file.

## One open action

Under the list, show the label and the count. Name the accounts only when the
action also touched another account. Show the balance effect only when the
money changed. A payee-only change has no balance line.

Show only the fields that changed, as before → after, one line per change.
When every transaction changed the same way, those lines sit once under the
action. When a transaction changed differently, its lines sit under that
transaction. List each transaction on one line: date, payee, the category when
it has one split, amount, and **New**, **Accepted**, or **Removed**. A memo,
and each split when there is more than one, are further lines under that
transaction. Nothing is hidden behind a button. A rename done while New stays
on the list after acceptance; the mark follows the transaction’s current
state. Acceptance is its own row.

The selected transaction is highlighted. Opening an action selects the first
transaction still in the ledger, or the transaction History was opened from
when that one is still in the ledger. **Show in ledger** is one button on the
open action. It selects that transaction and returns to the ledger. A removed
transaction stays text. **Undo** sits beside **Show in ledger**.

Import file and Accept transactions do not offer a per-transaction Undo.
Opening an import names the accounts and the count. It does not list each
imported transaction, and it does not list the rules that ran during that
import. The user undoes the import as a whole.

## Undo

**Undo** sits beside **Show in ledger** on every open action. The button says
**Undo**. The ledger button and Ctrl+Z stay newest-only.

The list of what else must be undone is the later applied actions that wrote
a transaction this action wrote, then the later actions that wrote a
transaction those actions wrote, and so on. It includes the action that was
opened. It is read from the membership already stored with each action. Nothing
is written to build the list. An opening that wrote no transaction is not on
a payee list. Its own list is only itself. A currency change that recorded
those transactions is on the list. Acceptance after a rename includes those
transactions. An unrelated change stays applied and off the list.

When that list is the one action, reuse the existing confirmation. Its title
is **Undo &lt;that action&gt;?**. **Undo** restores that action. **Cancel** writes
nothing. This is the path even when the action is not the newest one in the
file.

When the list is longer, one modal asks once. The title is **To undo, these
changes also need to be undone**. The scrollable list is newest first. Each
line is the change the page already shows, such as **Payee: C3 → C4**. **Yes,
undo all** restores every action on the list, newest first, or none of them.
**Cancel** writes nothing. The restore uses the before-values already saved
with each change. The closure is computed again when Yes is chosen. A stale
page says to refresh the action and try again.

A member of a selection rename, bulk category change, or rule application
cannot be undone alone. Remove the always-visible Review Undo button and the
extra Review group Undo buttons.

An undone action leaves the list. This page is the stack for this visit, not
an archive of undone work.

While the application is busy, keep the controls present and unavailable.
An unsaved ledger edit still asks Save, Discard, or Stay before Undo. Saving
that edit becomes the new newest action.

## What belongs on the list

An action appears in this account when it includes at least one transaction
here, is an opening or currency change for this account, or is an import that
created transactions here. The row describes the whole action, including other
accounts it touched. The Undo confirmation uses that same whole-action scope.

Use the labels the Undo button already stores:

| Action | Row |
| --- | --- |
| Selection rename | Rename payee |
| Set memo | Set memo |
| Restore imported values | Restore imported values |
| Delete | Delete transactions |
| Category assignment, including a transfer assignment | Change category |
| One saved editor edit whose stored label is Edit transactions | Edit transactions |
| Rule application the user starts after import | Apply rules |
| Review Merge, Link, or the other saved Review decisions | Review transactions |
| Transfer change | Change transfer |
| Add transaction | Add transaction |
| Opening correction | Correct opening |
| Currency change | Change currency |
| Acceptance | Accept transactions |
| A finished import | Import file |

Correct opening and Change currency are included because either one can be
the action that has to be undone before an older transaction change. They are
not a new financial command.

A bulk category change is one Change category row even when the stored label
is still Edit transactions. The opened detail shows the category before and
after. Do not add a format change just to store a clearer label.

## What stays off the list

- Each transaction inserted by an import, and each rule result produced while
  that import was running. Storage already skips a separate Undo action for
  rule application while the import action is open (`import_action` is set).
  The page must not invent those rows. If a check finds a separate rule action
  that was published as part of an import, that is a defect in the existing
  Undo model. Report it. Do not list it as its own history row, and do not
  design a way to undo one rule result inside an import.
- Rule definitions, report definitions, category and account name edits,
  settings, and Help.
- Older-import removal and account Discard. Those require an empty stack and
  are not Undo steps.
- Actions already undone in this visit.
- Internal operation ids in headings.

Save and apply remains one Apply rules action. Undoing it restores the
transactions and keeps the saved rule.

## Example

Synthetic Chequing account. Four imported transactions, payee Cafe. No private
records.

1. **Import file · 4 transactions.** Rules that categorized them during the
   import do not appear.
2. **Rename payee · 4 transactions**, each marked New. The change is Cafe →
   Cafe North. Undo of this row, while it is the newest action, uses the
   single confirmation.
3. **Accept transactions.** The same four are now Accepted. The rename row
   remains, with the mark updated. Undo of the rename lists Accept
   transactions and then the rename. Yes, undo all returns the four to New
   and restores Cafe. Cancel leaves both actions applied.
4. A later **Apply rules** that the user runs from Rules is a new row. It is
   not folded into the import.

One rename of two New transactions and two Accepted transactions is one row.
Each transaction keeps its own New or Accepted mark. A newer edit in Savings
that does not share these transactions stays applied. It is not on this
Undo list.

## Behaviour that stays

- One newest-first file-wide action. The same head as the ledger, Review,
  Rules, and account Undo buttons.
- Selection edits stay atomic. The storage refusal that blocks undoing one
  member of a selection stays.
- Whole-import rollback keeps the accounts, categories, and openings that
  import was allowed to keep.
- Undoing acceptance returns those transactions to New.
- Definitions and settings do not clear the stack.
- Exact money. No matching or financial-policy change.
- Successful close expires the stack. Failed close and crash recovery keep it.
- No new journal and no format bump. Read the live action chain: applied
  actions still reachable from the head, newest first. `summaries()` also
  returns undone and preparing headers; the page does not show those. Member
  before/after text for a user’s own edit is already on the prior and result
  revisions (`versions` for New, entry revisions for Accepted). Import has no
  per-new-row edit journal; do not build one to fill this page.

If a required label, count, or member cannot be read from that chain, stop
and say so. Do not invent a format change or a second history table in order
to finish the page.

## Verification after Go

Use synthetic data only. Record the commands and the actual results in
[status](status.md). Do not claim this design session ran them.

- Rename four New transactions: one row, payee before → after. Undo of that
  row, when nothing later shares those transactions, uses the single
  confirmation and restores that row even if newer unrelated actions exist.
- A later edit of the same transactions opens the modal. The list is the later
  edit, then the rename. Yes, undo all restores both. Cancel writes nothing.
  Undoing only the later edit, from its own row, leaves the rename.
- Accept, then rename: both rows remain. The earlier rename stays visible and
  its transactions are marked Accepted.
- One rename of mixed New and Accepted transactions: one row, each transaction
  marked.
- An import that also creates transactions in another account: one Import file
  row, both accounts named. No per-transaction import rows. No Apply rules row
  for rules that ran during that import. A later user-started Apply rules is
  its own row, and undoing it keeps the saved rule.
- A bulk category change is one row.
- A payee-only change has no balance line. A change that moves money shows the balance effect.
- A newer action in another account that does not share these transactions
  stays off the Undo list. One that shares a transaction is on the list.
- A member of the selection cannot be undone alone.
- Show in ledger is one button on the open action and selects the highlighted transaction.
- Switching accounts does not keep the previous account’s open action.
- The dead Review Undo button is gone.
- Close and reopen: the list is empty and the payee change remains.
- A crash or a failed close keeps the list.

Update the Change history expectations in
`tests/ui/committed_ui_tests.cpp` to this presentation. Keep the storage
refusal that a selection member cannot be undone alone. The ledger Undo
confirmation for Rename payee stays.

Linux and Windows remain unverified unless those runs are actually made.

## Manual

Do not edit manual chapters or screenshots for this design.
[MAN-020](manual-update-backlog.md#man-020--change-history-workspace) records
the later pass. MAN-017’s note about the later History UI design points here.

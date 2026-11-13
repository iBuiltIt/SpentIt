# Change history

Last updated: 2026-11-12

The page reads the live file-wide Undo chain, newest first. One saved action
is one row. A successful close clears the list. Opening a file does not. A
failed close or a crash keeps it. The creation-time opening is not an Undo
action.

## Rows

The page lists new and accepted transaction changes, the import that brought
them in, and the acceptance that accepted them. Rules and other work during an
import stay inside that one Import row. A later rule application the user
starts is its own Apply rules row. Undo of an import is the existing
whole-import rollback. Undoing Apply rules restores the transactions and keeps
the saved rule.

A bulk category change is one Change category row. The stored label may remain
“Edit transactions”. The page says Change category.

The account under the heading is a dropdown. It opens on the account you came
from. Choosing another account stays on this page. The empty page has no Back
to account button. The window Back button leaves the page.

## An open action

Opening a row shows the label, the count, and one line per field that changed.
The account line appears only when the action also touched another account.
The balance line appears only when the money changed. Each transaction is one
line, with its category when it has one split. A memo and extra splits are
further lines. The selected transaction is highlighted.

**Show in ledger** and **Undo** sit on an open action that can be undone.
When another import still holds transactions this action would remove, and a
later action would release them, the row stays and names that action. If that
import is on another account, the row names the account to switch to. Undo
appears after it is undone. A row that cannot be undone, and that no later
action would make undoable, is left off the page. The same eligibility decides
**Undo import…** on Review. Undoing an earlier Accept leaves a later Accept in
place, and the account stays frozen while any acceptance remains. When nothing
later wrote one of that action’s transactions, the existing confirmation undoes
that action, even if a newer unrelated action is the head. When later actions
share a transaction, one dialog lists that closure, newest first, including the
action that was opened. **Yes, undo all** restores the whole list, newest
first, or none of it. **Cancel** writes nothing.

Cmd+Z / Ctrl+Z undoes only the newest action in the file. The ledger has no
Undo button beside search. The page has no Refresh, Inspect, Show full
transaction, Hide transaction, Redo, skip, or undo-to-here. Help opens
`help:add-correct-transactions#undo-a-saved-change`.

There is no second history table. A first amount repair, stored with one
repair row, is listed as that one transaction.

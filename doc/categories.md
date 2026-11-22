# Categories and selection

A category is an independently assignable name. It may contain one colon, shown
as `Stem: Name`. A second colon is rejected. `Auto: Gas` does not create `Auto`.
Renaming, archiving, or deleting one name does not change another that shares
its stem. Identity is case-insensitive. Display keeps the created capitalization
until an explicit rename.

Accounts appear in the category editor as read-only rows. Account names and
confirmed aliases share this namespace and identify transfers, including
unbracketed QIF split names. Use account commands to change an account.

## Income

Categories has an Income checkbox. A category the person adds, and a
category created from an import, starts unchecked. A new file that uses the
starter categories checks Income for Income: Business / Self-Employment,
Income: Investment & Interest, Income: Other, Income: Pension & Retirement,
and Income: Salary & Wages. Taxes: Income is an expense. Every other starter
category starts unchecked. An existing file keeps the checks it already has.
The flag does not follow the name and does not change transaction signs or
report arithmetic. Rename and archive keep it.

Default order is Expenses A–Z, Income A–Z, then Accounts A–Z. Uncategorized
stays first where it is offered. Workspace Z–A and Order added apply inside
each ordinary group for that visit. Accounts stay A–Z. A new row starts at the
bottom for the visit. Changing its Income flag puts it in the matching group
immediately.

Reports keeps explicit saved category selections. Its checklist has tri-state
Expenses and Income groups. A group action includes categories hidden by
search and does not change Uncategorized or the other group. Changing a flag
or adding a category does not change a saved report's selection.

## Editor and pickers

The editor is a compact table. New category adds a blank row. Enter or leaving
the field saves. Escape cancels. Invalid or duplicate names stay in the field
and use the bottom status line. Rename, Archive, Unarchive, and Delete unused
category are row actions. Delete requires no current transaction assignments
(including unresolved rows and archived accounts), no current rule targeting the
category (including disabled rules), and no saved-report selection. Past
assignments and superseded/deleted rules do not block deletion. Historical
identities and definitions remain internal; deletion does not erase import facts.
You can delete an unused ordinary category while bracketed QIF transfer
destinations are awaiting review.
If an active Undo or Redo would restore the category, ask to clear the file-wide
stacks and save that expiry with deletion atomically. Otherwise preserve both
stacks and delete immediately. Cancel and failed saves preserve the category and
history. Other row actions save immediately. Show archived lists archived rows
only.

Sort choices last for the visit, not as a saved preference. Selectors always
use the grouped alphabetical order above, sized to the current labels.

The ledger **Change category…** picker applies immediately to the selection
through the atomic assignment command, with one Undo. A report list can include
several accounts in that one save. Ordinary category changes
preserve an unresolved duplicate decision. A split transaction needs a split
choice. An ambiguous multi-split selection is refused. Keep the viewport.

### Searchable Change category

Choosing **Change category…** from the ledger context menu opens a compact popup
at that location, with **Filter categories…** focused above the scrollable list.
An empty filter shows the complete existing grouped order. Typing matches any
part of the displayed category or account name, ignoring case. Clearing the
field restores the full list; reopening starts with an empty filter.

The first eligible match is highlighted after filtering. Up/Down moves the
highlight, skips disabled destinations, and scrolls it into view while typing
remains available. Enter or Tab chooses the highlighted item. A mouse click
chooses the clicked item. Both close the picker. Escape or a click outside
dismisses it without assigning. No matches shows **No matches**, and Enter and
Tab leave the picker open. The current account
remains disabled. The existing split-choice step still follows where required.
The account ledger and report transaction list share this picker.

Choosing an account assigns a transfer through the existing transfer commands.
Choosing an ordinary category again ends the link, removes only a disposable
generated draft, and restores both sides together on Undo. That choice is made
on the original transaction. On the entry created for the transfer, the same
choice changes nothing and says to change the category on the original
transaction in the other account, which removes this entry. Current account
destinations are disabled in the menu. Rules still cannot create transfers.

## Starter categories

**Use default categories** starts checked and creates this list. Unchecked
creates an empty catalog. Names stay editable. Each line is one category. The
five Income names in the Income section above start checked. Taxes: Income and
the other names start unchecked. ATM & Cash Advance and Electronics & Devices
are expenses. A category added later starts unchecked. An existing file keeps
the categories it already has. This list is written only when the file is
created.

The starter names and flags are defined by [the storage catalog](../src/storage/category_store.inc) and checked by [the category tests](../tests/storage/category_tests.cpp).

Successful close keeps category history. Do not delete that history as a
cleanup.

## Allocation ownership

Stable allocation identities and original source-allocation associations stay
with the allocation. Reordering splits does not invent a new source association
from the display index. A new manual split has no source association. Explicit
manual edits, including blanks, stay manual. A caller cannot clear that
protection by relabeling ownership. A rule mark is stored only with a
rule-owned field.

Category history that is no longer referenced is part of the deferred retention
cleanup. Do not remove it without a separate Go.

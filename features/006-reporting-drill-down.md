# 006 — Reporting and drill-down

Status: **active**
Last updated: 2026-11-18

Reports is one sidebar entry below Rules. It lists saved definitions and edits
the selected one. Show opens the result. The top-bar Back returns from Report
transactions to the result, and from the result to the saved definitions.
Forward reopens each of those views in order. A drill-down remembers the report,
currency, period, and category or total. Returning recalculates its membership,
including an empty list if the category now has no contributing transactions.
If the period was changed or the currency or category is unavailable, the
application explains why the previous amount cannot reopen.
Opening a new report result or drill-down replaces Forward history from that point.
Refresh report recalculates the open result. There is no separate
report-creation dialog and no per-report sidebar entry.

## Money

Totals are exact sums of the included allocations, not whole-transaction
amounts. Drill-down shows those allocations and reconciles to the total.
Opening balances and transfer principal are excluded. Categorized fees are
included. Uncategorized is selected in a new report.

Net spending: outflows are positive, refunds reduce spending, and income is
negative. The Income flag does not change signs or arithmetic. Results use
current edited values and include new and accepted transactions. One result set
per currency. No combined grand total and no inferred conversion.

Rows are one per category that is not exactly zero in every shown period.
A category at zero in the main period and every comparison is omitted from
the table and the charts. Its allocations remain in the period total. A
negative amount stays. There is no inclusive stem subtotal. A checklist
selection of a stem can include matching `Stem:` categories; the user can turn
individuals off. Direct rows must not double-count.

Saved presets move with today. Custom dates stay fixed. Show the resolved
inclusive dates. Use calendar years and quarters, Last 6 complete months, and
February 28 when a comparison has no February 29. No timezone shift.

A period whose category totals are negative keeps its table. Bar can show those
amounts. Do not draw a pie from absolute values or drop the negative categories.
Bar scales are shared across periods inside one currency.

### Previous comparisons

**Previous calendar month** and **Previous year** show the full month or year
before the start of the period immediately above. As the main period, they
use today's month or year. **Same period previous year** is available for
comparisons and shifts both dates of the period above back one year. In a
consecutive chain, the original month and day carry through to each year.
February 29 becomes February 28 only in years without a leap day. A saved
Year to date comparison chain uses the current cutoff each time it runs.

Existing saved periods retain their prior date rules until a period edit is
saved. The old month and full-year presets use today, and the old same-period
preset uses the main period. Changing a preset or date, adding a comparison,
or removing one updates the report's Previous presets together. Other report
edits and duplicating a report retain its existing period rules.

## Charts and drill-down

Charts come before one striped table of the remaining categories. Grouped-period bars
are an additional saved choice. Pie is available when the period is suitable.
When a category table's period columns are wider than its pane, a horizontal
scrollbar appears directly below the period headings. The headings and amounts
scroll together, with the Category column pinned on the left. Chart scrolling
is independent of the table.

Drill-down reuses the ledger editor over the contributing transaction
identities, with the account and the included amount. Saved edits and Undo
refresh the report. Reopening uses current membership. Change category, Rename payee, Set memo, and choosing an account apply to the
whole selection in one save, including rows from several accounts. One Undo
reverses that selection. Choosing an account gives each transaction its own
opposite posting in that account. A row already in the chosen account, or a
source account in another currency, stops the save and changes nothing. A
transaction with more than one split is still changed one at a time. Delete
and Restore stay inside one account. Choosing an account from
Change category, or from the category field in the open transaction, shows
that account's name in the Category column on the transaction's top line.
The name and the transfer arrow are shown for every account in the list.
The name matches the category field, including while the transaction stays
open, and the transfer arrow points at that account. Undo restores the
previous category text. Merge, Link, and creation
stay in the account ledger. The Account heading sorts the list. Rows from one
account stay together, newest date first inside that account. Other
dataset changes mark the result stale until Refresh. Create rule… is available
from the context menu for one eligible transaction. Save, apply and back returns
to the report transactions and refreshes the result. Unavailable context-menu
commands stay visible with disabled text.

## Workspace

Empty Reports offers New report. One new draft at a time. The selected
definition owns Duplicate and Delete. Delete report… is a named confirmation
and saves only that removal. Unrelated drafts remain. After deletion, select
the existing new draft first.

Each row has one Show report or Save and show action. Leaving with unsaved
report edits offers Save all changes, Discard all changes, or Cancel, and that
question keeps Help. The page has no Help with reports button. Top-bar Help
opens the reports chapter. From the transaction list it opens the section for
the transactions behind an amount. A stale result offers Help with this result.
Account and category checklists name their scope. Expenses and
Income are separate tri-state groups. Clear expenses and Clear income clear
that whole group, including rows hidden by search, and do not change
Uncategorized or the other group.

Date fields use overwrite editing. Formatting does not replace the text on
every keystroke. The calendar writes the unsaved definition. Leaving the field
normalizes it.

## Save as PDF

Save as PDF… beside Refresh report and File → Save report as PDF… export the
visible result with a preview. Readable pages and the open file's folder are
the defaults. Paper, orientation and Fit on one page are selectable. Tables
paginate vertically and group wide comparisons, repeating the main period.
PDFs contain exact report amounts and separate currencies; they are not
password-protected. See the [PDF contract](../doc/report-pdf.md).

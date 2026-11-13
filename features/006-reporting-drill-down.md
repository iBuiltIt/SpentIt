# 006 — Reporting and drill-down

Status: **active**
Last updated: 2026-11-12

Reports is one sidebar entry below Rules. It lists saved definitions and edits
the selected one. Show opens the result. The top-bar Back returns to the list.
Refresh report recalculates the open result. The transaction list keeps Back
to report. There is no separate report-creation dialog and no per-report
sidebar entry.

## Money

Totals are exact sums of the included allocations, not whole-transaction
amounts. Drill-down shows those allocations and reconciles to the total.
Opening balances and transfer principal are excluded. Categorized fees are
included. Uncategorized is selected in a new report.

Net spending: outflows are positive, refunds reduce spending, and income is
negative. The Income flag does not change signs or arithmetic. Results use
current edited values and include new and accepted transactions. One result set
per currency. No combined grand total and no inferred conversion.

Rows are one per category, with no inclusive stem subtotal. A checklist
selection of a stem can include matching `Stem:` categories; the user can turn
individuals off. Direct rows must not double-count.

Saved presets move with today. Custom dates stay fixed. Show the resolved
inclusive dates. Use calendar years and quarters, Last 6 complete months, and
February 28 when a comparison has no February 29. No timezone shift.

A period whose category totals are negative keeps its table. Bar can show those
amounts. Do not draw a pie from absolute values or drop the negative categories.
Bar scales are shared across periods inside one currency.

## Charts and drill-down

Charts come before one full-length striped category table. Grouped-period bars
are an additional saved choice. Pie is available when the period is suitable.

Drill-down reuses the ledger editor over the contributing transaction
identities, with the account and the included amount. Saved edits and Undo
refresh the report. Reopening uses current membership. Group edits stay inside
one account. Merge, Link, and creation stay in the account ledger. Other
dataset changes mark the result stale until Refresh.

## Workspace

Empty Reports offers New report. One new draft at a time. The selected
definition owns Duplicate and Delete. Delete report… is a named confirmation
and saves only that removal. Unrelated drafts remain. After deletion, select
the existing new draft first.

Each row has one Show report or Save and show action. Leaving with unsaved
report edits offers Save all changes, Discard all changes, or Cancel. Help
stays available. Account and category checklists name their scope. Expenses and
Income are separate tri-state groups. Clear expenses and Clear income clear
that whole group, including rows hidden by search, and do not change
Uncategorized or the other group.

Date fields use overwrite editing. Formatting does not replace the text on
every keystroke. The calendar writes the unsaved definition. Leaving the field
normalizes it.

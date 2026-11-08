# 006 — Reporting and drill-down

The [Income grouping follow-up](010-categories-and-selection.md#income-grouping--owner-go-2026-11-06)
adds Expenses/Income checklist sections and tri-state group selection. It changes
selection convenience only; net-spending calculations and explicit saved category
IDs retain their existing behavior.


Status: **active**  
Last updated: 2026-11-08  
Implementation: initial report workspace, saved definitions, exact aggregation,
shared snapshot drill-down, separate Bar/Pie charts and reusable calendar are
implemented under owner Go (2026-11-04). The [handoff](../doc/reports-calendar-handoff.md)
records accepted behavior and implementation decisions; [status](../doc/status.md#reports-and-shared-calendar--2026-11-04)
records verification and platform limits. The feature remains active while the
remaining native/platform verification is outstanding.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded catalog and account transfer destinations are implemented. The initial category report scope is implemented.

- Report spending by time period, including last year, this year, and comparisons between them.
- Show categories ordered from most costly to least costly.
- Double-click a category to inspect the transactions included in that category and period.
- Retain other selected report criteria during drill-down rather than silently broadening the result.
- Opening balances are excluded from income/spending reports.
- One Reports sidebar entry below Rules opens a report list and selected editor, following the Rules workspace. Show opens a separate result view; Back returns. This supersedes individual saved-report sidebar entries and a creation dialog. See the [handoff](../doc/reports-calendar-handoff.md) for the settled chart and calendar interactions. Ledger Withdrawal/Deposit totals remain distinct from allocation-based category reports.

## Implemented design

Aggregate category allocations rather than whole transaction totals. For a transaction with a transport allocation and a fee allocation, each category receives only its relevant amount. Drill-down shows the relevant allocation amount alongside enough parent-transaction context to inspect it without double-counting it.

The definition and immutable report snapshot share explicit filters: date boundaries, account set, category/subcategory scope, currency, and committed state, with additional criteria only when supported. A result initially opens drill-down from one immutable snapshot. The editable
drill-down retains the opened transaction identities and refreshes totals after
saved edits; other dataset changes mark results stale until Refresh.

Financial semantics are now accepted, not proposals: use net spending (outflows
positive, refunds reduce spending, income negative), current edited values, new
and committed transactions, no opening balances or transfer principal, and
included categorized fees. Uncategorized is selected by default in new reports.
All selects current entries only; future entries do not silently broaden scope.

Saved presets move with the current date; Custom stays fixed. Display resolved
inclusive boundaries. Use calendar years/quarters, Last 6 complete months, and
February 28 for a missing comparison February 29. Keep financial dates free of
timezone shifts. See the handoff for the full accepted preset meanings.

Show one direct row per category, without inclusive subtotals, and separate
currency tables/chart sets without a combined grand total. Bar scales are shared
across periods within each currency. A period with negative category totals keeps
its table and explains that Bar can display its amounts; do not draw a pie using
absolute values or silently omit negative categories.

## Acceptance criteria

- Every displayed category total equals the sum of included allocations in its drill-down, exactly in the reporting currency.
- Splits never cause double counting of a transaction's whole amount.
- Date/account/category/currency criteria remain identical between totals and detail.
- Drafts, opening balances, and transfer principal follow explicit inclusion policies.
- Uncategorized transactions and refunds have tested, visible behavior.
- Boundary dates, leap years, independent colon-category rollups, and comparison periods have deterministic fixtures.
- Separate currencies are not silently added together; unsupported conversion is clearly reported.

## Verification limits and deferred scope

- The renderer uses the existing Qt Quick Canvas dependency; no new graph package
  is required. Optional Hide zero rows and chart-hover cross-highlighting remain
  unimplemented refinements. See status for native/platform verification limits.
- Additional payee/memo/note/amount filters, fiscal reporting, conversion,
  inclusive group subtotals, printing, exports and time-series charts are deferred.
- Printing, exports, conversion and other deferred additions require their own
  scoped authorization; the initial report/calendar implementation Go is complete.

## Category selection follow-up — accepted 2026-10-28

Categories are [independent assignable names](010-categories-and-selection.md#independent-categories--accepted-2026-10-28), optionally containing one colon. A future report category checklist may include both `Auto` and `Auto: Gas`; selecting Auto checks its matching `Auto:` categories, and the user can deselect any of them individually. A stem without a separately created category is not an assignable parent object.

Presentation settled on 2026-11-04: show direct category rows only in the first
version. Auto 1000, Gas 50 and Other 25 yield total 1075 without an additional
inclusive Auto subtotal. The checklist shortcut above remains; inclusive group
subtotals are deferred. Starter Income categories remain ordinary neutral
classifications, not rules for recognizing report income.

## Compact results and editable drill-down — owner Go, 2026-11-04

The [follow-up contract](../doc/reports-calendar-handoff.md#compact-results-and-editable-report-ledger--owner-go-2026-11-04)
supersedes the initial read-only detail cards and table-first, separate-bar-only
layout. Charts precede one full-length, striped category table. Grouped-period
bars are an additional saved choice. Drill-down reuses the ledger editor over a
fixed set of contributing transaction identities, with account and exact included
amount columns, contributing-split markers and automatic report refresh after
saved edits/Undo. Reopening uses current membership. Group edits retain the
existing one-account boundary; Merge/Link and creation remain in account ledgers.
Formats are 56/55; see status for verification.


## Reports workspace usability — owner Go, 2026-11-08

Accepted and implemented presentation follow-up. Empty Reports offers New report
without search or definition actions. Search filters the list honestly, reports
no matches, and retains the independent selected editor with an outside-results
notice. New and Unsaved changes identify draft state. One new draft is allowed;
New report keeps its label and is disabled while that draft exists. Duplicate
report is hidden until another new draft can be created. Duplicate copies the selected current definition, including
its unsaved edits, without saving the source.

The selected editor owns Duplicate report and Delete report… beside its heading.
Accounts/categories precede periods and Chart. Whole-list selection shortcuts name
their scope; group and parent-category selection semantics are unchanged. Chart
and preset selectors accommodate their full labels; narrow layouts stack sections.
Selected Save report / Discard changes appear only for edits; a new report uses
Discard draft. Each list row has one Show report / Save and show action. Showing
a changed definition explicitly advertises the selected-only save. No unsaved
preview or transaction-application mode is added.

Delete report… opens a named modal Delete report / Cancel confirmation, noting
unsaved edits when present. Confirmation persists only that deletion using saved
surrounding definitions; unrelated drafts survive. Failure retains the local
definitions. Success selects an existing new draft first, otherwise a nearby
remaining report or the empty state. This supersedes staged deletion; there is
no new definition Undo. Escape cancels.

Leaving lists changed report names and offers Save all changes / Discard all
changes / Cancel. Save-all remains atomic and validates all outstanding edits.
Help with reports remains visible independently of feedback. Local refusals use
the permanent error-colored status line. Busy controls remain disabled in place.
Results retain separate currency calculations and show their account scope;
drill-down retains the existing exact membership, editing protections and Undo.

Financial/report policies are unchanged. See [status](../doc/status.md) for
executed checks and limits. Manual updates are deferred under MAN-016.


### Interaction corrections — owner follow-up, 2026-11-08

New report is above Search reports, with a stable label and disabled state when
a new draft exists. Help with reports is at the workspace bottom. Report details
and account/category count headings sit above their action rows.

All three search clear buttons clear their own field. Save and report selection
release the previous editor focus. Account/category lists have explicit,
interactive scrollbars with reserved thumb space. Clear expenses and Clear
income replace Clear categories; each clears its entire group, including
search-hidden categories, without changing Uncategorized or the other group.
Existing group checkboxes and Select all categories remain.

Report dates use overwrite editing. While a date field has focus its raw text
and caret remain under the user's control; validation continues updating the
unsaved Custom range, but formatting does not replace text on each keystroke.
Calendar selection still updates only the unsaved definition. Normalization on
leaving the field and saved validation retain existing date policies.


Owner placement refinement (2026-11-08): Duplicate report / Delete report sit
below the name field. Category actions stay in Clear expenses, Clear income,
Select all categories order, so conditional Select all visibility does not shift
the Clear buttons. Account action order is unchanged.

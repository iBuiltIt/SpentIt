# 006 — Reporting and drill-down

Status: **active**  
Last updated: 2026-11-04  
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

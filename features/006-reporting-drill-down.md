# 006 — Reporting and drill-down

Status: **active**  
Last updated: 2026-10-28  
Implementation: none; requirements and proposed design only. On 2026-10-28 the owner said the finished eight-point sequence was the work before this feature. Implementation waits for an explicit Go. The open questions below are unset.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded catalog and account transfer destinations are implemented. Category reports remain unimplemented.

- Report spending by time period, including last year, this year, and comparisons between them.
- Show categories ordered from most costly to least costly.
- Double-click a category to inspect the transactions included in that category and period.
- Retain other selected report criteria during drill-down rather than silently broadening the result.
- Opening balances are excluded from income/spending reports.
- Saved reports appear in the sidebar; selecting one opens it in the right pane. The Reports `+` button opens a creation dialog. Ask the owner for its contents when that work begins. See [main layout and ledger search totals](005-transaction-sheet-navigation.md); ledger Withdrawal/Deposit totals are distinct from allocation-based category reports.

## Proposed design

Aggregate category allocations rather than whole transaction totals. For a transaction with a transport allocation and a fee allocation, each category receives only its relevant amount. Drill-down shows the relevant allocation amount alongside enough parent-transaction context to inspect it without double-counting it.

Define an explicit filter object shared by totals and detail: date boundaries, account set, category/subcategory scope, currency, and committed state, with additional criteria only when supported. Use one consistent data snapshot or refresh totals and details together if edits occur during drill-down.

Use committed data by default. Transfers and opening balances do not count as spending; categorized fees do. Keep uncategorized spending visible. Define whether refunds reduce spending and how negative category totals sort. These policies must be visible and tested rather than assumed.

Separate full-calendar-year comparison from equivalent year-to-date comparison. Resolve inclusive/exclusive boundaries and leap-year behavior explicitly. Transaction dates are financial dates and should not shift because of timezone conversion.

Initially show separate totals per currency. Consolidated home-currency totals require a defined valuation policy, recorded rates, and rounding treatment; missing rates must never become zero or a guessed value.

## Acceptance criteria

- Every displayed category total equals the sum of included allocations in its drill-down, exactly in the reporting currency.
- Splits never cause double counting of a transaction's whole amount.
- Date/account/category/currency criteria remain identical between totals and detail.
- Drafts, opening balances, and transfer principal follow explicit inclusion policies.
- Uncategorized transactions and refunds have tested, visible behavior.
- Boundary dates, leap years, parent-category rollups, and comparison periods have deterministic fixtures.
- Separate currencies are not silently added together; unsupported conversion is clearly reported.

## Open questions

- Fiscal versus calendar years, year-to-date defaults, date-boundary conventions, and leap-day comparison.
- Refund/net-spending and income policy, detailed rollup presentation/filter behavior, account selection, and sorting of credits. Feature 010 accepts one grouping level, non-assignable parents, and descendant rollups; it does not settle refund treatment.
- Additional criteria such as payee, memo, note, or amount range.
- Charts, exports, saved-report configuration/creation-dialog details, and consolidated CAD reporting remain unspecified. The sidebar entry point for saved reports is accepted.

## Category selection follow-up — accepted 2026-10-28

Categories are [independent assignable names](010-categories-and-selection.md#independent-categories--accepted-2026-10-28), optionally containing one colon. A future report category checklist may include both `Auto` and `Auto: Gas`; selecting Auto checks its matching `Auto:` categories, and the user can deselect any of them individually. A stem without a separately created category is not an assignable parent object.

Presentation remains open. The owner discussed either showing direct Auto spending plus subcategories with a separate total, or showing an inclusive Auto total with subcategory detail. For direct Auto 1000, Gas 50 and Other 25, the inclusive total is 1075; detail and subtotal must not be added twice. This does not authorize report implementation or settle income/refund/currency policies. Starter Income categories are ordinary neutral classifications, not rules for recognizing report income.

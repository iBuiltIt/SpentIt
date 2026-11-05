# Reports workspace and reusable calendar — next-agent handoff

Last updated: 2026-11-04

State: **initial scope implemented under owner Go, 2026-11-04**.
The owner first authorized this design handoff, then explicitly authorized the
reporting and calendar implementation. The accepted policies below remain the
contract. See [status](status.md#reports-and-shared-calendar--2026-11-04) for actual
verification and remaining platform limits.

Read [project status](status.md), [project layout](project-layout.md),
[reporting requirements](../features/006-reporting-drill-down.md),
[Rules workspace contract](../features/004-rules-and-explanations.md#single-rules-workspace-and-independent-edits--owner-go-2026-11-01),
and [user-facing language](user-facing-language.md) before continuing.

## Accepted report workspace direction

The owner wants reports to answer “Where did my money go?” using a familiar
Rules-style list and selected editor, then a separate result view.

- One **Reports** entry below Rules in the sidebar opens the workspace. This
  supersedes earlier individual saved-report sidebar entries and a creation dialog.
- The upper list contains report names and a **Show** action per report. Example
  names: This year, Last 6 months, 2026 vs 2025 YTD. These names illustrate intent;
  they do not settle moving-period semantics or require seeded reports.
- Selecting a report populates **Report details** below: name, account checklist,
  category checklist, graph choice, main start/end dates and comparison periods.
- Rename through the name field or context menu; deletion is available through
  the context menu. Users can select individual accounts/categories or all/none.
- The main range has a presets menu. Comparison controls add a custom range or
  a range from a preset; multiple comparison periods are supported.
- Requested presets include YTD, This Year, Last Year, This Quarter, Last Quarter,
  and potentially This Month and Last Month. Precise semantics remain below.
- Showing a report navigates to its result. Back returns to the Reports workspace.
- Follow the Rules Save / Discard / Stay paradigm for unsaved definitions.
- Printing is wanted later; the first focus is displaying reports. Calendar
  interaction is shared with transaction date editing.

## Initial result and graph layout — superseded in part

The [compact-results follow-up](#compact-results-and-editable-report-ledger--owner-go-2026-11-04)
supersedes the chart placement, duplicate legends and separate-bar-only restriction
in this initial contract. The financial policies remain unchanged.

The table has a category column, one column for the main period, then one per
comparison in the configured order. Totals appear above category amounts. The
result supports horizontal scrolling when periods exceed the viewport. The graph
area appears below the table. Category colour swatches appear when a graph is
selected; a chart legend can also carry that key.

**Use one chart per period for both Pie and Bar.** This was explicitly settled
after discussing alternative combined layouts. Do not add a By category toggle,
a combined-period pie, or Separate/Compare layout settings in this scope.

- Graph choices for the first version: **None, Bar, Pie**. Line is deferred until
  a meaningful time-series design is specified.
- Each chart shows the period name, exact dates and total.
- Charts sit side by side in table-period order, with horizontal scrolling.
- Category colours and category order are consistent across periods.
- All bar charts use the same monetary axis scale for comparable bar heights.
- Pies are equal-sized circles showing category proportions. Their labelled
  totals convey differences in overall spending.
- Example: category amounts 25, 25, 25, 25 yield four equal slices and total 100;
  comparison amounts 33, 33, 33, 0 yield three equal slices and total 99. The zero
  category has no slice; it does not cause the remaining categories to change colour.
- Overlapping periods are legitimate independent comparisons. Never combine them
  into a pie suggesting that they are disjoint shares of one total.

## Accepted reusable calendar interaction

The owner explicitly agreed to this complete interaction. Use a small calendar
icon beside a date field and **one popup whose contents switch** between day,
month and year selection, rather than stacking additional windows.

### Day view

- Open at the field's current date; an empty field opens at today.
- Header has month navigation arrows, a clickable month name and a clickable year.
- Month arrows move one month, crossing year boundaries naturally.
- The weekday header is Sunday-first: Su Mo Tu We Th Fr Sa.
- Show the selected date with a filled selection and today with a small outline;
  retain both meanings when they coincide.
- Choosing a day updates the field and closes the popup.
- **Today** selects today and closes the popup.

### Month and year views

- Clicking the month name replaces the day view with a 3-row, 4-column grid:
  Jan Feb Mar Apr / May Jun Jul Aug / Sep Oct Nov Dec.
- Highlight the displayed month. Choosing a month returns to the day grid.
- Clicking the year replaces the day view with a scrollable four-column year
  grid, initially positioned around the displayed year and highlighting it.
- Permit navigation back to 1900 and forward into future years. The upper bound
  must follow supported application dates, not the current year. Inspect existing
  date validation before choosing or exposing a bound; surface any conflict.
- Choosing a year returns to the day grid.
- Month/year choices only navigate. The field changes only on day or Today
  selection. Browsing January 31 into February must not silently edit the field.

### Dismissal, keyboard and integration

- Escape from month/year selection returns to the day view. Escape from the day
  view closes without changing the field. Clicking outside also closes unchanged.
- Support arrow-key navigation and Enter selection; preserve direct date typing.
- Use the dataset's date display format in the field.
- In transaction editing, choosing a date changes the unsaved editor field only;
  it must not save the transaction or trigger the editor's Enter-to-save handler.
- The same picker is used for main and comparison report dates and transaction
  date editing. Financial dates must not shift through timezone conversions.

## Recommendations retained for confirmation/detail design

The owner broadly agreed with the initial recommendations, but subsequent
agreement explicitly settled graphs and the calendar. These remaining refinements
are useful defaults to review, not evidence that every detail was individually
approved:

- Searchable report list, New report, and Duplicate; definitions stored in the
  active encrypted dataset. Selected Save/Discard, retained independent drafts
  when switching selection, and Save and show for a changed definition.
- Searchable account/category checklists with separate All and None actions and
  a selected-count label. All selecting current entries only is now accepted
  below; future entries must not silently change saved scope.
- Removable comparison rows and a labelled Presets menu instead of an unexplained
  arrow. Preserve selected report, search and scroll on returning from results.
- Freeze category column and period headers; show selected zero rows, optionally
  allowing Hide zero rows. Sort by main-period spending and keep comparison order
  aligned. Totals need no category colour swatch.
- Stable category colour assignments within a saved report rather than recolouring
  on every open/sort. Best-effort distinct colours are supplemented by labels,
  hover highlighting and exact values; do not promise unlimited distinct colours.
- Double-click a category-period amount to drill into that exact cell; total
  drill-down includes all selected allocations. Existing accepted requirements
  already require category/period drill-down and preservation of other filters.
- Proposed empty-chart wording: No amounts in this period. Negative-value pie
  handling is now accepted below.

## Report policies — explicitly accepted 2026-11-04

The owner answered Agree to each of these five decision groups. Do not reopen
these as unresolved policy questions. The subsequent implementation Go covers these accepted policies.

1. **Net spending and negative pies.** Outflows are positive; refunds reduce
   category spending; income is negative. Spending 100 and a refund of 20 reports
   80. Refunds exceeding spending yield a negative category. Expenses of 3,000
   and salary of 5,000 yield net outflow of -2,000 when both are selected.
   Category selection determines spending-only, income-only or mixed scope;
   category names do not classify income. Make the sign convention clear in the
   result. If a period has negative category totals, retain its table and explain
   that Bar can display those amounts instead of drawing a misleading pie.
   Never take absolute values or silently omit negatives.
2. **Moving/fixed periods and boundaries.** Saved presets retain their meaning
   when reopened/shown: This year is the current calendar year, YTD is January 1
   through today, Last month is the previous complete calendar month, and Custom
   retains exact dates. Always display resolved dates. Manual date edits switch
   the range to Custom. Distinguish Previous calendar month from Same period last
   year; clamp February 29 to February 28 when the comparison year lacks that day.
   Both boundary dates are inclusive. Use calendar years/quarters initially.
   The six-month preset is **Last 6 complete months**; a through-today version can
   be added separately later. Do not silently substitute that version.
3. **Direct category rows.** First version has one row per actual category,
   without inclusive group subtotals. Auto 100, Auto: Gas 50 and Auto: Repairs 25
   produce three rows and total 175. Selecting Auto can still check matching
   Auto: categories, individually deselectable. Do not display an inclusive Auto
   175 as another additive row. Table categories, chart colours and drill-down
   have a direct correspondence. Group subtotals are deferred.
4. **Separate currencies.** Allow selection of accounts in different currencies,
   but produce a separate table and chart set for each currency, with no combined
   grand total. Shared bar-axis scales apply across periods within a currency,
   not across currencies. Conversion and exchange-rate policy remain deferred.
5. **Selection and inclusion.** All selects accounts/categories existing now;
   later additions do not automatically enter saved reports. Uncategorized is an
   explicit choice selected by default for a new report. Exclude transfer
   principal even when only one side's account is selected; include categorized
   transfer fees. Use current edited dates, categories and amounts. Include new
   and committed transactions; exclude opening balances.

Remaining detail design includes renderer/dependency selection, refresh mechanics
and the UI refinements above. Fiscal reporting, conversion, group subtotals,
printing, exports and time-series charts are outside the initial scope, not
reasons to reopen these accepted policies.

Already settled elsewhere: new/uncommitted transactions belong in default future
reports alongside committed transactions; opening balances are excluded. The old
committed-only reporting proposal is superseded. Allocation totals must reconcile
exactly to drill-down; selected amounts must never use binary floating point as
authoritative money. Do not silently hide unresolved transactions or invent
report-specific duplicate matching.

## Graph package investigation, not dependency selection

SpentIt currently uses Qt Quick/QML and declares Qt 6.8 minimum in
`src/ui/CMakeLists.txt`. Evaluate a compatible pinned release rather than assuming
a current upstream branch will build with that minimum.

- **KQuickCharts** is the first suggested trial: native Qt Quick charts and LGPL
  chart code. Its development branch inspected on 2026-11-04 requires Qt 6.10 and
  KDE Extra CMake Modules. Check the selected release, full dependency/license
  inventory, signed bars, colour control, interaction and packaging on supported
  platforms. No integration or platform verification was performed here.
- **KDChart** is an alternative: its official repository states MIT licensing
  and Qt 6 support. Evaluate the additional integration needed for our Quick UI.
- **Qt Graphs / Qt Charts** have GPL/commercial licensing; do not adopt their GPL
  option as though compatible with the owner's no-resale direction. Qt Charts is
  deprecated since Qt 6.10. No final application license has been selected.

Sources checked 2026-11-04:

- [KQuickCharts project](https://github.com/KDE/kquickcharts)
- [KQuickCharts chart license example](https://github.com/KDE/kquickcharts/blob/master/src/BarChart.h)
- [KQuickCharts development build requirements](https://github.com/KDE/kquickcharts/blob/master/CMakeLists.txt)
- [KDChart official repository](https://github.com/KDAB/KDChart)
- [Qt Graphs licensing](https://doc.qt.io/qt-6/qtgraphs-index.html)
- [Qt Charts deprecation and licensing](https://doc.qt.io/qt-6/qtcharts-index.html)

Keep graph rendering separate from exact report calculations. Presentation-only
coordinates may use the renderer's numeric types; amounts and tooltip labels
must come from exact report results. Future printing should consume the same
report data, rather than requiring a screenshot of the scrolling view.

## Suggested next-agent sequence and verification obligations

1. Read this handoff and inspect existing date helpers, editor navigation,
   Rules guards, repository commands and tests. Relevant starting points include
   `src/ui/ImportMain.qml` and `src/ui/CMakeLists.txt`; do not copy financial logic
   into QML. Check current status rather than assuming historical format numbers.
2. Implementation Go was subsequently received for both scopes. Further changes
   outside this initial report/calendar contract still need their own Go.
3. Once authorized, implement and verify the reusable calendar independently.
   Cover month/year crossings, leap days, supported bounds, future years,
   navigation without edits, dismissal, keyboard focus, direct typing and the
   transaction editor's unsaved state. Check native placement near window edges.
4. Once implementation scope is authorized, implement
   testable exact allocation aggregation and shared drill-down filters, saved
   definitions and the workspace/result navigation. Keep heavy queries off the
   UI thread and results consistent across table, chart and detail snapshots.
5. Trial/pin a graph dependency and verify separate-period rendering, shared bar
   scales, stable colours, empty/negative results and wide scrolling. Printing
   and line/time-series charts remain deferred.
6. Verify synthetic split/refund/income, date boundaries, overlapping ranges,
   transfers/fees, new and committed rows, category rollups, currency separation,
   overflow, stale refresh and durable definition-save failure paths as relevant.
   Do not claim financial completeness from UI tests alone. Record actual platform
   checks and unverified limits in status.

## Implemented architecture — 2026-11-04

- `reports.hpp`/`reports.cpp` define explicit account/category selections, moving
  presets, inclusive resolved ranges and exact allocation aggregation. Each cell
  retains indices into the same immutable entry snapshot used by its drill-down.
  Overflow rejects the report; opposite-signed terms may cancel before narrowing.
- SQLCipher stores report definitions, selections and ordered periods in formats
  54/53. A revision-checked full-image save is atomic; an identical retry of the
  immediately completed revision returns successfully. Selected Save constructs
  that image from saved definitions plus only the selected draft. Leaving Save
  validates every draft before storage is called. Missing current accounts or
  categories produce an explicit edit-selection error, never scope expansion.
- The serial worker collects all selected accounts inside one SQLite transaction
  and restores the previous account selection even on failure/cancellation.
  Current ledger rows include accepted and new entries; opening balances never
  enter aggregation. Transfer endpoints and destination labels exclude principal;
  ordinary fee splits remain eligible. Unbound ordinary imported categories use
  the current matching catalog identity, or Uncategorized when still unassigned.
  Existing import failures are visibly identified as outside the report.
- Results retain their snapshot until Show/Refresh. Dataset changes mark an old
  result stale and disable opening further drill-down from it; Refresh replaces
  table, graphs and detail together. Presets resolve against the local calendar
  date when shown. No live midnight timer is claimed.
- The renderer uses the existing Qt Quick Canvas dependency, not a new graph
  library. The earlier KQuickCharts/KDChart suggestions were not adopted. Bar/Pie
  coordinates are presentation-only doubles; all labels and totals originate from
  exact C++ amounts. Negative pies retain the table, show an explanation and draw
  no slices. Category identity determines colour. Category/period headers remain
  fixed while the results scroll; each currency has its own shared bar scale.
- The shared picker uses QDate calendar arithmetic with supported years 0001–9999,
  a virtualized year grid, Sunday-first days and one popup. Month/year browsing
  never writes the field. Invalid typed dates cannot silently become today or a
  different valid format. The transaction editor receives only an unsaved date.

Optional Hide zero rows, chart-hover cross-highlighting and report export/printing
are not implemented. Labels and exact-value legends supplement category colours.
Fiscal periods, currency conversion, group subtotals and time-series remain outside
this initial scope. Native/platform and workload limits are recorded in status.

## Compact results and editable report ledger — owner Go, 2026-11-04

This follow-up supersedes the initial table-before-chart layout and the prohibition
on combined bar comparisons. The owner approved the following changes:

- A result has charts first (when enabled), then one compact category table with
  ledger-style alternating backgrounds and colour swatches. The table is also the
  category legend. One outer vertical scrollbar exposes every category; neither
  the category table nor the charts have a nested vertical scrolling area.
  Horizontal scrolling remains available for wide comparisons.
- Saved Graph choices are None, Bar — separate periods, Bar — grouped periods,
  and Pie. Grouped bars place the configured periods next to each other within
  each category, in period order. Category hues remain stable; period shades,
  ordered date labels and exact hover values distinguish periods. Currencies
  remain separate and signed amounts retain the accepted reporting convention.
- Double-clicking a cell opens the existing ledger/editor over only the original
  contributing transaction identities. Each transaction appears once, with an
  Account column and an In report contribution separate from Withdrawal/Deposit.
  Split markers identify the currently contributing allocations.
- Successful edits and Undo regenerate the report. The opened identity set stays
  fixed while editing: recategorized or redated transactions remain visible with
  “No longer included”. Deleted transactions disappear until restored by Undo.
  Reopening a cell selects its newly calculated membership. Back preserves the
  result view and uses the ledger's Save / Discard / Stay guard for unsaved edits.
- Report search can narrow this fixed identity set, never broaden it. Returning
  restores the ordinary account's filter and sort. The report has no running
  account balance or opening-balance row.

Implementation uses existing account-scoped correction, category and Undo
commands. Individual edits route to the row's account. Group edits require one
account; mixed-account requests fail before mutation. Merge/Link and creation
remain in the ordinary account ledger. This restriction is surfaced rather than
splitting one requested group operation into independently committed commands.

The additional stored graph value introduces experimental formats **56 with
rules / 55 without**. Earlier formats are rejected unchanged; no migration is
provided. Automatic refresh failure leaves the result stale and displays a
refresh message, without undoing or retrying a saved financial change. See
[status](status.md) for actual verification and limits.

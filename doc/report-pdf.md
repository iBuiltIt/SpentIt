# Report PDF export

Owner Go: 2026-11-15. This is presentation-only export; it changes neither the
report definition nor the database format. See [report requirements](../features/006-reporting-drill-down.md).

## Entry points and dialog

Save as PDF… sits beside Refresh report. File → Save report as PDF… is enabled
only while a generated report result is visible and the worker is idle. It is
unavailable in the definitions list and transaction drill-down. A stale result
asks the user to refresh before opening the PDF dialog.

The modal dialog defaults to Letter, Automatic orientation, and Readable pages.
It shows a page preview, Previous/Next page, and Page X of Y. Paper also offers
A4; orientation also offers Portrait and Landscape. Fit on one page scales the
complete report, including all comparisons and currencies. Below 80% scale the
dialog explains that text is small and suggests Readable pages. Preview and PDF
use the same measured page plan and drawing routine. Preview images stay in
memory; no temporary plaintext preview is written.

The initial filename is the report name with `.pdf`, beside the open SpentIt
file. Filename characters forbidden on Windows are replaced, reserved device
names are prefixed, and the filename stem is bounded. If the folder is missing
or unwritable, the suggestion uses Qt's platform Documents location. Browse
uses the native save picker. Save confirms replacement of an existing file.
The open dataset path, including a resolved symlink alias, is refused as an
export destination. A PDF suffix is required.

The dialog states **The PDF is not password-protected.** This is an explicitly
requested plaintext export, independent of dataset encryption and password
settings. No source records, transactions, database path, or attachments are
embedded. The PDF contains the displayed report summary, account names,
resolved periods, currency sections, category amounts, totals, chart selection,
and any warning about imported records without reportable amounts.

## Layout

Readable pages use ten-point body text, white paper and dark text, with wrapped
category labels and exact preformatted monetary strings. Automatic orientation
uses portrait for up to three periods, landscape for more. A report that fits
stays on one page; larger reports flow across pages. Each page has the report
title and page number.

Tables repeat headings and currency on vertical continuations. Comparisons
that exceed the available width are partitioned into groups; each group
repeats category names and the main period. Each group includes its period
totals. The repeated main-period values are context, not additional amounts.
Amounts are never recomputed or summed by the PDF renderer. Different currencies
remain separate, with no combined total.

Charts precede their currency's table. Short separate-period charts can sit
side by side. Large bar charts split into complete category blocks, preserving
the currency-wide scale and category numbering. Grouped bars retain period
order and show a period key. Pie charts use the report's category colours;
negative periods show the same reason for withholding a pie as the report.
Tables include category colour keys. Text and shapes are drawn as PDF text and
vectors. Unicode font fallback uses the installed Qt/platform fonts.

Readable mode refuses an individual label that cannot fit on a page rather
than clipping it. Layout is bounded to 1,000 pages and 300,000 drawing marks;
exceeding either asks for fewer categories or comparisons. Fit mode may produce
very small text, which is disclosed in the preview.

## Execution and failures

Layout, PNG preview generation and PDF writing run on the existing serial
worker. The worker exports a frozen copy of the displayed report presentation,
whose monetary strings were produced from the engine snapshot. Generating a
new report clears its PDF plan. Save refuses a stale report.

The PDF writer is Qt GUI's QPdfWriter, using the already-linked LGPL-available
module. No new library or schema change is required. Drawing commands and
preview state contain financial information and are not logged.

Writes use QSaveFile with direct-write fallback disabled. A checked output
device detects failed or short PDF writes, including the final trailer. Cancel
is checked before the replacement boundary and sealed immediately before
commit. An error or cancellation before commit preserves an existing
file; success is returned only after commit. Failure of PDF work does not
invalidate the financial report, so changing destination and retrying remains
possible. Closing the dialog releases its preview and page plan.

## Verification

Automated synthetic coverage lives in
[PDF tests](../tests/ui/report_pdf_tests.cpp) and
[report UI tests](../tests/ui/report_ui_tests.cpp). Actual run results and
platform limitations are recorded in [status](status.md). The manual update is
queued in [the backlog](manual-update-backlog.md).

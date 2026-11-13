# 005 — Transaction sheet and navigation

Status: **active**
Last updated: 2026-11-13

The durable ledger is the application sheet. Editing, Set memo, Rename payee,
Restore, splits, expressions, and amount formatting are in
[doc/ledger-editing.md](../doc/ledger-editing.md). Search, selection size, and
bulk commands are in [doc/search-ledger.md](../doc/search-ledger.md). Duplicate
markers and Review navigation are in [doc/review-sheet.md](../doc/review-sheet.md).

## Ledger

Columns are Date, Payee, Category, Withdrawal, Deposit, and Balance. Until
a column is dragged, the widths stay in the ratios 0.14, 0.24, 0.17, 0.15,
0.14, and 0.16. Money is right-aligned. Dragging the right edge of a heading
changes that column only. The other columns keep their widths, and the row
grows to the right. A horizontal scrollbar appears when the row is wider than
the window. Making the window wider shows more of that row and does not
stretch columns that have been dragged. Each column has a minimum. Date and
the money columns stay wide enough for the heading and a normal value, and
every heading stays visible. A column stops growing at 4,000 pixels. The drag
does not sort. Clicking the heading still sorts. Each heading edge shows a
narrow shaded notch. The edge highlights while the pointer is on it.

Double-clicking that edge sizes the column to the longest text in the
current list, including rows scrolled out of sight. A search limits the
list. The column may become wider or narrower. A row that shows a split
count is measured as that label. The same minimum and 4,000 pixel limit
apply, and the other columns stay put.

The widths are remembered on this computer, outside the file. They are whole
numbers, one per column. A saved list of the wrong length, or a width below
24 or above 4,000, is ignored and the ratios return. The account ledger and
the report transaction list remember their widths separately. The report list
adds Account and In report, with default ratios 0.12, 0.20, 0.18, 0.16, 0.12,
0.10, and 0.12 until its own columns are dragged.

Search, the duplicate bar, and the filtered totals stay with the window. Only
the headings and rows scroll sideways. A cut-off cell shows its full text in
a tooltip while the pointer rests on that cell. A row marked as a possible
duplicate keeps that tip when the cell under the pointer is not cut off.
Split category and memo lines use the same cut-off tip.

The window minimum is 720 by 560. The default size is 1440 by 850. Below
1,080 pixels of content width, before any column is dragged, the ledger
scrolls sideways.

Double-click toggles the editor. Changing selection leaves the editor in place.
Enter accepts a complete validated edit. Escape cancels the pending text.
Session Undo restores a saved edit. Other dataset mutations end that Undo
sequence.

Leaving an account records an identity and offset anchor. Back, Forward, and
the account button restore it after layout. A hidden refresh must not replace
it. A missing anchor falls back to a clamped row. Session positions clear when
the document changes.

The sidebar lists each account with a session balance control, Committed or
Projected. It is not a saved setting. The ordinary ledger balance includes new
and accepted rows. That sidebar control does not change ledger search or sort.

Appearance follows the system, with light as the unknown fallback. There is no
separate appearance preference. Ordinary rows alternate. New rows use their own
pair. Selection takes priority over striping. Expanded splits stay in the
parent stripe.

**+ New Account** reveals and focuses the name field without jumping the rest
of the list out of view. Account drag order is a saved sidebar order.

Select All and Deselect apply to the focused transaction list, including
offscreen filtered rows, and exclude Opening Balance. Selection-based commands
have no fixed row ceiling. All-or-none writes, cancellation before the commit
seal, and group Undo remain.

## Search

Search covers the displayed fields and original imported text, including the
`QIF:` token for retained occurrence bytes. An invalid pattern is an error, not
an empty result. Clearing search restores the previous view unless the user
opened a result; that result then stays selected and visible. Clear and Escape
honor Save, Discard, and Stay.

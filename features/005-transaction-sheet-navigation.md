# 005 — Transaction sheet and navigation

Status: **active**
Last updated: 2026-11-09

The durable ledger is the application sheet. Editing, Set memo, Rename payee,
Restore, splits, expressions, and amount formatting are in
[doc/ledger-editing.md](../doc/ledger-editing.md). Search, selection size, and
bulk commands are in [doc/search-ledger.md](../doc/search-ledger.md). Duplicate
markers and Review navigation are in [doc/review-sheet.md](../doc/review-sheet.md).

## Ledger

Columns are Date, Payee, Category, Withdrawal, Deposit, and Balance, in the
ratios 0.14, 0.24, 0.17, 0.15, 0.14, and 0.16. Money is right-aligned. The user
cannot drag a column, and widths are not saved. The window minimum is 720 by
560. The default size is 1440 by 850. Below 1080 pixels of content width the
ledger scrolls sideways.

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

## Not done

- Multiline notes and a persisted column or sidebar layout.
- Spoken VoiceOver output. Named accessibility is present. Spoken output was
  not verified.
- The exact additional keyboard shortcut left open by the early sheet layout.
  Do not invent one.

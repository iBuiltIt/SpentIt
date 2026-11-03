# Ledger editing and session Undo

Status: implemented under owner Go, 2026-11-01; verification is recorded in
[status](status.md). This supersedes the older inline Save draft / Cancel edit
and explicit committed-edit opt-in presentation.

## Accepted interaction

- Double-click opens a transaction; double-clicking that same transaction closes
  it. Double-clicking another switches the one active editor.
- Single-click selection remains independent. The open editor stays attached to
  its transaction, with no empty reserved space or move to the bottom panel.
  Filtered-out and otherwise absent records retain the existing fallback inspector.
  Follow-up Go (2026-11-01): ordinary single-click selection does not enter the
  global busy state or flash the editor/sidebar disabled styles. An unchanged
  selection is a no-op. Plain selection uses the published ledger and cached
  transfer flags; range/toggle selection retains the existing bounded worker path.
  Actual operations continue to disable controls while running.
- Enter accepts the complete valid edit durably and retains its viewport and field
  focus. Ordinary category-picker choices also accept the edit. There are no
  Save draft / Cancel edit buttons or separate committed-correction Apply step.
- Escape abandons the current, unaccepted buffer. Invalid input remains visible;
  it cannot replace saved values. Leaving an unaccepted buffer still offers
  Save / Discard / Stay. These choices concern that buffer, not previous saves.
- A split edit is accepted as a complete transaction. Intermediate split amounts
  may be inconsistent while typing, but acceptance requires exact reconciliation.
- Cmd+Z / Ctrl+Z in a clean editor or the Undo edit button reverses accepted
  edits. While typing unaccepted text, the text field retains native text Undo.
  Enter does not advance to another transaction.

## Category completion — owner Go, 2026-11-01

The allocation category button is replaced with a text field. Uncategorized is
blank. Focus opens a dropdown with case-insensitive substring matching against
available category names and account names/aliases. Empty text shows all choices,
including an explicit Uncategorized choice first so clearing can remove an
assignment. Archived/deleted categories and the current account are excluded.
Existing retained classifications remain visible until deliberately replaced.
Unresolved imported transfer allocations offer account destinations only and use
the existing explicit transfer-resolution command.

Typing highlights the first match; Up/Down changes that selection. Click, Enter
or Tab chooses it using the existing complete-edit acceptance or guarded transfer
command. Enter/Tab moves to transaction Memo for a single allocation, or that
split's memo for multiple allocations. Date, Payee, Amount, Category and Memo
support forward/reverse keyboard navigation. Memo follows allocations visually.
Add split remains a button. The dropdown has an interactive scrollbar.

No matches leaves the field open without creating or assigning a category.
Escape closes the dropdown and restores its prior text; Escape with it closed
retains the editor's existing discard behavior. Leaving a search without choosing
restores the saved label. Category searching alone does not mutate the edit.
Other fields retain Enter-to-accept behavior and session Undo remains unchanged.

## Undo scope and persistence

Accepted edits survive closing and reopening. The editor Undo capability is
session-only, for a consecutive sequence of accepted edits; other dataset
mutations end that sequence. Selection, inspection and filtering do not end it.
Undo restores values, split identities, ownership/provenance, and any transfer
relationships changed by the edit together. No partial restoration is permitted.
There is no new Redo command.

The worker retains only operation identities and transaction/account references
for this capability. Exact snapshots use the existing encrypted durable journal,
its mutation-generation gate and stacked reversal logic. Accept and Undo are
complete atomic repository operations with durable outcome lookup. Failed
validation preserves the buffer; an unknown outcome or failed post-save refresh
requires reopening, with an accurate explanation. Committed edits remain in
persistent history, whose existing strict individual Undo is still available
when eligible after a restart. Session Undo does not erase history.

The ledger command requires an existing valid transaction. First repair of a
malformed import still uses the existing repair path in Review; restoring an
absent pre-repair transaction is outside this ledger-edit scope. Existing generated
counterpart and protected-transfer edit restrictions remain in force.

No schema change: formats remain 40 with rules / 39 without. The full-audit
verifier identity changes with the implementation. No migrations or private
financial fixtures are introduced.

## Opening balance editing — owner Go, 2026-11-01

The stored opening amount and date are usable immediately, including the initial
zero. There is no separate confirmation action or opening-confirmation Commit
blocker. Review no longer shows the ordinary opening setup strip. Transfer
warnings describe transfer problems without asking for opening confirmation.

Double-clicking the ledger Opening Balance row opens the existing date/amount
dialog. The account menu offers the same correction command from creation onward.
A successful edit is atomic and historied; account Undo restores the prior amount
and date under its existing later-change checks. Transaction values and source
facts stay unchanged. Existing exact-money, overflow, archived-account and
committed-date validation remain. Import mapping still explicitly chooses zero
or the reported amount; it needs no subsequent acknowledgement.

Formats remain 40/39. The stored historical `confirmed` field is retained as
revision metadata, not a prerequisite to use an opening, correct it or commit.
An untouched zero opening may be frozen by Commit at revision zero; verification
still checks its creation amount/date and all financial/history relationships.
Legacy headless reported-balance interpretation APIs are separate from the
desktop's fixed creation-time mapping choices.

## Restore imported values — owner Go 2026-11-02

Show original imported transaction opens a compact Format / Restore imported
values header and the selectable original record snippet. Parsed fields and
individual Copy to edit buttons are no longer displayed. Both the checkbox
indicator and label toggle the panel. Editor content reserves scrollbar space.

Restore replaces the entire unsaved edit buffer with parsed imported date,
payee, memo, total and original splits/categories. Existing split identities are
matched by source ordinal where available; additional edited splits disappear
from the buffer. Enter validates and saves through the ordinary edit command;
Escape discards the restoration. Unavailable original dates/amounts become empty
fields requiring correction, rather than retaining corrected values silently.
Existing category, transfer and financial validation still applies. Restore does
not reverse Merge or acceptance, change the account, or recreate relationships
outside the existing edit command. Generated entries without imported values
cannot use Restore. Accepted edits retain normal history and session Undo.

Follow-up Go (2026-11-02): restoring payee, transaction memo and retained
source-split memos returns those fields to source ownership, clearing prior rule
marks. Storage requires exact equality with retained imported text. Typing in a
field after Restore makes that field manual again, even if its text equals the
original. Save/Escape/reload ends the editor's restoration intent. Categories
retain the existing validation and Uncategorized eligibility policy. This fixes
Restore followed by rule Apply assigning a category but protecting the restored
payee as though it had been typed manually. New and committed edits use their
existing atomic history/Undo paths; no format change or data rewrite is required.


## Selected Rename payee and Restore — owner Go 2026-11-02

The ledger right-click menu offers **Rename payee…** and **Restore imported
values…** for one or multiple selected transactions, including selections made
through search. Rename opens one text field, prefilled only when all selected
payees agree. Confirming assigns that text to each selected payee as a manual
edit; all other fields remain unchanged.

Restore always asks for confirmation, names the selected count, and explains
that dates, amounts, payees, memos and splits/categories will be replaced. Cancel
or dismissal does nothing. Confirming restores parsed original values, returns
restored text to source ownership and removes edited extra splits. The command
does not rerun rules, change acceptance/account or reverse Merge. Rules continue
to match original imported text; clearing a category is sufficient to make an
otherwise eligible transaction available to its categorizing rule again.

Both commands are one atomic operation across new and committed rows, with
one **Undo payee rename** or **Undo restore** action and the normal Cmd+Z/Ctrl+Z
shortcut. Undo restores the exact prior values, ownership, splits and affected
transfer relationships for the entire selection. These actions join the existing
session edit Undo sequence and its mutation barrier; closing the document ends
the UI capability. Committed history persists and cannot reverse a single child
of a selection operation. Search, selection and ledger viewport are retained
where their identities remain available; rows can leave a filter after a rename
or restoration.

Storage validates the complete frozen selection and mutation generation before
writing. A stale selection or any invalid member rejects the entire operation;
no selected row is silently skipped. Generated counterpart entries are excluded.
Missing/invalid original fields, unavailable original categories, changed currency
or removed original source-split associations require individual editing. Pending
linked transfers retain the existing editor restriction. Committed transfer edits
use the existing relationship planner and exact group Undo; overlapping changes
to the same relationship reject the selection. Restoration does not create
categories or silently repair incomplete originals.

The storage command derives values from retained originals, records its full
request for idempotent retries, and shares the encrypted snapshot journal with
ordinary edits. Cancellation remains available until the final durable commit
gate. Read-only selection changes cannot acknowledge mutations. See
[status](status.md) for actual verification and remaining limits.

## Split-only editor actions — owner Go 2026-11-02

Remove and Create rule from split memo appear only when the editor has more than
one allocation. Add split remains available for ordinary transactions. Returning
to one allocation hides both controls; Remove is also disabled and its handler
rejects activation at one allocation, preserving the final category line. Existing
transfer-removal restrictions and imported-source eligibility for memo rules
remain in force. Memo and amount field presentation is unchanged by this fix.

## Shared category completion — owner Go 2026-11-02

The ledger and rule editor now share `CategoryCompletion.qml`. Gaining focus
selects the current text for replacement; later clicks/dragging select text
normally. Typing filters the still-open list immediately, retaining keyboard
focus. The list renders ampersands literally. Up/Down, Enter/Tab, mouse choice,
Escape, split-field navigation and category Undo retain their existing behavior.
Ledger choices still include Uncategorized and eligible transfer accounts, and
assignment still uses the existing immediate command. Rule choices and saves
remain separate caller responsibilities. See [status](status.md) for checks.

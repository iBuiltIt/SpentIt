# Ledger editing and session Undo

Last updated: 2026-11-18

Manual and imported postings share transfer assignment, explicit Link, editing,
and Undo. Rules match imported fields. The release rules database is version 1.

This supersedes the older inline Save draft / Cancel edit
and explicit committed-edit opt-in presentation.

## Accepted interaction

- Double-click opens a transaction; double-clicking that same transaction closes
  it. Double-clicking another switches the one active editor. While the saved
  splits are visible, a double-click on the “Saved splits” line or on any split
  line opens that transaction. One click selects it, including Shift and
  Command or Ctrl. A right-click opens the ledger menu. The transfer arrow on
  a split still opens the other account. The editor replaces those lines, so
  closing stays a double-click on the top line. The account ledger and the
  report transaction list use this row.
- Single-click selection remains independent. The open editor stays attached to
  its transaction, with no empty reserved space or move to the bottom panel.
  Filtered-out and otherwise absent records retain the existing fallback inspector.
  Follow-up Go (2026-11-01): ordinary single-click selection does not enter the
  global busy state or flash the editor/sidebar disabled styles. An unchanged
  selection is a no-op. Plain selection uses the published ledger and cached
  transfer flags; range/toggle selection uses the worker path, without a fixed row ceiling
  under the [2026-11-06 follow-up](search-ledger.md#selection-size--owner-go-2026-11-06).
  Actual operations continue to disable controls while running.
- Enter accepts the complete valid edit durably and retains its viewport and field
  focus. Ordinary category-picker choices also accept the edit. There are no
  Save draft / Cancel edit buttons or separate committed-correction Apply step.
- Escape abandons the current, unaccepted buffer. Invalid input remains visible;
  it cannot replace saved values. Leaving an unaccepted buffer still offers
  Save / Discard / Stay. These choices concern that buffer, not previous saves.
- A split edit is accepted as a complete transaction. Intermediate split amounts
  may be inconsistent while typing, but acceptance requires exact reconciliation.
- Cmd+Z / Ctrl+Z in a clean editor reverses accepted edits. The ledger has no
  Undo button beside search. While typing unaccepted text, the text field
  retains native text Undo. Enter does not advance to another transaction.

## Ledger rule creation and menu availability — owner Go, 2026-11-08

The ledger context menu lists **Create rule…**, **Change category…**,
**Add transaction…**, **Set memo…**, **Set payee…**, **Merge**, **Unmerge**,
**Link to other account…**, **Restore imported values…**, and
**Delete selected…**, in that order.

With the account or report transaction list focused, **Option+R** on macOS
or **Alt+R** on Windows and Linux opens **Create rule…** for one eligible
selection. **Option+C** or **Alt+C** opens **Change category…** and focuses
its filter. The shortcuts use the same selection and edit guards as the menu.

The account and report ledger context menus offer Create rule… for exactly one eligible
selected transaction without opening its editor. Right-click preserves an existing
multiple selection; the action never silently chooses one row from it. Unsaved-edit
guards run before context inspection, and stale inspector data cannot supply the
rule target. One target opens a draft directly; several targets use a chooser with
Cancel. That draft’s button reads **Save, apply and back**. A successful click
saves the rule, applies it to every active account unless the Apply section
was limited to one account, and returns through **Back** to the ledger. A
failed save stays in Rules. Existing original-field eligibility and the
one-new-rule limit remain.
The open editor does not list which rule assigned the payee, memo, or
category. **Edit rule…** still opens one. See the
[Rules entry point](../features/004-rules-and-explanations.md).

Report-ledger Create rule… uses the same source inspection without opening the
transaction editor. Save, apply and back returns to the report transactions,
with refreshed allocations and totals. Commands unavailable in report ledgers
(Add transaction, Merge, Unmerge, and Link) remain visible and disabled.
Change category, Set payee, Set memo, and choosing an account in that list
can include several accounts in one save. One Undo reverses that selection.
Each chosen account transfer gets its own opposite posting. A row already in
the chosen account, or a source account in another currency, stops the save
and changes nothing. A transaction with more than one split is still changed
one at a time. Delete and Restore stay inside one account. Choosing an account, from the right-click menu or from the
category field in the open transaction, shows that account's name in the
Category column on the top line. The name matches the category field without
closing the transaction, and the transfer arrow points at that account. Both
are shown for every account in a report list, including accounts other than
the open one. The
account ledger uses the same column text. Undo restores the previous category
text. The Account heading sorts the list, newest date first within
each account.

Disabled ledger menu text and submenu arrows use a distinct colour in light and
dark themes and do not highlight on hover. Financial command eligibility is
unchanged. Verification and platform limits are in [status](status.md).

## Category completion — owner Go, 2026-11-01

The allocation category button is replaced with a text field. Uncategorized is
blank. Focus opens a dropdown with case-insensitive substring matching against
available category names and account names/aliases. Empty text shows all choices,
including an explicit Uncategorized choice first so clearing can remove an
assignment. Archived/deleted categories and the current account are excluded.
Existing retained classifications remain visible until deliberately replaced.
For a split with an unresolved imported transfer destination, choose an
ordinary category or Uncategorized to replace the destination. Choosing an
account uses the explicit transfer-resolution command. The original imported
destination remains available.

Typing highlights the first match; Up/Down changes that selection. Click or
Enter chooses the highlighted row, using the existing complete-edit acceptance
or guarded transfer command. Choosing moves to transaction Memo for a single
allocation, or that split's memo for multiple allocations. Tab and amount
selection are in
[Amount and category Tab](#amount-and-category-tab--owner-go-2026-11-17).
Date, Payee, Amount, Category and Memo support forward/reverse keyboard
navigation. Memo follows allocations visually. Add split remains a button.
The dropdown has an interactive scrollbar.

Enter with no match leaves the field open without creating or assigning a
category. Escape closes the dropdown and restores its prior text; Escape with it closed
retains the editor's existing discard behavior. Leaving a search without choosing
restores the saved label. Category searching alone does not mutate the edit.
Other fields retain Enter-to-accept behavior and session Undo remains unchanged.

## Correcting the date of a New transfer

For a manually entered posting, see [Editing a New manual transfer](#editing-a-new-manual-transfer).

Double-click the original transaction to edit its Date. Press Enter to save or
Escape to discard the edit. The transaction stays New and the link stays in
place. The transfer editor's existing restrictions on other fields still apply.

If the other side is a generated entry that is also New, the correction updates
both dates in one save. From that generated entry, **Edit date on original
transaction** opens the original and focuses Date. Two independently entered or
imported postings keep their own dates. An accepted counterpart keeps its date.
An attached or proposed bank match locks the date.

**Undo Correct transfer date** restores the previous date or dates in one step.
Redo reapplies the correction through the same validation. Amounts, allocation
identities, source evidence and acceptance status are preserved.

## Editing a New manual transfer

Double-click the manually entered transaction to edit its date, payee, memo,
amount, and splits, including their categories, memos, and amounts. From its
generated entry, **Edit original transaction** opens that editor. Press Enter to
save. A single split follows the total, and multiple splits must add up to it.

An eligible New generated counterpart follows the original's date, the opposite
of its linked split amount, and its payee and memo changes. A split memo takes
precedence over the transaction memo for its generated entry. Review reevaluates
matches after the save.

SpentIt asks before saving an edit that would break a link. A removed transfer
split, a zero amount, or a changed destination can break its link. A changed
amount can also break a link to an independent or accepted posting. Corrections
that affect a bank match require the same decision. The confirmation explains
which postings stay, which generated entries are removed, and whether the edit
releases a proposed bank match. Cancel leaves the edit open.

The original edit and its changes across accounts save together as one Undo
step. Undo and Redo restore the corresponding postings and links together.

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
When the dialog opens, Amount has focus and its entire value is selected.
Typing or pasting replaces the selected amount.
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
or removed original source-split associations require individual editing. Bulk
Restore stays unavailable for pending linked transfers. Committed transfer edits
use the existing relationship planner and exact group Undo; overlapping changes
to the same relationship reject the selection. Restoration does not create
categories or silently repair incomplete originals.

The storage command derives values from retained originals, records its full
request for idempotent retries, and shares the encrypted snapshot journal with
ordinary edits. Cancellation remains available until the final durable commit
gate. Read-only selection changes cannot acknowledge mutations. See
[status](status.md) for actual verification and remaining limits.

## Selected Set memo — owner Go 2026-11-09

The ledger right-click menu offers **Set memo…** for one or multiple selected
transactions, including a search selection. The menu name stays **Set memo…**.
The dialog is where the user sees how many selected transactions already have a
memo.

The dialog says “Enter a memo for 1 selected transaction.” or “Enter a memo for
N selected transactions.” When any transaction memo already contains text, it
adds “1 of N already has a memo.” or “K of N already have a memo.” and shows an
unchecked box, **Overwrite 1 existing memo** or **Overwrite K existing memos**.
The text field is filled only when every selected transaction memo is the same
nonempty text. Otherwise it is empty.

**Set memo** writes that text onto each selected transaction whose memo is blank.
A memo made only of spaces, tabs, carriage returns, line feeds, vertical tabs, or
form feeds counts as blank, both in the selection and in the field. Real text,
including surrounding spaces, is stored as typed. Checking the box writes the text
onto every selected transaction, including a memo that already matches that text.
If every selected memo already has text and the box is unchecked, **Set memo**
stays disabled. If the field is blank and the box is checked, the button says
**Clear memo** or **Clear memos** and removes those existing memos.
Transactions that are already blank stay as they are. Cancel or dismissal does
nothing.

This changes the transaction memo only, the Memo field in the transaction editor.
Split memos, including the memo on a transaction’s only split, stay unchanged.
Payee, date, amount, category and transfer links stay unchanged. A memo this
command writes is a manual memo, so a later rule leaves it alone. A memo left in
place because overwrite was off stays as it is.

The command follows Rename payee for membership. One or many new and accepted
rows are one atomic operation, with one **Undo Set memo…** action. The whole
selection is rejected when any row cannot be edited, including a generated
transfer entry, a merged row, a stale selection or an invalid member. No selected
row is skipped because it is invalid. Rows that already have a memo are left
unchanged only when overwrite is off; that is a successful partial update, not a
rejected selection. When the command changes nothing, it adds no Undo action.
Search, selection and the ledger viewport are retained where their identities
remain available. Storage validates the frozen selection and records the memo and
overwrite choice for an exact retry. A changed retry is rejected. Help for the
dialog stays on Add/correct transactions, “Change a selection or delete entries.”
The manual already describes Set memo.

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
focus. The list renders ampersands literally. Up/Down, Enter, mouse choice,
Escape, split-field navigation and category Undo retain their existing behavior.
Tab through an unchanged field is specified with amount selection below.
Ledger choices still include Uncategorized and eligible transfer accounts, and
assignment still uses the existing immediate command. Rule choices and saves
remain separate caller responsibilities. See [status](status.md) for checks.

## Amount and category Tab — owner Go 2026-11-17

Tab or Shift+Tab into the transaction amount or a split amount selects the
whole amount. A click that gives the field focus does the same. A later click
in the focused field places the caret. Typing replaces the selection. Leaving
a valid amount still replaces an expression with its formatted answer.

Tab into a category field opens the suggestions and selects the text. Tab
again, without typing and without Up or Down, closes the list and moves on.
The category stays as it is, and the transaction is not saved. One split moves
to the transaction Memo. Several splits move to that split's memo, then its
amount, then the next category or the transaction Memo.

Typing a search, or pressing Up or Down, makes the next Tab choose the
highlighted suggestion and then move to that same field. Enter still chooses.
Enter with no match leaves the field open and assigns nothing. Tab with no
match restores the saved label and moves on. Shift+Tab still abandons an open
search and moves back. A transfer label such as `Savings [Account]`, or an
unmapped label, is unchanged by that pass-through Tab. The rule editor uses
the same distinction. Pass-through Tab there moves to the Transactions choice
and does not change the rule's category.

## Split amount editing — owner Go 2026-11-03

Follow-up owner Go (2026-11-03): a designated balancing split replaces the
fixed Split 1 rule. On first opening an automatic editor, choose the largest
eligible amount by absolute magnitude, preferring the transaction's sign. Ties
use existing row order; a zero total has no sign preference. Transfer destinations
(including account aliases) and linked allocations are ineligible. A previously
saved manual mode remains manual. If no eligible split exists, validate the
entered amounts without automatic adjustment.

The chosen row is labelled **Automatic remainder**. Its stable allocation identity
is saved with the next balanced edit and retained through reopening, acceptance,
backup and Undo; it does not switch when another split becomes larger. Other
entered amounts stay fixed. Enter calculates `remainder = total - sum(other splits)`
with exact money arithmetic, including zero and negative results. Changing the
total also recalculates the same remainder. Arithmetic expressions are supported by the follow-up below.

Typing in the balancing split's amount disables automatic adjustment for that
transaction. Category/memo edits do not. Removing it disables adjustment; removing
an earlier row retains the balancing identity at its new position. Restore imported
values preserves a saved manual choice and the balancing identity. If that identity
is absent or now protected as a transfer, automatic adjustment remains unavailable;
it never silently chooses a replacement. Edit Undo restores amounts and editing
metadata together. The identity may remain recorded after removal so subsequent
edits cannot silently select a different row.

A balanced manual edit saves on Enter. An unbalanced manual edit stays in the
editor without changing the last saved transaction. Leaving that edit, including
closing its transaction, switching transactions or quitting, asks:
“The splits add up to [sum]. Change the transaction amount from [total] to [sum]?”
**Change amount** saves the total and splits atomically, then continues the pending
action only after successful saving. **Keep editing**, Escape or dismissing the
dialog cancels that action and retains the entire edit buffer. Invalid amounts,
precision or overflow remain visible and cannot be saved or rounded. Existing
validation and transfer restrictions still apply.

A crash can lose the unaccepted buffer; the saved transaction remains balanced.
No crash recovery draft or automatic editor restoration is added. Escape in the
editor retains the existing explicit discard behavior.

The encrypted draft and entry revision metadata stores the balancing identity
and manual-mode flag alongside every complete transaction snapshot, including
Undo history. See [status](status.md) for verification.


## Amount expressions — owner Go 2026-11-03

Accepted: monetary entry supports decimal numbers, whitespace, unary plus/minus,
`+`, `-`, `*`, `/` and parentheses, with normal precedence and left-associative
binary operations. No variables, references, functions, percent or exponent syntax.
For example, `-9.34+(12*0.25)` produces `-6.34`.

Transaction totals, split amounts, opening corrections and manual transfer amounts
use the shared exact engine evaluator. Leaving a valid amount field replaces the
expression with its formatted answer; Enter/save also evaluates before submitting
an amount. Invalid input stays available for correction and cannot save. Only the
answer enters transaction storage, history and Undo; no formula or expression
history is stored. No format change is needed (48/47).

Intermediate fractions remain exact: `(10/3)*3` is `10.00`. Final results must fit
the currency's retained scale and signed 64-bit minor-unit range. `10/3` in CAD is
rejected rather than rounded. Division by zero, invalid syntax, excessive length,
nesting and checked intermediate overflow produce explicit errors. Evaluation is
bounded to 4096 input bytes, 64 recursive levels and reduced rational numbers with
unsigned 64-bit numerator/denominator magnitudes. A calculation that exceeds an
intermediate bound is rejected even if a later operation could reduce it.

Existing split balancing, manual-mode selection, transfer protections and atomic
save/Undo validation still apply to evaluated amounts. An invalid or unbalanced
edit cannot change the saved transaction. Bank-file and storage decimal parsing
remain strict and do not evaluate expressions. See [status](status.md) for tests
and platform limits.

## Amount formatting — owner Go 2026-11-03

Accepted and implemented: Settings → Current file settings offers **Use amount formatting**
and a format selector. New files enable it and derive the initial style from the
creation-time currency. The saved selection applies to every account in that file;
changing an account's currency does not reset it. Amounts retain their own currency's
scale. No currency symbol is added to ledger numbers.

Supported styles are `1,234.56` (`comma-dot`), `1.234,56` (`dot-comma`) and
`1 234,56` (`space-comma`, ordinary ASCII spaces). Turning formatting off retains
the chosen style but uses ungrouped decimal-point display and entry (`1234.56`).
The defaults are product policy, not unique regional rules for each currency:
EUR, BRL, ARS, CLP, COP, IDR, TRY and DKK use dot/comma; SEK, NOK, PLN, CZK, RUB
and HUF use space/comma; all others, including CAD and USD, use comma/dot.
Users may select any supported style.

Display uses exact integer minor units, including signed limits and zero-, three-
and four-place currencies. Ledger, sidebar balances, split editors, opening
amounts, Review, rule match amounts and history presentation share the formatter.
Entry and each numeric literal in a calculation use the active style's decimal
separator. Grouping is optional; when present, the first group has one to three
digits and every subsequent group exactly three. Grouping after the decimal
separator is invalid. No locale guessing, punctuation stripping without validation,
floating-point conversion or rounding is used. Spaces around arithmetic operators
remain accepted. Formatting happens on existing field completion/save boundaries,
not during typing. Existing exact-expression limits and final precision checks
remain in force. Imported files and canonical stored decimals retain their own
parsers; only the resulting Money value is stored.

The encrypted singleton preference is updated atomically without changing financial
rows, account revisions or ledger mutation generation. Failed saves are read back
before UI success; unknown outcomes require reopening. Preference changes invalidate
cached presentation. See [status](status.md) for verification and platform
limits.

## Text-field Home and End — owner Go 2026-11-03

All editable single-line fields use shared Home/End handling: Home moves to
position zero, End to the text end, and Shift extends selection to that boundary
while retaining its anchor. Selected text uses ordinary copy/delete/replacement
behavior. This covers setup/passwords, dialogs, search, account/category names,
rules, ledger and split editors, and the synthetic sheet. Control/Alt/Meta
combinations retain Qt's platform behavior. Completion and existing Enter, Tab,
Escape and Undo handling remain in their field-specific handlers.

The implementation uses shared Qt Quick code on all platforms. macOS verification
is recorded in [status](status.md); Windows/Linux behavior remains unverified.
Read-only multiline displays retain Qt's existing navigation.

## Delete a selection

For one linked pair, choose **Delete and keep other entry** or **Delete both
entries** from either account. The dialog identifies the other entry and shows
both options' balance changes in each account's currency. Keep preserves the
other posting and its original imported transfer destination, when it has one.
Otherwise Keep leaves it Uncategorized. Delete both takes both postings out of
the ledger and retains any original import records. The deletion and link
change save as one action. One Undo restores both sides.

Resolve pending or attached matches in Review first. If the selected transaction
has several links, or its counterpart has other splits, change the transfer
splits individually before deleting the row.

With more than one selected transaction, Delete removes the ordinary rows and
leaves linked rows in place. The confirmation lists both counts and shows the
balance change. When all selected entries are linked, select one at a time or
change their transfer categories before deleting the remaining entries.

A transaction whose link has already been released is an ordinary transaction
for this command. A pending import-match proposal still has to be settled before
deletion. Archived accounts and invalid or unavailable members reject the whole
command. A stale selection is rejected. The ordinary rows and the Undo receipt
commit together. Cancellation before that commit leaves every row in place.
Undo restores the deleted ordinary transactions in a bulk selection.

## Linked selection deletion and large-operation performance — owner Go 2026-11-06

Read-only planning and the final command both derive changes from retained rows.

Bulk edits/deletion and Undo reuse bounded prepared statements for the operation.
Committed-row writes maintain exact running balances initialized once per account;
transfer Undo reuses its current relationship set and account balances. A full
check validates the baseline and the current session. It does not reconstruct
expired ordinary history. Ordinary open does not run that check. Currency/category history queries start from
the relevant operation tables, strict Undo uses the complete member key, and draft
Undo uses an audit-local lookup capped at 65,536 members / 16 MiB, falling back to
the original query above that budget. No integrity/durability check is disabled.
Undo and committed replay share a bounded revision-to-history lookup and reuse
already-read facts, while retaining validation. The release rules database is version 1.
The clean-marker open policy is in [fast-startup.md](fast-startup.md). Full
audit still checks retained history. It does not run on every ordinary open.


## Edit refresh performance — owner Go, 2026-11-16

A memo-only edit still validates and saves the complete transaction and its Undo
record atomically. When amounts, dates, allocations, other fields and transfer
relationships are unchanged, it retains prepared import readiness and replaces
the edited pending draft image after commit. Review text exclusions still use
the new memo. Other edits retain their full readiness refresh.

Unchanged accepted entries survive an ordinary edit without relationship changes.
Current row reads are reusable only at the same connection change count; rollback
clears them. Account facts are bounded to the current operation and invalidated
by writes or rollback. Transfer candidate eligibility compares the relevant
financial fields and the opening-balance payee classification, while text
exclusions are evaluated separately. Category display labels are reused within
one view build.

These are in-memory optimizations. The release database remains version 1;
commit synchronization, financial validation, recovery checks and Undo are
unchanged. Measurements and verification limits are in [status](status.md).

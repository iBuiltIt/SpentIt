# Unified Review sheet

Last updated: 2026-11-21

The release rules database is version 2. Incomplete preparation is cleaned up on
cancellation or the next open. Completed uncommitted imports remain. Accept
transactions accepts the file. Account Discard is account-owned.

Each unresolved explicit transfer gets a generated entry, including when a
plausible imported posting exists.

- [Workspace and actions](#review-workspace)
- [Duplicate eligibility](#common-duplicate-policy)
- [Transfer suggestions, persistence, and Undo](#persistence-and-undo)
- [Automatic Merge and Link](#automatic-one-to-one-review)
- [Staging and applying decisions](#pending-transfer-decisions)

## Selected-row Merge correction

A duplicate group has one Merge action. For groups larger than two rows it is
unavailable until at least two rows are checked and submits exactly those
identities. It reads **Merge N selected rows** when one selected row is kept
automatically, and **Merge N selected rows…** when keeping a row opens a dialog.
The competing whole-group Merge and Merge selected rows actions are replaced by
this one action. A two-row group continues to offer **Merge 2 rows** without
requiring checkboxes.

When a survivor choice is required, show only eligible rows from that selected
set, with full wrapped categories for every split, split amounts/memos, transaction
memo, account, date, payee, amount/currency and New/Accepted status. A separate
Keep this transaction button chooses the survivor. The dialog states how many
selected transactions become one and how many unselected transactions remain
unchanged. Cancel writes nothing. Existing automatic-survivor protections,
financial validation, atomic commands and Undo remain unchanged. Batch inclusion
uses the same selected identities and the richer survivor dialog.

## Review workspace

Review puts supporting tools at the top, question-specific actions
beside their items, and one **Accept transactions** action in a footer that states
its all-account scope. **Accept transactions** stays at the bottom right even
when there are no new transactions and a result mark remains. Hide unavailable
actions; remove the Review-only Close button and the read-only imported-category
catalog button/dialog. Keep explicit Close or Cancel on dialogs. Required
import-time missing-category decisions stay.

- Group questions into possible duplicates, possible transfers and import
  problems, preserving account order within each kind. Merge/Link/Keep separate
  name the action. Select all merges includes only groups with enough selected
  rows and an unambiguous survivor. Apply decisions saves the selected merges
  and transfer decisions together and preserves the visible question anchor.
- Acceptance is direct without concerns. Outstanding questions or acceptance
  warnings open a dialog; Accept anyway is available only when all warnings are
  overridable. Failed imports remain visible and may remain after acceptance.
  The old selected-import “blocking commit” dialog is removed. Dataset-wide
  acceptance concerns not already represented by questions appear inline.
  Unmatched transfer entries are the exception in the section below: Review
  omits them from that list and counts them in the Accept dialog.
  A transfer whose destination account is not in the file is the warning list
  beside **Accept transactions**, described below.
- Undo import lists descriptions with separate **Undo…** buttons, retaining the
  existing preview/confirmation. The account picker filters imports; undo affects
  the entire selected source, potentially including other accounts.
- **Hidden transfer matches…** in Settings closes with Close; contextual **Hide
  suggestions…** remains beside transfer candidates. Exclusions do not delete rows.
- Set up imported transfers appears only for unresolved mapped transfers and
  explains matching and counterpart creation before confirmation. The existing
  command matches within the selected import and then checks explicit transfers
  across pending imports. The confirmation therefore discloses file-wide scope;
  it is an explanation, not an exact per-posting dry-run preview. No matching,
  generation, acceptance, storage, or format policy changes are introduced.

## Missing destination warnings

A transfer allocation whose destination name identifies no account in the file
is a warning. It is not an import-problem row and it has no **Correct** action.
Review shows **Warnings [n]** beside **Accept transactions** when n is at least
1. Each line names the account, the date in the file’s date format, the payee
when the transaction has one, and the destination, and says that name is not an
account. Choosing the line opens that ledger row. The sidebar button and the
Review heading read **Review (questions)** and add **[warnings]** only when a
warning remains. While Review is still checking, both read **Review**.

**Accept transactions** still refuses until that account exists and the transfer
is linked. There is no Acknowledge action. **Accept anyway** stays unavailable
while one of these warnings remains. A destination that names an account already
in the file, and an ambiguous account name, stay on their existing review path.
**Set up imported transfers…** stays available.

## One question per transfer

Review shows each candidate pair once, including on cards with several
possible partners. When both directions offer the same pair, keep its
outgoing card's choice. If both rows or neither row have an outgoing
allocation, use occurrence order. A choice offered in only one direction
stays on its original card, including when it starts from a deposit. A card
that loses all its choices disappears. An imported row still offered as a
partner does not get its own unmatched card for the generated entry.

Separate payments retain their own choices. For two indistinguishable
withdrawals and two indistinguishable deposits, Review starts with two
outgoing cards. Checking one pair stages that link and checks the remaining
mutually unique exact pair. The existing pending-decision and allocation
checks still apply.

**Not this transfer** on the kept question records the pair, so the other
direction does not return. **Link** is unchanged. The Review count is the
number of questions after this collapse. Duplicate groups and import-problem
cards still count. Missing-destination warnings stay in their own count.

If an edit breaks a saved Link, Review can offer those postings again when
they meet the current matching criteria. A saved **Not this transfer** decision
still suppresses its pair until Undo or an explicit ledger Merge for that pair.
See [Merge after Not this transfer](#merge-after-not-this-transfer).

## Link card

**Link** joins the two imported lines on the card and removes the created
entries. The other button reads **Not this transfer**. It records that this
pair is not the match. Both imported lines stay. The created entry stays.
Neither line becomes ordinary spending. A one-row card for that created entry
can remain, and it can still offer **Accept without bank match**.

**Accept without bank match** is not shown on a card that already lists the
other imported line. It stays on a card that shows only the transfer. **Set up imported transfers…** stays.

## Unmatched transfer count

Review does not list a transfer that has not been matched to an imported
transaction above the questions. That line does not open the transaction.

**Accept transactions?** replaces those lines with one line for each account
that has at least one. One entry reads **Account. There is 1 unmatched
transfer transaction.** More than one reads **Account. There are N unmatched
transfer transactions.** N is the number of transfer entries in that account
that still have no bank match. It is not the Review question count.

**Accept anyway** remains the action when every warning can be overridden.
**Return to review** closes the dialog. A warning that must be fixed stays on
its own line, and **Accept anyway** stays hidden while one remains. Question
cards, missing-destination warnings, and **Set up imported transfers…** stay.

## Ledger transfer actions

**Set up imported transfers…** is in Review. Ledger **Link** joins two existing
transactions: select one, switch accounts, select the other, then **Link**
again. A transaction with one split can be linked that way. A transaction with
more than one split still needs the split chosen; Review uses **Split to link**
on a link question. Cmd+Z undoes a Link while that Link is the newest saved
action. The Undo on an open Change history row stays. Review has no
newest-action Undo button.

**Accept without bank match** stays on a Review card that shows only the
transfer. An Import problems line that names a transaction still opens that
transaction. A line that does not name a transaction opens that account’s ledger.

## Newest-action Undo buttons removed

The ledger has no Undo button beside search, no **Undo category change**, and
no **Undo review**. Review has no copy of that newest-action button. **Undo
import…** stays.

Cmd+Z, and Ctrl+Z on Windows and Linux, undoes only the newest saved action,
with confirmation for import rollback. The Undo on an open Change history row stays.
**Undo opening or currency…** in the account menu stays. Rules has no **Undo
application** button. A rule application is undone with that same shortcut
when it is the newest saved action. The saved rule remains.

The ledger duplicate bar opens for a possible duplicate in the current account,
or while that check is still running. A category change can still be the newest
review-stack action. That alone does not open the bar.

## Ledger duplicate resolution

Accepted: keep the existing matching policy and Merge source-association storage.
The losing posting's imported representations continue to attach to the survivor;
this work does not change retention, formats, clean-open verification or close policy.

The ledger gives possible duplicates a thin rose edge. Once a match is chosen,
the two rows in the active pair get a rose outline and the labels **Selected**
and **Match**. Selecting a marked row offers its direct candidates. When there
are several, choose one by clicking its ledger row or using the candidate
picker. The picker shows **New** or **Accepted** and the balance after each
posting, including when the ledger filter hides that row. The balance helps
locate a row. Compare the original records to decide whether the entries are
duplicates.

The comparison bar identifies the active pair and shows which row Merge will
keep when the survivor is determined. It offers **Merge**, **Keep this pair
separate**, **Show match**, and **Next possible duplicate**. **None of these
match** rejects the selected row's direct candidate comparisons. Right-click
**Resolve duplicate** exposes the same actions. Read-only files cannot resolve
questions. Review discovery runs automatically in the document worker after
open and import.

Rows remain in ledger order with surrounding transactions visible. Show match
scrolls to an available row; revealing a hidden match explicitly clears the filter.
Review retains its overview and batch controls, adds New/Accepted styling and
Show in ledger. Navigation and inspection do not answer questions. Existing unsaved
edit guards apply. Merge and Separate retain the viewport; Merge selects the survivor.
Cmd+Z / Ctrl+Z reverses the decision through existing session Undo. The ledger
duplicate bar does not offer a separate Undo button. Navigation to another
question is explicit, not an automatic jump.

The survivor rules below supersede the earlier committed-only default.
A unique protected row (linked, manually entered or accepted) is retained
without a survivor prompt. With no protected rows, a unique row from the earliest
import is retained even if still new. Multiple rows from that earliest import or
conflicting protected rows require an explicit choice; a losing row's account link
cannot be discarded by Merge. Domain validation rechecks amounts, membership,
generation, survivor eligibility and relationships. Existing manual-original
attachment restrictions remain enforced.

Focused Separate uses existing encrypted Review choices/receipts and atomic Undo:
two selected identities reject that pair; one selected origin rejects its current
direct candidates. An empty selection retains Review's group-wide No behavior.
Other comparisons remain outstanding, including clean remainders from partial
Merge. No schema change or widened automatic matching is introduced.

## Resolution performance and automatic discovery

Accepted: each decision remains immediately durable and individually undoable.
Do not introduce accumulated unsaved resolutions or change Merge source retention.
Review discovery is automatic during the worker's open/import refresh; report
Checking for possible duplicates… while that refresh is in progress. A completed
open/import must publish the question count and ledger markers without requiring
a visit to Review. Discovery is not an integrity audit and does not change the
clean-marker verification policy. The permanent status area shows Selected: n
for the ledger (including report detail), independently of operation/error text.
Opening Balance is excluded and existing filtered selection semantics apply.

Reuse unaffected validated committed drafts during ordinary Review resolutions;
cache only in memory, under a bounded memory budget. Failed/other mutations must
invalidate reuse. Transactional reads, full recovery, explicit verification and
backup validation must not consume that presentation cache. Keep the existing
matching predicate and all source/relationship checks. Test actual command-to-UI
latency using roughly 19,000 transactions, a 15,000-row main account, many retained
transfers and nine outstanding duplicate questions, not just small functional
fixtures.

Within a Review build, transfer discovery reuses each posting's opening-balance
classification and each category and counterpart eligibility result. Candidate
order, date ranges, amount checks, and the existing per-posting limit still
govern the suggestions published at open and after edits.

Review reads ledger rows without calculating their display-only source order.
It reads the newest accepted original dates only when a possible adjacent-day
comparison needs them. An accepted memo edit can refresh the affected cards
from the saved row when the current sheet is reusable. Applicable memo
exclusions, dependent duplicate decisions and changes to transfer or financial
inputs require a rebuild.

## User interaction

Review is above Categories as the first sidebar action. Review and Commit open
the same compact dataset-wide workspace. Within a question type, use account order. Import does not automatically open Review. A row can show its splits.
**Inspect** opens that transaction in its account ledger, among the surrounding
transactions, and leaves the transaction editor closed. **Back** returns to
Review with the question that was on screen. If an edit removes that question,
Review shows the next question that remains. Inspect keeps Include match and
checked rows for that return. Leaving Review another way clears them. A ledger
search that would hide the transaction is cleared. Enter on a question and
double-clicking a payee use Inspect. For a repeated-payee import warning,
**Inspect** also opens the ledger editor so the payee can be edited. Saving a
manual payee correction clears the Review warning while the transaction stays
New. Undo restores the warning when it restores the prior payee and its
ownership. The original QIF remains available in the ledger. Link questions
stay in Review. Ledger Link joins two existing transactions. The count is outstanding questions, including correction tasks, rather
than candidate transactions. Changing account/import, inspecting a record or
attempting Commit does not resolve a question. The selected-import summary is
explicitly scoped and no longer supplies a competing unresolved count.

Same-account groups offer **Merge** and **Keep separate**. A larger group
requires at least two checked rows for Merge. Unselected rows remain together when
at least two remain. Exactly one committed selected row is the survivor automatically;
otherwise the user chooses which row to keep. Merge preserves that row's current
fields, splits and relationships. Different effective totals require correction
first; a losing row with an existing transfer relationship cannot be merged.

Link questions show the transaction, candidate accounts, and the split
allocations where relevant. Changing a candidate resets the split selectors
to the new unique exact pair, or to **Select split** if there is none.
For unequal eligible allocations, Link opens a choice to split the larger
allocation, categorize the remainder and link the common amount atomically.
Both account totals and unrelated splits remain unchanged. Independently
supported postings are retained. See [pending decisions](#pending-transfer-decisions)
for current selection defaults.

**Repair record** opens the existing account ledger editor for repairable date
or amount problems and retains the inclusion or duplicate decision after saving.
**Review duplicate** brings a record that needs a duplicate disposition to that
editor, including a reimport of a previously removed transaction. **Inspect**
opens the ledger for transfer issues. Import-level issues offer **Choose
categories** or **Resume import** when available. **View source** displays the
original text for records that cannot be repaired. Review identifies what was
excluded from the balance and points to Undo import. Accept can take valid
transactions while failures remain visible. Review remains accessible when its
count reaches zero if a Review decision can still be undone.

The ledger has separate **Merge** (Alt+M) and **Link** (Alt+L) commands. Merge uses
the same survivor and persistence rules. Link retains the existing allocation
selection and cross-account navigation workflow.

## Batch Merge and stable status

Every right-pane workspace has a permanent bottom status line. Progress, success
and errors occupy that reserved space; long messages expose Details. Review keeps
its visible group anchor (or the next surviving group) when resolved groups leave.

**Include match** is shown only on a two-row duplicate question. It selects that
question as pending UI intent. A question with more than two rows has no Include
match control. Check the rows to pair. **Apply decisions** includes every
two-row question marked Include match and every larger question with exactly
two checked rows whose obvious kept row is the single accepted, manual, or
linked row. Use a larger question's Merge button for other selections. The
button merges the checked rows and asks
which row to keep when none is chosen automatically. Changing the
checked rows clears that question's Include match staging. Zero or multiple
committed selections on a two-row question require choosing a survivor; one
committed selection keeps that row automatically. Numbered selected-group labels
correspond to validation errors. Leaving Review clears selections. Inspect keeps Include match and checked rows
until Review is shown again, or until that ledger visit is left for another
workspace. Selections that still match a refreshed question stay available.
Selections are held for the current Review visit.

Two rows left after applying a partial merge come back as a two-row question, with
Include match. One row left leaves the question and stays in the ledger. That
is not **Keep separate**.

A partial Merge keeps an unanswered remainder of at least two rows visible even
when those rows were clean in their original import. Existing encrypted unresolved
choices seed that remainder; the same engine comparison still decides its edges.
A later explicit answer retires the remainder, and Undo restores it. This does
not seed suggestions in a fresh clean base import without a prior partial decision.

Under the 2026-11-06 owner Go, the repository command has no fixed group or
selected-row count ceiling (including the Undo reader). It rechecks their generation, membership, amounts, survivor and relationship protections,
and saves all or none. Cancellation is available while staging, with a final atomic
seal immediately before COMMIT. Cancellation that wins that race rolls back all
staged work; a sealed commit completes and is reversed through Undo instead.

One parent Review receipt owns the batch's Undo stack entry. Each group has its own
child answer, choices, source associations and financial-history members, so answering
one group never records an answer between unrelated groups. Existing Review tables encode parent/child relationships in reserved answer question
values (`batch`, `batch-child:<operation>`); no schema change is needed. Child IDs
are derived from the parent's operation identity. Exact request receipts reject
changed retries. Undo restores children in reverse order in one transaction and
preserves earlier mutation barriers. The caller refreshes its snapshot once.

Profiling found repeated source loads and saved-choice queries in the same-account
peer comparison loop. Review now loads comparison facts once per build and evaluates
the unchanged engine predicate before querying saved choices. This eliminates
irrelevant database work; it does not change duplicate or transfer matching policy.
The loop can still compare many pairs; this is not a constant-time performance claim.
Use the [diagnostics guide](agent-diagnostics-guide.md#attribute-elapsed-time) to measure the current build.

## Common duplicate policy

All supported parsers produce common original facts. Same-account ordinary
amount-based duplicate suggestions require equal original currency, exact signed
original amount and the same known calendar date, independent of QIF/OFX/QFX/QBO/ASO
labels. Payee/category differences do not disqualify a review candidate. Unknown
original amounts/dates retain their explicit uncertainty paths; no date or amount
is fabricated. Different known dates do not qualify an ordinary same-day candidate.

A new row that missed the same-day comparison can still join a review question
when its original date is older than the newest accepted original date in that
account. The other row must share the account, currency, and exact signed
original amount, and the known dates must be one calendar day apart. If that
new row touches a same-day group, the question also takes other ungrouped
same-amount rows within one day of the group's members. That step runs once.
Consecutive days do not collapse into one question. A same-day pair that
nothing touches stays its own question. **Select all merges** skips a question
whose dates differ. After a partial merge, leftover rows of that question stay
together when they still share the amount, including when those dates are two
days apart. Shared bank IDs still match across any dates. Disjoint bank IDs
do not. Manual rows stay on the same day. Automatic recognition does not use
this window.

Shared original or retained bank IDs remain a separate evidence-based reason to
review conflicting facts. Disjoint nonempty bank-ID sets disqualify candidates.
Multiple retained IDs remain available to the matcher after Merge; they must not
be erased into an apparently ID-less record. Automatic recognition still requires
strict original ID/currency/amount/date/type agreement or the existing unique
exact original date/currency/amount/payee key. Retained IDs alone cannot authorize
automatic bank recognition. Original evidence, target liveness, account scope and
one-to-one occurrence guards remain authoritative.

The engine owns these decisions. Storage candidate queries retrieve a conservative
superset from eligible account rows; both import classification and row candidate
lists filter through the engine matcher. Review uses the same matcher for additional
same-import/unresolved peers, and the one-day pass above for the rows that miss
it. SQL contains no format-based matching exception. The separate transfer Link
and grouped reciprocal-transfer policies are unchanged.
A recorded explicit duplicate decision retains its date override during history
validation; it is not reclassified as corruption just because suggestions tighten.
New import/review candidates never receive that override. Existing stale-target,
account, amount/currency and bank-ID checks remain. No timezone inference,
per-account date setting or schema change is introduced.

## Persistence and Undo

See [release schema](release-schema.md) for the current file format and
upgrade policy.

Review decisions, source associations, row changes, link changes and their history
are saved atomically in encrypted storage. No success is reported until durable
completion; an uncertain outcome closes the session if its receipt cannot be read.
Operation identities support retry without repeating a saved operation.

Merge retires extra postings and retains their source representations on the
survivor. Exact original-field recognition now searches those representations,
while preserving original account/currency, uniqueness, bank-ID conflicts,
occurrence multiplicity and removed-target guards. Suggestions do not broaden
automatic exclusion. Manual link suggestions compare eligible opposite-sign allocations in the same
currency within seven calendar days inclusive. Equal magnitudes qualify; unequal
amounts require a resolved explicit outgoing destination, outgoing greater than
incoming, and a difference no greater than 4% of outgoing. No payee/memo keyword
is treated as destination evidence. Ordinary categories on either allocation,
including unmapped imported category text, disqualify it from suggestions. Empty
or Uncategorized/Uncategorised allocations remain eligible. Account categories
must resolve to the other endpoint's account on both sides. Split eligibility is
independent per allocation; acceptance rechecks the selected allocations. Exact
case-insensitive Opening Balance and Starting Balance payee labels are excluded
from suggestions. Category edits refresh candidates. Manual Link and known-transfer
waiting tasks remain available. More than 4096 candidate
counterparts produces a correction task requesting manual selection.

Cmd+Z / Ctrl+Z reverses decisions in order. Text editors
retain their own Undo. Each reversal restores financial rows, associations,
relationships and question state atomically; it retains persistent history.
Individual financial children cannot be undone outside the grouped decision.
See [file-wide Undo and Redo](action-undo-design.md) for ordering and session
lifetime.

Link exclusions are immutable encrypted events with preview, save, edit and remove
operations. They match Payee or transaction/split Memo, with dataset-wide scope by
default and optional endpoint-account scope. Exact matching is whole-field Unicode
caseless, with no whitespace or punctuation normalization; regex uses the bounded
adapter. Either endpoint can suppress a suggestion. Saved No decisions remain
separate. Exclusions neither hide waiting transfers nor change duplicate recognition
or manual linking. Rules-disabled documents expose the same Link suggestions section.

Resolved explicit transfers create generated counterparts in the import transaction
and retain a waiting Review task until bank evidence or explicit acceptance. Review
acceptance can be undone. Generation also applies when ambiguous plausible imported counterparts exist,
as explicitly chosen by the owner. Temporary duplicate balances remain visible
until matching or linking removes the generated duplicates. Linking two independent postings safely
retires generated duplicates, and Review Undo restores each account's exact balance.

### Batched counterpart generation

The owner authorized optimizing the measured generation bottleneck while preserving
this policy. Generation requires an existing atomic import operation. It retains the
current transfer set across appends, reuses SQL statements and each origin account's accepted
starting balance, and appends draft links to a local working set. Each link still
uses the engine's generation validation and writes its own request, transfer version
and financial-history entry. Draft-only generation cannot change accepted balances.
Account writability, currency, operation collisions and sequence limits remain
checked. The complete resulting relationship set is validated before the caller can
commit; cancellation or failure discards all staged children through the existing
transaction. The working set and balance reuse are local to this invocation, so
rollback cannot leave them available to a later attempt.

Review construction runs on the existing import worker. Cached raw candidate edges
are updated for old and new date/amount neighborhoods when rows or occupied
allocations change; alias changes invalidate all edges. Exclusion and saved-answer
filtering is separate. Restart reconstructs the cache from persisted state. Reading
the current rows still has linear cost. No claim of native large-dataset
responsiveness is made by the functional fixtures.

## Verification

Run `import.review_sheet`, `import.review_policy`, `import.review_batch`,
`import.ledger_duplicates`, `import.automatic_review`, and the matching
durability sweeps. Cover amount differences, repeated occurrences, source
associations, both accounts' exact balances, reopen and exact retries.
Use the focused Review cases in [Import UI tests](../tests/ui/import_model_tests.cpp)
for staging, keyboard and viewport behavior. See [building](building.md).

## Ledger same payment and result marks

This section supersedes the ledger right-click **Resolve duplicate** sentence and the sentence that ledger **Link** joins any two selected transactions.

A payment Review would ask about as one transfer, and that is shown twice in one account, gets a blue edge on both rows. One row is the bank transaction. The other is the extra entry created from the other account. Rose stays the mark for a possible same purchase. The bar above the ledger names the payment and offers **Merge**, **Not this transfer**, **Show match**, and **Next same payment**.

The ledger command for two rows in the current account is **Merge**.

- A suggested purchase keeps one transaction. **Keep this pair separate**, **None of these match**, and **Show match** stay on the bar.
- Two selected transactions that are not that payment use the same keep-one Merge.
- The two rows of a payment shown twice use the same join as Review's **Link**. Merge keeps the bank transaction, removes the extra entry, and connects the other account's payment. It does not ask which row to keep. A payment with more than one split is answered in Review with **Split to link**.
- An entry created for a transfer, selected with a transaction that is not its pair, is not offered the keep-one dialog.

**Link to other account…** is offered for one selected transaction. Choose it, switch accounts, select the other transaction, and choose it again. Both transactions stay. It is not offered while two transactions in the current account are selected.

The ledger shows result marks:

- An indigo edge means this transaction was kept when another was merged into it. The tooltip says **Merged**.
- A green edge means this transaction is linked to a transaction that stays in another account. The tooltip names that account.
- A created entry that is still waiting for a bank transaction has no green edge. Choosing an account as the category, which creates the other side, has no green edge. A transfer paired from the imported files has the green edge until accept.
- A row can show both result marks. The marks remain after close and reopen. Undo removes the merge or the link, and the mark goes with it. **Accept transactions** clears the marks. A linked payment still shows the other account on its category.
- Change history lists the action until the file is closed. A keep-one merge and a same-payment merge are **Merge**. A link that leaves both transactions in place is **Link**. Undo on that row reverses it. Command+Z still reverses only the newest action.
- The ledger menu **Unmerge** is available on one joined row, including after the file is closed. On the kept row of a single keep-one merge, it puts the transaction that left back on the ledger as it was at the merge, and leaves a later edit of the kept row in place. If that row is also linked, the keep-one merge is reversed first and the link stays. On one green linked row with no keep-one merge to undo, it asks before separating that link. If the join removed an extra entry, the message names every account that gets a row back. The blue edges come back on those rows while they are still new. Merge joins that payment again, and Review shows both lines with Link and Not this transfer. After Accept, Unmerge stays available on that payment. The accepted payments stay accepted, the extra entries come back and are not accepted, and those accepted rows have no blue edge. Review still shows both lines. If both transactions stayed, the message says both stay and they will no longer be one transfer. Ambiguous merge or link histories stay unavailable. **Unmerge** is its own Change history row. Undo of that row puts the join back.

Review's duplicate questions and transfer questions stay as they are.

### Merge after Not this transfer

Select the imported bank posting and the extra entry SpentIt created for the
payment in the same account, then choose **Merge**. When the current transfer
matching rules allow one exact allocation pair and a saved **Not this
transfer** decision hides it, the confirmation shows both bank postings.
Confirming keeps and links the originals and removes the extra entries for
that payment. A generated entry already accepted without a bank match is
outside this override.

The explicit choice supersedes the rejection for the selected two bank
postings, including when the rejection covered several possible partners.
Other saved decisions remain effective. The merge saves as one **Merge**
action, including when Auto Link is on. Undo restores the original
relationships and makes the prior rejection effective again. Redo replays
the same pair. This action is available after closing and reopening the file
while the selected entries remain eligible.

### Unmerge safety and retries

Restoring removed transfer rows requires every affected relationship to match
its recorded merge result, allowing acceptance alone. A later relationship
change, including a corrected date, blocks Unmerge until that change is undone.
This check applies to both accounts and the released generated relationships.
A retry of a completed Unmerge returns its existing receipt without another
mutation; changing the selection or reusing another command's identity fails.
The ledger obtains availability and confirmation text from one preview.

### Unmerge after acceptance

In the rules-enabled application, a keep-one Unmerge restores the excluded
transaction as New even if its original batch has been accepted. It keeps the
original posting identity and bank evidence. The kept transaction retains its
acceptance and later edits. The displayed balance includes the restored amount
immediately. The accepted balance changes when the restored transaction is
itself accepted.

Restoration, source reassociation, and the Unmerge Undo record save together.
Undo rejoins the postings and Redo restores them again. Ordinary file-wide
acceptance can publish the restored row separately. The original publication
still records its historical exclusion. See [release schema](release-schema.md).

## Automatic one-to-one review

Settings has two checkboxes immediately above **Hidden transfer matches…**:
**Automatically merge one-to-one duplicate suggestions** and **Automatically
link one-to-one transfer suggestions**. Both default on.
These preferences are local to this file path on this computer. A path with no
saved choice, including a copy opened from another path, starts with both on.
A saved choice stays with that path.
Changing a preference preserves Undo and clears pending Redo.

After an import finishes its category choices and rules, take one fixed Review
snapshot. Only pairs involving that import are eligible. Plan mutually unique
same-calendar-day pairs first, reserving both endpoints.
Then inspect the remaining candidate relationships once for mutually unique
adjacent-day duplicate pairs and transfer pairs throughout the existing seven-day
window. The second pass counts all remaining competitors, including farther
transfer dates; it does not repeatedly eliminate choices. Mirror questions count
as one pair; generated transfer entries use the existing Link command.
Ambiguous relationships stay manual. A wider question may contain a unique
same-day pair; question size alone does not disqualify it. Both passes are planned
from the one snapshot before any writes. Opening a file, changing a setting,
editing a row,
and Undo do not trigger automatic decisions.

A duplicate pair needs equal known original amounts and current totals, and an
unambiguous survivor under the existing protection and import-order rules.
Same-day pairs retain equal original and current date checks. Adjacent-day pairs
must satisfy the engine's existing adjacent-day suggestion predicate, including
bank-ID and manual-row restrictions; both current dates must still equal their
own original dates. Duplicate suggestion discovery and its accepted-date frontier
are unchanged. The survivor keeps its date; no date is invented or corrected.
Rows already in a keep-one merge stay manual for duplicate Merge. An accepted
posting in an active transfer can be kept in an automatic Merge with an
unaccepted, unlinked posting from the current import. The original payees must
match exactly and be nonempty, and both postings must pass the same duplicate
checks above. The full payee is compared as text. Account numbers in the payee
are never interpreted as transfer destinations. The accepted posting keeps its
date, splits, and transfer link. Other transfer-related duplicate choices stay
manual.
Links require exactly one eligible allocation pair with exact opposite, nonzero
amounts in the same currency. Either transaction may have other splits; their
amounts, categories, memos and identities stay unchanged. The plan records the
selected allocation identities and rechecks them before saving. Multiple eligible
allocation pairs, occupied endpoints, and amount differences stay manual. Existing
suggestion exclusions and category/destination/date constraints still apply.
Each planned pair is revalidated from its two transactions, including Unmerge,
before it is written. Resolving one pair does not add another. A pair that is
no longer a clean one-to-one match, or that cannot be safely unmerged, is not
saved.

All of these pairs are one save inside the import. They are not separate Undo
steps. Undo of the import removes the import and these pairs together. Unmerge
still breaks one link or one merge on its own, and that Unmerge is an ordinary
Undo step. When the kept posting also has a transfer link, Unmerge restores the
duplicate posting first and preserves the transfer.

While this step runs, the progress text says **Checking duplicate and transfer
suggestions**, then **Merging duplicate suggestions, N of M** or **Linking
transfer suggestions, N of M**. Cancel is available again after the import
rows have been saved. Cancel saves none of the automatic pairs. The import
stays, and the summary says to review the remaining matches. A failure does
the same: the import stays, and none of the automatic pairs stay.

These are automatic Merge/Link decisions, not Accept transactions. During an
import they are written with that import, not as later Undo steps. Import Undo
removes them with the import. Unmerge remains available for one pair. An
uncertain durable outcome closes the session for recovery.

The indigo **Merged** mark appears on the kept transaction after a manual or
automatic keep-one Merge, even if that transaction was already accepted.
It remains until the next **Accept transactions**, Undo of the merge, or
Unmerge. The green linked-account mark for automatic links also remains until
a later Accept, including when a linked endpoint was already accepted.
Marks persist across close and reopen. Accept is available when only these
marks remain. Undo of Accept restores the marks it cleared.
An accepted survivor can be unmerged when the removed row and source
relationships still satisfy the existing checks. Successful close expires
ordinary Undo while preserving the result marks and eligible Unmerge.

For an already accepted transaction, the merge must be newer than the latest
non-reversed Accept to show **Merged**. This also applies to merges saved
before this change. Automatic Review requests carry an optional `|auto` suffix
in the existing request payload, which still identifies automatic links.
Close retains the latest non-reversed Accept request as the mark boundary,
without keeping its ordinary Undo stack or changing the schema.

### Reopen an automatic link after a later import

A later import can add a competing match for an allocation that Auto Link
previously paired. Before applying new automatic decisions, check eligible
automatic links made since the last Accept. If both linked postings are New
and the previous relationship can safely be restored, reopen the link and
show the choices in Review. Existing date, amount, currency, category,
destination and hidden-match rules still apply. All independent postings
remain available. The import summary directs the user to Review.

For example, an outgoing posting can link to the only equal incoming posting
seen so far. If another eligible incoming posting arrives, Review offers both
bank postings. The next automatic matching pass leaves the reopened choices
for the user. Accepted links and links chosen by the user stay in place.

Reopening shares the import's automatic matching save and Undo. Cancelling
that save leaves the imported rows and the earlier link. Undoing the import
that reopened the match restores the earlier link, including after closing
and reopening if the import is still eligible for Undo. A conflicting later
edit or relationship prevents restoration. See [import Undo](import-undo.md).

## Automatic follow-on links after a manual Link

When Automatically link one-to-one transfer suggestions is enabled, manually
linking a Review pair through an entry point that supports follow-ons also
handles the remaining unambiguous pairs in that
connected group of transfer suggestions. Overlapping cards belong to the same
group; unrelated groups are not included. With the setting off, only the selected
pair is linked. The Review card's Link button always saves only its own pair.
See [pending decisions](#pending-transfer-decisions).

Capture the group's membership before the manual decision. Within one database
transaction, write the manual Link, refresh Review once, and plan the remaining
pairs using the existing same-day priority and seven-day transfer window. Both
endpoints must belong to the original group. The manually selected transactions
cannot be selected again, but any remaining eligible splits on them still count
as competing candidates. All amount, currency, category, destination, exclusion,
relationship, unique-allocation and Unmerge safeguards remain. Other splits stay
unchanged. A remaining 2:2 or 1:2 group stays manual; there is no repeated guessing
or equal-count pairing. Automatic follow-ons do not recursively trigger discovery.

The manual Link and its follow-ons commit together as one ordinary Review action,
with one Undo step. Cancel or failure saves none of them. Redo replays exactly the
saved pairs, regardless of preferences, without discovering more. Unmerge can
still break one pair; Undo of that Unmerge restores it. Neither Link nor its
follow-ons accept transactions. Automatic result marks retain their existing
Accept behavior. A summary reports additional linked pairs and that Undo reverses
them together. Progress and cancellation remain on the existing worker path.

Batch parent/child receipts hold the group, including all link
snapshots. Exact retries return the saved pair count; changed requests are refused.
No schema change or new financial history format is introduced.

## Pending transfer decisions

Review keeps three separate states: undecided, Link to a chosen partner, and
Not this transfer. Checkboxes show the actual pending partner. Unchecking a
chosen partner stages Not this transfer; it does not save. **Mark not a
transfer** stages that answer for the card, including every suggested partner
on a multiple-choice card. **Decide later** leaves the card out of the batch.
Each card states its pending decision.

Mutually unique one-to-one exact-allocation suggestions start checked.
Differing-amount suggestions start unchecked, even with only one candidate.
Single-amount candidates show **Amounts differ — review before linking**.
Choosing the candidate enables Link, which opens the difference question;
Apply decisions does not save that card. This supersedes the automatic check
for differing amounts. Checked cards
sort before multiple-choice cards, retaining their place when unchecked or
set aside. Other cards start undecided, including a multiple-choice card with
one same-day suggestion. Choosing a partner can check a remaining mutually
unique partner on another card. Related cards stay adjacent in their original
suggestion group. Inferred links do not move into the initial one-to-one
section, and their pending label identifies them as suggestions. Choosing a
different partner releases and recalculates automatic choices in that connected
group. Mark not a transfer, unchecking a partner, and Decide later only clear
dependent automatic checks.
Cleared suggestions stay undecided across refreshes until a positive partner
choice permits inference again. Explicit choices, rejection, and Decide later are
never overwritten by this convenience; unrelated pending decisions stay intact.
A transaction can have only one pending link.

**Apply decisions** in the Review header saves selected duplicate merges,
links, transfer rejections, and checked **Accept without bank match** decisions
in one atomic operation and one Undo step. The header summarizes what will be
saved. **Clear selection**, immediately to the right, clears the checked rows
and staged choices, including preselected link suggestions. A refresh keeps
those suggestions clear until a new partner choice permits inference. Use
per-card Link for split choices or amount differences. Untouched multiple-choice
cards and Decide later cards stay open. Failure or cancellation rolls back the
whole batch. Redo replays the saved decisions.

The per-card **Link** and **Not this transfer** still save immediately. Review
card Link applies only that pair, even with Auto Link enabled, so other staged
decisions remain pending. Other existing manual-link entry points retain their
Auto Link behavior. A refreshed Review preserves unrelated decisions by stable
transaction identity and unchanged row facts; a changed pending decision on a
remaining card is flagged for a new choice and excluded from Apply decisions.
Resolved cards leave the pending set. Inspect preserves the pending set;
leaving Review by another route clears it. These are in-memory choices, not
saved drafts that survive closing or restarting.

## Unavailable transfer choices

A partner checkbox that conflicts with another pending link stays visible but
is disabled and dimmed. Releasing that choice immediately restores availability.
Transaction details and Inspect remain available. The checked partner can still
be unchecked, and switching partners can still release and recalculate automatic
suggestions. The visual state follows the same conflict check as selection;
matching and saving behavior are unchanged.

## Batch accept without a bank match

On a waiting-only transfer card, **Accept without bank match** starts unchecked.
In **Unmatched transfers in**, choose the account that holds the generated
entry. **Select all** checks its waiting-only cards. Individual checkboxes and
**Deselect all** let the user change that account's selection before saving.
Transfers with candidate matches use their existing link controls and offer
**Hide suggestions…**.

**Apply decisions** saves checked unmatched transfers with the other pending
Review choices in one atomic operation and one Undo step. It records the
decision without creating bank evidence or accepting a New transaction.
**Accept transactions** remains a separate file-wide action. Redo replays the
saved decisions. Cancellation, stale choices, and failure follow the same
rules as other pending decisions.

## Accounts that don’t need bank matches

In **Settings → Accounts that don’t need bank matches**, add the account
that holds the entries SpentIt creates for transfers. For listed active
accounts, waiting-only cards are omitted from Review and their generated
entries are accepted without a bank match during **Accept transactions**.
Existing and future waiting-only entries follow the same rule. Candidate
matches, ordinary acceptance warnings and blockers follow the existing
Review and acceptance flow. A candidate that appears later is still eligible
for Review.

Adding or removing an account changes the local preference immediately.
Removing it brings any still-waiting cards and warnings back. The application
stores the preference on this computer for this file at its current path,
using the file and account creation identities. Renaming an account or
changing the file password preserves the preference. Copies at other paths
and independently created replacement files start with an empty list.
Archiving pauses the setting for that account until it is reactivated.

The settings change preserves Undo and clears pending Redo. It creates no
financial operation. The existing file-wide acceptance operation saves both
sides and the unmatched decision atomically. Undo of acceptance restores
the prior transaction and transfer state. The active preference still applies
to Review after Undo.

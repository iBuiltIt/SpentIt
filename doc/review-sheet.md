# Unified Review sheet

Last updated: 2026-11-12

The release rules database is version 1. Incomplete preparation is cleaned up on
cancellation or the next open. Completed uncommitted imports remain. Accept
transactions accepts the file. Account Discard is account-owned.

Each unresolved explicit transfer gets a generated entry, including when a
plausible imported posting exists. The interaction below is the current Review
sheet.

## Selected-row Merge correction — owner Go, 2026-11-08

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

## Review workspace cleanup — owner Go, 2026-11-08

Accepted and implemented: supporting tools at the top, question-specific actions
beside their items, and one **Accept transactions** action in a footer that states
its all-account scope. Hide unavailable actions; remove the Review-only Close
button and the read-only imported-category catalog button/dialog. Keep explicit
Close or Cancel on dialogs. Required import-time missing-category decisions stay.

- Group questions into possible duplicates, possible transfers and import
  problems, preserving account order within each kind. Merge/Link/Keep separate
  name the action. Select all merges includes only groups with enough selected
  rows and an unambiguous survivor; it never invents ambiguous choices. Clear
  merge selection affects batch intent only. Batch Merge remains atomic and
  preserves the visible question anchor.
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
- Hidden transfer suggestions moves to Settings with Close; contextual Hide
  suggestions remains beside transfer candidates. Exclusions do not delete rows.
- Transfers has an explicit account picker that stays in Review. Add transfer
  reveals the creation form; switching accounts clears its visibility. Transaction
  matching controls require Review inspection rather than a hidden ledger choice.
- Set up imported transfers appears only for unresolved mapped transfers and
  explains matching and counterpart creation before confirmation. The existing
  command matches within the selected import and then checks explicit transfers
  across pending imports. The confirmation therefore discloses file-wide scope;
  it is an explanation, not an exact per-posting dry-run preview. No matching,
  generation, acceptance, storage, or format policy changes are introduced.

The manual already describes this workspace.

## Missing destination warnings — owner Go, 2026-11-12

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

## One question per transfer — owner Go, 2026-11-12

Before Review is published, two link questions that name the same two
imported lines are one transfer. The published question is the one that
starts from the outgoing line. The other line stays on that question. The
other question is not published, and it does not come back as its own
unmatched card.

A second payment stays a second question. A question that names a third
line stays. When only one of the two lines has a created entry, that
question stays, including when it starts from the deposit. When neither
line is outgoing, the earlier occurrence is the one that stays.

**Not this transfer** on the kept question records the pair, so the other
direction does not return. **Link** is unchanged. The Review count is the
number of questions after this collapse. Duplicate groups and import-problem
cards still count. Missing-destination warnings stay in their own count.

## Link card — owner Go, 2026-11-12

**Link** joins the two imported lines on the card and removes the created
entries. The other button reads **Not this transfer**. It records that this
pair is not the match. Both imported lines stay. The created entry stays.
Neither line becomes ordinary spending. A one-row card for that created entry
can remain, and it can still offer **Accept without bank match**.

**Accept without bank match** is not shown on a card that already lists the
other imported line. It stays on a card that shows only the transfer. The
Transfers panel is unchanged. **Set up imported transfers…** stays.

## Unmatched transfer count — owner Go, 2026-11-12

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

## Transfers panel removed — owner Go, 2026-11-12

Review has no **Transfers…** button and no Transfers panel. That removes Add
transfer, the panel’s account picker, transfer history, and the match, delete,
and accept actions that lived only on that panel. Nothing replaces the history.

**Set up imported transfers…** stays. Ledger **Link** still joins two existing
transactions: select one, switch accounts, select the other, then **Link**
again. A transaction with one split can be linked that way. A transaction with
more than one split still needs the split chosen; Review uses **Split to link**
on a link question. Cmd+Z undoes a Link while that Link is the newest saved
action. The Undo on an open Change history row stays. Review has no
newest-action Undo button.

**Accept without bank match** stays on a Review card that shows only the
transfer. An Import problems line that names a transaction still opens that
transaction. A line that does not name a transaction opens that account’s ledger.

This replaces the Transfers panel described in the 2026-11-08 workspace cleanup
and the Link card note that the panel was unchanged.

## Newest-action Undo buttons removed — owner Go, 2026-11-12

The ledger has no Undo button beside search, no **Undo category change**, and
no **Undo review**. Review has no copy of that newest-action button. **Undo
import…** stays.

Cmd+Z, and Ctrl+Z on Windows and Linux, undoes only the newest saved action,
after the existing confirmation. The Undo on an open Change history row stays.
**Undo opening or currency…** in the account menu and **Undo application** on
Rules stay.

The ledger duplicate bar opens for a possible duplicate in the current account,
or while that check is still running. A category change can still be the newest
review-stack action. That alone does not open the bar.

## Ledger duplicate resolution — owner Go, 2026-11-07

Accepted: keep the existing matching policy and Merge source-association storage.
The losing posting's imported representations continue to attach to the survivor;
this work does not change retention, formats, clean-open verification or close policy.

The ordinary ledger marks suspected duplicates with a rose edge and comparison
outline, preserving new/accepted striping and selection. Selecting a marked row
exposes its direct candidates, not every transitive member of the Review group.
The comparison bar labels New/Accepted, identifies the selected match and survivor,
and offers Merge, Keep this pair separate, Show match and Next possible duplicate.
Several candidates require choosing one by clicking its ledger row or using the
candidate picker. None of these match rejects only the selected origin's candidate
comparisons. Right-click Resolve duplicate exposes the same actions. Read-only files
cannot resolve questions. Review discovery runs automatically in the document worker after open and import.

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

The manual already describes ledger duplicate review.

## Resolution performance and automatic discovery — owner Go, 2026-11-07

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
fixtures. Record measurements and limitations in [status](status.md).

## User interaction

Review is above Categories as the first sidebar action. Review and Commit open
the same compact dataset-wide workspace,
grouped by account. Import does not automatically open Review. Inline rows expose
full transaction editing, splits and original-source inspection. Link questions
stay in Review. Ledger Link joins two existing transactions. The count is outstanding questions, including correction tasks, rather
than candidate transactions. Changing account/import, inspecting a record or
attempting Commit does not resolve a question. The selected-import summary is
explicitly scoped and no longer supplies a competing unresolved count.

Same-account groups ask **Are these the same?** Yes merges the group; No saves
that the displayed rows are separate. Groups larger than two also offer **Selected
yes**, enabled for at least two selections. Unselected rows remain together when
at least two remain. Exactly one committed selected row is the survivor automatically;
otherwise the user chooses which row to keep. Merge preserves that row's current
fields, splits and relationships. Different effective totals require correction
first; a losing row with an existing transfer relationship cannot be merged.

Link questions show one transaction and candidates with account names. Under the
owner Go of 2026-11-03, a sole candidate is selected automatically and Yes is
enabled when the worker is idle. With multiple candidates, the unique same-date candidate is preselected; no exact
date or tied exact dates leave the choice unselected. Choose a candidate then Yes,
or use No without selecting. Automatic selection does not confirm a
link. No saves rejection of the displayed
source/candidate comparisons. Whenever a candidate is selected, by the user or the initial default, its unique
eligible exact opposite-amount allocation pair is preselected. Changing candidates
resets both allocation choices to that pair, or to **Select split** when no unique
exact pair exists. Users can change the split selection before confirming. These
are presentation defaults only; the existing Link command revalidates the selection.
Each Review question has a solid top separator, spacing and alternating themed
background. Split selectors are indented, labelled **Split to link**, and kept
within their owning transaction row rather than spanning the question. Same-currency opposite amounts of different magnitudes open **Link
10.00 and categorise 0.40 as [Uncategorized]**. No dismisses only this popup. Yes
splits the larger allocation, assigns the remainder's chosen ordinary category,
and links the common amount in one transaction. Both account totals and unrelated
splits remain unchanged. No currency conversion is inferred. Replacing a purely
generated counterpart is identified in the question; independently supported
postings are retained.

**Needs correction** tasks use the existing record/import repair paths. Invalid
records cannot be accepted by acknowledging a warning. Commit can accept valid
transactions while failures remain visible. Review remains accessible when its
count reaches zero if a Review decision can still be undone.

The ledger has separate **Merge** (Alt+M) and **Link** (Alt+L) commands. Merge uses
the same survivor and persistence rules. Link retains the existing allocation
selection and cross-account navigation workflow.

## Batch Merge and stable status — owner Go, 2026-10-31

Every right-pane workspace has a permanent bottom status line. Progress, success
and errors occupy that reserved space; long messages expose Details. Review keeps
its visible group anchor (or the next surviving group) when resolved groups leave.

**Include match** is shown only on a two-row duplicate question. It selects that
question as pending UI intent. A question with more than two rows has no Include
match control; check the transaction rows that pair. **Merge pairs** saves, in
one step, every two-row question marked Include match and every larger question
with exactly two checked rows whose obvious kept row is the single accepted,
manual, or linked row. Any other checked count in a larger question is left
unchanged. **Merge pairs** does not choose between two new rows or two
accepted rows. That question's Merge button merges the checked rows, and asks
which row to keep when none is chosen automatically. Changing the
checked rows clears that question's Include match staging. Zero or multiple
committed selections on a two-row question require choosing a survivor; one
committed selection keeps that row automatically. Numbered selected-group labels
correspond to validation errors. Leaving Review or a changed dataset generation
clears selections; read-only inspection retains them. No selections persist
across restarts. Link is not batched and matching eligibility is unchanged.

Two rows left after **Merge pairs** come back as a two-row question, with
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
one group never records an answer between unrelated groups. Existing encrypted
schema 40/39 tables encode parent/child relationships in reserved answer question
values (`batch`, `batch-child:<operation>`); no schema change is needed. Child IDs
are derived from the parent's operation identity. Exact request receipts reject
changed retries. Undo restores children in reverse order in one transaction and
preserves earlier mutation barriers. The caller refreshes its snapshot once.

Profiling found repeated source loads and saved-choice queries in the same-account
peer comparison loop. Review now loads comparison facts once per build and evaluates
the unchanged engine predicate before querying saved choices. This eliminates
irrelevant database work; it does not change duplicate or transfer matching policy.
The loop can still compare many pairs; this is not a constant-time performance claim.
See [status](status.md) for measured workloads and verification limits.

## Common duplicate policy — owner Go, 2026-10-31

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

New datasets use release schema version 1. A different number is refused, and
the file bytes stay the same. There is no migrator. See the
[file format](../AGENTS.md#current-file-format).

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
The stack persists across reopening. Any intervening dataset mutation ends the
safely undoable sequence, even if unrelated; read-only navigation does not. Undoing
a newer decision cannot cross that earlier barrier. This is deliberately
conservative rather than attempting to overwrite later changes. There is no Redo.

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

### Batched counterpart generation — 2026-10-30

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
rollback cannot leave them available to a later attempt. See [status](status.md)
for actual verification and timing evidence.

Review construction runs on the existing import worker. Cached raw candidate edges
are updated for old and new date/amount neighborhoods when rows or occupied
allocations change; alias changes invalidate all edges. Exclusion and saved-answer
filtering is separate. Restart reconstructs the cache from persisted state. Reading
the current rows still has linear cost. No claim of native large-dataset
responsiveness is made by the functional fixtures.

## Revision verification — macOS, 2026-10-30

- Release policy tests cover the seven-day boundary, leap/year transitions,
  inclusive 4%, overflow boundaries, explicit destinations, exclusion scope and
  removal, saved No independence, persistent waiting tasks and acceptance/Undo.
- Eight focused Qt cases pass in Release, ASan/UBSan and rules-disabled builds,
  including workspace navigation/inspection, exclusion editing and transfer
  controls. Final Release visual inspection also checked column alignment.
- Eleven final engine/Review/grouped-import tests and six focused Qt cases pass
  under ASan/UBSan and rules-disabled Release. Repaired imports without an original
  amount retain imported provenance; manual transactions remain explicitly manual.
- Six new policy recovery scenarios passed 662 injected write/sync positions,
  before/after durable-boundary interruptions, idempotent retries, verified
  backup restores and encrypted-source marker checks.
- A synthetic 20,000-row workload produced exactly 10,000 expected pairs and no
  unexpected pairs. Import took 39,230 ms, first Review 974 ms, five cached opens
  13 ms, and an amount save plus affected Review 1,318 ms in a concurrent test run.
- All 94 selected Release regression tests pass, including the full import UI
  executable and grouped imports. Separate long-running fault/crash and diagnostics
  suites are excluded from this selection.
- The six existing Review recovery scenarios pass 1,122 injected write/sync
  positions, durable-boundary interruptions, retries and verified backup restores.
  Grouped import, repaired-transfer generation and Abort pass all 387 write-failure
  positions and 387 abrupt-crash positions, plus durable-boundary interruptions.
  Statement rollback and overlap recovery sweeps pass.
- The rules-enabled `./build/SpentIt` executable is rebuilt. Native interaction,
  Linux and Windows remain unverified. Document file-link and whitespace checks pass.

## Baseline verification — macOS, 2026-10-30

Synthetic fixtures only. The new `import.review_sheet` case covers saved Yes/No,
reopening, alternative-source reimport, partial four-row groups, repeated Undo,
multiple committed rows, an intervening-mutation Undo barrier, generated-counterpart
replacement, amount differences and exact balances. A 53-question fixture spans two
accounts and includes an invalid record; counts stay stable during account changes,
inspection, blocked Commit and reopening. This verifies the new count contract;
the owner's original private-data count-flip sequence was not reproduced.

Qt offscreen interaction tests exercise the actual Review/Commit entry points,
saved decisions, standard keyboard Undo, the amount-difference popup's No/Yes
behavior and category choice. Its rendered popup was inspected using synthetic
data. The rules-enabled Release regression selection passed **93/93** in 81.75 s:

```sh
ctest --test-dir build/import-ui-rules-release --output-on-failure \
  -E 'io$|crashes$|faults$|failures$|backup_publication|benchmark|diagnostics\.' -j4
```

This selection excludes the expensive existing failure suites and diagnostics;
Review has its own dedicated failure sweep:

- Six scenarios: Merge and Undo with one committed survivor, difference split/link
  and Undo, and Merge/Undo involving multiple committed rows. The sweeps passed
  **1,126 injected write/sync failure positions** (173, 145, 197, 165, 237, 209),
  alternating disk-full and I/O errors. Each trial reopened, verified, retried as
  needed and restored an encrypted backup. Raw-file marker scans found no synthetic
  source marker. All twelve process interruptions, before/after durable commit for
  each scenario, passed receipt/retry/replay checks.
- The final rules-enabled Release storage case passed with positive-difference,
  existing-split, operation-identity, saved-No and post-Merge account-rollback
  checks. The complete import UI executable passed **51 Qt test cases** in 62.526 s.
- The final ASan/UBSan build passed the Review storage case and both focused Review
  UI interactions (`ASAN_OPTIONS=detect_leaks=0`; no leak-check claim).
- The rules-disabled Release build passed the same storage case and both focused
  UI interactions. The difference category resets to Uncategorized for each popup.
- Changed document file links and `git diff --check` passed. The local
  `./build/SpentIt` rules-enabled Release executable was rebuilt.

The failure runner is `spentit_review_sheet_tests faults`; an optional final mode
number starts a suffix of the six scenarios. The four original scenarios passed
via CTest in 394.72 s; the two committed-row scenarios passed separately using
`spentit_review_sheet_tests faults 4`. No private datasets were used.

Native interaction, keyboard-layout conflicts, Linux and Windows remain unverified.

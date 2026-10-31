# Unified Review sheet

The owner authorized the [workspace and link-learning revision](../features/005-transaction-sheet-navigation.md#accepted-matching-policy-and-implementation-go--2026-10-30)
on 2026-10-30. The revision is implemented in the working tree; the interaction
and persistence contract below includes it. The owner settled grouped imports with ambiguous plausible
counterparts: each unresolved explicit transfer gets its generated entry.
The broader transaction-sheet feature remains active.

## User interaction

Review is above Categories as the first sidebar action. Review and Commit open
the same compact dataset-wide workspace,
grouped by account. Import does not automatically open Review. Inline rows expose
full transaction editing, splits and original-source inspection. Transfer details
and manual resolution are available within Review. The count is outstanding questions, including correction tasks, rather
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

Link questions show one transaction and candidates with account names. Select one
then Yes, or use No without selecting. The latter saves rejection of the displayed
source/candidate comparisons. Split transactions require explicit allocation
selection. Same-currency opposite amounts of different magnitudes open **Link
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

## Common duplicate policy — owner Go, 2026-10-31

All supported parsers produce common original facts. Same-account ordinary
amount-based duplicate suggestions require equal original currency, exact signed
original amount and the same known calendar date, independent of QIF/OFX/QFX/QBO/ASO
labels. Payee/category differences do not disqualify a review candidate. Unknown
original amounts/dates retain their explicit uncertainty paths; no date or amount
is fabricated. Different known dates alone never qualify ordinary candidates.

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
same-import/unresolved peers. SQL contains no format-based matching exception.
The common policy does not widen discovery within an otherwise clean base import.
The separate transfer Link and grouped reciprocal-transfer policies are unchanged.
A recorded explicit duplicate decision retains its date override during history
validation; it is not reclassified as corruption just because suggestions tighten.
New import/review candidates never receive that override. Existing stale-target,
account, amount/currency and bank-ID checks remain. No timezone inference,
per-account date setting or schema change is introduced.

## Persistence and Undo

New datasets use experimental schema **38 with rules / 37 without**. Older
experimental datasets are rejected unchanged, without migration under the
[pre-release policy](../AGENTS.md#security-and-recovery-discipline).

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

Cmd+Z / Ctrl+Z and one Review Undo button reverse decisions in order. Text editors
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

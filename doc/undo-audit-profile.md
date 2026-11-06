# Undo and full-audit profiling follow-up

Date: 2026-11-06. Status: **first two improvements implemented under owner Go; verification below**.

The owner requested profiling after commit `07be74e`, following the earlier
15,000-row measurement of roughly 18 seconds for Undo and 59 seconds for full audit.
The baseline investigation is retained below; the subsequent owner Go authorized
the first two improvements, implemented in the follow-up section.

## Workload and method

A temporary C++ driver linked the committed rules-enabled Release storage, engine,
and diagnostics libraries. It created a fresh SQLCipher synthetic account, imported
N distinct ordinary transactions, accepted them, deleted the whole selection,
undid that deletion, and ran `ImportStore::verify()`. It asserted zero rows/balance
after deletion and restoration of every row and the exact original balance after
Undo. Each process removed its synthetic dataset after closing it.

This exercises N transactions, three committed snapshots per transaction, and
2N individual change/Undo history operations plus acceptance. It contains no
linked transfers, category assignments, currency reassignments or duplicate matches.
It measures repository execution, excluding UI refresh, opening/closing approval
and setup. Files were freshly created per run; operating-system caches were not
flushed. Runs were serial, with no competing agent builds or test runs. These are
macOS ARM64 measurements, not platform-independent latency guarantees.

One separate diagnostics-enabled run collected cumulative phase/counter deltas
and bounded native samples from its exact PID. Three subsequent 15,000-row runs
used disabled collection and no sampling. Additional 1,000/5,000-row runs measured
scaling. The temporary driver SHA-256 was
`ffc53789c2e64cd4baa4d5520d629c02a5e9fc4f4314b333a74d596972b27380`.
The repository's `spentit_review_sheet_tests deletion_benchmark N` reproduces the
same financial workload; the temporary driver adds phase markers and diagnostics
metric deltas. No private financial data or running application was inspected.

## Whole-operation measurements

All six synthetic runs passed their financial assertions and full audit.

| Transactions | Runs without collection/sampling | Delete (s) | Undo (s) | Full audit (s) |
|---|---:|---:|---:|---:|
| 1,000 | 1 | 0.594 | 0.646 | 1.007 |
| 5,000 | 1 | 3.048 | 3.848 | 9.002 |
| 15,000 | 3, median | 9.696 | 18.892 | 64.812 |
| 15,000 | range | 9.239–10.087 | 18.726–19.036 | 62.977–64.973 |

For 3× as many rows (5k to 15k), median audit time grew 7.2× and Undo 4.9×,
while deletion grew 3.2×. This supports the repeated-scan diagnosis; it is not
merely a large fixed startup cost. The 1k/5k figures are single observations,
not distributions. This isolated series is slower than the earlier 59-second
observation; retain the measured range rather than promising that earlier time.

## Measured attribution

The separate instrumented 15,000-row run took 17.91 seconds for Undo and 61.22
seconds for full audit. Instrumentation/sampling timings are not substituted for
the unsampled timing distribution.

| Audit phase | Seconds | Share of full audit |
|---|---:|---:|
| Category/ownership/history verification | 41.554 | 67.9% |
| Committed financial-history replay | 15.019 | 24.5% |
| Imported source verification | 3.336 | 5.4% |
| Rules verification | 0.726 | 1.2% |
| Integrity, cipher integrity and foreign-key checks together | 0.520 | 0.8% |
| Transfer verification | 0.051 | 0.1% |

These are sibling phase spans; nested inclusive times were not added. Remaining
account/group/unclassified verification work accounts for the small remainder.

Undo prepared 64 statements and reused statements 1,424,970 times (about 95 statement
uses per selected transaction). Full audit prepared 145 and reused statements
2,384,979 times (about 159 uses per transaction). Reuse avoids preparation cost,
but it does not avoid query execution or the rows examined by each query.
Undo's final COMMIT span was 0.070 seconds, under 0.4% of its total.

A five-second Undo sample contained 3,933 main-thread samples. `entry_snapshot`
was on 3,147 stacks (80.0%); its `entry_currency` lookup was on 2,612 (66.4%). These
are nested shares, not additive. A later five-second audit sample contained 4,039
main-thread samples entirely in category verification; 1,770 (43.8%) included
`entry_currency`, and 3,776 (93.5%) included SQLite VM execution. A separate earlier
audit sample covered integrity/source/category startup and was not treated as
representative of the entire audit. The native sample percentages describe those
windows, not exact phase-wide CPU attribution.

## Query-plan and work-count check

A separate probe used the same pinned SQLCipher library (SQLite 3.53.4), a reduced
in-memory synthetic schema retaining the relevant production keys/indexes, and
30,001 ledger operations representing acceptance plus delete/Undo for 15,000 rows.
It executed the production revision-currency and category-operation lookup SQL.
This is a query-plan/VM-work experiment, not an encrypted-disk throughput benchmark
or a proposed production implementation. No user/application file was altered.

Both original queries reported `SCAN o` on `ledger_operations`. For the last
transaction's revision 2, each traversed 30,000 rows: the currency query used
180,032 SQLite VM steps and the category query 180,027.

Adding only a covering `(batch, ordinal, result_revision, kind, id)` index still
searched just `batch=?`, rather than an exact revision. The currency query used
150,032 VM steps; for revision 1 it actually increased work from 90,030 to 150,025
steps. Its full-scan counter became zero despite scanning a large index range.
**A zero full-scan counter, or an index alone, is therefore insufficient evidence
that this query is fixed.**

Separating initial acceptance (through `accepted_transactions` and operation ID)
from later revisions, combined with that index, produced an exact
`batch=? AND ordinal=? AND result_revision=?` search. Revision 1 and 2 lookups each
used **30 VM steps**; revision 0 used 37. All three returned the same history
sequence as the original query (1, 15,001 and 30,001). The roughly 6,000× work
reduction concerns one late revision lookup, not total Undo/audit elapsed time.
It establishes the lookup design opportunity without claiming an application
speedup that has not been implemented or tested.

## Remaining causes

1. **Repeated searches for the operation behind a transaction revision.**
   `entry_currency` in [account_money.inc](../src/storage/account_money.inc) finds
   a snapshot's history sequence before resolving its historical currency. The
   category verifier independently performs a similar operation lookup in
   [category_store.inc](../src/storage/category_store.inc). Both queries combine
   initial acceptance and later row revisions with an OR condition. The ledger
   operation table has sequence and operation-ID indexes, but no index supporting
   exact `(batch, ordinal, result_revision)` lookup. Repeating history scans for
   every snapshot produces superlinear growth. The earlier optimization of account
   currency-operation lookups did not eliminate this preceding revision lookup.
2. **Repeated reconstruction of the same facts.**
   [write_review_row](../src/storage/review_sheet_store.inc) reloads source/current
   entry/expected result and several fields from the same Review member while
   restoring a row. The outer Undo loop already read some of these facts.
   [verify_committed](../src/storage/committed_store.inc) scans ledger operations,
   then calls `committed_change_outcome` to reread that operation and reconstruct
   its receipt/snapshots; Undo verification also reloads the original receipt.
   These are concrete redundant reads; their individual costs beyond the sampled
   revision lookup have not been separately timed.
3. **Bounded snapshot caching is not a substitute for indexed access.**
   Audit caches have deliberate byte/member caps. Once a snapshot is unavailable
   from cache, the expensive revision search is repeated. Increasing the caps
   would merely move that threshold. Cache saturation was not measured in this
   run and is a hypothesis for further instrumentation, not an attributed result.
4. **Transfer-heavy operations have an additional scaling risk.**
   Transfer Undo now retains its relationship set, but still scans/validates the
   complete set per restored link and sorts using linear link lookup. Transfer
   replay also rebuilds whole sets. This is visible in code, but the ordinary-row
   workload above does not establish its real-world cost.

## Prior proposal (first two subsequently authorized)

**First: share an exact revision-to-history lookup.** Build a compact lookup once
per audit/command, containing operation identity, sequence, kind and account for
needed revisions; resolve initial acceptance through `accepted_transactions`.
Share it between historical-currency resolution and category/financial checks.
A bounded operation-scoped lookup can retain the current file format. Preserve
missing/ambiguous history detection and currency-reassignment history. During Undo,
newly appended operations must update the lookup or use a fresh exact lookup;
mutable row heads must never come from stale cached state. Define the memory bound
and correct streaming/fallback behavior explicitly.

An alternative is split initial-acceptance/later-revision SQL plus a covering
revision index. Index creation and current-format handling must be settled before
implementing that alternative; this investigation does not modify any dataset's
schema. A simple index alone should not be assumed sufficient for the OR query.

**Second: reuse already-read immutable facts within a command/audit.** Carry the
Review member's prior/result/child fields together, reuse the current and expected
snapshots when they name the same immutable revision, and cache immutable source
facts per operation. Replay committed history from its existing cursor rather than
reconstructing every public receipt through additional queries. Keep all exact
before/after, ownership, relationship, dependency and balance comparisons.

**Third: separately profile transfer-heavy selections.** If whole-set validation
is material, maintain endpoint/link indexes and per-account local totals during
replay, validate each changed relationship against those indexes, and retain the
required complete-set reconciliation. This needs its own workload and recovery
coverage; the ordinary-row measurements do not justify a promised transfer speedup.

The first two changes are the priority. Proposed engineering targets for this
15,000-row ordinary workload are Undo below 5 seconds and full audit below 10
seconds, with near-linear growth in rows/history. These are targets to measure,
not demonstrated results or guarantees. Full integrity checking, exact arithmetic,
encryption, cancellation, the single durable transaction and strict Undo remain
requirements. No weaker audit mode or background-success shortcut is proposed.

## Acceptance evidence requested by the proposal

- Repeat serial 1k/5k/15k workloads and add 50k plus deeper per-row histories;
  report median/range, memory, statement uses and rows/VM steps examined.
- Exercise multiple acceptances within one import, currency corrections and their
  Undo, mixed manual/imported/new/accepted rows, category/rule ownership and splits.
- Add linked selections with retained and deleted counterparts, unrelated splits,
  old generated postings, duplicate dependencies and later-change barriers.
- Prove corrupt/missing/duplicate revision history still fails audit; do not replace
  validation with assumptions about uniqueness absent from the schema.
- Repeat cancellation, disk-full/I/O sweeps, before/after-commit crashes, retries,
  reopened Undo and sanitizer coverage. Read-only audit must leave data unchanged.

## Implementation follow-up — owner Go 2026-11-06

The first two improvements are implemented without a format change (58/57).
Historical currency and category validation share an operation-scoped map from
transaction/revision to history order, operation kind and account. Revision zero
uses the exact accepted transaction's publication. Later revisions are read once
in ledger sequence order; appended Undo operations extend the map. Missing or
ambiguous mappings remain errors. Currency-reassignment markers retain their
existing separate validation path.

The map retains at most 262,144 members and an estimated 64 MiB, whichever comes
first. Beyond that budget, uncached keys use the exact SQL lookup with duplicate
checking; no history is skipped. This fallback preserves correctness but is not
a constant-time lookup on the current schema. Transaction rollback and physical
history cleanup clear the map. It is destroyed at the end of the repository
operation; it never supplies mutable row heads. Audit and selection-command
contexts check cancellation while reading the initial history stream.

Review Undo carries prior/result/child facts from its member cursor and passes the
already-read current entry and source into the restore step. The expected snapshot
is reused only when its revision equals the current snapshot's revision. Source
facts are reused within that restore step, not cached across source-association
mutations. Committed audit receipts are assembled from the existing history cursor;
the exact before/after, dependency, ownership, relationship, ordering and balance
checks still run. Integrity checks, encryption, durable transaction boundaries,
final cancellation gates and Undo eligibility remain unchanged.

New regressions reject missing and duplicate revision mappings and verify that
cancelled committed/category audits leave file bytes unchanged, then retry a full
audit successfully. No manual text or screenshots need updating for this internal
performance change. Transfer-set optimization (the third proposal) remains deferred.

Verification completed for the main implementation: 35 selected Release storage
checks passed after correcting one existing test's obsolete deletion-progress
trigger; the first aggregate run reported that fixture failure, and its corrected
manual-transaction suite passed separately. Coverage includes selection/editor/
linked-deletion write-failure sweeps, committed I/O and crash sweeps, acceptance
ordering and recovery, mixed new/accepted/manual rows, transfers and split
preservation, currency changes and Undo, category/rule validation and ownership,
later-change barriers, reopened Undo, audit cancellation and read-only file checks.
Four additional import-Undo/rollback/UI/QML checks passed (UI: 84 passed, one existing
skip). Rules-disabled and ASan/UBSan runs passed linked deletion, selection edits,
acceptance ordering/recovery, committed history and corruption validation. The
new corruption fixture initially inserted an already trigger-created history row;
that setup was corrected and both validation binaries passed. These were macOS
checks; Linux/Windows and native large-selection interaction were not retested.

An initial implementation scaling run passed 50,000 ordinary transactions:
Delete 29.4786 s, Undo 22.6381 s, audit 73.7659 s. Maximum process RSS was 632.88 MiB
and peak memory footprint 457.11 MiB (`time -l`, including fixture creation and all
phases). This run preceded only the final dependency-statement lifetime adjustment;
it is not included in the final 15k medians. Audit scaling beyond 15k remains
nonlinear; this run does not attribute the remaining growth to a particular cache
or query. The revision map's budget-exhaustion fallback was not separately forced.

### Final-build 15k results

The same temporary synthetic driver was relinked against the final Release
libraries (binary SHA-256
`f9c1ff7b40f5919a37c8b83249d4d243c2131df3830911c19c133e51101dfbf6`).
No agent builds or tests ran concurrently. As in the baseline, collection was
**disabled** for the three comparison runs. All assertions passed.

| Phase | Prior median (s) | Final median (s) | Final range (s) | Speedup |
|---|---:|---:|---:|---:|
| Delete | 9.69624 | 8.27868 | 8.07584–8.40420 | 1.17× |
| Undo | 18.89170 | 6.72186 | 6.64790–6.78229 | 2.81× |
| Audit | 64.81210 | 11.59040 | 11.50500–11.60860 | 5.59× |

The final runs used 238.19–243.06 MiB maximum RSS and 158.53–158.92 MiB peak memory
footprint (`time -l`, whole process including setup, not the revision map alone).
No comparable baseline memory measurement was collected. One final 1k run took
0.497665 / 0.404897 / 0.717017 s and one 5k run took
2.50379 / 2.06138 / 3.77023 s (Delete / Undo / audit).
The proposed Undo <5 s and audit <10 s targets remain unmet. An earlier pass of
the implementation measured 6.20645 / 10.95950 s median Undo/audit; those faster
numbers are not substituted for the final-build results.

A separate final diagnostics-enabled 15k run took 8.08082 / 6.44606 / 11.36570 s.
Undo prepared 62 statements and reused 764,972 (prior profile: 64 / 1,424,970).
Audit prepared 146 and reused 2,174,979 (prior: 145 / 2,384,979). The first
implementation held the current dependency statement while validating the original
Undo receipt, forcing 15,000 additional prepares; narrowing that statement's
lifetime removed those prepares. The final adjustment passed eight focused Release
checks and two each with rules disabled and ASan/UBSan. No validation was removed.

Final audit spans: category/provenance 5.505121 s, imported evidence 2.748519 s,
committed replay 1.809764 s, rules 0.746543 s, integrity/cipher/FK 0.486979 s,
transfers 0.052555 s. Undo's durable COMMIT was 0.065261 s. These are separately
instrumented measurements, not additive components of the uninstrumented medians.
Full-query VM-step/row counters were not collected for this final implementation;
the earlier query-only probe remains a separate result. The remaining category,
evidence and replay work and the 50k growth need further profiling before choosing
another optimization.

The repository's ordinary workload can be rerun with
`build/import-ui-rules-release/tests/storage/spentit_review_sheet_tests deletion_benchmark 15000`;
use fresh processes and avoid concurrent builds/tests for comparisons.

### Deeper-history check

A final-build 5,000-row workload repeated Delete and Undo five times, with a full
audit and exact row/balance assertions after every cycle. All five passed. The
last cycle had 11 snapshots per row and 50,000 row operations plus acceptance.

| Cycle | Delete (s) | Undo (s) | Audit (s) |
|---|---:|---:|---:|
| 1 | 2.65760 | 2.19393 | 3.67698 |
| 2 | 2.70796 | 2.23293 | 5.14488 |
| 3 | 2.68819 | 2.23378 | 6.64341 |
| 4 | 2.71028 | 2.24032 | 8.06906 |
| 5 | 2.75279 | 2.24177 | 9.87491 |

Maximum process RSS was 143.92 MiB, peak memory footprint 75.05 MiB. These timings
show stable Undo cost with this history depth while full audit grows with retained
history; they do not establish a bound for every workload.

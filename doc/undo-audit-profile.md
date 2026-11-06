# Undo and full-audit profiling follow-up

Date: 2026-11-06. Status: **profiled; further implementation proposed, awaiting owner Go**.

The owner requested profiling after commit `07be74e`, following the earlier
15,000-row measurement of roughly 18 seconds for Undo and 59 seconds for full audit.
The existing implementation remains unchanged by this investigation.

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

## Proposed next implementation

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

## Acceptance evidence for a later Go

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

# Broader storage-budget investigation

**Owner stop, 2026-11-08:** the generic row-capture Undo architecture was rejected.
See the [rethink handoff](../../../doc/action-undo-rethink-handoff.md). This material is historical investigation
evidence, not authority to resume implementation, review or budget approval.

2026-11-08. **Owner Go covers further prototype measurement; no final storage cap
or production Undo delivery is accepted.** The owner indicated that roughly 8%
lasting overhead might be acceptable, then authorized the broader measurements
needed to distinguish lasting cost from session peaks. This supersedes stopping
this investigation at the earlier no-growth result; it does not waive correctness,
recovery or unlimited growth. See the [historically approved design and follow-up](../../../doc/action-undo-rejected-design.md).

## Measurement scope

Three builds run the same financial workload source:

- **baseline**: `cf8f1a5`, the original storage comparator;
- **current**: unchanged production storage at the implementation starting point;
- **prototype**: the actual repository compiled against the isolated immutable-body /
  active-membership adapter, with first-before capture, explicit temporary pins and
  close-time expiration/collection.

The prototypes are opt-in test executables. Production storage, application UI,
final Change History layout and manual text/screenshots remain unchanged. Full
unified Undo is still incomplete. These results measure the implemented probe,
not a guarantee of the completed feature's footprint.

The common driver is `tests/storage/action_storage_workloads.cpp`. All modes use
synthetic CAD amounts and fixed valid dates. No private data is used. Every close
has explicit full verification beforehand, balances are checked after the
financial actions and reopen, and prototype final inspection refuses remaining
inverse/pin rows or payload bodies without active memberships.

| Mode | Common financial workload |
| --- | --- |
| import | Import N ordinary rows, close, reopen and close. |
| edits | Import/accept N rows, then eight accepted payee edits in each of four sessions; money stays unchanged. |
| undo | Import/close N rows, import/close a second N, remove the eligible older second source, close/reopen. |
| linked | N ordinary rows plus one transfer per import/account; accept the first, link the later bank posting, close, remove the older second import and verify the original relationship and both balances. |
| mixed | N primary and N/4 secondary-account rows with longer synthetic memo text and a split every tenth row. Accept, save report definitions, make 24 accepted payee edits across three sessions, rename the account, import additional N/4 batches and remove each only in a later completed session. Check both accounts. |

The 1,000/10,000 matrix preserves short source IDs from the historical comparator.
Because the UI uses 36-character UUID identities and composite keys repeat them,
a separate import-only sensitivity build defines `SPENTIT_WORKLOAD_UUID_IDS` and
uses fixed synthetic UUID-length source IDs at larger scales. Its results and
source hashes are separate; the original matrix sources are preserved in
[matrix/source-snapshot](matrix/source-snapshot).
The initial `matrix/` attempt was intentionally interrupted during the slow
10,000-row edit verification and is marked incomplete. The subsequent run is
recorded separately in `matrix-final/`, with its completed cases retained and
remaining cases measured after the second read-query optimization below. The
[assembled completed matrix](completed-matrix/cost-summary.csv) contains all 30
unique results and preserves each component's provenance. UUID import sensitivity completed using
the earlier read adapter; that workload contains no accepted-entry snapshot
lookup affected by the later optimization. Some independent processes overlapped
in wall time. Each process tracks only its own VFS file lengths, and elapsed times
are not used for performance conclusions.

The mixed workload legitimately removes prior-session imports; it does not use
selective removal of a current-session import. A prototype durable record-sequence
high-water counter prevents a removed-but-pinned record's sequence from being
reused by the subsequent import. Source order likewise uses its explicit
high-water allocator. This is bounded allocator coverage, not implementation of
all the approved action/version counters.

## What the byte counts mean

- **Retained B-tree payload** comes from the separate read-only SQLCipher DBSTAT
  analyzer after the final successful close. It includes serialized table/index
  content and schema, rather than assuming file length equals retained history.
- **Physical main-file length** is measured after each operation/close. SQLite
  retains freed pages: expiration makes pages reusable and does not shrink the
  file. No VACUUM, padding, weaker durability or omitted dependencies is used.
- **Live/free page bytes** distinguish allocated pages from reusable freelist pages
  at intermediate checkpoints. They are not precise serialized payload sizes.
- **Simultaneous physical peak** is the maximum sum of actual SQLite-VFS file
  lengths at one observed I/O state: main database, rollback journal, WAL,
  temporary and other VFS files. It is not the sum of their independent maxima.
  Every successful write/truncate/open/close/delete updates the instrumentation.
  A lifetime maximum is preserved before every phase reset, including creation,
  reopen and verification work. Unexpected I/O failures or invalid telemetry
  invalidate a normal workload result.
- **Inverse codec bytes** sum stored key bytes and first-before image bytes, with
  a sample before coalescing and after it. Capture only inserts first images during
  each forward command, so its pre-coalescing endpoint retains that command's
  largest inverse set. SQL record/index overhead is additionally present in the
  physical/DBSTAT measurements.
- **Pinned-only body codec bytes** count a body once when it is pinned and has no
  live membership. A body with a live membership is not counted again merely
  because a pin exists. These are logical typed-codec body sizes, not allocated
  disk pages. Pin counts/bytes are sampled at command boundaries; unlike the VFS
  event peak, this is not a general proof of every possible intra-command pin peak.

Shared-memory mapped extent is reported separately and excluded from physical
file totals. In-memory caches/temp data, filesystem allocation blocks, external
non-VFS files and unrelated application files are outside these measurements.
The actual store is serial; the VFS's exact simultaneous ordering assumes serial
file mutations. No filenames or financial values are emitted by VFS telemetry.

Timing includes test instrumentation, metadata reads, capture/coalescing and
explicit verification; it is not a native-UI responsiveness claim. The prototype
retains legacy history structures and conservatively captures some metadata that
a complete action coordinator may replace. Conversely, the final coordinator,
all payload families, full tokens/receipts, crash recovery and all command families
are not implemented. These costs must not be presented as exact final-product
predictions in either direction.

The first scaled run exposed a query-plan cost in the isolated split views:
existing snapshot verification materialized all `versions` and allocation metadata
rows for each entry.
The prototype's prepared-read adapter now uses equivalent unique-key scalar
fallback lookups for the snapshot and audit UNION queries. It changes no stored
schema, data, indexes or financial checks. Independent review accepted this
bounded rewrite; typed original/rewritten query results and EXPLAIN plans are
recorded in the [query optimization checks](query-optimization/ctest-detail.txt).
The first rewrite's 1,000-row edit workload and the second rewrite's 1,000-row
mixed workload each produced identical log values (apart from elapsed time) and
byte-identical DBSTAT rows before/after their change. The
[allocation query checks](allocation-query-optimization/ctest-detail.txt) preserve
full audit plans with both materializations removed and typed edge cases for
missing/NULL metadata, category-version selection, missing versions and UNION ALL
multiplicity. Incomplete attempts remain historical evidence; completed result
components retain their own source/executable provenance. This does not establish
general query or UI performance acceptance.

The large linked fixture deliberately contains many ordinary rows with the same
date and amount. A bounded, read-only two-second sample of the verified synthetic
prototype process found all 168 samples in `review_sheet`/`build_review_sheet`,
predominantly account-name matching and regular-expression compilation, while
constructing link candidates. This remaining long phase was actual review matching,
not snapshot-view materialization or byte accounting. The benchmark and financial
checks were left intact. Timing remains excluded from performance acceptance.

## Actual close semantics and verification

The isolated adapter recognizes the exact existing clean-marker publication SQL.
Its cleanup hook runs **inside the repository's existing close transaction**, after
ordinary pruning and before marker publication. It expires inverse/action rows,
releases pins and collects unreachable immutable bodies together with successful
close. No clean marker is published if cleanup fails. Without an installed journal
hook, a journal-bearing document is refused rather than silently expired. Payload
metadata can be reloaded from the authenticated prototype schema on a new process.

[CTest summary](verification/ctest.txt) and [detailed output](verification/ctest-detail.txt)
record **3/3 passing** macOS Release tests:

- Typed codec/capture/registry regression tests.
- Rerunnable SQLCipher VFS tests: journal DELETE/TRUNCATE/PERSIST, reopen/multiple
  handles, WAL/shared-memory mapping, temporary/delete-on-close, path aliases,
  reset preservation and a controlled simultaneous peak that differs from the
  sum of independent file maxima.
- Actual SQLCipher close tests: a failure after cleanup, cancellation after cleanup,
  a precommit refusal and first-write disk-full injection restore the complete
  persistent semantic/journal/pin/body/marker state. Lost close acknowledgement
  preserves the successful expiry. Process exit immediately before and after the
  commit boundary recovers the corresponding coherent retained/expired state and
  exact money. Each failed close is followed by full existing verification.

These are bounded close tests, not a complete write/sync fault sweep or full unified
Undo recovery acceptance. Sanitizer, rules-disabled, native UI and other-platform
verification remain unexecuted for this measurement step.

## Independent review scope

The fresh implementation reviewer has approved the bounded capture/lifecycle/VFS
measurement changes, both prepared-read optimizations and their typed-result and
storage-parity checks. The [final independent review](implementation-review.md)
returned **APPROVED, zero blocking findings** for the bounded prototype and
assembled evidence, reconciling all 30 matrix runs and 810 physical phase rows,
plus the six UUID sensitivity cases. Approval
here concerns the prototype and its reproducible storage evidence only; it cannot
approve an owner storage-budget decision or establish delivery of unified Undo.

Production still lacks the common action coordinator/API, full prepared-import
protocol, complete command and payload-family conversion, all monotonic allocators,
complete action-aware recovery verification and UI routing. The earlier bounded
rename/acceptance inverse tests still have the documented legacy verifier mismatch;
the broader forward/close tests do not turn those into full-verification passes.
Final Change History layout and manual text/screenshots remain separate work.

## Results and decision

**All 36 unique runs completed successfully:** three variants for five workloads
at 1,000 and 10,000 rows (30 runs), plus UUID import sensitivity at 10,000 and
50,000 rows (six runs). All prototype successful-close checkpoints have zero
inverse/action rows and pins; every final payload audit has zero bodies without
live memberships. Exact balances, linked relationships, forward verification and
reopen checks passed within these workloads.

At 10,000 rows, prototype increases versus unchanged current storage are:

| Workload | Retained B-tree payload | Main file after close | Simultaneous VFS lifetime peak |
| --- | ---: | ---: | ---: |
| Import | +11.8% | +55.6% | +89.9% |
| Repeated accepted edits | +12.2% | +76.9% | +116.2% |
| Older import removal | +11.8% | +89.6% | +117.9% |
| Linked transfers | +12.5% | +33.1% | +48.2% |
| Mixed sessions | +7.5% | +38.6% | +49.0% |

Across the complete matrix and UUID sensitivity, retained-content growth versus
current ranges from **7.5% to 22.1%**, final physical-file growth from **26.4% to
89.6%**, and simultaneous-peak growth from **27.4% to 117.9%**. The largest retained
ratio is UUID import at 10,000 rows; the largest final-file and peak ratios are
older import removal at 10,000 rows. That removal retains up to 5,130,036 logical
codec bytes of pinned-only immutable bodies during the session, then releases
them at successful close. See the [complete matrix summary](completed-matrix/cost-summary.csv)
and [exact comparisons against both baselines](completed-matrix/comparisons.csv).

The completed UUID-length import sensitivity shows why a retained-content
percentage alone is insufficient. At 50,000 rows, after successful close:

| Metric (bytes) | `cf8f1a5` | Current | Prototype | Prototype versus current |
| --- | ---: | ---: | ---: | ---: |
| Retained B-tree payload | 57,455,057 | 49,156,708 | 59,831,026 | +21.7% |
| Main file after close | 68,014,080 | 61,399,040 | 105,791,488 | +72.3% |
| Simultaneous VFS lifetime peak | 68,183,632 | 68,249,128 | 143,726,072 | +110.6% |

The same prototype retained payload is +4.1% versus the older `cf8f1a5`
comparator, while its physical main file is +55.5% and its peak is +110.8%.
The 10,000-row UUID test shows similar direction: +22.1% retained content,
+67.6% main file and +102.7% peak versus current. The 50,000-row inverse
codec maximum is 22,200,323 bytes; pinned-only body maximum is zero for this
import-only workload. Inverse/pin rows and orphan bodies are zero after close,
but freed pages remain in the main file. These are durable disk costs, even when
the corresponding Undo content has expired. See the
[UUID cost summary](uuid/cost-summary.csv), [comparisons](uuid/comparisons.csv)
and raw per-phase logs for exact values and provenance.

The coordinator's [runner](run.py) freezes executable copies and provenance and
records raw per-phase logs and final DBSTAT data. The [summarizer](summarize.py)
refuses an incomplete matrix and compares prototype costs separately with both
baseline and current production. It reports measurements without applying the
superseded no-growth threshold. The completed results support the budget proposal
below; no larger storage cap is inferred from this investigation's authorization.

### Proposed next-stage benchmark gates — not accepted

The owner declined to adopt this proposal pending investigation of compaction,
ordinary-backup behavior and reportable reusable space. The owner has about
19,000 accumulated transactions; UUID 50,000 is an import-only scale probe, not
a claim about typical editing volume. Preserve these uncompacted results while
investigating disposable synthetic copies. No production Compact feature is
authorized by this measurement follow-up.

Pending owner decision, the proposed next-stage
budget is measured against **unchanged current storage at `b4b65df`** on each of
these fixed synthetic cases: all five modes at 1,000 and 10,000 rows, plus the
UUID-length import cases at 10,000 and 50,000 rows.

| Dimension | Proposed maximum relative to current |
| --- | ---: |
| Retained B-tree payload after successful close | 1.30× |
| Physical main-file length after successful close | 2.00× |
| Simultaneous VFS lifetime peak | 2.50× |

These are proposed regression thresholds with deliberate headroom above observed
costs, not measured requirements, an accepted waiver, or a promise that the final
feature will fit. Every completed benchmark case fits these proposed thresholds.
Preserve
the older `cf8f1a5` comparison as historical context rather than silently switching
comparators when a result is less favorable.

The proposal applies to these workloads only. Session growth depends on the
number and size of actions and removed payloads; no fixed percentage is a universal
bound for arbitrary sessions or datasets. The budget must never cause silent
truncation, early expiry or weakened durability. Successful close must still leave
zero inverse/action rows, zero pins and zero payload bodies without a live owner.
Failed/cancelled close and crashes retain the required recovery state.

If the owner accepts these provisional gates, broad implementation must be
remeasured with its complete coordinator, receipt, payload and command coverage
before delivery, with correctness/recovery gates unchanged. If this lasting-file
and temporary-disk envelope is unacceptable, rework the representation and repeat
the measurements before broad conversion. No numeric budget or implementation
resumption beyond the measurement scope is inferred from the earlier Go.

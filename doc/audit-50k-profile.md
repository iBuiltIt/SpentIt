# 50k full-audit profiling

Date: 2026-11-06. Status: **three follow-up improvements implemented under owner Go; verification below**.

The preceding Undo/audit improvements are committed as `0edf528`. This investigation
uses synthetic data only; it neither opens private datasets nor changes application
validation. See the [earlier report](undo-audit-profile.md) for the 15k comparison
and the initial 73.7659-second 50k observation.

## Method and reproduced result

The same driver and final rules-enabled macOS ARM64 Release libraries created a
fresh SQLCipher account with 50,000 distinct ordinary transactions, accepted all,
deleted the whole selection, undid deletion and ran full verification. It asserted
zero rows/balance after Delete and the original row count/exact balance after Undo.
No transfers, category definitions or currency reassignments occur in this workload.
Setup and UI refresh are excluded from phase times; operating-system caches were
not flushed. No agent builds or tests ran concurrently with a timed run.

The committed-build driver SHA-256 was
`f9c1ff7b40f5919a37c8b83249d4d243c2131df3830911c19c133e51101dfbf6`.
One diagnostics-enabled run with three five-second native samples from its exact
PID reproduced Delete / Undo / audit at **28.9854 / 22.0495 / 74.4080 seconds**.
Every assertion passed. Sampling is observational overhead; this is a reproduction
and phase attribution, not a new uninstrumented median.

| Audit phase | Seconds | Share |
|---|---:|---:|
| Committed-history replay | 29.761496 | 40.0% |
| Category/provenance validation | 27.989840 | 37.6% |
| Imported evidence | 9.206485 | 12.4% |
| Rules | 3.741120 | 5.0% |
| Integrity/cipher/FK | 2.168204 | 2.9% |
| Transfers | 1.502034 | 2.0% |

The transfer phase still performs reconciliation on this no-transfer workload;
its time is not a measurement of transfer-heavy behavior. Other phase time is small.
There were 146 statement preparations and 14,374,035 reuses, versus 146 and
2,174,979 in the preceding final 15k profile. Rows grew 3.33× but statement uses
6.61×. Preparation is not the dominant remaining problem.

The five-second samples began approximately 0, 20 and 45 seconds into the audit.
The middle sample had 3,701 main-thread samples: 3,699 were within category
validation and 2,459 within `entry_snapshot`. The last sample had 3,671 main-thread
samples, all within committed replay: 2,976 were within `entry_snapshot`, including
1,213 within `load_row`. These are nested inclusive counts and must not be added.
They identify reconstruction/read paths, not individual SQL costs.

## Observation-only cache/query instrumentation

A temporary copy of the storage translation unit counted audit-only source-row
and snapshot cache hits/misses/admission failures, and summed each SQL template's
`sqlite3_step` duration, VM instructions and full-scan steps. It used the same
compiler optimization, SQLCipher and dependency libraries. No query, cache budget,
validation predicate or transaction boundary was changed. Only the temporary
synthetic driver enabled collection, and logs contain SQL templates and aggregate
counts, never bound parameters. This instrumentation was not added to the app.

The SQL count means step calls (including row and done results), not executions.
Step timings exclude statement binding, construction, copying and C++ validation;
instrumentation overhead prevents substituting their wall times for baseline
medians. The temporary instrumentation's SQL counters reset on each statement
checkout, avoiding accumulation from prepared-statement reuse.

Both observation-only runs passed every financial assertion and full validation.
Their audits took 11.3449 s at 15k and 73.6044 s at 50k. The committed application
run's phase attribution was reproduced closely; these timings are not independent
performance baselines because of instrumentation.

| Audit cache/work | 15k rows | 50k rows |
|---|---:|---:|
| Source-row cache hits | 135,000 | 442,372 |
| Source-row cache misses/reconstructions | 15,000 | 268,192 |
| Source rows retained | 15,000 | 36,363 |
| Source-row estimated bytes | 55,365,000 | 134,215,833 |
| Source-row admission refusals | 0 | 231,829 |
| Snapshot cache hits | 105,000 | 139,436 |
| Snapshot misses/reconstructions | 45,000 | 360,564 |
| Distinct snapshots in workload | 45,000 | 150,000 |
| Snapshots retained | 45,000 | 59,758 |
| Snapshot estimated bytes | 50,535,000 | 67,108,234 |
| SQL step calls | 2,880,136 | 17,671,944 |
| SQL VM instructions | 74,377,171 | 389,147,934 |
| SQL full-scan steps | 614,993 | 2,049,993 |
| Time inside SQL step calls | 7.003628 s | 46.244308 s |

**Measured cause: bounded caches stop admitting entries, rather than replacing
older entries.** [VerificationRows](../src/storage/import_store.cpp) has a 128 MiB /
65,536-row budget; [entry_snapshot](../src/storage/committed_store.inc) has a
64 MiB / 65,536-snapshot budget. Both hit their byte limits first in the 50k
fixture. The first rows remain resident throughout verification; later rows remain
uncached however often they are revisited. The 15k fixture fits both budgets.

Consequently, 50k has 210,564 snapshot reconstructions beyond its 150,000 distinct
snapshots, and 218,192 source-row reconstructions beyond its 50,000 distinct rows.
`load_row` reconstructs the complete RowView, including current draft, source
associations, ownership, metadata and state. Historical consumers often need only
its source facts. These misses amplify many individually targeted queries. The
counts explain the observed change in slope; they do not establish an asymptotic
quadratic algorithm or the exact speedup of a proposed cache policy.

The revision-to-history map introduced by `0edf528` is a separate cache: this
workload has 100,000 change/Undo revisions, within its 262,144-member and 64 MiB
budgets. Its budget-exhaustion fallback is not the cause of this cliff.

### Where SQL time goes

Top targeted reads in the instrumented 50k audit:

| Query purpose | Step calls | Seconds | Full-scan steps |
|---|---:|---:|---:|
| Snapshot allocations (`entry_allocation_values`) | 721,128 | 7.2864 | 0 |
| Historical currency timeline | 978,759 | 4.6309 | 0 |
| Revision-zero acceptance/history | 380,480 | 2.0599 | 0 |
| Associated source IDs | 268,192 | 1.9871 | 0 |
| Snapshot fields (`entry_values`) | 360,564 | 1.4664 | 0 |
| Draft allocation metadata | 836,384 | 1.3797 | 0 |
| Committed currency-reassignment markers | 410,564 | 1.3239 | 0 |
| Pending deletion state | 268,192 | 1.2875 | 0 |

The allocation query took 0.8810 s for 90,000 steps at 15k. Its step count grew
8.01× and time 8.27×; the main growth is repeated work, not a newly expensive single
lookup. Historical currency performs almost a million timeline queries even though
this fixture has no currency changes; reading account creation currency adds
another 978,759 steps / 0.8040 s. Empty and unchanged facts still cost SQL calls.

Total full-scan steps grew 3.33×, proportional to row count. Leading scans are
whole-history counts, ordered snapshot enumeration and ordering reconciliation.
The hot point reads above reported zero full scans and bounded VM work per use.
This does **not** prove every query is optimal, but it argues against another
blanket index addition as the first response. The preceding OR-query scan problem
has been replaced here by redundant reconstruction and read amplification.

## Proposed improvements (subsequently authorized)

1. **Use bounded audit working sets that follow the current transaction.** Keep
   source facts and the current/prior/original snapshots together while checking
   a transaction or receipt. Category validation already reads a head before
   calling `entry_snapshot`; pass that known source rather than reconstructing it
   again. Keep the head through consecutive revisions in the ordered category
   cursor. Carry the same source through committed before/after and original-Undo
   receipt validation. Replace audit-only first-entry retention with a bounded
   rolling/LRU policy where reuse spans calls. Retain the byte/member limits and
   recompute on eviction. Do not cache mutable source associations or heads across
   commands. This targets the measured cache cliff without merely moving it by
   increasing memory budgets.
2. **Read snapshot data in ordered batches/streams.** Supply fields, allocations,
   allocation identities, ownership and rule metadata from bounded ordered cursors
   or chunks instead of several point queries for each reconstructed snapshot.
   Feed the same engine validators and exact before/after/dependency/balance checks.
   Preserve order/position checks, orphan/extra-row detection and exact counts;
   do not use inner joins that silently hide missing metadata or allocations.
   This targets the remaining mandatory reads as well as repeated reads, and
   should be measured separately from the working-set change. Keep all data in
   memory within explicit limits or use existing encrypted storage; no plaintext
   spill files.
3. **Reuse the audited historical currency timeline.** Load each account's creation
   currency and ordered currency-change/Undo facts once per audit, then resolve
   currency at a history order from that timeline. Retain explicit per-revision
   reassignment markers and all validation of changes, reversals and definitions.
   An empty timeline can be reused only after it is actually read/validated, not
   assumed from the current currency. The measured timeline-plus-creation reads
   alone cost 5.43 s at 50k; that is a work opportunity, not a promised wall-time
   saving. A bounded indexed fallback must remain correct for large histories.

Prioritize the working-set change, then streamed reconstruction; the currency
lookup is a smaller, independently measurable follow-up. Do not skip the full
history pass, omit source validation, weaken integrity checks, certify early, or
replace exact comparisons with unchecked hashes. The SQLite integrity/cipher/FK
phase is only 2.9% of the reproduced audit and should remain intact. Transfer-heavy
optimization remains separately unmeasured.

## Acceptance checks from the proposal

- Repeat serial 15k and 50k runs, adding sizes around both observed byte-budget
  thresholds (about 20k rows for these snapshots and 36k for these RowViews).
  Vary split counts/text lengths: thresholds are bytes, not universal row limits.
- Report cache admissions/evictions, reconstructions, SQL steps/VM work, memory,
  phase timings, medians and ranges. Compare each change separately; do not promise
  an elapsed-time target from these counts alone.
- Force eviction and budget fallback while validating deep histories, currency
  changes/Undo, categories/rule ownership, split provenance, source associations,
  duplicate dependencies and linked transactions.
- Reject missing, duplicate, orphaned and out-of-order facts in every stream,
  including facts on either side of a chunk/eviction boundary. Compare rejection
  behavior with the current verifier on synthetic damaged files.
- Preserve cancellation checkpoints and unchanged file bytes on cancelled audits;
  retry cleanly. Repeat financial recovery, strict Undo and sanitizer coverage for
  any shared reader changed. No changes to durable/atomic command boundaries.

At the profiling checkpoint, only documentation changed after the implementation
commit; the later implementation is described below. Measurements cover synthetic
macOS repository execution, not native UI response, private data, other platforms
or transfer-heavy accounts.

## Implementation follow-up — owner Go 2026-11-06

The three improvement areas are implemented; schema remains 58/57. The source-row
cache retains its original bounded admission policy. The snapshot cache retains
a stable prefix (up to 60 MiB / 61,440 entries), with remaining space used for
least-recently-used replacement within the total 64 MiB / 65,536-entry budget,
including estimated recency bookkeeping. Oversized entries are reconstructed
without caching. Category verification retains
one head/source while walking its ordered revisions and passes that source into
snapshot reconstruction. Recency state and currency timelines are scoped to the
read-only audit; they do not cache mutable financial state across commands.

The new snapshot reader streams one snapshot at a time through one ordered
`UNION ALL` query containing tagged snapshot fields, allocation values, ownership
metadata and optional rule marks. It preserves separate streams for allocation
values and metadata rather than joining away missing rows. Required fields,
positions, allocation/metadata cardinality, rule-mark pairs and ownership,
category bindings, exact money and draft validation are all checked. Existing
whole-history counts, ordering, orphan/FK checks and exact replay remain enabled.
No multi-snapshot chunk is materialized. Cancellation is checked while consuming
the stream; temporary SQL storage retains the existing memory-only setting.

Currency creation and ordered change/Undo definitions are read once per account
within an audit, with binary search by history order. The cache has a 32 MiB /
65,536-entry data budget and at most 4,096 account slots; oversized timelines
release their partial data and use the unchanged exact query. Currency-reassignment
markers retain their original validation. Mutation paths do not use this cache.
No transaction boundary, durable gate, Undo eligibility or file-format rule changed.

New corruption cases cover missing snapshot metadata and displaced allocation
positions. An opt-in large-text fixture (`spentit_review_sheet_tests audit_eviction`)
uses 10,000 rows with 6,000-character memos to cross both byte budgets, then checks
cancellation after category verification, retry, reopened Undo and corruption in
a deleted historical revision of the final row. It is separate from timing fixtures.


### Verification of the implementation

The selected macOS Release suite passed 36/36 checks, including committed replay,
category/rule validation, account lifecycle, linked deletion and fault injection,
selection edits and faults, cancellation, strict Undo, transfer behavior, import
rollback, and offscreen UI/QML. Before the final cache-policy adjustment, focused
rules-disabled checks passed 8/8, ASan/UBSan checks passed 8/8 plus a five-check
rerun, and the large-text eviction fixture passed in Release and ASan/UBSan.

After replacing whole-cache LRU with the final stable/rolling policy, ten focused
Release checks, six rules-disabled checks and six ASan/UBSan checks passed. The
large-text fixture was rerun and passed in Release; its earlier sanitizer run
exercised the whole-cache LRU trial, not the final stable/rolling admission policy.
These checks do not establish native large-selection interaction or Linux/Windows
behavior.

The stream and cache changes were benchmarked together, not isolated individually.
The 32 MiB currency-timeline overflow fallback was not separately forced; normal
currency changes/Undo and the unchanged uncached query are covered by account
lifecycle checks. Transfer-heavy performance remains unmeasured. No manual text
or screenshots changed because this work changes no user-visible workflow.


### Rejected whole-cache LRU trial

A first implementation applied LRU replacement to both complete caches. Three
15k audits took 9.53925–9.73685 s (median 9.72919 s), but the serial 50k audit
regressed to 86.2779 s. Instrumentation reproduced 83.9923 s, with 363,650 source
reconstructions and 402,504 snapshot reconstructions, versus 268,192 / 360,564
before the change. SQL VM work increased to 461,066,627 instructions. Sequential
passes displaced data needed by later passes. All correctness assertions passed;
the policy was rejected for performance. The final policy above restores stable
source retention and reserves most snapshot space against sequential eviction.


### Final serial benchmark results

Fresh synthetic datasets, same Delete / Undo / full-audit driver and assertions as
above; no concurrent agent builds/tests, no cache flush and no UI timing. The
final uninstrumented driver SHA-256 is
`71e3b6c9139f90857c7f432b325af09d73b34c0e8eec16b5507fd1d00dae6cf1`.
The observation-only counter driver SHA-256 is
`e35d2484d9d491c8efd4a963b3ab247dca4b34df71914e76726d8fe81ffdc1c8`.

Three final 15k runs passed:

| Operation | Prior median | Final median | Final range |
|---|---:|---:|---:|
| Delete | 8.27868 s | 8.14710 s | 7.63341–8.46165 s |
| Undo | 6.72186 s | 6.96275 s | 6.77193–7.30479 s |
| Full audit | 11.59040 s | 9.67948 s | 9.63375–9.96292 s |

The 15k audit improved 16.5% (1.20×). Undo was 3.6% slower in these runs; this
follow-up primarily changes audit reads and does not establish an Undo speedup.
Whole-process maximum RSS ranged from 224,428,032 to 264,929,280 bytes.

The final uninstrumented 50k run passed at **28.3940 s Delete, 21.8212 s Undo,
57.9683 s full audit**, versus the earlier 29.4786 / 22.6381 / 73.7659 s.
The audit improved 21.4% (1.27×); this is a single-run comparison, not a median.
Whole-process maximum RSS was 677,150,720 bytes (645.8 MiB), including setup and
command execution, not just the bounded audit caches. Audit time still grows
faster than transaction count across the cache thresholds; this is an improvement,
not elimination of the large-history cost.


The final counter run passed at 58.0461 s full audit. Its observations support the
plain timing result (instrumentation is not a separate baseline):

| 50k audit work | Prior | Final |
|---|---:|---:|
| Source reconstructions | 268,192 | 184,014 |
| Snapshot reconstructions | 360,564 | 341,639 |
| SQL step calls | 17,671,944 | 12,569,745 |
| SQL VM instructions | 389,147,934 | 372,223,208 |
| SQL full-scan steps | 2,049,993 | 2,049,993 |
| Statement preparations / reuses | 146 / 14,374,035 | 141 / 8,789,191 |

Final source hits were 257,625, snapshot hits 158,361 and snapshot evictions
285,103. Estimated peak cache bytes were 134,215,833 source / 67,108,232 snapshot,
within the unchanged 128/64 MiB budgets. Source misses fell 31.4%, snapshot misses
5.2% and SQL calls 28.9%; VM work fell only 4.3% because a streamed query does more
work per call. The unchanged full-scan count is consistent with retaining all
whole-history validation passes.

| Final audit phase | Seconds |
|---|---:|
| Committed replay | 24.361843 |
| Category/provenance | 18.942791 |
| Imported evidence | 7.966456 |
| Rules | 3.365003 |
| Integrity/cipher/FK | 1.965011 |
| Transfers | 1.407806 |

Committed replay and category/provenance still account for about 75% of audit time.
The remaining 341,639 snapshot reconstructions exceed the 150,000 distinct
snapshots; the bounded caches still cannot retain an entire large history.
Further work should target shared ordered replay/source reconstruction, measured
against these counters, rather than reducing integrity checks or skipping history.


Intermediate final-build scaling runs also passed:

| Rows | Delete | Undo | Full audit | Whole-process max RSS |
|---|---:|---:|---:|---:|
| 20,000 | 10.2403 s | 8.65997 s | 13.5602 s | 336,445,440 bytes |
| 36,000 | 18.9151 s | 15.3798 s | 30.9740 s | 548,798,464 bytes |

These are single runs around the previously observed cache thresholds, not
universal row limits: allocation count and text size change cache occupancy.


A final 5,000-row deeper-history run repeated Delete, Undo and full audit five
times on one file, asserting zero rows/balance after every Delete and the original
count/exact balance after every Undo. All five cycles passed. Audit times were
3.40038 / 4.78132 / 5.95716 / 6.98212 / 10.0218 s as history grew to 55,000
snapshots. Delete stayed within 2.68148–2.79123 s and Undo within
2.07524–2.34239 s. Whole-process maximum RSS was 150,192,128 bytes. This confirms
correct deeper replay and records its remaining growth; it is not a new baseline
comparison or a bound on arbitrary histories.
